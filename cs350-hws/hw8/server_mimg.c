#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <sched.h>
#include <signal.h>
#include <assert.h>
#include <pthread.h>

/* Needed for wait(...) */
#include <sys/types.h>
#include <sys/wait.h>

/* Needed for semaphores */
#include <semaphore.h>

/* Include struct definitions and other libraries that need to be
 * included by both client and server */
#include "common.h"

#define BACKLOG_COUNT 100
#define USAGE_STRING				\
	"Missing parameter. Exiting.\n"		\
	"Usage: %s -q <queue size> "		\
	"-w <workers: 1> "			\
	"-p <policy: FIFO> "			\
	"<port_number>\n"

/* 4KB of stack for the worker thread */
#define STACK_SIZE (4096)

/* Mutex needed to protect the threaded printf. DO NOT TOUCH */
sem_t * printf_mutex;

/* Synchronized printf for multi-threaded operation */
#define sync_printf(...)			\
	do {					\
		sem_wait(printf_mutex);		\
		printf(__VA_ARGS__);		\
		sem_post(printf_mutex);		\
	} while (0)

/* START - Variables needed to protect the shared queue. DO NOT TOUCH */
sem_t * queue_mutex;
sem_t * queue_notify;
/* END - Variables needed to protect the shared queue. DO NOT TOUCH */

/* Global array of registered images and its length -- reallocated as we go! */
struct image ** images = NULL;
uint64_t image_count = 0;

/* SEMAPHORES */
sem_t ** image_locks = NULL;
sem_t * image_array_lock;
sem_t * socket_lock;

/* Track amount of requests being processed and already completed */
uint64_t * requests_received = NULL;
uint64_t * requests_completed = NULL;

struct request_meta {
	struct request request;
	struct timespec receipt_timestamp;
	struct timespec start_timestamp;
	struct timespec completion_timestamp;
};

enum queue_policy {
	QUEUE_FIFO,
	QUEUE_SJN
};

struct queue {
	size_t wr_pos;
	size_t rd_pos;
	size_t max_size;
	size_t available;
	enum queue_policy policy;
	struct request_meta * requests;
};

struct connection_params {
	size_t queue_size;
	size_t workers;
	enum queue_policy queue_policy;
};

struct worker_params {
	int conn_socket;
	int worker_done;
	struct queue * the_queue;
	int worker_id;
};

enum worker_command {
	WORKERS_START,
	WORKERS_STOP
};


void queue_init(struct queue * the_queue, size_t queue_size, enum queue_policy policy)
{
	the_queue->rd_pos = 0;
	the_queue->wr_pos = 0;
	the_queue->max_size = queue_size;
	the_queue->requests = (struct request_meta *)malloc(sizeof(struct request_meta)
						     * the_queue->max_size);
	the_queue->available = queue_size;
	the_queue->policy = policy;
}

/* Add a new request <request> to the shared queue <the_queue> */
int add_to_queue(struct request_meta to_add, struct queue * the_queue)
{
	int retval = 0;

	sem_wait(queue_mutex);

	if (the_queue->available == 0) {
		retval = 1;
	} else {
		the_queue->requests[the_queue->wr_pos] = to_add;
		the_queue->wr_pos = (the_queue->wr_pos + 1) % the_queue->max_size;
		the_queue->available--;

		sem_post(queue_notify);
	}

	sem_post(queue_mutex);

	return retval;
}

/* Add a new request <request> to the shared queue <the_queue> */
struct request_meta get_from_queue(struct queue * the_queue)
{
	struct request_meta retval;

	sem_wait(queue_notify);
	sem_wait(queue_mutex);

	retval = the_queue->requests[the_queue->rd_pos];
	the_queue->rd_pos = (the_queue->rd_pos + 1) % the_queue->max_size;
	the_queue->available++;

	/* Move this to ensure requests are actually being processed in order */
	// sem_post(queue_mutex);

	return retval;
}

