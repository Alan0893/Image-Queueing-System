/*******************************************************************************
* Simple FIFO Order Server Implementation
*
* Description:
*     A server implementation designed to process client requests in First In,
*     First Out (FIFO) order. The server binds to the specified port number
*     provided as a parameter upon launch.
*
* Usage:
*     <build directory>/server <port_number>
*
* Parameters:
*     port_number - The port number to bind the server to.
*
* Author:
*     Renato Mancuso
*
* Affiliation:
*     Boston University
*
* Creation Date:
*     September 10, 202
*
* Last Changes:
*     September 22, 2024
*
* Notes:
*     Ensure to have proper permissions and available port before running the
*     server. The server relies on a FIFO mechanism to handle requests, thus
*     guaranteeing the order of processing. For debugging or more details, refer
*     to the accompanying documentation and logs.
*
*******************************************************************************/

#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <sched.h>
#include <signal.h>

/* Needed for wait(...) */
#include <sys/types.h>
#include <sys/wait.h>

/* Needed for semaphores */
#include <semaphore.h>

/* Include struct definitions and other libraries that need to be
 * included by both client and server */
#include "common.h"

#include <pthread.h>
#include <string.h>
#include <arpa/inet.h>
#include <time.h>

#define BACKLOG_COUNT 100
#define USAGE_STRING				\
	"Missing parameter. Exiting.\n"		\
	"Usage: %s <port_number>\n"

/* 4KB of stack for the worker thread */
#define STACK_SIZE (4096)

/* START - Variables needed to protect the shared queue. DO NOT TOUCH */
sem_t * queue_mutex;
sem_t * queue_notify;
/* END - Variables needed to protect the shared queue. DO NOT TOUCH */

/* Max number of requests that can be queued */
#define QUEUE_SIZE 500

volatile int terminate_flag = 0;

struct queue {
    /* IMPLEMENT ME */
	struct request_meta requests[QUEUE_SIZE];	// Array to hold requests
    int front;	// Index of the front of the queue
    int rear;	// Index of the end of the queue
    int count;	// Number of current items in the queue
};

struct worker_params {
    /* IMPLEMENT ME */
	struct queue *the_queue;		// Pointer to the shared queue
	int conn_socket;
	int term;
};

/* Add a new request <request> to the shared queue <the_queue> */
int add_to_queue(struct request_meta to_add, struct queue * the_queue)
{
	int retval = 0;
	/* QUEUE PROTECTION INTRO START --- DO NOT TOUCH */
	sem_wait(queue_mutex);
	/* QUEUE PROTECTION INTRO END --- DO NOT TOUCH */

	/* WRITE YOUR CODE HERE! */
	/* MAKE SURE NOT TO RETURN WITHOUT GOING THROUGH THE OUTRO CODE! */
	if (the_queue->count == QUEUE_SIZE) {	// Check if the queue is full
        retval = -1; // Queue full
    } else {
		// Add request to the end of the queue - FIFO
        the_queue->requests[the_queue->rear] = to_add;
		// Update end index (circular queue)
        the_queue->rear = (the_queue->rear + 1) % QUEUE_SIZE;
		// Increment the count of requests (size of queue)
        the_queue->count++;
    }

	/* QUEUE PROTECTION OUTRO START --- DO NOT TOUCH */
	sem_post(queue_mutex);
	sem_post(queue_notify);
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
	// Get the request at the front of the queue
	retval = the_queue->requests[the_queue->front];
	// Update the front pointer (cicular buffer)
    the_queue->front = (the_queue->front + 1) % QUEUE_SIZE;
	// Decrement the count of requests in the queue
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
	//int i;
	/* QUEUE PROTECTION INTRO START --- DO NOT TOUCH */
	sem_wait(queue_mutex);
	/* QUEUE PROTECTION INTRO END --- DO NOT TOUCH */

	/* WRITE YOUR CODE HERE! */
	/* MAKE SURE NOT TO RETURN WITHOUT GOING THROUGH THE OUTRO CODE! */
	printf("Q:[");
    for (int i = 0; i < the_queue->count; i++) {
        int index = (the_queue->front + i) % QUEUE_SIZE;	// Circular buffer indexing
		uint64_t req_id = the_queue->requests[index].request.req_id;
        printf("R%lu", req_id);
        if (i < the_queue->count - 1) {
            printf(",");
        }
    }
    printf("]\n");

	/* QUEUE PROTECTION OUTRO START --- DO NOT TOUCH */
	sem_post(queue_mutex);
	/* QUEUE PROTECTION OUTRO END --- DO NOT TOUCH */
}

