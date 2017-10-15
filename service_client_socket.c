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
	
	*string = malloc((len + 1) * sizeof(char*));

	if(string == NULL){
		return -1;
	}

	snprintf(*string, len + 1, "%s", buffer);

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

		int bad_request = 0;

		/* find the first space, the end of the request method */
		char *space = strchr(buffer, ' ');

		/* if there's no space fail */
		if(space == NULL){
			bad_request = 1;
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
				bad_request = 1;
			} else {

				/* get the length of the uri, extract */
				request_uri_len = space - buffer - request_method_name_len - 1;				
				
				/* check malloc didn't break */
				if(extract(&request_uri, request_uri_len, buffer + request_method_name_len + 1) == -1){
					free(request_method_name);	
					perror("malloc");
					return -1;
				}

				/* null should redirect to index */
				if(strcmp(request_uri, "/") == 0){
					sprintf(request_uri, "/index.html");
				}

				/* find the next element (version) */
				char *space = strchr(buffer + request_uri_len + 2, '\n');

				if(space == NULL){
					bad_request = 1;
				} else {
					
					http_version_len = space - buffer - request_uri_len - 4;
					
					/* check malloc didn't break */
					if(extract(&http_version, http_version_len, buffer + request_uri_len + request_method_name_len + 2) == -1){
						
						free(request_method_name);
						free(request_uri);			
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

		/* initialise our return variables */
		char *return_message;
		char *response_code;
		char *content;
		char *file_name;
		char *content_type;
		char content_length[12];

		if(bad_request == 1){

			response_code = malloc(sizeof(char*) * 16);
			content = malloc(sizeof(char*) * 25);

			if(!content || !response_code){
				free(content);
				free(response_code);
				free(request_method_name);
				free(request_uri);			
				free(http_version);
				perror("malloc");
				return -1;
			}

			snprintf(response_code, 16, "400 Bad Request");
			snprintf(content, 24, "<h1>400 Bad Request</h1>");
			sprintf(content_length, "%i", 24);

		} else {

			/* print as debugging for now */
			printf("request_method_name: %s\n", request_method_name);
			printf("request_uri: %s\n", request_uri);
			printf("http_version: %s\n\n", http_version);

			/* trim the first slash off the uri so we have the file */
			file_name = malloc(sizeof(char*) * strlen(request_uri));

			sprintf(file_name, "%s", &request_uri[1]);
			file_name[strlen(request_uri) - 1] = '\0';

			char *ext;
			ext = strchr(file_name, '.');

			if(!ext){
				ext = malloc(sizeof(char*) * 6);
				snprintf(ext, 6, ".html");
				strcat(file_name, ".html");
			}

			if(strcmp(ext, ".html") == 0){
				content_type = malloc(sizeof(char*) * 10);
				snprintf(content_type, 10, "text/html");
			} else if (strcmp(ext, ".jpg") == 0){
				content_type = malloc(sizeof(char*) * 11);
				snprintf(content_type, 11, "image/jpeg");
			}

			FILE *file;

			if(strcmp(content_type, "text/html") == 0){
				file = fopen(file_name, "r");
			} else {
				file = fopen(file_name, "rb");
			}
		
			long size = 0;

			if(file){

				if(fseek(file, 0L, SEEK_END) == 0){
					
					size = ftell(file);

					if(size == -1){
						fclose(file);					
						free(request_method_name);
						free(request_uri);			
						free(http_version);
						perror("content size");
						return -1;
					}

					content = malloc(sizeof(char*) * (size + 1));

					if(!content){
						fclose(file);					
						free(request_method_name);
						free(request_uri);			
						free(http_version);
						perror("malloc");
						return -1;
					}
				
					if(fseek(file, 0L, SEEK_SET != 0)){
						fclose(file);					
						free(request_method_name);
						free(request_uri);			
						free(http_version);
						free(content);
						perror("content size");
						return -1;
					}

					size_t len = fread(content, sizeof(char), size, file);

					if(ferror(file) != 0){
						fclose(file);					
						free(request_method_name);
						free(request_uri);			
						free(http_version);
						free(content);
						perror("reading file");
						return -1;
					} else {
						content[len++] = '\0';
					}
				}

				response_code = malloc(sizeof(char*) * 7);

				if(!response_code){
					fclose(file);					
					free(response_code);
					free(content);
					free(request_method_name);
					free(request_uri);			
					free(http_version);
					perror("malloc");
					return -1;
				}

				fclose(file);
				sprintf(response_code, "200 OK");
				sprintf(content_length, "%lu", size + 1);
			
			} else {

				response_code = malloc(sizeof(char*) * 14);
				content = malloc(sizeof(char*) * 23);

				if(!response_code || !content){
					free(response_code);
					free(content);
					free(request_method_name);
					free(request_uri);			
					free(http_version);
					perror("malloc");
					return -1;
				}

				sprintf(response_code, "404 Not Found");
				sprintf(content, "<h1>404 Not Found</h1>");
				sprintf(content_length, "%i", 23);
			}
		}
		
		//return_message = malloc(2000);
		return_message = malloc(sizeof(char*) * (39 + strlen(content_type) + strlen(http_version) + strlen(response_code) + strlen(content_length) + strlen(content)));

		if(!return_message){
			free(request_method_name);
			free(request_uri);			
			free(http_version);
			free(response_code);
			free(content_length);
			free(content);
			perror("malloc");
			return -1;
		}


		sprintf(return_message, "%s %s\r\nContent-Length: %s\r\nContent-Type: %s\r\n\r\n%s", http_version, response_code, content_length, content_type, content);

		printf("response:\n%s\n", return_message);

		write(s, return_message, strlen(return_message) + 1);

		if(write(s, return_message, strlen(return_message) + 1) != strlen(return_message) + 1){			
			perror("write");
			return -1;
		}
		
		free(request_method_name);
		free(request_uri);			
		//free(http_version);
		//free(response_code);
		//free(content);
		//free(return_message);
		//free(file_name);

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

