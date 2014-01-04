#ifndef __TCPROXY_FMACRO_H_
#define __TCPROXY_FMACRO_H_

#define _BSD_SOURCE

#if defined(__linux__) || defined(__OpenBSD__)
#define _XOPEN_SOURCE 700
#else
#define _XOPEN_SOURCE
#endif

#define _LARGEFILE_SOURCE
#define _FILE_OFFSET_BITS 64

#endif  //__TCPROXY_FMACROS_H_
