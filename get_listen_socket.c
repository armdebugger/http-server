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

}