#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include "socket_wrap.h"
#include "debug_print.h"

int send_msg_by_sockaddr(const void *buf, size_t len, const struct sockaddr_in *from)
{
    int ret;
    int socket_fd;

    socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_fd == -1) {
        debug_print("socket fail");
        exit(errno);
    }

    ret = sendto(socket_fd, buf, len, 
                 0, (const struct sockaddr *)from, 
                 (socklen_t)sizeof(struct sockaddr_in));

    if (ret < 0) {
        debug_print("sendto return %d", ret);
    }

    close(socket_fd);
    return ret;
}

int broadcast_msg(uint16_t dst_port, const void *buf, size_t len)
{
    int ret;
    int socket_fd;
    struct sockaddr_in server;
    const int on = 1;

    socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_fd == -1) {
        debug_print("socket fail");
        exit(errno);
    }

    ret = setsockopt(socket_fd, SOL_SOCKET, SO_BROADCAST, &on, sizeof(on));
    if (ret < 0) {
        debug_print("setsockopt fail");
        exit(errno);
    }

    server.sin_family = AF_INET;
    server.sin_port = htons(dst_port);
    server.sin_addr.s_addr = inet_addr("255.255.255.255");

    ret = sendto(socket_fd, buf, len, 
                 0, (const struct sockaddr *)&server, 
                 (socklen_t)sizeof(server));

    close(socket_fd);
    return ret;
}