/* Main logic of the worker thread */
/* IMPLEMENT HERE THE MAIN FUNCTION OF THE WORKER */
void *worker_main(void *arg) {
    struct request_meta req_meta;
    struct timespec start_timestamp, completion_timestamp, receipt_time;
    struct request req;
    struct response server_res;

    struct worker_params *params = (struct worker_params *)arg;
    struct queue *the_queue = params->the_queue;
    int conn_socket = params->conn_socket;

    // Infinite loop to process requests
    while (1) {
        // Check for termination signal
        if (params->term) {
            break; // Exit if termination is requested
        }

        // Check if the queue is empty and if we need to terminate
        if (the_queue->count == 0) {
            continue; // If empty, just wait for new requests
        }

        // Get the next request from the queue
        req_meta = get_from_queue(the_queue);
        receipt_time = req_meta.receipt_ts;
        req = req_meta.request;

        // Record the start time before processing
        clock_gettime(CLOCK_MONOTONIC, &start_timestamp);
        // Simulate request processing by busy-waiting
        get_elapsed_busywait(req.req_length.tv_sec, req.req_length.tv_nsec);
        // Record the completion time after processing
        clock_gettime(CLOCK_MONOTONIC, &completion_timestamp);

        server_res.req_id = req.req_id;
        server_res.reserved = 0;
        server_res.ack = 0;

        send(conn_socket, &server_res, sizeof(server_res), 0);
        
        printf("R%lu:%.6f,%.6f,%.6f,%.6f,%.6f\n",
            req.req_id,
            TSPEC_TO_DOUBLE(req.req_timestamp),
            TSPEC_TO_DOUBLE(req.req_length),
            TSPEC_TO_DOUBLE(receipt_time),
            TSPEC_TO_DOUBLE(start_timestamp),
            TSPEC_TO_DOUBLE(completion_timestamp)
        );

        dump_queue_status(the_queue);
    }

    // Optionally, clean up resources here if necessary
    return NULL;
}


/* Main function to handle connection with the client. This function
 * takes in input conn_socket and returns only when the connection
 * with the client is interrupted. */
void handle_connection(int conn_socket) {
    struct queue *the_queue = malloc(sizeof(struct queue));
    the_queue->front = 0;
    the_queue->rear = 0;
    the_queue->count = 0;

    /* Start the worker thread */
    struct worker_params worker_data = { the_queue, conn_socket, 0 };

    pthread_t worker_thread;
    if (pthread_create(&worker_thread, NULL, worker_main, (void *)&worker_data) != 0) {
        perror("Failed to create worker thread");
        free(the_queue);
        close(conn_socket);
        return;
    }

    /* Request handling logic */
    struct request *request = malloc(sizeof(struct request));  // Allocate memory for request

    struct request_meta req;
    ssize_t in_bytes;
    struct timespec receipt_timestamp;

    do {
        in_bytes = recv(conn_socket, request, sizeof(struct request), 0);
		
        clock_gettime(CLOCK_MONOTONIC, &receipt_timestamp);
        if (in_bytes > 0) {
            // Add each request to the queue
            req.receipt_ts = receipt_timestamp;
            req.request = *request;  // Dereference and copy the content of request
            add_to_queue(req, the_queue);
        }
    } while (in_bytes > 0);

    /* Clean up */
    terminate_flag = 1;
    worker_data.term = 1;
    add_to_queue(req, the_queue);

    /* Notify any worker thread waiting on the queue */
    sem_post(queue_notify);

    /* Wait for orderly termination of the worker thread */
    pthread_join(worker_thread, NULL);

    /* Free the queue and close the connection */
    free(the_queue);
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

	/* Get port to bind our socket to */
	if (argc > 1) {
		socket_port = strtol(argv[1], NULL, 10);
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
	handle_connection(accepted);

	free(queue_mutex);
	free(queue_notify);

	close(sockfd);
	return EXIT_SUCCESS;

}
