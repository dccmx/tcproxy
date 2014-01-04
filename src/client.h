#ifndef __TCPROXY_CLIENT_H_
#define __TCPROXY_CLIENT_H_

struct BufferList;

#define CLIENT_TYPE_CLIENT 1
#define CLIENT_TYPE_SERVER 1 << 1

typedef struct Client {
  int fd;
  int flags;
  int type;
  struct Client *remote;
  struct BufferList *blist;
} Client;

Client *CreateClient(const int fd, const int type);
void FreeClient(Client *c);

#endif //__TCPROXY_CLIENT_H_
