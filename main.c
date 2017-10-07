#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#include <assert.h>

#include "get_listen_socket.h"
#include "service_listen_socket.h"

char *myname = "unknown";

int main(int argc, char **argv) {

	int p, s; // port number var, socket var
	char *endp; // for end of string checking later

	/* our name is in the first argument, check it's there */
	assert(argv[0] && *argv[0]);
	myname = argv[0];

	/* check we have two arguments */
	if (argc != 2) {
		fprintf(stderr, "%s: usage is &s port\n", myname, myname);
		exit(1);
	}

	/* another argument check, this time for port no */
	assert(argv[1] && *argv[1]);

	/* convert the port string to a number, endp gets a pointer to the last character */
	p = strtol(argv[1], &endp, 10);

	/* check that the 'port no' is actually a number */
	if (*endp != '\0') {
		fprintf(stderr, "%s: %s is not a number\n", myname, argv[1]);
		exit(1);
	}

	/* check if the port no is valid (needs to be in 1024-65535) */
	if(p < 1024 || p > 65535){
		fprintf(stderr, "%s: %d should be in range 1024..65535 to be usable\n", myname, p);
		exit(1);
	}

	/* create the listening socket on port p */
	s = get_listen_socket(p);

	/* get_listen_socket returns -1 if can't get socket*/
	if (s < 0) {
		fprintf(stderr, "%s: cannot get listen socket\n", myname);
		exit(1);
	}

	/* if there's an error we should exit*/
	if (service_listen_socket(s) != 0) {
		fprintf(stderr, "%s: cannot process listen socket\n", myname);
		exit(1);
	}

	exit(0);
}