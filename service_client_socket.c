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

typedef enum{
	NOT_SUPPORTED = 0, GET = 1, HEAD = 2
}method;

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

		/* error flags */
		int bad_request = 0;
		int server_error = 0;
		int unsupported_protocol = 0;

		/* method enum */
		method method_name;

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
				server_error = 1;
			} else {

				if(strcmp(request_method_name, "GET") == 0){
					method_name = 1;
				} else if (strcmp(request_method_name, "HEAD") == 0){
					method_name = 2;
				} else {
					method_name = 0;
				}

				/* find the next element (uri) */
				char *space = strchr(buffer + request_method_name_len + 1, ' ');
		
				/* if there's no more info, bad request */
				if(space == NULL){
					bad_request = 1;
					free(request_method_name);
				} else {

					/* get the length of the uri, extract */
					request_uri_len = space - buffer - request_method_name_len - 1;				
			
					/* check malloc didn't break */
					if(extract(&request_uri, request_uri_len, buffer + request_method_name_len + 1) == -1){
						server_error = 1;
					} else {

						/* null should redirect to index */
						if(request_uri[0] != '/'){
							bad_request = 1;
						} else if(strcmp(request_uri, "/") == 0){
							sprintf(request_uri, "/index.html");
						}

						/* find the next element (version) */
						char *space = strchr(buffer + request_uri_len + 2, '\n');

						if(space == NULL){
							bad_request = 1;
							free(request_method_name);
							free(request_uri);
						} else {
				
							http_version_len = space - buffer - request_uri_len - 4;
				
							/* check malloc didn't break */
							if(extract(&http_version, http_version_len, buffer + request_uri_len + request_method_name_len + 2) == -1){
					
								server_error = 1;
							}

							//http_version[http_version_len] = '\0';
							http_version[http_version_len + 1] = '\0';
							printf("|%s|\n", http_version);							

							char temp[http_version_len];
							sprintf(temp, "%s", http_version);
							temp[5] = '\0';

							if(strcmp(temp, "HTTP/") != 0){
									bad_request = 1;
							} else {
								sprintf(temp, "%s", http_version + 5);
								printf("|%s|\n", temp);

								if(strcmp(temp, "1.1") != 0 && strcmp(temp, "1.0") !=0){
									unsupported_protocol = 1;
								}
							}
						}
					}
				}
			}
		}

		/* initialise our return variables */
		char response_header[200]; 
		char* file_name;
		char response_code[50];
		char content_type[15];
		unsigned char *content;
		size_t content_length = 0;

		if(!server_error){

			sprintf(response_header, "%s ", http_version);

			if(bad_request){
				// bad request return 400
				sprintf(response_code, "400 Bad Request");
				file_name = malloc(sizeof(char*) * 9);				
				sprintf(file_name, "400.html");

			} else if (method_name == 0){
				// not implemented, return 501
				sprintf(response_code, "501 Not Implemented");
				file_name = malloc(sizeof(char*) * 9);
				sprintf(file_name, "501.html");

			} else if (unsupported_protocol){
				// not implemented, return 501
				sprintf(response_code, "505 Unsupported Protocol");
				file_name = malloc(sizeof(char*) * 9);
				sprintf(file_name, "505.html");

			} else {

				/* print as debugging for now */
				printf("request_method_name: %s\n", request_method_name);
				printf("request_uri: %s\n", request_uri);
				printf("http_version: %s\n\n", http_version);
				
				
				file_name = malloc(sizeof(char*) * (strlen(request_uri) + 1));
				sprintf(file_name, "%s", request_uri);
				file_name++;

				free(request_method_name);
				free(request_uri);

			}


			char *ext;
			ext = strchr(file_name, '.');

			char extension[5];

			if(!ext){
				sprintf(extension, ".html");
				strcat(file_name, ".html");
			} else {
				sprintf(extension, "%s", ext);
			}

			/* set content type appropriately */
			if (strcmp(extension, ".jpg") == 0 || strcmp(extension, ".jpeg") == 0){
				sprintf(content_type, "image/jpeg");
			} else if (strcmp(extension, ".png") == 0){
				sprintf(content_type, "image/png");
			} else if (strcmp(extension, ".gif") == 0){
				sprintf(content_type, "image/jpeg");
			} else {
				sprintf(content_type, "text/html");
			}

			/* open file with right method depending on text or binary */
			FILE *fp;	

			if(strcmp(content_type, "text/html") == 0){
				fp = fopen(file_name, "r");
			} else {
				fp = fopen(file_name, "rb");
			}

			if(!fp){
				// not found, return 404 page
				sprintf(response_code, "404 Not Found");
				sprintf(content_type, "text/html");
				fp = fopen("404.html", "r");
			} else if (!(method_name == 0) && !bad_request && !server_error && !unsupported_protocol){
				// everything's good
				sprintf(response_code, "200 OK");
			}

			// get size of file
			fseek(fp, 0L, SEEK_END);
			content_length = ftell(fp);
			rewind(fp);

			content = malloc((content_length + 1) * sizeof(char*));

			if(!content){
				free(http_version);
				perror("malloc");
				return -1;

			}

			fread(content, sizeof(char*), content_length, fp);

			sprintf(response_header, "HTTP/1.1 %s\r\nAccept-Ranges: bytes\r\nContent-Length: %zu\r\nContent-Type: %s\r\n\r\n", response_code, content_length, content_type);

			printf("response:\n%s\n", response_header);

			if(write(s, response_header, strlen(response_header)) != strlen(response_header)){			
				perror("write");
				free(content);
				free(http_version);
				return -1;
			}

			if(method_name == GET){
			 	if(write(s, content, content_length) != content_length){
					perror("write");
					free(content);
					free(http_version);
					return -1;
				}
			}


		} else {
			perror("malloc");
			free(request_method_name);
			free(request_uri);
			free(http_version);
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

