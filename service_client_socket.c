/* based on code provided by Ian Batten, modified to send
 * HTTP responses to clients */

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

#define buffer_size 1024

/* enum for the different request methods supported */
typedef enum{
	NOT_SUPPORTED = 0, GET = 1, HEAD = 2
}method;

/* function to produce html for items within a directory */
char *read_directory(char *directory){
	
	/* create various structures to hold info about the diretory */
	DIR *dp;
	struct dirent *entry;
	struct stat statbuf;

	/* store the current working directory for later */
	char cwd[BUFSIZ];
	getcwd(cwd, sizeof(cwd));

	/* find the parent of the current directory */
	char updir[strlen(directory)];
	int last_slash = 0;

	if(strcmp(".", directory) != 0){
		for(int i = 0; i < strlen(directory); i++){
			if(directory[i] == '/'){
				last_slash = i;
			}
		}

		strcpy(updir, directory);
		updir[last_slash] = '\0';
	}

	/* open the directory and change to it */
	dp = opendir(directory);
	chdir(directory);

	/* first write the top of the html (including up button if not in index */
	char *content = malloc(sizeof(char*) * (85 + strlen(directory)));

	if(!content){
		return NULL;
	}

	if(strcmp(".", directory) != 0){
		sprintf(content, "<!DOCTYPE html>\n<html>\n<body>\n<h1>index/%s</h1>\n<a href = \"/%s\">Up</a><br>\n", directory, updir);
	} else {
		sprintf(content, "<!DOCTYPE html>\n<html>\n<body>\n<h1>index</h1>\n");
	}

	size_t current_size = 15 + strlen(directory);

	/* iterate through all the files in the directory */
	while((entry = readdir(dp)) != NULL){
		lstat(entry->d_name, &statbuf);

		/* ignore . and .. */		
		if(strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0){
			
			/* write appropriate html for folders (bold) and files (italics) */
			char name[BUFSIZ];
			if(S_ISDIR(statbuf.st_mode)){
				sprintf(name, "<b><a href = \"%s/%s\">/%s</a></b><br>\n", directory, entry->d_name, entry->d_name);
			} else {
				sprintf(name, "<i><a href = \"%s/%s\">%s</a></i><br>\n", directory, entry->d_name, entry->d_name);
			}		
			
			/* reallocate size in the buffer */
			if(strlen(content) + strlen(name) + 1 > current_size){
				content = realloc(content, sizeof(char*) * (strlen(content) + strlen(name) + 1));

				if(!content){
					return NULL;
				}

			}
	
			/* add to the buffer */
			strcat(content, name);
			current_size = strlen(content) + 1;
		
		}
	}

	/* finish off the html */
	strcat(content, "</body>\n</html>");

	/* return to the original directory */
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
		char *byte_range = NULL;

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
		int byte_range_yes = 0;

		while((header = strtok(NULL, "\r\n")) != NULL){
			if(strncmp(header, "Range: bytes=", 13) == 0){
				byte_range = malloc(sizeof(char*) * (strlen(header) + 1));

				if(!byte_range){
					perror("malloc");
					return -1;
				}

				strcpy(byte_range, header);
				byte_range_yes = 1;
			}

		}

		int byte_length = 1;

		if(byte_range_yes){
			byte_length = strlen(byte_range) - 12;
		}

		char byte_ranges[byte_length];

		/* get rid of the malloc'd string and get just the numbers */
		if(byte_range_yes){
			strcpy(byte_ranges, byte_range + 13);
			free(byte_range);
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

		/* if an index page exists, serve it for a null uri*/
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
			sprintf(file_name, "errors/400.html");
			method_name = 1;

		} else if (unsupported_protocol){
			// not implemented, return 501
			sprintf(response_code, "505 Unsupported Protocol");
			sprintf(file_name, "errors/505.html");

		} else if (method_name == 0){
			// not implemented, return 501
			sprintf(response_code, "501 Not Implemented");
			sprintf(file_name, "errors/501.html");

		} else if (file_status != 0){
			// not found, return 404
			sprintf(response_code, "404 Not Found");
			sprintf(file_name, "errors/404.html");
		} else {
			// everything's ok, return 200
			sprintf(response_code, "200 OK");
		}
		
		FILE *fp;
		int file = 1;

		if(file_status == 0){
			if(strcmp(file_name, ".") == 0 || S_ISDIR(sb.st_mode)){
				file = 0;
			} else if(!S_ISREG(sb.st_mode)){
				sprintf(response_code, "403 Forbidden");
				sprintf(file_name, "errors/403.html");
			}
		}

		char *ext;
		ext = strchr(file_name, '.');

		char extension[5];

		if(!ext){
			sprintf(extension, ".html");
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
		} else if(strcmp(extension, ".html") == 0 || file == 0){
			sprintf(content_type, "text/html");
		} else {
			sprintf(content_type, "text/plain");
		}
	
		/* read file or generate directory page */
		if(file == 1){
			if(strcmp(content_type, "text/html") == 0){
				fp = fopen(file_name, "r");
			} else {
				fp = fopen(file_name, "rb");
			}

			/* get size of file */
			fseek(fp, 0L, SEEK_END);
			content_length = ftell(fp);
			rewind(fp);

			content = malloc((content_length + 1) * sizeof(char*));

			if(!content){
				perror("malloc");
				return -1;
			}
		
			/* read the file into the content buffer */
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

		int requested_range_not_satisfiable = 0;
		int ranges = 1;
					
		/* find out how many byteranges have been requested */
		if(byte_range_yes){

			for(int i = 0; i < strlen(byte_ranges); i++){
				if(byte_ranges[i] == ','){
					ranges++;
				}
			}	
		}

		int markers[ranges][2];
		
		/* do byte range stuff */
		if(byte_range_yes){	
			
			char *tokens = byte_ranges;
			int i = 0;

			/* read each byterange declared */
			while((tokens = strtok(tokens, ",")) != NULL){
				
				if(tokens[0] == ' '){
					tokens++;
				}

				/* get the start and the end of the range */
				for(int j = 0; j < strlen(tokens); j++){
					if(tokens[j] == '-'){

						char a[j];
						strcpy(a, tokens);
						a[j] = '\0';
						markers[i][0] = atoi(a);		

						char b[strlen(tokens) - j];
						strcpy(b, tokens + j + 1);
						markers[i][1] = atoi(b);
					}
				}
	
				i++;
				tokens = NULL;
			}

			/* check that the ranges are satisfiable */
			for(int j = 0; j < ranges; j++){

				if(markers[j][0] > strlen(content) || markers[j][1] > strlen(content)){
					requested_range_not_satisfiable = 1;
				}
			}
			
			/* if not satisfiable return 416 error */
			if(requested_range_not_satisfiable){
				sprintf(response_code, "416 Requested Range Not Satisfiable");
				sprintf(content_type, "text/html");
				FILE *fp = fopen("errors/416.html", "r");

				fseek(fp, 0L, SEEK_END);
				content_length = ftell(fp);
				rewind(fp);

				content = realloc(content, (content_length + 1) * sizeof(char*));

				if(!content){
					perror("malloc");
					return -1;
				}

				fread(content, sizeof(char*), content_length, fp);
				fclose(fp);
			} else {
				sprintf(response_code, "206 Partial Content");
			}
			


		}

		/* different headers if byteranges are requested */
		if(!byte_range_yes || requested_range_not_satisfiable){
			sprintf(response_header, "HTTP/1.1 %s\r\nAccept-Ranges: bytes\r\nContent-Length: %zu\r\nContent-Type: %s\r\n\r\n", response_code, content_length, content_type);
		} else {
			
			/* different headers if multiple byteranges are requested */
			if(ranges == 1){

				char *trimmed_content = calloc(markers[0][1] - markers [0][0] + 1, sizeof(char*));

				if(!trimmed_content){
						perror("malloc");
						free(content);
						return -1;
					}

				/* get the byterange requested */
				strncpy(trimmed_content, &content[markers[0][0]], markers[0][1] - markers [0][0] + 1);
				free(content);
				content = trimmed_content;

				/* how long the requested byterange is */
				int byte_count = markers[0][1] - markers [0][0] + 1;

				/* print the header with byterange headers */
				sprintf(response_header, "HTTP/1.1 %s\r\nAccept-Ranges: bytes\r\nContent-Range: bytes %i-%i/%zu\r\nContent-Length: %i\r\nContent-Type: %s\r\n\r\n", response_code, markers[0][0], markers[0][1], content_length, byte_count, content_type);

				content_length = byte_count;


			} else {

				char *trimmed_content = malloc(sizeof(char*) * 17);

				/* each byterange is surrounded by these tags */
				strcpy(trimmed_content, "--3d6b6a416f9b5\r\n");
		
				for(int j = 0; j < ranges; j++){

					int byte_count = markers[j][1] - markers[j][0] + 1;

					/* each byterange gets its own header with length information */
					char range_header[100];
					sprintf(range_header, "Content-Type: %s\r\nContent-Range: bytes %i-%i/%zu\r\n", content_type, markers[j][0], markers[j][1], content_length);

					/* allocate a bigger amount of content for the next byterange */
					char *bigger_trim = calloc(strlen(trimmed_content) + strlen(range_header) + byte_count, sizeof(char*));

					if(!bigger_trim){
						perror("malloc");
						free(content);
						return -1;
					}

					strcpy(bigger_trim, trimmed_content);
					free(trimmed_content);
					trimmed_content = bigger_trim;
					strcat(trimmed_content, range_header);
					
					/* get the next byterange of content */
					char *range_content = calloc((byte_count + 1), sizeof(char*));

					if(!range_content){
						perror("malloc");
						free(content);
						return -1;
					}

					strncpy(range_content, &content[markers[j][0]], byte_count);
					strcat(trimmed_content, range_content);
					free(range_content);
					strcat(trimmed_content, "\r\n--3d6b6a416f9b5\r\n");
				}

				/* free the old content and use the new one with all the trimmings */
				free(content);
				content = trimmed_content;
				content_length = strlen(content);			

				/* print out the new repsponse header with overall length and tag information */
				sprintf(response_header, "HTTP/1.1 %s\r\nAccept-Ranges:bytes\r\nContent-Length: %lu\r\nContent-Type: multipart/byteranges; boundary=3d6b6a416f9b5\r\n\r\n", response_code, content_length);
			}

		}

		printf("response:\n%s", response_header);

		/* send the response header first */
		if(write(s, response_header, strlen(response_header)) != strlen(response_header)){			
			perror("write");
			free(content);
			return -1;
		}

		/* only need to send content if GET was requested */
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

