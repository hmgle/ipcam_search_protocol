#include "already_running.h"

#ifndef PATH_MAX
#define PATH_MAX	1024
#endif

#define LOCKMODE	(S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)

static char *get_cur_proc_basename(char *cur_proc_basename, size_t bufsiz);
static char *get_cur_proc_dir(char *cur_proc_dir, size_t bufsiz);
static int lockfile(int fd);

static char *get_cur_proc_basename(char *cur_proc_basename, size_t bufsiz)
{
	int count;

	count = readlink("/proc/self/exe", cur_proc_basename, bufsiz);
	if (count < 0) {
		perror("readlink");
		exit(errno);
	}
	cur_proc_basename[count] = '\0';
	strcpy(cur_proc_basename, basename(cur_proc_basename));

	return cur_proc_basename;
}

static char *get_cur_proc_dir(char *cur_proc_dir, size_t bufsiz)
{
	int count;

	count = readlink("/proc/self/exe", cur_proc_dir, bufsiz);
	if (count < 0) {
		perror("readlink");
		exit(errno);
	}
	cur_proc_dir[count] = '\0';
	dirname(cur_proc_dir);
	strcat(cur_proc_dir, "/");

	return cur_proc_dir;
}

static int lockfile(int fd)
{
	struct flock fl;

	fl.l_type = F_WRLCK;
	fl.l_start = 0;
	fl.l_whence = SEEK_SET;
	fl.l_len = 0;
	return(fcntl(fd, F_SETLK, &fl));
} /* int lockfile(int fd) */

int already_running(void)
{
	char cur_proc_dir[PATH_MAX];
	char cur_proc_basename[PATH_MAX];
	int fd;
	char buf[16];
	int ret;

	get_cur_proc_basename(cur_proc_basename, PATH_MAX);
	get_cur_proc_dir(cur_proc_dir, PATH_MAX);
	strcat(cur_proc_dir, "._");
	strcat(cur_proc_dir, cur_proc_basename);
	strcat(cur_proc_dir, "_locked_.pid");

	fd = open(cur_proc_dir, O_RDWR|O_CREAT, LOCKMODE);
	if (fd < 0) {
		fprintf(stderr, "can't open %s: %s\n", 
			cur_proc_dir, strerror(errno));
		exit(1);
	}
	if (lockfile(fd) < 0) {
		if (errno == EACCES || errno == EAGAIN) {
			close(fd);
			return 1;
		}
		fprintf(stderr, "can't open %s: %s\n", 
			cur_proc_dir, strerror(errno));
		exit(1);
	}
	ret = ftruncate(fd, 0);
	if (ret == -1)
		perror("ftruncate");

	sprintf(buf, "%ld", (long)getpid());
	ret = write(fd, buf, strlen(buf)+1);
	if (ret == -1)
		perror("write");
	return 0;
} /* int already_running(void) */
