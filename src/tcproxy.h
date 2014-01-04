#ifndef __TCPROXY_TCPROXY_H_
#define __TCPROXY_TCPROXY_H_

#include <arpa/inet.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/resource.h>
#include <sys/socket.h>

#include "adlist.h"
#include "ae.h"
#include "anet.h"
#include "buffer_list.h"
#include "client.h"
#include "config.h"
#include "log.h"
#include "policy.h"
#include "zmalloc.h"

#define VERSION "0.9.3"

#define NOTUSED(V) ((void) V)

extern Policy* policy;
extern aeEventLoop *el;
extern char error_[1024];
#endif //__TCPROXY_TCPROXY_H_
