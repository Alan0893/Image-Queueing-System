/*******************************************************************************
* Single-Threaded FIFO Image Server Implementation w/ Queue Limit
*
* Description:
*     A server implementation designed to process client
*     requests for image processing in First In, First Out (FIFO)
*     order. The server binds to the specified port number provided as
*     a parameter upon launch. It launches a secondary thread to
*     process incoming requests and allows to specify a maximum queue
*     size.
*
* Usage:
*     <build directory>/server -q <queue_size> -w <workers> -p <policy> <port_number>
*
* Parameters:
*     port_number - The port number to bind the server to.
*     queue_size  - The maximum number of queued requests.
*     workers     - The number of parallel threads to process requests.
*     policy      - The queue policy to use for request dispatching.
*
* Author:
*     Renato Mancuso
*
* Affiliation:
*     Boston University
*
* Creation Date:
*     October 31, 2023
*
* Notes:
*     Ensure to have proper permissions and available port before running the
*     server. The server relies on a FIFO mechanism to handle requests, thus
*     guaranteeing the order of processing. If the queue is full at the time a
*     new request is received, the request is rejected with a negative ack.
*
*******************************************************************************/
#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <sched.h>
#include <signal.h>
#include <pthread.h>

/* Needed for wait(...) */
#include <sys/types.h>
#include <sys/wait.h>

/* Needed for semaphores */
#include <semaphore.h>

/* Include struct definitions and other libraries that need to be
 * included by both client and server */
#include "common.h"

/* OTHER IMPORTS */
#include "imglib.h"
#include <inttypes.h>
#include <getopt.h>
#include <getopt.h>
#include <unistd.h>
#include <time.h>

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

typedef struct {
	struct request request;
	struct timespec receipt_timestamp;
	struct timespec start_timestamp;
	struct timespec completion_timestamp;
} RequestMeta;

typedef enum {
	QUEUE_FIFO,
	QUEUE_SJN
} QueuePolicy;

typedef struct {
	size_t rd_pos;
	size_t wr_pos;
	size_t count;
	size_t max_size;
	QueuePolicy policy;
	RequestMeta *queue;
} Queue;

typedef struct {
	size_t queue_size;
	size_t workers;
	QueuePolicy policy;
} ConnectionParams;

typedef struct
{
	Queue *queue;
	int conn_socket;
	int worker_done;
	int worker_id;
	struct ImageStorage *image_storage;
} WorkerParams;

typedef enum {
	WORKERS_START,
	WORKERS_STOP
} WorkerCommand;

/*----------------------------------------------------------------------*/
/* IMAGE STUFF HERE */
typedef struct {
	uint64_t image_id;
	struct image *image;
} ImageItem;

typedef struct {
    size_t image_count;
    ImageItem *img_arr;
    size_t capacity;
} ImageStorage;

uint64_t image_id_counter = 1;
pthread_mutex_t image_storage_lock;

void init_img_storage(ImageStorage *storage, size_t init_cap) {
	storage->image_count = 0;
	storage->capacity = init_cap;
	storage->img_arr = malloc(sizeof(ImageItem) * init_cap);
}

void free_image_storage(ImageStorage *storage) {
	for (size_t i = 0; i < storage->image_count; ++i) {
		deleteImage(storage->img_arr[i].image);
	}
	free(storage->img_arr);
}

uint64_t generate_unique_image_id() {
	return image_id_counter++;
}

int add_image_to_storage(ImageItem *image_to_add, ImageStorage *storage) {
	if (storage->image_count >= storage->capacity) {
		storage->capacity *= 2;
		storage->img_arr = realloc(storage->img_arr, sizeof(ImageItem) * storage->capacity);
	}

	storage->img_arr[storage->image_count++] = *image_to_add;
	return 1;
}
/*----------------------------------------------------------------------*/

void queue_init(Queue *queue, size_t queue_size) {
	queue->max_size = queue_size;
	queue->queue = malloc(sizeof(RequestMeta) * queue_size);
	queue->rd_pos = 0;
	queue->wr_pos = 0;
	queue->count = 0;
	queue->policy = QUEUE_FIFO;
}

