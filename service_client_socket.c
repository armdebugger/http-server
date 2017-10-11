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

/* extract a section from a buffer of length len and copy into the string pointer pointed to by string */
int extract(char **string, size_t len, char *buffer){
	
	*string = malloc(len * sizeof(char*));

	if(string == NULL){
		return -1;
	}

	strncpy(*string, buffer, len);

	return 0;
}

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
		char *request_method_name = NULL;
		size_t request_method_name_len = 0;	
		char *request_uri = NULL;
		size_t request_uri_len = 0;

		/* find the first space, the end of the request method */
		char *space = strchr(buffer, ' ');

		/* if there's no space fail */
		if(space == NULL){
			if(write(s, "400", 3) != 3){
				perror("write");
				return -1;
			}
		} else {
				
			/* get the length of the method name, extract */
			request_method_name_len = space - buffer;
			
			if(extract(&request_method_name, request_method_name_len, buffer) == -1){
				perror("malloc");
				return -1;
			}

			//request_method_name = malloc(request_method_name_len * sizeof(char*));
			//strncpy(request_method_name, buffer, request_method_name_len);

			char *space = strchr(buffer + request_method_name_len + 1, ' ');
			
			if(space == NULL){
				if(write(s, "400", 3) != 3){
					perror("write");
					return -1;
				}			
			} else {

				/* get the length of the uri, malloc and copy */
				request_uri_len = space - buffer - request_method_name_len - 1;				
				//request_uri = malloc(request_uri_len * sizeof(char*));
				//strncpy(request_uri, buffer + request_method_name_len + 1, request_uri_len);
				
				if(extract(&request_uri, request_uri_len, buffer + request_method_name_len + 1) == -1){
					perror("malloc");
					return -1;
				}

				if(strcmp(request_uri, "/") == 0){
					strcpy(request_uri, "/index.html");
				}
			}
		}
	
		printf("request_method_name: %s\n", request_method_name);
		printf("request_uri: %s\n", request_uri);
	
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

