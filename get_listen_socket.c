#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>

#include <memory.h>
#include <string.h>

#include <sys/socket.h>
#include <netinet/in.h>

#include <assert.h>

#include "service_listen_socket.h"

int get_listen_socket(const int port) {
	
	fprintf(stderr, "binding to port %d\n", port);

	/* struct to hold an address, presumably this is defined somewhere */
	struct sockaddr_in6 my_address;

	/* zero the structure out, costs very little */
	memset(&my_address, '\0', sizeof(my_address));
	my_address.sin6_family = AF_INET6;		// ipv6 address
	my_address.sin6_addr = in6addr_any;		// all interfaces
	my_address.sin6_port = htons(port);		// network order, historical reasons

	/* get a socket for listening */
	int s = socket(PF_INET6, SOCK_STREAM, 0);

	/* can't get a socket, return -1 */
	if (s < 0) {
		perror("socket");
		return -1;
	}

	const int one = 1;

	/* a 'nice to have', not essential so don't exit */
	if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one)) != 0){
		perror("setsocket");
	}

	/* binding the address to the socket*/
	if (bind(s, (struct sockaddr *) &my_address, sizeof(my_address)) != 0) {
		perror("bind");
		return -1;
	}

	/* set the socket as listening for messages, 5 is the 'backlog' that nobody really understands*/
	if (listen(s, 5) != 0) {
		perror("listen");
		return -1;
	}

	/* return our shiny new socket */
	return s;


}