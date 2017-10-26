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
		char *request_uri = NULL;
		char *http_version = NULL;
		char *middle = NULL;
		char *bytes = NULL;

		/* error flags */
		int bad_request = 0;
		int server_error = 0;
		int unsupported_protocol = 0;

		/* method enum */
		method method_name = 0;
	
		request_method_name = strtok(buffer, " ");
		request_uri = strtok(NULL, " ");
		http_version = strtok(NULL, "\r\n");
		
		int range_potential = 1;

		while(range_potential){

			middle = strtok(NULL, "R");

			if(!middle){
				range_potential = 0;
			} else {
				char temp[strlen(middle) + 1];
				strcpy(temp, middle);
				temp[12] = '\0';

				printf("%s\n", temp);

				if(strcmp(temp, "ange: bytes=") == 0){
					middle = strtok(NULL, "=");
					bytes = strtok(NULL, "\r\n");
					range_potential = 0;
				}
			}
		}

		printf("%s\n", bytes);

		//printf("|%s|%s|%s|\n", request_method_name, request_uri, http_version);

		if(!request_method_name || !request_uri || !http_version || request_uri[0] != '/'){
			bad_request = 1;
		} else {
			char temp[strlen(http_version) + 1];
			strcpy(temp, http_version);
			temp[5] = '\0';
			if(strcmp(temp, "HTTP/") != 0){
				bad_request = 1;
			} else if(strlen(http_version) == 8){
				strcpy(temp, http_version + 5);
				if(strcmp(temp, "1.1") != 0 && strcmp(temp, "1.0") != 0){
					unsupported_protocol = 1;
				}			
			} else {
				unsupported_protocol = 1;
			}

			if(strcmp(request_method_name, "GET") == 0){
				method_name = 1;
			} else if (strcmp(request_method_name, "HEAD") == 0){
				method_name = 2;
			}

			if(strcmp(request_uri, "/") == 0){
				sprintf(request_uri, "/index.html");
			}
		}

		int file_length = 8;

		if(request_uri){
			file_length = strlen(request_uri) + 6;
		}

		/* initialise our return variables */
		char response_header[200]; 
		char response_code[50];
		char content_type[15];
		char file_name[file_length];
		unsigned char *content;
		size_t content_length = 0;

		sprintf(response_header, "%s ", http_version);

		if(bad_request){
			// bad request return 400
			sprintf(response_code, "400 Bad Request");		
			sprintf(file_name, "400.html");
			method_name = 1;

		} else if (unsupported_protocol){
			// not implemented, return 501
			sprintf(response_code, "505 Unsupported Protocol");
			sprintf(file_name, "505.html");

		} else if (method_name == 0){
			// not implemented, return 501
			sprintf(response_code, "501 Not Implemented");
			sprintf(file_name, "501.html");

		} else {
			// try and find the file
			sprintf(file_name, "%s", request_uri + 1);
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

		printf("%s\n", file_name);

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
			perror("malloc");
			return -1;

		}

		fread(content, sizeof(char*), content_length, fp);

		sprintf(response_header, "HTTP/1.1 %s\r\nAccept-Ranges: bytes\r\nContent-Length: %zu\r\nContent-Type: %s\r\n\r\n", response_code, content_length, content_type);

		printf("response:\n%s\n", response_header);

		if(write(s, response_header, strlen(response_header)) != strlen(response_header)){			
			perror("write");
			free(content);
			return -1;
		}

		if(method_name == GET){
		 	if(write(s, content, content_length) != content_length){
				perror("write");
				free(content);
				return -1;
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

