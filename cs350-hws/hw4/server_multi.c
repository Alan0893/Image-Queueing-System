/*******************************************************************************
* Multi-Threaded FIFO Server Implementation w/ Queue Limit
*
* Description:
*     A server implementation designed to process client requests in First In,
*     First Out (FIFO) order. The server binds to the specified port number
*     provided as a parameter upon launch. It launches worker threads to
*     process incoming requests and allows to specify a maximum queue size.
*
* Usage:
*     <build directory>/server -q <queue_size> -w <workers> <port_number>
*
* Parameters:
*     port_number - The port number to bind the server to.
*     queue_size  - The maximum number of queued requests
*     workers     - The number of parallel threads to process requests.
*
* Author:
*     Renato Mancuso
*
* Affiliation:
*     Boston University
*
* Creation Date:
*     September 25, 2023
*
* Last Update:
*     September 30, 2024
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

#include <getopt.h>

#define BACKLOG_COUNT 100
#define USAGE_STRING				\
	"Missing parameter. Exiting.\n"		\
	"Usage: %s -q <queue size> -w <number of threads> <port_number>\n"

/* Mutex needed to protect the threaded printf. DO NOT TOUCH */
sem_t * printf_mutex;
/* Synchronized printf for multi-threaded operation */
/* USE sync_printf(..) INSTEAD OF printf(..) FOR WORKER AND PARENT THREADS */
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

volatile int worker_done = 0;

struct request_meta {
	struct request request;
	struct timespec receipt_ts;
};


struct queue {
	struct request_meta *requests;
	int head;
	int tail;
	int size;
	int count;
};


struct connection_params {
	int conn_socket;
	size_t queue_size;
	int worker_count;
};


struct worker_params {
	struct queue *the_queue;
	int conn_socket;
	int thread_id;
	volatile int worker_done;
	pthread_t thread;
};


/* Helper function to perform queue initialization */
void queue_init(struct queue * the_queue, size_t queue_size)
{
	the_queue->head = 0;
	the_queue->tail = 0;
	the_queue->requests = malloc(queue_size * sizeof(struct request_meta));
	the_queue->size = queue_size;
	the_queue->count = 0;
}


/* Add a new request <request> to the shared queue <the_queue> */
int add_to_queue(struct request_meta to_add, struct queue * the_queue)
{
	int retval = 0;
	/* QUEUE PROTECTION INTRO START --- DO NOT TOUCH */
	sem_wait(queue_mutex);
	/* QUEUE PROTECTION INTRO END --- DO NOT TOUCH */

	/* Make sure that the queue is not full */
	if (the_queue->count == the_queue->size) {
		/* What to do in case of a full queue */
		retval = -1;

		/* DO NOT RETURN DIRECTLY HERE. The
		 * sem_post(queue_mutex) below MUST happen. */
	} else {
		/* If all good, add the item in the queue */
		/* IMPLEMENT ME !!*/
		the_queue->requests[the_queue->tail] = to_add;
		the_queue->tail = (the_queue->tail + 1) % the_queue->size;
		the_queue->count++;

		/* QUEUE SIGNALING FOR CONSUMER --- DO NOT TOUCH */
		sem_post(queue_notify);
	}

	/* QUEUE PROTECTION OUTRO START --- DO NOT TOUCH */
	sem_post(queue_mutex);
	/* QUEUE PROTECTION OUTRO END --- DO NOT TOUCH */
	return retval;
}


/* Add a new request <request> to the shared queue <the_queue> */
struct request_meta get_from_queue(struct queue * the_queue)
{
	struct request_meta retval;
	/* QUEUE PROTECTION INTRO START --- DO NOT TOUCH */
	sem_wait(queue_notify);
	sem_wait(queue_mutex);
	/* QUEUE PROTECTION INTRO END --- DO NOT TOUCH */

	/* WRITE YOUR CODE HERE! */
	/* MAKE SURE NOT TO RETURN WITHOUT GOING THROUGH THE OUTRO CODE! */
	retval = the_queue->requests[the_queue->head];
	the_queue->head = (the_queue->head + 1) % the_queue->size;
	the_queue->count--;

	/* QUEUE PROTECTION OUTRO START --- DO NOT TOUCH */
	sem_post(queue_mutex);
	/* QUEUE PROTECTION OUTRO END --- DO NOT TOUCH */
	return retval;
}


void dump_queue_status(struct queue * the_queue)
{
	/* QUEUE PROTECTION INTRO START --- DO NOT TOUCH */
	sem_wait(queue_mutex);
	/* QUEUE PROTECTION INTRO END --- DO NOT TOUCH */

	/* WRITE YOUR CODE HERE! */
	/* MAKE SURE NOT TO RETURN WITHOUT GOING THROUGH THE OUTRO CODE! */
	printf("Q:[");
	for (int i = the_queue->head; i != the_queue->tail; i = (i + 1) % the_queue->size) {
		printf("R%ld%s", the_queue->requests[i].request.req_id,
		    (((i + 1) % the_queue->size != the_queue->tail) ? "," : ""));
	}
	printf("]\n");

	/* QUEUE PROTECTION OUTRO START --- DO NOT TOUCH */
	sem_post(queue_mutex);
	/* QUEUE PROTECTION OUTRO END --- DO NOT TOUCH */
}


