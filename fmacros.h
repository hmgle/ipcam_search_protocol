#ifndef _REDIS_FMACRO_H
#define _REDIS_FMACRO_H

#ifndef _BSD_SOURCE
#define _BSD_SOURCE
#endif

#if defined(__linux__) || defined(__GLIBC__)
#define _XOPEN_SOURCE 700
#else
#define _XOPEN_SOURCE
#endif

#define _LARGEFILE_SOURCE
#define _FILE_OFFSET_BITS 64

#endif
