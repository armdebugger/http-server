#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <time.h>

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

	//time_t t = time(NULL);
	//struct tm tm = *localtime(&t);

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
		char *http_version = NULL;
		size_t http_version_len = 0;

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
			
			/* check malloc didn't break */
			if(extract(&request_method_name, request_method_name_len, buffer) == -1){
				perror("malloc");
				return -1;
			}

			/* find the next element (uri) */
			char *space = strchr(buffer + request_method_name_len + 1, ' ');
			
			/* if there's no more info, bad request */
			if(space == NULL){
				if(write(s, "400", 3) != 3){
					perror("write");
					return -1;
				}			
			} else {

				/* get the length of the uri, extract */
				request_uri_len = space - buffer - request_method_name_len - 1;				
				
				/* check malloc didn't break */
				if(extract(&request_uri, request_uri_len, buffer + request_method_name_len + 1) == -1){
					perror("malloc");
					return -1;
				}

				/* null should redirect to index */
				if(strcmp(request_uri, "/") == 0){
					strcpy(request_uri, "/index.html");
				}

				/* find the next element (version) */
				char *space = strchr(buffer + request_uri_len + 2, '\n');

				if(space == NULL){
					if(write(s, "400", 3) != 3){
						perror("write");
						return -1;
					}
				} else {
					
					http_version_len = space - buffer - request_uri_len - 4;
					
					/* check malloc didn't break */
					if(extract(&http_version, http_version_len, buffer + request_uri_len + request_method_name_len + 2) == -1){
						perror("malloc");
						return -1;
					}

				
					/* workaround because the newline messes up my code */
					for(int i = 0; i < strlen(http_version) + 1; i++){
						if(http_version[i] == '\n'){								
							http_version[i-1] = '\0';
						}
					}
				}
			}
		}

		/* print as debugging for now */
		printf("request_method_name: %s\n", request_method_name);
		printf("request_uri: %s\n", request_uri);
		printf("http_version: %s\n\n", http_version);

		

		char return_message[300];
		char response_code[20];
		char content[1000];
		char content_length[10];

		//strcpy(return_message, http_version);
		strcpy(return_message, http_version);
		strcat(return_message, " ");
		strcat(return_message, response_code);
		strcat(return_message, "\r\nDate: Sun, 15 Oct 2017 14:44:34 GMT\r\nContent-Length: ");
		strcat(return_message, content_length);
		strcat(return_message, "\r\nContent-Type: text/html\r\n\r\n");

		strcat(return_message, content);

		printf("%s\n", return_message);

		write(s, return_message, strlen(return_message) + 1);

		if(write(s, return_message, strlen(return_message) + 1) != strlen(return_message) + 1){
			printf("what");			
			perror("write");
			return -1;
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