/* Add a new request <request> to the shared queue <the_queue> */
int add_to_queue(RequestMeta request, Queue *queue, size_t queue_size, QueuePolicy policy) {
	sem_wait(queue_mutex);

	int retval = 0;
	if (queue->count < queue_size) {
		if (policy == QUEUE_FIFO) {
			queue->queue[queue->wr_pos] = request;
			queue->wr_pos = (queue->wr_pos + 1) % queue_size;
		} else if (policy == QUEUE_SJN) {
			size_t insert_pos = queue->wr_pos;
			while (insert_pos != queue->rd_pos && timespec_cmp(&queue->queue[(insert_pos - 1 + queue->max_size) % queue->max_size].request.req_length, &request.request.req_length) > 0) {
				queue->queue[insert_pos] = queue->queue[(insert_pos - 1 + queue->max_size) % queue->max_size];
				insert_pos = (insert_pos - 1 + queue->max_size) % queue->max_size;
			}
			queue->queue[insert_pos] = request;
			queue->wr_pos = (queue->wr_pos + 1) % queue->max_size;
		}
		queue->count++;

		sem_post(queue_notify);
	} else {
		retval = 1;
	}

	sem_post(queue_mutex);

	return retval;
}

/* Add a new request <request> to the shared queue <the_queue> */
RequestMeta get_from_queue(Queue *queue) {
	sem_wait(queue_notify);
	sem_wait(queue_mutex);

	RequestMeta retval = {0};

	retval = queue->queue[queue->rd_pos];
	queue->rd_pos = (queue->rd_pos + 1) % queue->max_size;
	queue->count--;
	
	sem_post(queue_mutex);

	return retval;
}

void dump_queue_status(Queue *queue) {
	sem_wait(queue_mutex);

	sync_printf("Q:[");

	for (size_t i = 0; i < queue->count; i++) {
		int pos = (queue->rd_pos + i) % queue->max_size;
		sync_printf("R%" PRIu64, queue->queue[pos].request.req_id);
		if (i < queue->count - 1)
			sync_printf(",");
	}
	sync_printf("]\n");

	sem_post(queue_mutex);
}

/* Main logic of the worker thread */
void *worker_main(void *arg) {
	WorkerParams *params = (WorkerParams *)arg;
	struct timespec now;
	RequestMeta req;
	struct response resp;
	int worker_id = params->worker_id;
	ImageStorage *image_storage = params->image_storage;

	clock_gettime(CLOCK_MONOTONIC, &now);
	sync_printf("[#WORKER#] %lf Worker Thread Alive!\n", TSPEC_TO_DOUBLE(now));

	while (!params->worker_done) {
		req = get_from_queue(params->queue);

		struct image *original_img = NULL;
		struct image *modified_img = NULL;
		int img_loc = -1;
		uint8_t err = 0;

		if (params->worker_done)
			break;

		clock_gettime(CLOCK_MONOTONIC, &req.start_timestamp);

		if (req.request.img_op != IMG_REGISTER) {
			for (size_t i = 0; i < image_storage->image_count; i++) {
				if (image_storage->img_arr[i].image_id == req.request.img_id) {
					original_img = image_storage->img_arr[i].image;
					img_loc = i;
					break;
				}
			}
		}

		switch (req.request.img_op) {
			case IMG_ROT90CLKW:
				modified_img = rotate90Clockwise(original_img, &err);
				break;
			case IMG_BLUR:
				modified_img = blurImage(original_img, &err);
				break;
			case IMG_SHARPEN:
				modified_img = sharpenImage(original_img, &err);
				break;
			case IMG_VERTEDGES:
				modified_img = detectVerticalEdges(original_img, &err);
				break;
			case IMG_HORIZEDGES:
				modified_img = detectHorizontalEdges(original_img, &err);
				break;
			case IMG_RETRIEVE:
				resp.req_id = req.request.req_id;
				resp.ack = 0;
				resp.img_id = req.request.img_id;
				send(params->conn_socket, &resp, sizeof(struct response), 0);

				struct image *image_clone = cloneImage(original_img, &err);
				sendImage(image_clone, params->conn_socket);
				deleteImage(image_clone);
				break;
			default:
				resp.ack = RESP_REJECTED;
				break;
		}

		if (modified_img != NULL && req.request.img_op != IMG_RETRIEVE) {
			if (req.request.overwrite && img_loc >= 0) {
				deleteImage(image_storage->img_arr[img_loc].image);
				image_storage->img_arr[img_loc].image = modified_img;
				resp.img_id = req.request.img_id;
			} else if (!req.request.overwrite) {
				uint64_t new_img_id = generate_unique_image_id();
				ImageItem img_item = {.image_id = new_img_id, .image = modified_img};
				add_image_to_storage(&img_item, image_storage);
				resp.img_id = new_img_id;
			}
			clock_gettime(CLOCK_MONOTONIC, &req.completion_timestamp);
			resp.req_id = req.request.req_id;
			resp.ack = RESP_COMPLETED;
			send(params->conn_socket, &resp, sizeof(struct response), 0);
		}

		sync_printf("T%d R%ld:%lf,%s,%d,%lu,%lu,%lf,%lf,%lf\n",
			worker_id,
			req.request.req_id,
			TSPEC_TO_DOUBLE(req.request.req_timestamp),
			OPCODE_TO_STRING(req.request.img_op),
			req.request.overwrite,
			req.request.img_id,
			resp.img_id,
			TSPEC_TO_DOUBLE(req.receipt_timestamp),
			TSPEC_TO_DOUBLE(req.start_timestamp),
			TSPEC_TO_DOUBLE(req.completion_timestamp)
		);

		dump_queue_status(params->queue);
	}

	return NULL;
}

