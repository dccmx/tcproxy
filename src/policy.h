#ifndef __TCPROXY_POLICY_H_
#define __TCPROXY_POLICY_H_


#define PROXY_RR 0
#define PROXY_HASH 1

typedef struct Hostent {
  char *addr;
  int port;
  int down;
  int index;  // 0,1,..., nhost - 1
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

void FreePolicy(Policy *policy);
Policy *ParsePolicy(const char *str);

#endif /* __TCPROXY_POLICY_H_ */

