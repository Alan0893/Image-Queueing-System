/*******************************************************************************
* Dual-Threaded FIFO Server Implementation w/ Queue Limit
*
* Description:
*     A server implementation designed to process client requests in First In,
*     First Out (FIFO) order. The server binds to the specified port number
*     provided as a parameter upon launch. It launches a secondary thread to
*     process incoming requests and allows to specify a maximum queue size.
*
* Usage:
*     <build directory>/server -q <queue_size> <port_number>
*
* Parameters:
*     port_number - The port number to bind the server to.
*     queue_size  - The maximum number of queued requests
*
* Author:
*     Renato Mancuso
*
* Affiliation:
*     Boston University
*
* Creation Date:
*     September 29, 2023
*
* Last Update:
*     September 25, 2024
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
	"Usage: %s -q <queue size> <port_number>\n"


/* START - Variables needed to protect the shared queue. DO NOT TOUCH */
sem_t * queue_mutex;
sem_t * queue_notify;
/* END - Variables needed to protect the shared queue. DO NOT TOUCH */

volatile int terminate = 0;

struct queue {
    /* IMPLEMENT ME */
	struct request_meta *requests;
	int head;
	int tail;
	int size;
	int count;
};

struct worker_params {
    /* IMPLEMENT ME */
	struct queue * the_queue;
	int conn_socket;
	int terminate;
};

/* Helper function to perform queue initialization */
void queue_init(struct queue * the_queue, size_t queue_size)
{
	/* IMPLEMENT ME !! */
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

	/* WRITE YOUR CODE HERE! */
	/* MAKE SURE NOT TO RETURN WITHOUT GOING THROUGH THE OUTRO CODE! */

	/* Make sure that the queue is not full */
	if (the_queue->count == the_queue->size) {
		/* What to do in case of a full queue */
		/* DO NOT RETURN DIRECTLY HERE */
		retval = -1;
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

/* Implement this method to correctly dump the status of the queue
 * following the format Q:[R<request ID>,R<request ID>,...] */
void dump_queue_status(struct queue * the_queue)
{
	int i;
	/* QUEUE PROTECTION INTRO START --- DO NOT TOUCH */
	sem_wait(queue_mutex);
	/* QUEUE PROTECTION INTRO END --- DO NOT TOUCH */

	/* WRITE YOUR CODE HERE! */
	/* MAKE SURE NOT TO RETURN WITHOUT GOING THROUGH THE OUTRO CODE! */
	printf("Q:[");
	for (i = the_queue->head; i != the_queue->tail; i = (i + 1) % the_queue->size) {
		printf("R%ld%s", the_queue->requests[i].request.req_id,
		    (((i + 1) % the_queue->size != the_queue->tail)?",":""));
	}
	printf("]\n");

	/* QUEUE PROTECTION OUTRO START --- DO NOT TOUCH */
	sem_post(queue_mutex);
	/* QUEUE PROTECTION OUTRO END --- DO NOT TOUCH */
}


/* Main logic of the worker thread */
/* IMPLEMENT HERE THE MAIN FUNCTION OF THE WORKER */
void *worker_main (void *arg) {
	struct timespec start_timestamp, completion_timestamp;
	struct worker_params * params = (struct worker_params *)arg;

	
	while (!terminate) {
		struct request_meta req;
		struct response resp;
		req = get_from_queue(params->the_queue);

		if (terminate) {
			break;
		}

		clock_gettime(CLOCK_MONOTONIC, &start_timestamp);
		busywait_timespec(req.request.req_length);
		clock_gettime(CLOCK_MONOTONIC, &completion_timestamp);

		resp.req_id = req.request.req_id;
		resp.reserved = 0;
		resp.ack = 0;
		send(params->conn_socket, &resp, sizeof(resp), 0);

		printf("R%ld:%lf,%lf,%lf,%lf,%lf\n", req.request.req_id,
		       TSPEC_TO_DOUBLE(req.request.req_timestamp),
		       TSPEC_TO_DOUBLE(req.request.req_length),
		       TSPEC_TO_DOUBLE(req.receipt_ts),
		       TSPEC_TO_DOUBLE(start_timestamp),
		       TSPEC_TO_DOUBLE(completion_timestamp)
			);

		dump_queue_status(params->the_queue);
	}
	return NULL;
}

/* Main function to handle connection with the client. This function
 * takes in input conn_socket and returns only when the connection
 * with the client is interrupted. */
void handle_connection(int conn_socket, size_t queue_size) {
    struct request_meta *req;
    struct queue *the_queue;
    size_t in_bytes;

    struct worker_params worker_params;
    int worker_id;

    /* Allocate and initialize the queue */
    the_queue = (struct queue *)malloc(sizeof(struct queue));
    if (the_queue == NULL) {
        perror("Failed to allocate memory for the queue");
        return;
    }
    queue_init(the_queue, queue_size);  // Use queue_size here

    /* Set up worker parameters */
    worker_params.conn_socket = conn_socket;
    worker_params.terminate = 0;
    worker_params.the_queue = the_queue;

    /* Create a worker thread to process requests */
    pthread_t worker_thread;
    worker_id = pthread_create(&worker_thread, NULL, worker_main, (void *)&worker_params);
    if (worker_id != 0) {
        free(the_queue);
        perror("Unable to create worker thread");
        return;
    }

    req = (struct request_meta *)malloc(sizeof(struct request_meta));
    if (req == NULL) {
        perror("Failed to allocate memory for request");
        return;
    }
    
    struct response resp;
    do {
        in_bytes = recv(conn_socket, &req->request, sizeof(struct request), 0);
        clock_gettime(CLOCK_MONOTONIC, &req->receipt_ts);

		int result = add_to_queue(*req, the_queue);

        if (in_bytes > 0) {
            /* Handle queue rejection if needed */
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

    /* Clean-up and orderly shutdown */
    terminate = 1;

    /* Wake up the worker if it's waiting */
    sem_post(queue_notify);

    /* Wait for the worker thread to terminate */
    pthread_join(worker_thread, NULL);

    /* Free allocated resources */
    free(the_queue->requests);
    free(the_queue);
    free(req);
    
    shutdown(conn_socket, SHUT_RDWR);
    close(conn_socket);
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

    size_t queue_size = 0;  // Initialize queue_size
    int option;

    /* Parse command line arguments */
    if (argc != 4) {
        fprintf(stderr, USAGE_STRING, argv[0]);
        return EXIT_FAILURE;
    }

    while ((option = getopt(argc, argv, "q:")) != -1) {
        switch (option) {
            case 'q':
                queue_size = atoi(optarg);  // Set queue size from argument
                if (queue_size <= 0) {
                    fprintf(stderr, "Queue size must be a positive number.\n");
                    return EXIT_FAILURE;
                }
                break;
            default:
                fprintf(stderr, USAGE_STRING, argv[0]);
                return EXIT_FAILURE;
        }
    }

    /* The port number should be the last argument */
    socket_port = atoi(argv[optind]);

    /* Create a socket */
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
	/* DONE - Initialize queue protection variables. DO NOT TOUCH */

	/* Ready to handle the new connection with the client. */
	handle_connection(accepted, queue_size);

	free(queue_mutex);
	free(queue_notify);

	close(sockfd);
	return EXIT_SUCCESS;

}