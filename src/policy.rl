#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "policy.h"

static struct hostent host;
static int addr_p;
static int have_addr;

%%{
  machine policy_parser;
  access policy->;
  variable p policy->p;
  variable pe policy->pe;
  variable eof policy->eof;

  action init_host {
    addr_p = 0;
    host.addr[addr_p] = '\0';
    have_addr = 0;
  }

  action have_addr {
    have_addr = 1;
  }

  action init_port {
    host.port = 0;
  }

  action append_addr {
    host.addr[addr_p] = fc;
    addr_p++;
  }

  action append_port {
    host.port = host.port * 10 + (fc - '0');
  }

  action finish_addr {
    host.addr[addr_p] = 0;
  }

  action listen_addr {
    if (!have_addr) {
      host.addr[0] = '\0';
    }
    policy->listen = host;
  }

  action append_host {
    if (!have_addr) {
      host.addr[0] = '\0';
    }
    policy->nhost++;
    policy->hosts = realloc(policy->hosts, sizeof(struct hostent) * policy->nhost);
    policy->hosts[policy->nhost - 1] = host;
  }

  action set_rr {
    policy->type = PROXY_RR;
  }

  action set_hash {
    policy->type = PROXY_HASH;
  }

  action error {
    print_fatal("policy syntax error around:\"%s\"\n", fpc);
  }
  
  ws = (' ');
  port = (digit {1,5});
  dottedip = (digit {1,3} '.' digit {1,3} '.' digit {1,3} '.' digit {1,3});
  addr = ('localhost' | 'any' | dottedip) $append_addr %finish_addr;
  host = ((addr ':' >have_addr)? port >init_port $append_port) >init_host;

  type = ('rr' %set_rr | 'hash' %set_hash);
  group = (type ws* '{' ws* host (ws+ >append_host host)* ws* '}' >append_host);

  policy = (host %listen_addr ws* '->' ws* (host >set_rr %append_host | group));

  main := (policy) $!error;
}%%

%% write data;

int policy_parse(struct policy *policy, const char *p) {
  policy->p = p;
  policy->pe = p + strlen(p);
  policy->eof = policy->pe;

  %% write exec;

  if (policy->cs == %%{write error;}%%) {
    return -1;
  } else if (policy ->cs < %%{write first_final;}%%) {
    return 1;
  }

  return 0;
}

int policy_init(struct policy *policy) {
  memset(policy, 0, sizeof(struct policy));
  %% write init;
  return 0;
}

