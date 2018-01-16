#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <curl/curl.h>
#include "gfserver.h"

#define BUFSIZE (4096)

ssize_t handle_with_file(gfcontext_t *ctx, char *path, void* arg){
	int fildes;
	size_t file_len, bytes_transferred;
	ssize_t read_len, write_len;
	char buffer[BUFSIZE];
	char *data_dir = arg;

	strncpy(buffer,data_dir, BUFSIZE);
	strncat(buffer,path, BUFSIZE);

	/* Some code has been intentionally removed */
	bytes_transferred = 0;
	while(bytes_transferred < file_len){
		read_len = read(fildes, buffer, BUFSIZE);
		if (read_len <= 0){
			fprintf(stderr, "handle_with_file read error, %zd, %zu, %zu", read_len, bytes_transferred, file_len );
			return SERVER_FAILURE;
		}
		write_len = gfs_send(ctx, buffer, read_len);
		if (write_len != read_len){
			fprintf(stderr, "handle_with_file write error");
			return SERVER_FAILURE;
		}
		bytes_transferred += write_len;
	}

	return bytes_transferred;
}

