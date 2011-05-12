#ifndef _UTIL_H_
#define _UTIL_H_

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#define FATAL(s) do {\
  fprintf(stderr, "[%s]: ", s);\
  perror("");\
  exit(EXIT_FAILURE);\
}while(0)

struct buf {
  char data[1024];
};

int bind_addr(const char *host, int port);
int setnonblock(int fd);

#endif /* _UTIL_H_ */
