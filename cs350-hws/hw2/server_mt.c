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
*     September 10, 2023
*
* Last Changes:
*     September 16, 2024
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

/* Include struct definitions and other libraries that need to be
 * included by both client and server */
#include "common.h"


#define BACKLOG_COUNT 100
#define USAGE_STRING				\
	"Missing parameter. Exiting.\n"		\
	"Usage: %s <port_number>\n"

/* 4KB of stack for the worker thread */
#define STACK_SIZE (4096)

/* Main logic of the worker thread */
/* IMPLEMENT HERE THE MAIN FUNCTION OF THE WORKER */
void *worker_main(void *arg) {
    struct timespec current_time;

    clock_gettime(CLOCK_REALTIME, &current_time);
    printf("[#WORKER#] %.6f Worker Thread Alive!\n",
        current_time.tv_sec + current_time.tv_nsec / 1e9
    );

    while (1) {
        // Busy-wait for 1 second
        struct timespec busywait_duration = { 1, 0 };
        busywait_timespec(busywait_duration);

        // Print still alive message
        clock_gettime(CLOCK_REALTIME, &current_time);
        printf("[#WORKER#] %.6f Still Alive!\n",
               current_time.tv_sec + current_time.tv_nsec / 1e9
        );

        // Sleep for 1 second
        sleep(1);
    }

    return NULL;
}

// Function to handle client connections
void handle_connection(int conn_socket) {
    pthread_t worker_thread;

    // Start the worker thread
    if (pthread_create(&worker_thread, NULL, worker_main, NULL) != 0) {
        perror("Failed to create worker thread");
        return;
    }

    struct request * req = (struct request *)malloc(sizeof(struct request));
	struct response * resp = (struct response *)malloc(sizeof(struct response));
	struct timespec receipt, completion;
	size_t in_bytes;

	do {
		in_bytes = recv(conn_socket, req, sizeof(struct request), 0);
		clock_gettime(CLOCK_MONOTONIC, &receipt);

		if (in_bytes) {
			busywait_timespec(req->req_length);
			clock_gettime(CLOCK_MONOTONIC, &completion);

			/* Now provide a response! */
			resp->req_id = req->req_id;
			resp->ack = 0;
			send(conn_socket, resp, sizeof(struct response), 0);

			printf("R%ld:%lf,%lf,%lf,%lf\n", req->req_id,
			       TSPEC_TO_DOUBLE(req->req_timestamp),
			       TSPEC_TO_DOUBLE(req->req_length),
			       TSPEC_TO_DOUBLE(receipt),
			       TSPEC_TO_DOUBLE(completion)
			    );
		}
	} while (in_bytes);

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