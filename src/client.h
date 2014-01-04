#ifndef __TCPROXY_CLIENT_H_
#define __TCPROXY_CLIENT_H_

struct BufferList;
struct Hostent;

#define CLIENT_TYPE_CLIENT 1
#define CLIENT_TYPE_SERVER 1 << 1

typedef struct Client {
  int fd;
  int type;

  struct Hostent* host;
  struct Client *remote;
  struct BufferList *blist;
} Client;

void InitFreeClientsList(const int nhost);
void ReleaseFreeClientsList();

Client *CreateClient(const int fd);

#endif //__TCPROXY_CLIENT_H_
