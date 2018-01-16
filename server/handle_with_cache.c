#include <stdlib.h>
#include <fcntl.h>
#include <curl/curl.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include <sys/ipc.h>
#include <sys/msg.h>

#include <sys/mman.h>
#include <sys/stat.h>
#include <semaphore.h>

#include "gfserver.h"

#define NAME_SIZE 256
#define REQUEST_KEY 1234
#define RESPONSE_KEY 5678
#define SEGMENT_SIZE 4096

struct msgRequest {
	long mtype;

	struct msgRequest_data {
		char mtext[NAME_SIZE];
		char fullPath[2000];
		int uniqueID;
		size_t segsize;
	} thisData;
};

struct msgResponse_Data {
	long fileSize;
	char sharedSegmentName[NAME_SIZE];
	char semaphoreName[NAME_SIZE];
	int readLen;
};

struct msgResponse {
	long mtype;
	struct msgResponse_Data thisData;
};

struct WorkerFuncData_Curl {
	int uniqueID;
	char* thisServer;
	size_t segsize;
	int nsegments;
};

//Replace with an implementation of handle_with_cache and any other
//functions you may need.

int SendRequest(char* fullURL, char* path, int thisID, size_t segsize){
	// Request contains ID and file name that it is requesting for
	key_t key = REQUEST_KEY;
	int msgflag = IPC_CREAT | 0666;
	int msqid;

	struct msgRequest thisMessage;
	thisMessage.mtype = 1;
	strcpy(thisMessage.thisData.mtext, path);
	strcpy(thisMessage.thisData.fullPath, fullURL);
	thisMessage.thisData.uniqueID = thisID;
	thisMessage.thisData.segsize = segsize;

	if((msqid = msgget(key, msgflag)) == -1){
		perror("msgger: msgget failed");
		return -1;
	}

	if(msgsnd(msqid, &thisMessage, sizeof(struct msgRequest_data), IPC_NOWAIT) < 0 ){
		return -1;
	} else {
		printf("Thread %d sent %s\n", thisMessage.thisData.uniqueID, thisMessage.thisData.mtext);
	}

	return 0;
}

int ReceiveResponse(int msqidResponse, int thisID, struct msgResponse* thisResponse){
	// Receive the response
	if(msgrcv(msqidResponse, thisResponse, sizeof(struct msgResponse_Data), thisID + 10, 0) < 0 ){
		perror("Message received error");
		exit(1);
	}

	return 0;
}

ssize_t handle_with_cache(gfcontext_t *ctx, char *path, void* arg){
	struct WorkerFuncData_Curl* thisData = arg;
	char *data_dir = thisData->thisServer;
	int uniqueID = thisData->uniqueID;
	size_t segmentSize = thisData->segsize;

	sem_t* semNSegments;
	semNSegments = sem_open("/semNumSegment", O_CREAT, 0644, thisData->nsegments);
	sem_wait(semNSegments);

	int s_value;
	sem_getvalue(semNSegments, &s_value);
	printf("Value of semNsegments semaphore is: %d\n", s_value);


	// data_dir is an argument populated by the server
	// path is specific to each thread: it is the file name
	char fullURL[2000];
	strcpy(fullURL, data_dir);
	strcat(fullURL, path);

	if(SendRequest(fullURL, path, uniqueID, thisData->segsize) != 0){
		sem_post(semNSegments);
		return SERVER_FAILURE;
	}

	puts("Request sent");

	struct msgResponse thisResponse;

//	key_t responseKey = RESPONSE_KEY;
//	int msgflag = IPC_CREAT | 0744;
	int msgflag = IPC_CREAT;

	int msqidResponse;
	if((msqidResponse = msgget(RESPONSE_KEY, msgflag)) == -1){
		perror("msgger: msgget for Response failed");
		sem_post(semNSegments);
		return SERVER_FAILURE;
	}

	puts("Receiving response");

	/*
	 * The code has been intentionally removed
	 * */


	gfs_sendheader(ctx, GF_OK, file_len);

	/* create the shared memory segment */
	char* sharedSegmentName = thisResponse.thisData.sharedSegmentName;
	int shm_fd = shm_open(sharedSegmentName, O_CREAT | O_RDONLY, 0666);
	if (shm_fd < 0){
		fprintf(stderr, "Shared segment not acquired %s", sharedSegmentName);
		sem_post(semNSegments);
	}

	ftruncate(shm_fd, segmentSize);
	char* ptr = mmap(0, segmentSize, PROT_READ, MAP_SHARED, shm_fd, 0);
	if (ptr == MAP_FAILED) {
		perror("Map failed");
		sem_post(semNSegments);
		return -1;
	}

	// Acquire semaphore to read data
	char* sem_ReadName = thisResponse.thisData.semaphoreName;
	sem_t* sem_ReadSegment = sem_open(sem_ReadName, O_CREAT, 0644, 0);

	sem_getvalue(sem_ReadSegment, &s_value);
	if (s_value != 0){
		sem_post(sem_ReadSegment);
		perror("Semaphore value not zero");
	}

	// Receiving data and sent it to client
	size_t bytes_transferred = 0;
	size_t read_len, write_len;
	while(bytes_transferred < file_len){
		sem_post(sem_ReadSegment);

		ReceiveResponse(msqidResponse, uniqueID, &thisResponse);
		read_len = thisResponse.thisData.readLen;

		if(read_len <= 0){
			sem_post(semNSegments);
			perror("read_len <= 0");
		}
		// Read from the shared memory
		write_len = gfs_send(ctx, ptr, read_len);

		if (write_len != read_len){
			fprintf(stderr, "Write len different from read len. Write len: %li, Read len %li\n", write_len, read_len);
			sem_post(semNSegments);
			return SERVER_FAILURE;
		}

		bytes_transferred += write_len;
	}

	puts("File transferred successfully\n");
	close(shm_fd);
	munmap(ptr, segmentSize);
	sem_post(semNSegments);

	return bytes_transferred;

}