/* Main logic of the worker thread */
int worker_main (void * arg)
{
	struct timespec now, start_timestamp, completion_timestamp;
	struct worker_params * params = (struct worker_params *)arg;

	/* Print the first alive message. */
	clock_gettime(CLOCK_MONOTONIC, &now);
	sync_printf("[#WORKER#] %lf Worker Thread Alive!\n", TSPEC_TO_DOUBLE(now));

	/* Okay, now execute the main logic. */
	while (!params->worker_done) {
		struct request_meta req = get_from_queue(params->the_queue);

		if (worker_done || params->worker_done) {
			break;
		}

		clock_gettime(CLOCK_MONOTONIC, &start_timestamp);
		busywait_timespec(req.request.req_length);
		clock_gettime(CLOCK_MONOTONIC, &completion_timestamp);

		struct response resp;

		resp.req_id = req.request.req_id;
		resp.reserved = 0;
		resp.ack = RESP_COMPLETED;
		
		send(params->conn_socket, &resp, sizeof(resp), 0);

		printf("T%d R%ld:%.6f,%.6f,%.6f,%.6f,%.6f\n",
			params->thread_id, req.request.req_id,
			TSPEC_TO_DOUBLE(req.request.req_timestamp),
			TSPEC_TO_DOUBLE(req.request.req_length),
			TSPEC_TO_DOUBLE(req.receipt_ts),
			TSPEC_TO_DOUBLE(start_timestamp),
			TSPEC_TO_DOUBLE(completion_timestamp)
		);

		dump_queue_status(params->the_queue);
	}

	return EXIT_SUCCESS;
}


/* This function will control all the workers (start or stop them). 
 * Feel free to change the arguments of the function as you wish. */
int control_workers(int start_stop_cmd, size_t worker_count, struct worker_params * common_params)
{
	/* IMPLEMENT ME !! */
	if (start_stop_cmd == 0) { // Starting all the workers
		for (size_t i = 0; i < worker_count; i++) {
			common_params[i].worker_done = 0;
			pthread_create(&common_params[i].thread, NULL, (void *(*)(void *))worker_main, &common_params[i]);
		}
	} else { // Stopping all the workers
		/* IMPLEMENT ME !! */
		for (size_t i = 0; i < worker_count; i++) {
			common_params[i].worker_done = 1;
		}
		for(size_t i = 0; i < worker_count; i++) {
			pthread_join(common_params[i].thread, NULL);
		}
	}

	/* IMPLEMENT ME !! */
	return EXIT_SUCCESS;
}


/* Main function to handle connection with the client. This function
 * takes in input conn_socket and returns only when the connection
 * with the client is interrupted. */
void handle_connection(int conn_socket, struct connection_params conn_params)
{
	struct queue *the_queue = malloc(sizeof(struct queue));
	queue_init(the_queue, conn_params.queue_size);

	struct worker_params *params = malloc(conn_params.worker_count * sizeof(struct worker_params));

	for (int i = 0; i < conn_params.worker_count; i++) {
		params[i].conn_socket = conn_socket;
		params[i].the_queue = the_queue;
		params[i].thread_id = i;
		params[i].worker_done = 0;
	}

	control_workers(0, conn_params.worker_count, params);

	struct request_meta *req = malloc(sizeof(struct request_meta));
	struct response resp;
	size_t in_bytes;

	do {
        in_bytes = recv(conn_socket, &req->request, sizeof(struct request), 0);

        clock_gettime(CLOCK_MONOTONIC, &req->receipt_ts);

		int result = add_to_queue(*req, the_queue);

        if (in_bytes > 0) {
            if (result == -1) {
                resp.req_id = req->request.req_id;
                resp.reserved = 0;
                resp.ack = RESP_REJECTED;
                send(conn_socket, &resp, sizeof(resp), 0);
				
                printf("X%lu:%.6f,%.6f,%.6f\n",
                       req->request.req_id,
                       TSPEC_TO_DOUBLE(req->request.req_timestamp),
                       TSPEC_TO_DOUBLE(req->request.req_length),
                       TSPEC_TO_DOUBLE(req->receipt_ts));
				dump_queue_status(the_queue);
            }
        }

    } while (in_bytes > 0);


	/* IMPLEMENT ME!! Gracefully terminate all the worker threads ! */
	control_workers(1, conn_params.worker_count, params);

	free(the_queue->requests);
	free(the_queue);
	free(req);
	free(params);

	shutdown(conn_socket, SHUT_RDWR);
	close(conn_socket);
	sync_printf("INFO: Client disconnected.\n");
}




/* Template implementation of the main function for the FIFO
 * server. The server must accept in input a command line parameter
 * with the <port number> to bind the server to. */
int main (int argc, char ** argv) {
	int sockfd, retval, accepted, optval;
	in_port_t socket_port;
	struct sockaddr_in addr, client;
	struct in_addr any_address;
	socklen_t client_len;


	struct connection_params conn_params;
	int option;

	if (argc != 6) {
		fprintf(stderr, USAGE_STRING, argv[0]);
		return EXIT_FAILURE;
	}

	while ((option = getopt(argc, argv, "q:w:")) != -1) {
		switch (option) {
			case 'q':
				conn_params.queue_size = atoi(optarg);
				break;
			case 'w':
				conn_params.worker_count = atoi(optarg);
				break;
			default:
				fprintf(stderr, USAGE_STRING, argv[0]);
				return EXIT_FAILURE;
		}
	}

	socket_port = atoi(argv[optind]);

	/* Parse all the command line arguments */
	/* IMPLEMENT ME!! */
	/* PARSE THE COMMANDS LINE: */
	/* 1. Detect the -q parameter and set aside the queue size in conn_params */
	//conn_params...
	/* 2. Detect the -w parameter and set aside the number of threads to launch */
	//conn_params...
	/* 3. Detect the port number to bind the server socket to (see HW1 and HW2) */
	//socket_port = ...


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


	/* Time to bind the socket to the right port  */
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


	/* Ready to handle the new connection with the client. */
	handle_connection(accepted, conn_params);


	free(queue_mutex);
	free(queue_notify);
	free(printf_mutex);


	close(sockfd);
	return EXIT_SUCCESS;
}
