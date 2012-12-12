#ifndef _SOCKET_WRAP_H
#define _SOCKET_WRAP_H

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

int send_msg_by_sockaddr(const void *buf, size_t len, const struct sockaddr_in *from);
int broadcast_msg(uint16_t dst_port, const void *buf, size_t len);

#endif