/* This function will start/stop all the worker threads wrapping
 * around the pthread_join/create() function calls */
int control_workers(WorkerCommand command, size_t worker_count, WorkerParams *common_params) {
	static pthread_t *worker_threads = NULL;
	static WorkerParams **worker_params = NULL;

	if (command == WORKERS_START) {
		worker_threads = malloc(worker_count * sizeof(pthread_t));
		worker_params = malloc(worker_count * sizeof(WorkerParams *));

		for (size_t i = 0; i < worker_count; ++i) {
			worker_params[i] = malloc(sizeof(WorkerParams));
			*worker_params[i] = *common_params;
			worker_params[i]->worker_id = i;
			pthread_create(&worker_threads[i], NULL, worker_main, worker_params[i]);
		}
	} else if (command == WORKERS_STOP) {
		for (size_t i = 0; i < worker_count; ++i) {
			worker_params[i]->worker_done = 1;
			sem_post(queue_notify);
			pthread_join(worker_threads[i], NULL);
			free(worker_params[i]);
		}
		free(worker_threads);
		free(worker_params);
	}
	return EXIT_SUCCESS;
}

/* Main function to handle connection with the client. This function
 * takes in input conn_socket and returns only when the connection
 * with the client is interrupted. */
void handle_connection(int conn_socket, ConnectionParams conn_params)
{
	RequestMeta *req = malloc(sizeof(RequestMeta));
	size_t in_bytes;

	Queue *queue = malloc(sizeof(Queue));
	queue_init(queue, conn_params.queue_size);

	WorkerParams params = {.conn_socket = conn_socket, .queue = queue};

	ImageStorage *image_storage = malloc(sizeof(ImageStorage));
	init_img_storage(image_storage, 10);
	params.image_storage = image_storage;

	control_workers(WORKERS_START, conn_params.workers, &params);

	do {
		in_bytes = recv(conn_socket, &req->request, sizeof(struct request), 0);
		clock_gettime(CLOCK_MONOTONIC, &req->receipt_timestamp);

		if (in_bytes <= 0)
			break;

		if (req->request.img_op == IMG_REGISTER) {
			struct image *img = recvImage(conn_socket);
			uint64_t img_id = generate_unique_image_id();
			ImageItem img_item = {.image_id = img_id, .image = img};

			clock_gettime(CLOCK_MONOTONIC, &req->start_timestamp);
			int added = add_image_to_storage(&img_item, image_storage);
			clock_gettime(CLOCK_MONOTONIC, &req->completion_timestamp);

			struct response resp = {.req_id = req->request.req_id, .img_id = img_id, .ack = added ? RESP_COMPLETED : RESP_REJECTED};
			send(conn_socket, &resp, sizeof(struct response), 0);

			if (added) {
				sync_printf("T%d R%ld:%lf,%s,%d,%lu,%lu,%lf,%lf,%lf\n",
					0,
					req->request.req_id,
					TSPEC_TO_DOUBLE(req->request.req_timestamp),
					OPCODE_TO_STRING(req->request.img_op),
					req->request.overwrite,
					req->request.img_id,
					resp.img_id,
					TSPEC_TO_DOUBLE(req->receipt_timestamp),
					TSPEC_TO_DOUBLE(req->start_timestamp),
					TSPEC_TO_DOUBLE(req->completion_timestamp)
				);
				dump_queue_status(queue);
			} else {
				sync_printf("X%ld:%lf,%lf,%lf\n", req->request.req_id,
					TSPEC_TO_DOUBLE(req->request.req_timestamp),
					TSPEC_TO_DOUBLE(req->request.req_length),
					TSPEC_TO_DOUBLE(req->receipt_timestamp)
				);
			}
		} else {
			int res = add_to_queue(*req, queue, conn_params.queue_size, conn_params.policy);
			if (res) {
				struct response resp = {.req_id = req->request.req_id, .ack = RESP_REJECTED};
				send(conn_socket, &resp, sizeof(struct response), 0);
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
	free(queue->queue);
	free(queue);
	free_image_storage(image_storage);
	free(image_storage);
	shutdown(conn_socket, SHUT_RDWR);
	close(conn_socket);
	sync_printf("INFO: Client disconnected.\n");
}

/* Template implementation of the main function for the FIFO
 * server. The server must accept in input a command line parameter
 * with the <port number> to bind the server to. */
int main(int argc, char **argv) {
	int sockfd, retval, accepted, optval, opt;
	in_port_t socket_port;
	struct sockaddr_in addr, client;
	struct in_addr any_address;
	socklen_t client_len;
	ConnectionParams conn_params = {.queue_size = 0, .workers = 1};

	while ((opt = getopt(argc, argv, "q:w:p:")) != -1)
	{
		switch (opt) {
			case 'q':
				conn_params.queue_size = strtol(optarg, NULL, 10);
				printf("INFO: setting queue size = %ld\n", conn_params.queue_size);
				break;
			case 'w':
				conn_params.workers = strtol(optarg, NULL, 10);
				printf("INFO: setting worker count = %ld\n", conn_params.workers);
				if (conn_params.workers != 1) {
					fprintf(stderr, USAGE_STRING, argv[0]);
					return EXIT_FAILURE;
				}
				break;
			case 'p':
				if (!strcmp(optarg, "FIFO")) {
					conn_params.policy = QUEUE_FIFO;
				} else {
					fprintf(stderr, USAGE_STRING, argv[0]);
					return EXIT_FAILURE;
				}
				break;
			default:
				fprintf(stderr, USAGE_STRING, argv[0]);
		}
	}

	if (!conn_params.queue_size) {
		fprintf(stderr, USAGE_STRING, argv[0]);
		return EXIT_FAILURE;
	}

	if (optind < argc) {
		socket_port = strtol(argv[optind], NULL, 10);
	} else {
		fprintf(stderr, USAGE_STRING, argv[0]);
		return EXIT_FAILURE;
	}

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0)
		return EXIT_FAILURE;

	optval = 1;
	setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (void *)&optval, sizeof(optval));
	any_address.s_addr = htonl(INADDR_ANY);

	addr.sin_family = AF_INET;
	addr.sin_port = htons(socket_port);
	addr.sin_addr = any_address;

	retval = bind(sockfd, (struct sockaddr *)&addr, sizeof(struct sockaddr_in));
	if (retval < 0)
		return EXIT_FAILURE;

	retval = listen(sockfd, BACKLOG_COUNT);
	if (retval < 0)
		return EXIT_FAILURE;

	client_len = sizeof(struct sockaddr_in);
	accepted = accept(sockfd, (struct sockaddr *)&client, &client_len);
	if (accepted == -1)
		return EXIT_FAILURE;

	printf_mutex = malloc(sizeof(sem_t));
	sem_init(printf_mutex, 0, 1);

	queue_mutex = malloc(sizeof(sem_t));
	queue_notify = malloc(sizeof(sem_t));
	sem_init(queue_mutex, 0, 1);
	sem_init(queue_notify, 0, 0);

	handle_connection(accepted, conn_params);

	free(queue_mutex);
	free(queue_notify);

	close(sockfd);
	return EXIT_SUCCESS;
}