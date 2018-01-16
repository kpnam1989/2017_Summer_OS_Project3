#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <getopt.h>
#include <stdio.h>
#include <pthread.h>

#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/mman.h>
#include <sys/shm.h>
#include <sys/stat.h>

#include <fcntl.h>

#include "shm_channel.h"
#include "simplecache.h"

#include <semaphore.h>

#if !defined(CACHE_FAILURE)
#define CACHE_FAILURE (-1)
#endif // CACHE_FAILURE

#define MAX_CACHE_REQUEST_LEN 1024
#define NAME_SIZE 256
#define REQUEST_KEY 1234
#define RESPONSE_KEY 5678

int msqid, msqidResponse, nthreads;

struct msgRequest_data {
	char mtext[NAME_SIZE];
	char fullPath[2000];
	int uniqueID;
	size_t segsize;
};

struct msgRequest {
	long mtype;
	struct msgRequest_data thisData;
};

struct msgResponse_Data {
	size_t fileSize;
	char sharedSegmentName[NAME_SIZE];
	char semaphoreName[NAME_SIZE];
	int readLen;
};

struct msgResponse {
	long mtype;
	struct msgResponse_Data thisData;

};

struct pThread_data {
	int threadID;
	int msqID;
	int msqIDresponse;
};

static void _sig_handler(int signo) {
	if (signo == SIGINT || signo == SIGTERM) {
		msgctl(msqid, IPC_RMID, (struct msqid_ds *) NULL);
		msgctl(msqidResponse, IPC_RMID, (struct msqid_ds *) NULL);
		puts("\nChannel removed\n");

		char segmentName[NAME_SIZE];
		char semaphoreName[NAME_SIZE];
		char tmpFilePath[NAME_SIZE];

		for (int i = 0; i < nthreads; i++) {
			snprintf(segmentName, NAME_SIZE, "/segmentName%d", i);
			snprintf(semaphoreName, NAME_SIZE, "/semaphoreName%d", i);
			snprintf(tmpFilePath, NAME_SIZE, "tmpfile%d", i);

			/* IPC mechanisms here*/
			shm_unlink(segmentName);
			sem_unlink(semaphoreName);
//			remove(tmpFilePath);

		}

		sem_unlink("/semNumSegment");
		exit(signo);
	}
}

#define USAGE                                                                 \
"usage:\n"                                                                    \
"  simplecached [options]\n"                                                  \
"options:\n"                                                                  \
"  -c [cachedir]       Path to static files (Default: ./)\n"                  \
"  -t [thread_count]   Num worker threads (Default: 1, Range: 1-1024)\n"      \
"  -h                  Show this help message\n"

/* OPTIONS DESCRIPTOR ====================================================== */
static struct option gLongOptions[] = { { "cachedir", required_argument, NULL,
		'c' }, { "help", no_argument, NULL, 'h' }, { "nthreads",
required_argument, NULL, 't' }, { NULL, 0, NULL, 0 } };

void Usage() {
	fprintf(stdout, "%s", USAGE);
}

