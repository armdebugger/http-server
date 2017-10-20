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

#include <pthread.h>

#include "service_client_socket.h"
#include "service_listen_socket.h"
#include "make_printable_address.h"

/* tcb struct, contains address info for a client */
typedef struct thread_control_block {
	int client;
	struct sockaddr_in6 their_address;
	socklen_t their_address_size;
} thread_control_block_t;

/* creating a client thread with address */
static void *client_thread(void *data) {
	thread_control_block_t *tcb_p = (thread_control_block_t *)data;
	char buffer[INET6_ADDRSTRLEN + 32];
	char *printable;

	assert(tcb_p->their_address_size == sizeof(tcb_p->their_address));

	printable = make_printable_address(&(tcb_p->their_address), tcb_p->their_address_size, buffer, sizeof(buffer));

	(void)service_client_socket(tcb_p->client, printable);

	free(printable);
	free(data);
	pthread_exit(0);
}

/* while loop to initialise new thread for each client */
int service_listen_socket(const int s) {

	while (1) {
		thread_control_block_t *tcb_p = malloc(sizeof(*tcb_p));

		if (tcb_p == 0) {
			perror("malloc");
			exit(1);
		}

		tcb_p->their_address_size = sizeof(tcb_p->their_address);

		/* calling accept as before but putting data in a tcb rather than on our stack */

		if ((tcb_p->client = accept(s, (struct sockaddr *) &(tcb_p->their_address), &(tcb_p->their_address_size))) < 0) {
			perror("accept");
			/* only temporary so carry on */
		}
		else {
			pthread_t thread;

			/* now we create a thread and start it by calling client_thread with the tcb_p pointer
			 * struct just on our stack would lead to race conditions */

			if (pthread_create(&thread, 0, &client_thread, (void *)tcb_p) != 0) {
				perror("pthread_create");
				goto error_exit;
			}

			int error = pthread_detach(thread);
				if(error != 0) {
					fprintf(stderr, "pthread_detach failed %s, continuing\n", strerror (error));
				}
			}
		}

error_exit:
	return -1;
}
