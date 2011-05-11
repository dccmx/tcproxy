#include "util.h"

int setnonblock(int fd) {
  int opts;
  if ((opts = fcntl(fd, F_GETFL)) < 0) {
    FATAL("GET FLAG");
  }
  opts = opts | O_NONBLOCK;
  if(fcntl(fd, F_SETFL, opts) < 0) {
    FATAL("SET NOBLOCK");
  }
  return 0;
}

int bind_addr(const char *host, int port) {
  int fd, on = 1;
  struct sockaddr_in addr;

  if ((fd = socket(PF_INET, SOCK_STREAM, 0)) == -1) {
    FATAL("socket");
  }

  if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(int)) ==-1) {
    FATAL("set sock opt");
  }

  memset(&addr, 0, sizeof(struct sockaddr_in));
  addr.sin_family = PF_INET;
  addr.sin_port = htons(port);
  inet_aton(host, &addr.sin_addr);

  if (bind(fd, (struct sockaddr*)&addr, sizeof(struct sockaddr_in)) == -1) {
    FATAL("bind");
  }

  if (listen(fd, 10) == -1) {
    FATAL("listen");
  }

  setnonblock(fd);

  return fd;
}