void dump_queue_status(struct queue * the_queue)
{
	size_t i, j;

	sem_wait(queue_mutex);
	sem_wait(printf_mutex);

	printf("Q:[");

	for (i = the_queue->rd_pos, j = 0; j < the_queue->max_size - the_queue->available;
	     i = (i + 1) % the_queue->max_size, ++j)
	{
		printf("R%ld%s", the_queue->requests[i].request.req_id,
		       ((j+1 != the_queue->max_size - the_queue->available)?",":""));
	}

	printf("]\n");
	sem_post(printf_mutex);

	sem_post(queue_mutex);
}

void register_new_image(int conn_socket, struct request * req)
{
	/* Lock the image registration array */
	sem_wait(image_array_lock);

	/* Increase the count of registered images */
	image_count++;

	/* Reallocate array of image pointers */
	images = realloc(images, image_count * sizeof(struct image *));
	if (!images) {	// Check if reallocation was successful
		perror("Failed to allocate memory for images");
		exit(EXIT_FAILURE);
	}

	/* Reallocate for image locks */
	image_locks = realloc(image_locks, image_count * sizeof(sem_t *));
	if (!image_locks) {
		perror("Failed to allocate memory for image_locks");
		exit(EXIT_FAILURE);
	}

	requests_completed = realloc(requests_completed, image_count * sizeof(uint64_t));
	requests_received = realloc(requests_received, image_count * sizeof(uint64_t));
	if (!requests_completed || !requests_received) {
		perror("Failed to allocate memory for requests_completed or requests_received");
		exit(EXIT_FAILURE);
	}

	/* Read in the new image from socket */
	struct image * new_img = recvImage(conn_socket);

	/* Store its pointer at the end of the global array */
	images[image_count - 1] = new_img;

	image_locks[image_count - 1] = malloc(sizeof(sem_t));
	sem_init(image_locks[image_count - 1], 0, 1);

	requests_completed[image_count - 1] = 0;
	requests_received[image_count - 1] = 0;

	/* Unlock the image registration array */
	sem_post(image_array_lock);

	/* Immediately provide a response to the client */
	struct response resp;
	resp.req_id = req->req_id;
	resp.img_id = image_count - 1;
	resp.ack = RESP_COMPLETED;

	/* Lock the connection socket */
	sem_wait(socket_lock);

	send(conn_socket, &resp, sizeof(struct response), 0);

	/* Unlock the connection socket */
	sem_post(socket_lock);
}

