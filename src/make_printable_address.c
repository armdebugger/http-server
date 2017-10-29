/* code provided by Ian Batten on Canvas for generating a 
 * printable address */

#include <string.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <netinet/in.h>
#include <assert.h>

char *make_printable_address(const struct sockaddr_in6 *const addr,
	const socklen_t addr_len,
	char *const buffer,
	const size_t buffer_size) {

	char printable[INET6_ADDRSTRLEN];

	assert(addr_len == sizeof(*addr));

	/* inet_ntop converts address structures into strings, and handles ipv6 too!
		ipv4 is converted into a special ipv6 format */

	if (inet_ntop(addr->sin6_family, &(addr->sin6_addr), printable, sizeof(printable)) == printable) {
		
		/* print to pretty address to the buffer */
		snprintf(buffer, buffer_size, "%s port %d", printable, ntohs(addr->sin6_port));

	}
	else {

		/* if the address can't be parsed */
		perror("inet_ntop");
		snprintf(buffer, buffer_size, "unparseable address");
	}

	return strdup(buffer);
}
