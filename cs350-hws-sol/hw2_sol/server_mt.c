/*******************************************************************************
* Single-threaded FIFO Order Server Implementation with Worker Thread
*
* Description:
*     A server implementation designed to process client requests in First In,
*     First Out (FIFO) order. The server binds to the specified port number
*     provided as a parameter upon launch. A worker thread is launched but
*     incoming requests are still processed in the parent thread.
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
*     September 28, 2023
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
#include <pthread.h>


/* Needed for wait(...) */
#include <sys/types.h>
#include <sys/wait.h>

/* Include struct definitions and other libraries that need to be
 * included by both client and server */
#include "common.h"

#define BACKLOG_COUNT 100
#define USAGE_STRING				\
	"Missing parameter. Exiting.\n"		\
	"Usage: %s <port_number>\n"



int worker_done = 0;

/* Main logic of the worker thread */
void  *worker_main (void * params)
{
	struct timespec now;

	/* Unused for now */
	(void)params;

	/* Print the first alive message. */
	clock_gettime(CLOCK_MONOTONIC, &now);
	printf("[#WORKER#] %lf Worker Thread Alive!\n", TSPEC_TO_DOUBLE(now));

	/* Okay, now execute the main logic. */
	while (!worker_done) {
		get_elapsed_busywait(1, 0);
		clock_gettime(CLOCK_MONOTONIC, &now);
		printf("[#WORKER#] %lf Still Alive!\n", TSPEC_TO_DOUBLE(now));
		get_elapsed_sleep(1, 0);
	}

	return NULL;
}

/* This function will start the worker thread wrapping around the
 * pthread_create() POSIX api function*/
int start_worker(pthread_t* p, void* params)
{
	int retval;

	retval = pthread_create(p, NULL, worker_main, params);

	return retval;
}

/* Main function to handle connection with the client. This function
 * takes in input conn_socket and returns only when the connection
 * with the client is interrupted. */
void handle_connection(int conn_socket)
{
	struct request * req;
	struct response * resp;
	struct timespec receipt, completion;
	size_t in_bytes;

	/* The connection with the client is alive here. Let's start
	 * the worker thread. */
	pthread_t p;
	int worker_id = start_worker(&p, NULL);

	if (worker_id < 0) {
		ERROR_INFO();
		perror("Unable to create worker thread");
		return;
	}

	printf("INFO: Worker thread started. Thread ID = %ld\n", p);

	/* We are ready to proceed with the rest of the request
	 * handling logic. */

	req = (struct request *)malloc(sizeof(struct request));
	resp = (struct response *)malloc(sizeof(struct response));

	do {
		in_bytes = recv(conn_socket, req, sizeof(struct request), 0);
		clock_gettime(CLOCK_MONOTONIC, &receipt);

		/* Don't just return if in_bytes is 0 or -1. Instead
		 * skip the response and break out of the loop in an
		 * orderly fashion so that we can de-allocate the req
		 * and resp varaibles, and shutdown the socket. */
		if (in_bytes > 0) {
			busywait_timespec(req->req_length);
			clock_gettime(CLOCK_MONOTONIC, &completion);

			/* Now provide a response! */
			resp->req_id = req->req_id;
			resp->reserved = 0;
			resp->ack = 0;
			send(conn_socket, resp, sizeof(struct response), 0);

			printf("R%ld:%lf,%lf,%lf,%lf\n", req->req_id,
			       TSPEC_TO_DOUBLE(req->req_timestamp),
			       TSPEC_TO_DOUBLE(req->req_length),
			       TSPEC_TO_DOUBLE(receipt),
			       TSPEC_TO_DOUBLE(completion)
			    );
		}
	} while (in_bytes > 0);

	/* Ask the worker thead to terminate */
	printf("INFO: Asserting termination flag for worker thread...\n");
	worker_done = 1;
	/* Wait for orderly termination of the worker thread */
	pthread_join(p, NULL);
	printf("INFO: Worker thread exited.\n");

	free(req);
	free(resp);
	shutdown(conn_socket, SHUT_RDWR);
	close(conn_socket);
	printf("INFO: Client disconnected.\n");
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

	/* Ready to handle the new connection with the client. */
	handle_connection(accepted);

	close(sockfd);
	return EXIT_SUCCESS;

}
