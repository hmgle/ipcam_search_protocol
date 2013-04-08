#include "para_parse.h"

char *para_parse_str(const char *src, const char *para, char *val)
{
	char *p;
	char *q;

	q = strstr(src, para);
	if (!q) {
		*val = '\0';
		return val;
	}
	if (*(q - 1) != '?' && *(q - 1) != '&') {
		*val = '\0';
		return val;
	}
	if (*(q + strlen(para)) != '=') {
		*val = '\0';
		return val;
	}
	q = strchr(q, '=');
	p = val;
	while (*++q)
		if (*q == '&')
			break;
		else
			*p++ = *q;
	*p = '\0';
	return val;
}

int para_parse_int(const char *src, const char *key, int *val, int default_val)
{
	char str[MAX_LINE] = {0};

	para_parse_str(src, key, str);
	if (str[0] == '\0') {
		*val = default_val;
		return *val;
	}
	/* *val = atoi(str); */
	*val = strtol(str, (char **)NULL, 10);
	return *val;
}

