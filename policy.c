
#line 1 "policy.rl"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "policy.h"

static struct hostent host;
static int addr_p;


#line 70 "policy.rl"



#line 18 "policy.c"
static const char _policy_parser_actions[] = {
	0, 1, 0, 1, 1, 1, 2, 1, 
	3, 1, 4, 1, 5, 1, 6, 1, 
	7, 1, 8, 2, 0, 1, 2, 6, 
	0, 3, 6, 0, 1
};

static const unsigned char _policy_parser_key_offsets[] = {
	0, 0, 5, 8, 10, 13, 15, 18, 
	20, 23, 25, 29, 31, 32, 40, 43, 
	45, 48, 50, 53, 55, 58, 60, 61, 
	62, 63, 64, 65, 66, 68, 70, 76, 
	79, 81, 84, 86, 89, 91, 94, 96, 
	100, 107, 108, 109, 110, 111, 112, 113, 
	114, 115, 116, 117, 118, 119, 120, 121, 
	122, 123, 124, 125, 126, 127, 129, 130, 
	131, 132, 133, 134, 135, 136, 137, 138, 
	139, 140, 142
};

static const char _policy_parser_trans_keys[] = {
	58, 97, 108, 48, 57, 46, 48, 57, 
	48, 57, 46, 48, 57, 48, 57, 46, 
	48, 57, 48, 57, 58, 48, 57, 48, 
	57, 32, 45, 48, 57, 32, 45, 62, 
	32, 58, 97, 104, 108, 114, 48, 57, 
	46, 48, 57, 48, 57, 46, 48, 57, 
	48, 57, 46, 48, 57, 48, 57, 58, 
	48, 57, 48, 57, 110, 121, 58, 97, 
	115, 104, 32, 123, 32, 123, 32, 58, 
	97, 108, 48, 57, 46, 48, 57, 48, 
	57, 46, 48, 57, 48, 57, 46, 48, 
	57, 48, 57, 58, 48, 57, 48, 57, 
	32, 125, 48, 57, 32, 58, 97, 108, 
	125, 48, 57, 110, 121, 58, 111, 99, 
	97, 108, 104, 111, 115, 116, 111, 99, 
	97, 108, 104, 111, 115, 116, 114, 32, 
	123, 110, 121, 58, 111, 99, 97, 108, 
	104, 111, 115, 116, 48, 57, 0
};

static const char _policy_parser_single_lengths[] = {
	0, 3, 1, 0, 1, 0, 1, 0, 
	1, 0, 2, 2, 1, 6, 1, 0, 
	1, 0, 1, 0, 1, 0, 1, 1, 
	1, 1, 1, 1, 2, 2, 4, 1, 
	0, 1, 0, 1, 0, 1, 0, 2, 
	5, 1, 1, 1, 1, 1, 1, 1, 
	1, 1, 1, 1, 1, 1, 1, 1, 
	1, 1, 1, 1, 1, 2, 1, 1, 
	1, 1, 1, 1, 1, 1, 1, 1, 
	1, 0, 0
};

static const char _policy_parser_range_lengths[] = {
	0, 1, 1, 1, 1, 1, 1, 1, 
	1, 1, 1, 0, 0, 1, 1, 1, 
	1, 1, 1, 1, 1, 1, 0, 0, 
	0, 0, 0, 0, 0, 0, 1, 1, 
	1, 1, 1, 1, 1, 1, 1, 1, 
	1, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 1, 0
};

static const short _policy_parser_index_offsets[] = {
	0, 0, 5, 8, 10, 13, 15, 18, 
	20, 23, 25, 29, 32, 34, 42, 45, 
	47, 50, 52, 55, 57, 60, 62, 64, 
	66, 68, 70, 72, 74, 77, 80, 86, 
	89, 91, 94, 96, 99, 101, 104, 106, 
	110, 117, 119, 121, 123, 125, 127, 129, 
	131, 133, 135, 137, 139, 141, 143, 145, 
	147, 149, 151, 153, 155, 157, 160, 162, 
	164, 166, 168, 170, 172, 174, 176, 178, 
	180, 182, 184
};

