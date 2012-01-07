#ifndef _POLICY_H_
#define _POLICY_H_

#include "util.h"

#define PROXY_RR 0
#define PROXY_HASH 1

struct hostent {
  char addr[16];
  int port;
};

struct policy {
  struct hostent listen;

  int type;

  struct hostent *hosts;
  int nhost;

  int curhost;

  //ragel stuff
  const char *p, *pe, *eof;
  int cs;
};

int policy_init(struct policy *policy);
int policy_parse(struct policy *policy, const char *p);

#endif /* _POLICY_H_ */

