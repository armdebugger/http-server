/* Based on code provided by Ian Batten from Canvas
 * and some of Eike Ritter's lecture code from Operating Systems */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <signal.h>

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

volatile sig_atomic_t stop = 0;
int error_code = 0;
int socket_int;
char *printable = NULL;

/* tcb struct, contains address info for a client */
typedef struct thread_control_block {
	int client;
	struct sockaddr_in6 their_address;
	socklen_t their_address_size;
} thread_control_block_t;

thread_control_block_t *tcb_p;

/* catch closing signals and end gracefully, freeing things */
void catch_signal(int sig){
	
	tcb_p->client = -1;	
	stop = 1;

}

/* creating a client thread with address */
static void *client_thread(void *data) {
	
	thread_control_block_t *tcb_p = (thread_control_block_t *)data;
	char buffer [INET6_ADDRSTRLEN + 32];

	/* copy data onto our stack so we can immediately free tcb */
	int client = tcb_p->client;
	struct sockaddr_in6 their_address = tcb_p->their_address;
	socklen_t their_address_size = tcb_p->their_address_size;

	free(tcb_p);

	assert(their_address_size == sizeof(their_address));
	printable = make_printable_address (&(their_address),
				      their_address_size,
				      buffer, sizeof (buffer));

	char printable_new[strlen(printable) + 1];
	strcpy(printable_new, printable);
	free(printable);

	(void)service_client_socket(client, printable_new);

	pthread_exit(0);
}

/* while loop to initialise new thread for each client */
int service_listen_socket(const int s) {

	socket_int = s;

	struct sigaction a;
	a.sa_handler = catch_signal;
	a.sa_flags = 0;
	sigemptyset(&a.sa_mask);
	sigaction(SIGINT, &a, NULL);
	sigaction(SIGTERM, &a, NULL);

	while(!stop){

		pthread_t server_thread = 0;

		tcb_p = malloc(sizeof(*tcb_p));
		pthread_attr_t pthread_attr; /* attributes for thread */

		if (tcb_p == 0) {
			perror("malloc");
			exit(1);
		}

		tcb_p->their_address_size = sizeof(tcb_p->their_address);

		/* calling accept as before but putting data in a tcb rather than on our stack */
		if ((tcb_p->client = accept(socket_int, (struct sockaddr *) &(tcb_p->their_address), &(tcb_p->their_address_size))) < 0) {
			perror("accept");
			
			/* exit signal has been caught */
			if(stop){

				free(tcb_p);
				pthread_cancel(server_thread);
				exit(0);
			}

		} else {

			if(pthread_attr_init(&pthread_attr)){
				fprintf(stderr, "creating initial thread attributes failed");
				free(tcb_p);
				exit(1);
			}

			if(pthread_attr_setdetachstate(&pthread_attr, PTHREAD_CREATE_DETACHED)){
				fprintf(stderr, "setting thread attributes failed\n");
				exit(1);
			}

			/* now we create a thread and start it by calling client_thread with the tcb_p pointer
			 * struct just on our stack would lead to race conditions */

			if (pthread_create(&server_thread, &pthread_attr, &client_thread, (void *)tcb_p) != 0) {
				perror("pthread_create");
				error_code = 1;
				free(tcb_p);
				exit(1);
			}
		}
	}

	return 0;

}