static const char _policy_parser_indicies[] = {
	2, 3, 4, 1, 0, 5, 6, 0, 
	7, 0, 8, 7, 0, 9, 0, 10, 
	9, 0, 11, 0, 12, 11, 0, 13, 
	0, 14, 15, 13, 0, 16, 17, 0, 
	18, 0, 18, 20, 21, 22, 23, 24, 
	19, 0, 25, 26, 0, 27, 0, 28, 
	27, 0, 29, 0, 30, 29, 0, 31, 
	0, 32, 31, 0, 33, 0, 34, 0, 
	35, 0, 32, 0, 36, 0, 37, 0, 
	38, 0, 39, 40, 0, 41, 42, 0, 
	42, 44, 45, 46, 43, 0, 47, 48, 
	0, 49, 0, 50, 49, 0, 51, 0, 
	52, 51, 0, 53, 0, 54, 53, 0, 
	55, 0, 56, 57, 55, 0, 58, 44, 
	45, 46, 57, 43, 0, 59, 0, 60, 
	0, 54, 0, 61, 0, 62, 0, 63, 
	0, 64, 0, 65, 0, 66, 0, 67, 
	0, 60, 0, 68, 0, 69, 0, 70, 
	0, 71, 0, 72, 0, 73, 0, 74, 
	0, 35, 0, 75, 0, 76, 77, 0, 
	78, 0, 79, 0, 12, 0, 80, 0, 
	81, 0, 82, 0, 83, 0, 84, 0, 
	85, 0, 86, 0, 79, 0, 33, 0, 
	0, 0
};

static const char _policy_parser_trans_targs[] = {
	0, 2, 9, 62, 65, 3, 2, 4, 
	5, 6, 7, 8, 9, 10, 11, 12, 
	11, 12, 13, 14, 21, 22, 25, 52, 
	60, 15, 14, 16, 17, 18, 19, 20, 
	21, 73, 23, 24, 26, 27, 28, 29, 
	30, 29, 30, 31, 38, 41, 44, 32, 
	31, 33, 34, 35, 36, 37, 38, 39, 
	40, 74, 40, 42, 43, 45, 46, 47, 
	48, 49, 50, 51, 53, 54, 55, 56, 
	57, 58, 59, 61, 29, 30, 63, 64, 
	66, 67, 68, 69, 70, 71, 72
};

static const char _policy_parser_trans_actions[] = {
	17, 19, 1, 19, 19, 3, 3, 3, 
	3, 3, 3, 3, 7, 5, 9, 9, 
	0, 0, 0, 25, 22, 25, 0, 25, 
	0, 3, 3, 3, 3, 3, 3, 3, 
	7, 5, 3, 3, 0, 0, 0, 15, 
	15, 0, 0, 19, 1, 19, 19, 3, 
	3, 3, 3, 3, 3, 3, 7, 5, 
	11, 11, 0, 3, 3, 3, 3, 3, 
	3, 3, 3, 3, 3, 3, 3, 3, 
	3, 3, 3, 0, 13, 13, 3, 3, 
	3, 3, 3, 3, 3, 3, 3
};

static const char _policy_parser_eof_actions[] = {
	0, 17, 17, 17, 17, 17, 17, 17, 
	17, 17, 17, 17, 17, 17, 17, 17, 
	17, 17, 17, 17, 17, 17, 17, 17, 
	17, 17, 17, 17, 17, 17, 17, 17, 
	17, 17, 17, 17, 17, 17, 17, 17, 
	17, 17, 17, 17, 17, 17, 17, 17, 
	17, 17, 17, 17, 17, 17, 17, 17, 
	17, 17, 17, 17, 17, 17, 17, 17, 
	17, 17, 17, 17, 17, 17, 17, 17, 
	17, 11, 0
};

static const int policy_parser_start = 1;
static const int policy_parser_first_final = 73;
static const int policy_parser_error = 0;

static const int policy_parser_en_main = 1;


#line 73 "policy.rl"

static void parse_error(const char* msg, const char *p) {
  printf("%s around \"%s\"\n", msg, p);
}

