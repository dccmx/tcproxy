#ifndef _UTIL_H_
#define _UTIL_H_

#include <stdlib.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdarg.h>
#include <unistd.h>

#include "pool.h"

#define RW_BUF_SIZE 32*1024

#define LOG_FATAL 0
#define LOG_ERROR 1
#define LOG_NOTICE 2
#define LOG_DEBUG 3

#define log_fatal(s...) do {\
  log_err(LOG_FATAL, s);\
  exit(EXIT_FAILURE);\
}while(0)

#define print_fatal(s...) do {\
  fprintf(stderr, "fatal: ");\
  fprintf(stderr, s);\
  fprintf(stderr, "\n");\
  exit(EXIT_FAILURE);\
}while(0)

struct rwbuffer {
  char data[RW_BUF_SIZE];
  int r, w;

  short flip;

  //cache the size
  int data_size, free_size;

  struct rwbuffer *next;
};


DECLARE_POOL(rwbuffer);

char *rwbuffer_read_buf(struct rwbuffer *buf);
char *rwbuffer_write_buf(struct rwbuffer *buf);

void rwbuffer_commit_read(struct rwbuffer *buf, int size);
void rwbuffer_commit_write(struct rwbuffer *buf, int size);

void update_time();

void log_err(int level, const char *msg, const char *fmt, ...);

int bind_addr(const char *host, short port);

int connect_addr(const char *host, short port);

int setnonblock(int fd);

#endif /* _UTIL_H_ */
