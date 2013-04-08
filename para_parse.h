#ifndef _PARA_PARSE_H
#define _PARA_PARSE_H

#include <string.h>

#ifndef MAX_LINE
#define MAX_LINE	1024
#endif

char *para_parse_str(const char *src, const char *key, char *def);
int para_parse_int(const char *src, const char *key, int *val, int default_val);

#endif
