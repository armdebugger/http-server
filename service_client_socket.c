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

/* why can I not use const size_t here? */
#define buffer_size 1024

typedef enum request_method_name{
	GET = 0,
	HEAD = 1
} request_method_name;

int service_client_socket (const int s, const char *const tag) {
	char buffer[buffer_size];
	size_t bytes;

  printf ("new connection from %s\n", tag);

	while ((bytes = read (s, buffer, buffer_size - 1)) > 0) {
    
		/* NUL-terminal the string */
		buffer[bytes] = '\0';

		/* print received request (debug) */
		printf("\nrequest received:\n%s", buffer);

		/* variables for http fields and headers */
		char *request_method_name;
		size_t request_method_name_len = 0;	
		char *request_uri;
		size_t request_uri_len = 0;

		/* find the first space, the end of the request method */
		char *space = strchr(buffer, ' ');

		/* if there's no space fail */
		if(space == NULL){
			if(write(s, "fail\n", 5) != 5){
				perror("write");
				return -1;
			}
		} else {
				
			/* get the length of the method name and malloc */
			request_method_name_len = space - buffer;
			request_method_name = malloc(request_method_name_len * sizeof(char*));

			strncpy(request_method_name, buffer, request_method_name_len);

			printf("request_method_name: %s\n", request_method_name);

			char *space = strchr(buffer + request_method_name_len + 1, ' ');
			
			if(space == NULL){
				if(write(s, "fail\n", 5) != 5){
					perror("write");
					return -1;
				}			
			} else {
				request_uri_len = space - buffer - request_method_name_len;
				
				request_uri = malloc(request_uri_len * sizeof(char*));
				
				strncpy(request_uri, buffer + request_method_name_len + 1, request_uri_len);
					
				char * return_request_uri = malloc(sizeof(char*) * (14 + request_uri_len));
				strcpy(return_request_uri, "request_uri: ");
				strcat(return_request_uri, request_uri);		
				strcat(return_request_uri, "\n");

				printf("%s", return_request_uri);
		}
	}
}
  
	/* bytes == 0: orderly close; bytes < 0: something went wrong */
	if (bytes != 0) {
		perror ("read");
		return -1;
	}
  
	printf ("connection from %s closed\n", tag);
	close (s);
	return 0;
}