/* Main logic of the worker thread */
void * worker_main (void * arg)
{
	struct timespec now;
	struct worker_params * params = (struct worker_params *)arg;

	clock_gettime(CLOCK_MONOTONIC, &now);
	sync_printf("[#WORKER#] %lf Worker Thread Alive!\n", TSPEC_TO_DOUBLE(now));

	while (!params->worker_done) {
		struct request_meta req;
		struct response resp;
		struct image * img = NULL;
		uint64_t img_id;

		uint64_t my_order;

		/*-------------------------------------------------------------------------------------------------------*/
		/* Get from queue and lock access */
		req = get_from_queue(params->the_queue);

		img_id = req.request.img_id;

		/* Protect access */
		sem_wait(image_array_lock);

		/* Requests can go out of order, create my_order */
		my_order = requests_received[img_id];

		/* Increment amount of requests being processed */
		requests_received[img_id]++;

		/* Unlock access */
		sem_post(image_array_lock);

		/* Unlock access to queue */
		sem_post(queue_mutex);
		/*-------------------------------------------------------------------------------------------------------*/

		if (params->worker_done)
			break;

		clock_gettime(CLOCK_MONOTONIC, &req.start_timestamp);

		/*-------------------------------------------------------------------------------------------------------*/
		/* Spinlock: wait until my_order == requests_completed[img_id] */
		sem_t * this_img_sem;
        while (1) {
			sem_wait(image_array_lock);

			if (my_order == requests_completed[img_id] ) {
				this_img_sem = image_locks[img_id];
				sem_post(image_array_lock);
				break; 
			}

			sem_post(image_array_lock);
		}
		/*-------------------------------------------------------------------------------------------------------*/

		/* Lock the image before accessing */
		sem_wait(this_img_sem);

		uint64_t original_img_id = img_id;

		img = images[img_id];
		assert(img != NULL);

		switch (req.request.img_op) {
		case IMG_ROT90CLKW:
			img = rotate90Clockwise(img, NULL);
			break;
		case IMG_BLUR:
		    img = blurImage(img, NULL);
			break;
		case IMG_SHARPEN:
		    img = sharpenImage(img, NULL);
			break;
		case IMG_VERTEDGES:
		    img = detectVerticalEdges(img, NULL);
			break;
		case IMG_HORIZEDGES:
		    img = detectHorizontalEdges(img, NULL);
			break;
		}

		if (req.request.img_op != IMG_RETRIEVE) {
			if (req.request.overwrite) {
				sem_wait(image_array_lock);
				deleteImage(images[img_id]);
				images[img_id] = img;
				sem_post(image_array_lock);
			} else {
				/* Lock image array for update */
				sem_wait(image_array_lock);
				
				uint64_t new_img_id = image_count++;
				images = realloc(images, image_count * sizeof(struct image *));
				if (!images) {
					perror("Failed to allocate memory for images");
					exit(EXIT_FAILURE);
				}
				
				/* Reallocate memory for locks */
				image_locks = realloc(image_locks, image_count * sizeof(sem_t *));
				if (!image_locks) {
					perror("Failed to allocate memory for image_locks");
					exit(EXIT_FAILURE);
				}
				requests_completed = realloc(requests_completed, image_count * sizeof(uint64_t));
				requests_received = realloc(requests_received, image_count * sizeof(uint64_t));
				if (!requests_completed || !requests_received) {
					perror("Failed to allocate memory for requests_completed or requests_received");
					exit(EXIT_FAILURE);
				}

				images[new_img_id] = img;

				image_locks[new_img_id] = malloc(sizeof(sem_t));
				sem_init(image_locks[new_img_id], 0, 1);

				requests_completed[new_img_id] = 0;
				requests_received[new_img_id] = 0;
				
				/* Unlock image array */
				sem_post(image_array_lock);	

				img_id = new_img_id;
			}
		}

		clock_gettime(CLOCK_MONOTONIC, &req.completion_timestamp);

		/* Lock the connection socket before sending */
		sem_wait(socket_lock);

		resp.req_id = req.request.req_id;
		resp.ack = RESP_COMPLETED;
		resp.img_id = img_id;

		send(params->conn_socket, &resp, sizeof(struct response), 0);

		if (req.request.img_op == IMG_RETRIEVE) {
			uint8_t err = sendImage(img, params->conn_socket);

			if(err) {
				ERROR_INFO();
				perror("Unable to send image payload to client.");
			}
		}

		/* Unlock the connection socket after sending */
		sem_post(socket_lock);

		/*-------------------------------------------------------------------------------------------------------*/
		/* Lock access */
		sem_wait(image_array_lock);
		
        requests_completed[original_img_id]++;

		/* Unlock access */
        sem_post(image_array_lock);		
		/*-------------------------------------------------------------------------------------------------------*/

		/* Unlock the image */
		sem_post(this_img_sem);

		sync_printf("T%d R%ld:%lf,%s,%d,%ld,%ld,%lf,%lf,%lf\n",
		       params->worker_id, req.request.req_id,
		       TSPEC_TO_DOUBLE(req.request.req_timestamp),
		       OPCODE_TO_STRING(req.request.img_op),
		       req.request.overwrite, req.request.img_id, img_id,
		       TSPEC_TO_DOUBLE(req.receipt_timestamp),
		       TSPEC_TO_DOUBLE(req.start_timestamp),
		       TSPEC_TO_DOUBLE(req.completion_timestamp));

		dump_queue_status(params->the_queue);
	}

	return NULL;
}

