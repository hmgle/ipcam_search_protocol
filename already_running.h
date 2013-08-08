#ifndef _ALREADY_RUNNING_H
#define _ALREADY_RUNNING_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <libgen.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

char *get_cur_proc_basename(char *cur_proc_basename, size_t bufsiz);
char *get_cur_proc_dir(char *cur_proc_dir, size_t bufsiz);
int lockfile(int fd);
int already_running(void);

#endif
