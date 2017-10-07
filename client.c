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

#include <netdb.h>

#include <pthread.h>

static char *myname = "unknown";

typedef enum connect_state {
	cs_none = 0,
	cs_socket = 1,
	cs_connect = 2,
	cs_connected = 3
} connect_state_t;

typedef struct fdpair{
	int from;
	int to;
	int shutdown_on_eof;
} fdpair_t;

const char *const cs_name[] = { "none", "socket", "connect", "connected" };

const int trace = 0;

#define trace_print(...) if (trace) { fprintf (stderr, __VA_ARGS__); }

static void * copy_stream(void *data) {

	fdpair_t *fdpair = (fdpair_t *)data;

	char buffer[BUFSIZ];
	int bytes;
	int finished_reading = 0;

	trace_print("copy_stream %d->%d, shutdown = %d\n", fdpair->from, fdpair->to, fdpair->shutdown_on_eof);

	while (!finished_reading) {
		bytes = read(fdpair->from, buffer, BUFSIZ);
		if (bytes < 0) {
			fprintf(stderr, "%d->%d read %s\n", fdpair->from, fdpair->to, strerror(errno));
			pthread_exit(0);
		}
		else if (bytes == 0) {
			trace_print("%d->%d end of file on input\n", fdpair->from, fdpair->to);
			finished_reading++;
			if (fdpair->shutdown_on_eof) {
				trace_print("shutdown (%d)\n", fdpair->to);
				if (shutdown(fdpair->to, SHUT_WR) != 0) {
					perror("shutdown");
				}
			}
		}
		else {
			assert(bytes > 0);
			if (write(fdpair->to, buffer, bytes) != bytes) {
				fprintf(stderr, "%d->%d write %s\n", fdpair->from, fdpair->to, strerror(errno));
				pthread_exit(0);
			}
		}
	}

	trace_print("%d->%d exiting\n", fdpair->from, fdpair->to);

	pthread_exit(0);
}

int main(int argc, char **argv) {

	/* check name is there */
	assert(argv[0] && *argv[0]);
	myname = argv[0];
	struct addrinfo *results, hints;
	int res;

	/* do we have all three arguments required */
	if (argc != 3) {
		fprintf(stderr, "%s: usage is %s host port\n", myname, myname);
		exit(1);
	}

	assert(argv[1] && argv[2]);

	memset(&hints, '\0', sizeof(hints));
	hints.ai_family = PF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM; /* set type as a stream */
	res = getaddrinfo(argv[1], argv[2], gai_strerror(res));

	/* if the client can't find the server */
	if (res != 0) {
		fprintf(stderr, "%s: cannot resolve %s:%s (%s)\n", myname, argv[1], argv[2], gai_strerror(res));
		exit(1);
	}

	connect_state_t phase = cs_none;
	int s;

	for (struct addrinfo *scan = results; phase != cs_connected && scan != 0; scan = scan->ai_next) {

		phase = cs_socket;

		s = socket(results->ai_family, results->ai_socktype, results->ai_protocol);

		/* set socket state accordingly */
		if (s >= 0) {
			phase = cs_connect;
			if (connect(s, results->ai_addr, results->ai_addrlen) == 0) {
				phase = cs_connected;
			}
			else {
				close(s);
			}
		}
	}

}