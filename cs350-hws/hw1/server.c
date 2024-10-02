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
* Last Update:
*     September 9, 2024
*
* Notes:
*     Ensure to have proper permissions and available port before running the
*     server. The server relies on a FIFO mechanism to handle requests, thus
*     guaranteeing the order of processing. For debugging or more details, refer
*     to the accompanying documentation and logs.
*
*******************************************************************************/

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

/* Include struct definitions and other libraries that need to be
 * included by both client and server */
#include "common.h"

#define BACKLOG_COUNT 100
#define USAGE_STRING				\
	"Missing parameter. Exiting.\n"		\
	"Usage: %s <port_number>\n"

/* Main function to handle connection with the client. This function
 * takes in input conn_socket and returns only when the connection
 * with the client is interrupted. */
static void handle_connection(int conn_socket)
{
	/* IMPLEMENT ME! */
	uint64_t request_id;	// unsigned 64-bit int encoding the request ID
  	struct timespec request_sent_timestamp;	// timestamp when client sent request
	struct timespec request_length;	// duration of how long server should busy-wait
	struct timespec receipt_timestamp;	// timestamp when server got the request
	struct timespec completion_timestamp;	// timestamp when server finished processing request
	uint8_t ack_value = 0;	// acknowledgement value - indicates request has been correctly handled
	uint64_t reserved_field = 0;	// future use
	ssize_t n;	// store number of bytes read or written during socket operations

	// infinite loop as long as connection is open
	while (1) {
		// read the request from the client
		n = read(conn_socket, &request_id, sizeof(request_id)); 
		if (n <= 0) {	// if read operation was unsuccessful or connection was closed
			if (n < 0) perror("read");	// error occurred
			break; // exit loop
		}

		// read the request_sent_timestamp from the client
		n = read(conn_socket, &request_sent_timestamp, sizeof(request_sent_timestamp));
		if (n <= 0) {	// if read operation was unsuccessful or connection was closed
			if (n < 0) perror("read");	// error occurred
			break; 
		}

		// read the request_length from the client
		n = read(conn_socket, &request_length, sizeof(request_length));
		if (n <= 0) {	// if read operation was unsuccessful or connection was closed
			if (n < 0) perror("read"); 	// error occurred
			break; 
		}

		// when the server received the request
		clock_gettime(CLOCK_MONOTONIC, &receipt_timestamp);	// record the receipt timestamp

		get_elapsed_busywait(request_length.tv_sec, request_length.tv_nsec);	// perform busy wait for the specified duration

		// when server completed processing the request
		clock_gettime(CLOCK_MONOTONIC, &completion_timestamp);	// record the completion timestamp

		// Send the response to the client
		uint8_t response[17]; // 8 bytes for request_id, 8 bytes for reserved, 1 byte for ack_value
		memcpy(response, &request_id, sizeof(request_id));	// copy request_id into res buffer
		memcpy(response + 8, &reserved_field, sizeof(reserved_field));	// copy reserved_field into res buffer
		memcpy(response + 16, &ack_value, sizeof(ack_value));	// copy ack_value into res buffer

		if (write(conn_socket, response, sizeof(response)) < 0) {	// send response to client
			perror("write");	// error - write failed
			break; // exit loop
		}

		// Print the request handling details
		printf("R%lu:%ld.%09ld,%ld.%09ld,%ld.%09ld,%ld.%09ld\n",
			request_id,	// request ID
			request_sent_timestamp.tv_sec, request_sent_timestamp.tv_nsec,	// sent timestamp
			request_length.tv_sec, request_length.tv_nsec,	// request length
			receipt_timestamp.tv_sec, receipt_timestamp.tv_nsec,	// receipt timestamp
			completion_timestamp.tv_sec, completion_timestamp.tv_nsec	// completion timestamp
		);
	}

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

	/* Ready to handle the new connection with the client. */
	handle_connection(accepted);

	close(sockfd);
	return EXIT_SUCCESS;
}
