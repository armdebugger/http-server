#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>

#include <memory.h>
#include <string.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <assert.h>

#include "service_client_socket.h"

#define buffer_size 1024

int service_client_socket(const int s, const char * const tag) {

	char buffer[buffer_size];
	size_t bytes;

	printf("new connection from %s\n", tag);

	/* read a buffer of bytes, leaving room for the the NUL for printf() */
	while ((bytes = read(s, buffer, buffer_size - 1)) > 0) {

		/* if bytes are not written successfully */
		if (write(s, buffer, bytes) != bytes) {
			perror("write");
			return -1;
		}

		/* NUL-terminate the string */
		buffer[bytes] = '\0';

		/* tidy printing, to trim errant newlines */
		if (bytes >= 1 && buffer[bytes - 1] == '\n') {
			if (bytes >= 2 && buffer[bytes - 2] == '\r') {
				strcpy(buffer + bytes - 2, "..");
			}
			else {
				strcpy(buffer + bytes - 1, ".");
			}
		}

#if (__SIZE_WIDTH__ == 64 || __SIZEOF_POINTER__ == 8)
		printf("echoed %ld bytes back to %s, \"%s\"\n", bytes, tag, buffer);
#else
		printf("echoed %d bytes back to %s, \"%s\"\n", bytes, tag, buffer);
#endif
	}

	/* if bytes < 0 something went wrong */
	if (bytes != 0) {
		perror("read");
		return -1;
	}

	printf("connection from %s closed\n", tag);
	close(s);
	return 0;

}