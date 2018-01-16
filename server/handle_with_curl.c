#include <stdlib.h>
#include <fcntl.h>
#include <curl/curl.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>

#include "gfserver.h"

struct WorkerFuncData_Curl {
	int uniqueID;
	char* thisServer;
};

//Replace with an implementation of handle_with_curl and any other
//functions you may need.

ssize_t handle_with_curl(gfcontext_t *ctx, char *path, void* arg){
	int fildes;
	size_t file_len, bytes_transferred;
	ssize_t read_len, write_len;

//	char *data_dir = "server";
	struct WorkerFuncData_Curl* thisData = arg;
	char *data_dir = thisData->thisServer;
	int uniqueID = thisData->uniqueID;
	printf("Unique ID is %i \n", uniqueID);

	char filePath[1028];
	snprintf(filePath, sizeof filePath, "tmpfile%d", uniqueID);


	// data_dir is an argument populated by the server
	// path is specific to each thread: it is the file name
	char buffer[4096];
	strcpy(buffer, data_dir);
	strcat(buffer, path);

//	printf("Data directory: %s\n", data_dir);
	printf("File name: %s\n",path);
	printf("Temp file %s\n", filePath);

	FILE *fp;
	fp = fopen(filePath, "w+");
	if (fp == NULL){
		printf("Cannot open file to write %s\n", filePath);
		return gfs_sendheader(ctx, GF_FILE_NOT_FOUND, 0);
	}

	curl_global_init(CURL_GLOBAL_ALL);
	CURL* easyhandle = curl_easy_init();
	curl_easy_setopt(easyhandle, CURLOPT_URL, buffer);
//	curl_easy_setopt(easyhandle, CURLOPT_WRITEDATA, stdout);
	curl_easy_setopt(easyhandle, CURLOPT_WRITEDATA, fp);
	CURLcode curlError = curl_easy_perform(easyhandle);

	if(curlError == CURLE_OK){
		long responsecode;
		curl_easy_getinfo(easyhandle, CURLINFO_RESPONSE_CODE, &responsecode);
		if( responsecode != 200) {
			curl_easy_cleanup(easyhandle);
			puts("File not found\n");
			remove(filePath);
			return gfs_sendheader(ctx, GF_FILE_NOT_FOUND, 0);
		}
	}

	curl_easy_cleanup(easyhandle);

	fclose(fp);

	memset(buffer, 0, sizeof(buffer));

	if( 0 > (fildes = open(filePath, O_RDONLY))){
		if (errno == ENOENT){
			/* If the file just wasn't found, then send FILE_NOT_FOUND code*/
			printf("Curl cannot find file %s\n", path);
			remove(filePath);
			return gfs_sendheader(ctx, GF_FILE_NOT_FOUND, 0);
		} else {
			/* Otherwise, it must have been a server error. gfserver library will handle*/
			remove(filePath);
			return SERVER_FAILURE;
		}
	}

	/* CODE INTENTIONALLY REMOVED */


	printf("Transferred successfully %s: %i\n\n", path, (int) bytes_transferred);

	remove(filePath);
	return bytes_transferred;
}