void* workerFunc(void *threadArg) {
	struct pThread_data* thisData = (struct pThread_data*) threadArg;
	struct msgRequest thisMessage;
	struct msgResponse thisResponse;
	int msqID, msqIDresponse, threadID;
	msqID = thisData->msqID;
	msqIDresponse = thisData->msqIDresponse;
	threadID = thisData->threadID;

	printf("Threads %d receive ID of request and response: %d %d\n", threadID,
			msqID, msqIDresponse);

	char segmentName[NAME_SIZE];
	char semaphoreName[NAME_SIZE];

	snprintf(segmentName, NAME_SIZE, "/segmentName%d", threadID);
	snprintf(semaphoreName, NAME_SIZE, "/semaphoreName%d", threadID);

	// To move outside the while-loop
	int shm_fd = shm_open(segmentName, O_CREAT | O_RDWR, 0666);

//	char tmpFilePath[1028];
//	snprintf(tmpFilePath, sizeof tmpFilePath, "tmpfile%d", threadID);

	size_t segmentSize;
	char* ptr;

	while (1) {
		while (msgrcv(msqID, &thisMessage, sizeof(struct msgRequest_data), 1, 0)
				> 0) {
			segmentSize = thisMessage.thisData.segsize;

			ftruncate(shm_fd, segmentSize);
			ptr = mmap(0, segmentSize, PROT_READ | PROT_WRITE,
			MAP_SHARED, shm_fd, 0);
			if (ptr == MAP_FAILED) {
				printf("Map failed %s\n", segmentName);
				return 0;
			}

			/*
			 * Some codes have been intentionally removed
			 * */

			memset(thisResponse.thisData.sharedSegmentName, '0', NAME_SIZE);
			memset(thisResponse.thisData.semaphoreName, '0', NAME_SIZE);

			strcpy(thisResponse.thisData.sharedSegmentName, segmentName);
			strcpy(thisResponse.thisData.semaphoreName, semaphoreName);

			// If found the file, open the semaphore
			sem_t* sem_write;

			if (file_len > 0) {
				sem_write = sem_open(semaphoreName, O_CREAT, 0644, 0);
				int s_value;
				sem_getvalue(sem_write, &s_value);
				if (s_value != 0)
					perror("Semaphore value not zero");
			}

			if (msgsnd(msqIDresponse, &thisResponse,
					sizeof(struct msgResponse_Data), IPC_NOWAIT) < 0) {
				perror("Response sent error");
			}

			if (file_len <= 0)
				continue;

			size_t bytes_transferred = 0;
			size_t read_len;

			while (bytes_transferred < file_len) {
				sem_wait(sem_write);
				read_len = read(fileDesc, ptr, segmentSize);
				if (read_len <= 0) {
					fprintf(stderr,
							"handle_with_file read error, %li, %li, %li",
							read_len, bytes_transferred, file_len);
					perror("Handle file error");
					return 0;
				}

				thisResponse.thisData.readLen = read_len;
				bytes_transferred += read_len;
				msgsnd(msqIDresponse, &thisResponse,
						sizeof(struct msgResponse_Data), IPC_NOWAIT);
			}

			printf(
					"File sent successful from cache_thread %d to server thread %d: %s\n\n",
					threadID, uniqueID, fileName);
			sem_unlink(semaphoreName);
		}
	}

	munmap(ptr, segmentSize);
	close(shm_fd);
	pthread_exit(NULL);
	return 0;
}

int main(int argc, char **argv) {
	nthreads = 1;
	char *cachedir = "locals.txt";
	char option_char;

	/* disable buffering to stdout */
	setbuf(stdout, NULL);

	while ((option_char = getopt_long(argc, argv, "c:ht:", gLongOptions, NULL))
			!= -1) {
		switch (option_char) {
		case 'c': //cache directory
			cachedir = optarg;
			break;
		case 'h': // help
			Usage();
			exit(0);
			break;
		case 't': // thread-count
			nthreads = atoi(optarg);
			break;
		default:
			Usage();
			exit(1);
		}
	}

	if ((nthreads > 1024) || (nthreads < 1)) {
		fprintf(stderr, "Invalid number of threads\n");
		exit(__LINE__);
	}

	if (SIG_ERR == signal(SIGINT, _sig_handler)) {
		fprintf(stderr, "Unable to catch SIGINT...exiting.\n");
		exit(CACHE_FAILURE);
	}

	if (SIG_ERR == signal(SIGTERM, _sig_handler)) {
		fprintf(stderr, "Unable to catch SIGTERM...exiting.\n");
		exit(CACHE_FAILURE);
	}

	/* Cache initialization */
	simplecache_init(cachedir);

	/* Add your cache code here */
//	key_t requestKey = REQUEST_KEY;
//	key_t responseKey = RESPONSE_KEY;
	int msgflag = IPC_CREAT | 0666;

	if ((msqid = msgget(REQUEST_KEY, msgflag)) == -1) {
		perror("msgger: msgget failed");
		exit(1);
	}

	if ((msqidResponse = msgget(RESPONSE_KEY, msgflag)) == -1) {
		perror("msgger: msgget failed");
		return -1;
	}

	pthread_t tid[nthreads];
	struct pThread_data threadData[nthreads];

	for (int i = 0; i < nthreads; i++) {
		threadData[i].msqID = msqid;
		threadData[i].msqIDresponse = msqidResponse;
		threadData[i].threadID = i;
		pthread_create(&tid[i], NULL, workerFunc, &threadData[i]);
	}

	while (1) {
		;
	}

	// Clear the Message queue channel
	pthread_exit(NULL);
	puts("Done");

	return 0;
}

int main_testSimpleCache() {
	char *cachedir = "locals.txt";
	simplecache_init(cachedir);

	//char* testName = "s3.amazonaws.com/content.udacity-data.com/courses/ud923/filecorpus/yellowstone.jpg";

	char* testName = "/courses/ud923/filecorpus/yellowstone.jpg";
	int fileDesc = simplecache_get(testName);

	if (fileDesc < 0) {
		printf("File not found: %s\n", testName);

		// TODO: check this break
		return 1;
	}

	size_t file_len = lseek(fileDesc, 0, SEEK_END);
	lseek(fileDesc, 0, SEEK_SET);
	printf("File size is %li\n", file_len);
	return 0;
}
