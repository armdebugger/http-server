#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <dirent.h>
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

char *read_directory(char *directory){
	DIR *dp;
	struct dirent *entry;
	struct stat statbuf;

	char cwd[BUFSIZ];
	getcwd(cwd, sizeof(cwd));

	char updir[strlen(directory)];
	int last_slash = 0;

	if(strcmp(".", directory) != 0){
		for(int i = 0; i < strlen(directory); i++){
			if(directory[i] == '/'){
				last_slash = i;
			}
		}
		printf("%i\n", last_slash);
		strcpy(updir, directory);
		updir[last_slash] = '\0';
		printf("up %s\n", updir);
	}

	dp = opendir(directory);
	chdir(directory);

	char *content = malloc(sizeof(char*) * (85 + strlen(directory)));
	if(strcmp(".", directory) != 0){
		sprintf(content, "<!DOCTYPE html>\n<html>\n<body>\n<h1>index/%s</h1>\n<a href = \"/%s\">Up</a><br>\n", directory, updir);
	} else {
		sprintf(content, "<!DOCTYPE html>\n<html>\n<body>\n<h1>index</h1>\n");
	}

	size_t current_size = 11 + strlen(directory);

	while((entry = readdir(dp)) != NULL){
		lstat(entry->d_name, &statbuf);
		if(strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0){
			
			
			char name[BUFSIZ];
			if(S_ISDIR(statbuf.st_mode)){
				sprintf(name, "<b><a href = \"%s/%s\">%s</a></b><br>\n", directory, entry->d_name, entry->d_name);
			} else {
				sprintf(name, "<i><a href = \"%s/%s\">%s</a></i><br>\n", directory, entry->d_name, entry->d_name);
			}		
			
			if(strlen(content) + strlen(name) + 1 > current_size){
				content = realloc(content, sizeof(char*) * (strlen(content) + strlen(name) + 1));
			}

			strcat(content, name);
			current_size = strlen(content) + 1;
		
		}
	}

	strcat(content, "</ul></body>\n</html>");

	closedir(dp);
	chdir(cwd);
	free(entry);
	return content;
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
		char *request_uri = NULL;
		char *http_version = NULL;

		/* error flags */
		int bad_request = 0;
		//int server_error = 0;
		int unsupported_protocol = 0;

		/* method enum */
		method method_name = 0;
	
		request_method_name = strtok(buffer, " ");
		request_uri = strtok(NULL, " ");
		http_version = strtok(NULL, "\r\n");

		char *header = NULL;

		//printf("fields\n");
		while((header = strtok(NULL, "\r\n")) != NULL){
			//printf("	%s\n", header);
		}

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
		char *content;
		size_t content_length = 0;
		
		struct stat sb;	
		int file_status = -1;

		if(strcmp(request_uri, "/") == 0){
			if(lstat("index.html", &sb) != 0){
				sprintf(file_name, ".");
			} else {
				sprintf(file_name, "index.html");
			}
			
			file_status = 0;
		} else {
			sprintf(file_name, "%s", request_uri + 1);
			file_status = lstat(file_name, &sb);
		}

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

		} else if (file_status != 0){
			// not found, return 404
			sprintf(response_code, "404 Not Found");
			sprintf(file_name, "404.html");
		} else {
			sprintf(response_code, "200 OK");
		}

		printf("file name: %s\n", file_name);
		
		FILE *fp;
		int file = 1;

		if(file_status == 0){
			if(strcmp(file_name, ".") == 0 || S_ISDIR(sb.st_mode)){
				file = 0;
			} else if(!S_ISREG(sb.st_mode)){
				sprintf(response_code, "403 Forbidden");
				sprintf(file_name, "403.html");
			}
		}

		char *ext;
		ext = strchr(file_name, '.');

		char extension[5];

		if(!ext){
			sprintf(extension, ".html");
			//strcat(file_name, ".html");
		} else {
			sprintf(extension, "%s", ext);
		}

		/* set content type appropriately */
		if (strcmp(extension, ".jpg") == 0 || strcmp(extension, ".jpeg") == 0){
			sprintf(content_type, "image/jpeg");
		} else if (strcmp(extension, ".png") == 0){
			sprintf(content_type, "image/png");
		} else if (strcmp(extension, ".gif") == 0){
			sprintf(content_type, "image/gif");
		} else {
			sprintf(content_type, "text/html");
		}
	
		if(file == 1){
			if(strcmp(content_type, "text/html") == 0){
				printf("%s\n", file_name);
				fp = fopen(file_name, "r");
			} else {
				fp = fopen(file_name, "rb");
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
			fclose(fp);
		} else {
			content = read_directory(file_name);
			
			if(!content){
				perror("malloc");
				return -1;
			}

			content_length = strlen(content) + 1;
		}

		sprintf(response_header, "HTTP/1.1 %s\r\nAccept-Ranges: bytes\r\nContent-Length: %zu\r\nContent-Type: %s\r\n\r\n", response_code, content_length, content_type);

		printf("response:\n%s", response_header);

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
		
		free(content);
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

