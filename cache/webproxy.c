#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <getopt.h>

#include "gfserver.h"

#include <semaphore.h>

/* note that the -n and -z parameters are NOT used for Part 1 */
/* they are only used for Part 2 */                         
#define USAGE                                                                         \
"usage:\n"                                                                            \
"  webproxy [options]\n"                                                              \
"options:\n"                                                                          \
"  -n [segment_count]  Number of segments to use (Default: 2)\n"                      \
"  -p [listen_port]    Listen port (Default: 8140)\n"                                 \
"  -t [thread_count]   Num worker threads (Default: 1, Range: 1-1024)\n"              \
"  -s [server]         The server to connect to (Default: Udacity S3 instance)\n"     \
"  -z [segment_size]   The segment size (in bytes, Default: 256).\n"                  \
"  -h                  Show this help message\n"


/* OPTIONS DESCRIPTOR ====================================================== */
static struct option gLongOptions[] = {
  {"segment-count", required_argument,      NULL,           'n'},
  {"port",          required_argument,      NULL,           'p'},
  {"thread-count",  required_argument,      NULL,           't'},
  {"server",        required_argument,      NULL,           's'},
  {"segment-size",  required_argument,      NULL,           'z'},         
  {"help",          no_argument,            NULL,           'h'},
  {NULL,            0,                      NULL,            0}
};

//extern ssize_t handle_with_curl(gfcontext_t *ctx, char *path, void* arg);
extern ssize_t handle_with_cache(gfcontext_t *ctx, char *path, void* arg);

struct WorkerFuncData_Curl {
	int uniqueID;
	char* thisServer;
	size_t segsize;
	int nsegments;
};

static gfserver_t gfs;

static void _sig_handler(int signo){
  if (signo == SIGINT || signo == SIGTERM){
    gfserver_stop(&gfs);
    exit(signo);
  }
}

/* Main ========================================================= */
int main(int argc, char **argv) {
  int i;
  int option_char = 0;
  unsigned short port = 8140;
  unsigned short nworkerthreads = 1;
  unsigned int nsegments = 2;
  size_t segsize = 256;
  char *server = "s3.amazonaws.com/content.udacity-data.com";

  /* disable buffering on stdout so it prints immediately */
  setbuf(stdout, NULL);

  if (signal(SIGINT, _sig_handler) == SIG_ERR) {
    fprintf(stderr,"Can't catch SIGINT...exiting.\n");
    exit(SERVER_FAILURE);
  }

  if (signal(SIGTERM, _sig_handler) == SIG_ERR) {
    fprintf(stderr,"Can't catch SIGTERM...exiting.\n");
    exit(SERVER_FAILURE);
  }

  /* Parse and set command line arguments */
  while ((option_char = getopt_long(argc, argv, "n:p:s:t:z:h", gLongOptions, NULL)) != -1) {
    switch (option_char) {
      case 'n': // segment count
        nsegments = atoi(optarg);
        break;   
      case 'p': // listen-port
        port = atoi(optarg);
        break;
      case 's': // file-path
        server = optarg;
        break;                                          
      case 't': // thread-count
        nworkerthreads = atoi(optarg);
        break;
      case 'z': // segment size
        segsize = atoi(optarg);
        break;
      case 'h': // help
        fprintf(stdout, "%s", USAGE);
        exit(0);
        break;
      default:
        fprintf(stderr, "%s", USAGE);
        exit(__LINE__);
    }
  }

  if (!server) {
    fprintf(stderr, "Invalid (null) server name\n");
    exit(__LINE__);
  }

  if (segsize < 128) {
    fprintf(stderr, "Invalid segment size\n");
    exit(__LINE__);
  }

  if (nsegments < 1) {
    fprintf(stderr, "Must have a positive number of segments\n");
    exit(__LINE__);
  }

 if (port < 1024) {
    fprintf(stderr, "Invalid port number\n");
    exit(__LINE__);
  }

  if ((nworkerthreads < 1) || (nworkerthreads > 1024)) {
    fprintf(stderr, "Invalid number of worker threads\n");
    exit(__LINE__);
  }

  /* This is where you initialize your shared memory */ 

  /* This is where you initialize the server struct */

  sem_t* sem_write;
  sem_write = sem_open("/semNumSegment", O_CREAT, 0644, nsegments + 1);
  int s_value;
  sem_getvalue(sem_write, &s_value);

  gfserver_init(&gfs, nworkerthreads);

  /* This is where you set the options for the server */
  gfserver_setopt(&gfs, GFS_PORT, port);
  gfserver_setopt(&gfs, GFS_MAXNPENDING, 12);
  gfserver_setopt(&gfs, GFS_WORKER_FUNC, handle_with_cache);

  struct WorkerFuncData_Curl WorkerData[nworkerthreads];

  for(i = 0; i < nworkerthreads; i++) {
	  WorkerData[i].thisServer = server;
	  WorkerData[i].uniqueID = i;
	  WorkerData[i].segsize = segsize;
	  WorkerData[i].nsegments = nsegments;

    gfserver_setopt(&gfs, GFS_WORKER_ARG, i, &WorkerData[i]);
  }
  
  /* This is where you invoke the framework to run the server */
  /* Note that it loops forever */
  gfserver_serve(&gfs);
  sem_unlink("/semNumSegment");
}