int control_workers(enum worker_command cmd, size_t worker_count, struct worker_params * common_params)
{
	static pthread_t * worker_pthreads = NULL;
	static struct worker_params ** worker_params = NULL;
	static int * worker_ids = NULL;

	if (cmd == WORKERS_START) {
		size_t i;

		worker_pthreads = (pthread_t *)malloc(worker_count * sizeof(pthread_t));
		worker_params = (struct worker_params **)
		malloc(worker_count * sizeof(struct worker_params *));
		worker_ids = (int *)malloc(worker_count * sizeof(int));

		if (!worker_pthreads || !worker_params || !worker_ids) {
			ERROR_INFO();
			perror("Unable to allocate arrays for threads.");
			return EXIT_FAILURE;
		}

		for (i = 0; i < worker_count; ++i) {
			worker_ids[i] = -1;

			worker_params[i] = (struct worker_params *)malloc(sizeof(struct worker_params));

			if (!worker_params[i]) {
				ERROR_INFO();
				perror("Unable to allocate memory for thread.");
				return EXIT_FAILURE;
			}

			worker_params[i]->conn_socket = common_params->conn_socket;
			worker_params[i]->the_queue = common_params->the_queue;
			worker_params[i]->worker_done = 0;
			worker_params[i]->worker_id = i;
		}

		for (i = 0; i < worker_count; ++i) {
			worker_ids[i] = pthread_create(&worker_pthreads[i], NULL, worker_main, worker_params[i]);

			if (worker_ids[i] < 0) {
				ERROR_INFO();
				perror("Unable to start thread.");
				return EXIT_FAILURE;
			} else {
				printf("INFO: Worker thread %ld (TID = %d) started!\n", i, worker_ids[i]);
			}
		}
	}
	else if (cmd == WORKERS_STOP) {
		size_t i;

		if (!worker_pthreads || !worker_params || !worker_ids) {
			return EXIT_FAILURE;
		}

		for (i = 0; i < worker_count; ++i) {
			if (worker_ids[i] < 0) {
				continue;
			}
			worker_params[i]->worker_done = 1;
		}

		for (i = 0; i < worker_count; ++i) {
			if (worker_ids[i] < 0) {
				continue;
			}
			sem_post(queue_notify);
		}

        for (i = 0; i < worker_count; ++i) {
            pthread_join(worker_pthreads[i],NULL);
            printf("INFO: Worker thread exited.\n");
        }

		for (i = 0; i < worker_count; ++i) {
			free(worker_params[i]);
		}

		free(worker_pthreads);
		worker_pthreads = NULL;

		free(worker_params);
		worker_params = NULL;

		free(worker_ids);
		worker_ids = NULL;
	}
	else {
		ERROR_INFO();
		perror("Invalid thread control command.");
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

/* Main function to handle connection with the client. This function
 * takes in input conn_socket and returns only when the connection
 * with the client is interrupted. */
void handle_connection(int conn_socket, struct connection_params conn_params)
{
	struct request_meta * req;
	struct queue * the_queue;
	size_t in_bytes;

	struct worker_params common_worker_params;
	int res;

	the_queue = (struct queue *)malloc(sizeof(struct queue));
	queue_init(the_queue, conn_params.queue_size, conn_params.queue_policy);

	common_worker_params.conn_socket = conn_socket;
	common_worker_params.the_queue = the_queue;
	res = control_workers(WORKERS_START, conn_params.workers, &common_worker_params);

	if (res != EXIT_SUCCESS) {
		free(the_queue);

		control_workers(WORKERS_STOP, conn_params.workers, NULL);
		return;
	}
	req = (struct request_meta *)malloc(sizeof(struct request_meta));

	do {
		in_bytes = recv(conn_socket, &req->request, sizeof(struct request), 0);
		clock_gettime(CLOCK_MONOTONIC, &req->receipt_timestamp);

		if (in_bytes > 0) {

			if(req->request.img_op == IMG_REGISTER) {
				clock_gettime(CLOCK_MONOTONIC, &req->start_timestamp);

				register_new_image(conn_socket, &req->request);

				clock_gettime(CLOCK_MONOTONIC, &req->completion_timestamp);

				sync_printf("T%ld R%ld:%lf,%s,%d,%ld,%ld,%lf,%lf,%lf\n",
				       conn_params.workers, req->request.req_id,
				       TSPEC_TO_DOUBLE(req->request.req_timestamp),
				       OPCODE_TO_STRING(req->request.img_op),
				       req->request.overwrite, req->request.img_id,
				       image_count - 1, /* Registered ID on server side */
				       TSPEC_TO_DOUBLE(req->receipt_timestamp),
				       TSPEC_TO_DOUBLE(req->start_timestamp),
				       TSPEC_TO_DOUBLE(req->completion_timestamp));

				dump_queue_status(the_queue);
				continue;
			}

			res = add_to_queue(*req, the_queue);

			if (res) {
				struct response resp;
				resp.req_id = req->request.req_id;
				resp.ack = RESP_REJECTED;
				
				/* Lock the connection socket before sending */
				sem_wait(socket_lock);

				send(conn_socket, &resp, sizeof(struct response), 0);

				/* Unlock the connection socket */
				sem_post(socket_lock);

				sync_printf("X%ld:%lf,%lf,%lf\n", req->request.req_id,
				       TSPEC_TO_DOUBLE(req->request.req_timestamp),
				       TSPEC_TO_DOUBLE(req->request.req_length),
				       TSPEC_TO_DOUBLE(req->receipt_timestamp)
					);
			}
		}
	} while (in_bytes > 0);

	control_workers(WORKERS_STOP, conn_params.workers, NULL);

	free(req);
	shutdown(conn_socket, SHUT_RDWR);
	close(conn_socket);
	printf("INFO: Client disconnected.\n");
}


/* Template implementation of the main function for the FIFO
 * server. The server must accept in input a command line parameter
 * with the <port number> to bind the server to. */
int main (int argc, char ** argv) {
	int sockfd, retval, accepted, optval, opt;
	in_port_t socket_port;
	struct sockaddr_in addr, client;
	struct in_addr any_address;
	socklen_t client_len;
	struct connection_params conn_params;
	conn_params.queue_size = 0;
	conn_params.queue_policy = QUEUE_FIFO;
	conn_params.workers = 1;

	/* Parse all the command line arguments */
	while((opt = getopt(argc, argv, "q:w:p:")) != -1) {
		switch (opt) {
		case 'q':
			conn_params.queue_size = strtol(optarg, NULL, 10);
			printf("INFO: setting queue size = %ld\n", conn_params.queue_size);
			break;
		case 'w':
			conn_params.workers = strtol(optarg, NULL, 10);
			printf("INFO: setting worker count = %ld\n", conn_params.workers);
			if (conn_params.workers < 1) {
				ERROR_INFO();
				fprintf(stderr, "At least one worker is needed!\n" USAGE_STRING, argv[0]);
				return EXIT_FAILURE;
			}
			break;
		case 'p':
			if (!strcmp(optarg, "FIFO")) {
				conn_params.queue_policy = QUEUE_FIFO;
			} else {
				ERROR_INFO();
				fprintf(stderr, "Invalid queue policy.\n" USAGE_STRING, argv[0]);
				return EXIT_FAILURE;
			}
			printf("INFO: setting queue policy = %s\n", optarg);
			break;
		default: /* '?' */
			fprintf(stderr, USAGE_STRING, argv[0]);
		}
	}

	if (!conn_params.queue_size) {
		ERROR_INFO();
		fprintf(stderr, USAGE_STRING, argv[0]);
		return EXIT_FAILURE;
	}

	if (optind < argc) {
		socket_port = strtol(argv[optind], NULL, 10);
		printf("INFO: setting server port as: %d\n", socket_port);
	} else {
		ERROR_INFO();
		fprintf(stderr, USAGE_STRING, argv[0]);
		return EXIT_FAILURE;
	}

	/* Now onward to create the right type of socket */
	sockfd = socket(AF_INET, SOCK_STREAM, 0);

	if (sockfd < 0) {
		ERROR_INFO();
		perror("Unable to create socket");
		return EXIT_FAILURE;
	}

	/* Before moving forward, set socket to reuse address */
	optval = 1;
	setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (void *)&optval, sizeof(optval));

	/* Convert INADDR_ANY into network byte order */
	any_address.s_addr = htonl(INADDR_ANY);

	/* Time to bind the socket to the right port  */🤲
	addr.sin_family = AF_INET;
	addr.sin_port = htons(socket_port);
	addr.sin_addr = any_address;

	/* Attempt to bind the socket with the given parameters */
	retval = bind(sockfd, (struct sockaddr *)&addr, sizeof(struct sockaddr_in));

	if (retval < 0) {
		ERROR_INFO();
		perror("Unable to bind socket");
		return EXIT_FAILURE;
	}

	/* Let us now proceed to set the server to listen on the selected port */
	retval = listen(sockfd, BACKLOG_COUNT);

	if (retval < 0) {
		ERROR_INFO();
		perror("Unable to listen on socket");
		return EXIT_FAILURE;
	}

	/* Ready to accept connections! */
	printf("INFO: Waiting for incoming connection...\n");
	client_len = sizeof(struct sockaddr_in);
	accepted = accept(sockfd, (struct sockaddr *)&client, &client_len);

	if (accepted == -1) {
		ERROR_INFO();
		perror("Unable to accept connections");
		return EXIT_FAILURE;
	}

	/* Initilize threaded printf mutex */
	printf_mutex = (sem_t *)malloc(sizeof(sem_t));
	retval = sem_init(printf_mutex, 0, 1);
	if (retval < 0) {
		ERROR_INFO();
		perror("Unable to initialize printf mutex");
		return EXIT_FAILURE;
	}

	/* Initialize queue protection variables. DO NOT TOUCH. */
	queue_mutex = (sem_t *)malloc(sizeof(sem_t));
	queue_notify = (sem_t *)malloc(sizeof(sem_t));
	retval = sem_init(queue_mutex, 0, 1);
	if (retval < 0) {
		ERROR_INFO();
		perror("Unable to initialize queue mutex");
		return EXIT_FAILURE;
	}
	retval = sem_init(queue_notify, 0, 0);
	if (retval < 0) {
		ERROR_INFO();
		perror("Unable to initialize queue notify");
		return EXIT_FAILURE;
	}
	/* DONE - Initialize queue protection variables */

	/*------------------------------------------------------------------------------*/
	/* INITIALIZATION OF SEMAPHORES HERE */
	image_array_lock = malloc(sizeof(sem_t));
    if (!image_array_lock) {
        perror("Failed to allocate image_array_lock");
        exit(EXIT_FAILURE);
    }
    sem_init(image_array_lock, 0, 1);

	socket_lock = malloc(sizeof(sem_t));
    if (!socket_lock) {
        perror("Failed to allocate socket_lock");
        exit(EXIT_FAILURE);
    }
    sem_init(socket_lock, 0, 1);


	/*requests_received = malloc(image_count, sizeof(uint64_t));
	if (!requests_received) {
		perror("Failed to allocate memory for requests_received");
		exit(EXIT_FAILURE);
	}

	requests_completed = malloc(image_count, sizeof(uint64_t));
	if (!requests_completed) {
		perror("Failed to allocate memory for requests_completed");
		exit(EXIT_FAILURE);
	}*/
	
	
	/*------------------------------------------------------------------------------*/

	/* Ready to handle the new connection with the client. */
	handle_connection(accepted, conn_params);

	free(queue_mutex);
	free(queue_notify);

	sem_destroy(socket_lock);
    free(socket_lock);

    sem_destroy(image_array_lock);
    free(image_array_lock);

	for (size_t i = 0; i < image_count; ++i) {
        if (images[i]) deleteImage(images[i]); // Free each image
        if (image_locks[i]) {
            sem_destroy(image_locks[i]);
            free(image_locks[i]);
        }
    }
    free(images);
    free(image_locks);

	free(requests_received);
	free(requests_completed);

	close(sockfd);
	return EXIT_SUCCESS;
}