int policy_parse(struct policy *policy, const char *p) {
  policy->p = p;
  policy->pe = p + strlen(p);
  policy->eof = policy->pe;

  
#line 186 "policy.c"
	{
	int _klen;
	unsigned int _trans;
	const char *_acts;
	unsigned int _nacts;
	const char *_keys;

	if ( ( policy->p) == ( policy->pe) )
		goto _test_eof;
	if (  policy->cs == 0 )
		goto _out;
_resume:
	_keys = _policy_parser_trans_keys + _policy_parser_key_offsets[ policy->cs];
	_trans = _policy_parser_index_offsets[ policy->cs];

	_klen = _policy_parser_single_lengths[ policy->cs];
	if ( _klen > 0 ) {
		const char *_lower = _keys;
		const char *_mid;
		const char *_upper = _keys + _klen - 1;
		while (1) {
			if ( _upper < _lower )
				break;

			_mid = _lower + ((_upper-_lower) >> 1);
			if ( (*( policy->p)) < *_mid )
				_upper = _mid - 1;
			else if ( (*( policy->p)) > *_mid )
				_lower = _mid + 1;
			else {
				_trans += (_mid - _keys);
				goto _match;
			}
		}
		_keys += _klen;
		_trans += _klen;
	}

	_klen = _policy_parser_range_lengths[ policy->cs];
	if ( _klen > 0 ) {
		const char *_lower = _keys;
		const char *_mid;
		const char *_upper = _keys + (_klen<<1) - 2;
		while (1) {
			if ( _upper < _lower )
				break;

			_mid = _lower + (((_upper-_lower) >> 1) & ~1);
			if ( (*( policy->p)) < _mid[0] )
				_upper = _mid - 2;
			else if ( (*( policy->p)) > _mid[1] )
				_lower = _mid + 2;
			else {
				_trans += ((_mid - _keys)>>1);
				goto _match;
			}
		}
		_trans += _klen;
	}

_match:
	_trans = _policy_parser_indicies[_trans];
	 policy->cs = _policy_parser_trans_targs[_trans];

	if ( _policy_parser_trans_actions[_trans] == 0 )
		goto _again;

	_acts = _policy_parser_actions + _policy_parser_trans_actions[_trans];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 )
	{
		switch ( *_acts++ )
		{
	case 0:
#line 17 "policy.rl"
	{
    addr_p = 0;
    host.addr[addr_p] = 0;
    host.port = 0;
  }
	break;
	case 1:
#line 23 "policy.rl"
	{
    host.addr[addr_p] = (*( policy->p));
    addr_p++;
  }
	break;
	case 2:
#line 28 "policy.rl"
	{
    host.port = host.port * 10 + ((*( policy->p)) - '0');
  }
	break;
	case 3:
#line 32 "policy.rl"
	{
    host.addr[addr_p] = 0;
  }
	break;
	case 4:
#line 36 "policy.rl"
	{
    policy->listen = host;
  }
	break;
	case 5:
#line 40 "policy.rl"
	{
    policy->nhost++;
    policy->hosts = realloc(policy->hosts, sizeof(struct hostent) * policy->nhost);
    policy->hosts[policy->nhost - 1] = host;
  }
	break;
	case 6:
#line 46 "policy.rl"
	{
    policy->type = PROXY_RR;
  }
	break;
	case 7:
#line 50 "policy.rl"
	{
    policy->type = PROXY_HASH;
  }
	break;
	case 8:
#line 54 "policy.rl"
	{
    parse_error("error", ( policy->p));
  }
	break;
#line 319 "policy.c"
		}
	}

_again:
	if (  policy->cs == 0 )
		goto _out;
	if ( ++( policy->p) != ( policy->pe) )
		goto _resume;
	_test_eof: {}
	if ( ( policy->p) == ( policy->eof) )
	{
	const char *__acts = _policy_parser_actions + _policy_parser_eof_actions[ policy->cs];
	unsigned int __nacts = (unsigned int) *__acts++;
	while ( __nacts-- > 0 ) {
		switch ( *__acts++ ) {
	case 5:
#line 40 "policy.rl"
	{
    policy->nhost++;
    policy->hosts = realloc(policy->hosts, sizeof(struct hostent) * policy->nhost);
    policy->hosts[policy->nhost - 1] = host;
  }
	break;
	case 8:
#line 54 "policy.rl"
	{
    parse_error("error", ( policy->p));
  }
	break;
#line 349 "policy.c"
		}
	}
	}

	_out: {}
	}

#line 84 "policy.rl"

  if (policy->cs == 
#line 360 "policy.c"
0
#line 85 "policy.rl"
) {
    return -1;
  } else if (policy ->cs < 
#line 366 "policy.c"
73
#line 87 "policy.rl"
) {
    return 1;
  }

  return 0;
}

int policy_init(struct policy *policy) {
  memset(policy, 0, sizeof(struct policy));
  
#line 379 "policy.c"
	{
	 policy->cs = policy_parser_start;
	}

#line 97 "policy.rl"
  return 0;
}

