#ifndef _POLICY_H_
#define _POLICY_H_

#include "util.h"

#define PROXY_RR 0
#define PROXY_HASH 1

typedef struct Hostent {
  char addr[16];
  int port;
} Hostent;

typedef struct Policy {
  Hostent listen;

  int type;

  Hostent *hosts;
  int nhost;

  int curhost;

  //ragel stuff
  const char *p, *pe, *eof;
  int cs;
} Policy;

int InitPolicy(Policy *policy);
int ParsePolicy(Policy *policy, const char *str);

#endif /* _POLICY_H_ */

