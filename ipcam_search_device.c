#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include "ipcam_message.h"

#define DEBUG_PRINT         1
#define debug_print(fmt, ...) \
    do { \
        if (DEBUG_PRINT) \
            fprintf(stderr, "debug_print: %s: %d: %s():" \
                    fmt "\n", __FILE__, __LINE__, __func__, \
                    ##__VA_ARGS__); \
    } while (0)

#define IPCAM_SERVER_PORT   6755
#define PC_SERVER_PORT      6756
#define MAX_MSG_LEN         512

int IPCAM_SERVER_FD = -1;
int IPCAM_CLIENT_FD = -1;

int initsocket(void);
void *deal_msg_func(void *p);
void broadcast_login_msg(void);

int initsocket(void)
{
    int ret;
    struct sockaddr_in ipcam_server;

    IPCAM_CLIENT_FD = socket(AF_INET, SOCK_DGRAM, 0);
    if (IPCAM_CLIENT_FD == -1) {
        debug_print("socket fail");
        exit(errno);
    }

    IPCAM_SERVER_FD = socket(AF_INET, SOCK_DGRAM, 0);
    if (IPCAM_SERVER_FD == -1) {
        debug_print("socket fail");
        exit(errno);
    }

    ipcam_server.sin_family = AF_INET;
    ipcam_server.sin_port = htons(IPCAM_SERVER_PORT);
    ipcam_server.sin_addr.s_addr = htonl(INADDR_ANY);

    ret = bind(IPCAM_SERVER_FD, (struct sockaddr*)&ipcam_server,
               sizeof(ipcam_server));
    if (ret == -1) {
        debug_print("bind fail");
        exit(errno);
    }

    return 0;
} /* int initsocket(void) */

void *deal_msg_func(void *p)
{
    return NULL;
}

void broadcast_login_msg(void)
{
    int ret;
    struct sockaddr_in server;
    const int on = 1;
    struct ipcam_search_msg login_msg;

    memset(&login_msg, 0, sizeof(login_msg));
    login_msg.type = IPCAMMSG_LOGIN;
    strncpy(login_msg.ipcam_name, "ipcam_test", sizeof(login_msg.ipcam_name));

    server.sin_family = AF_INET;
    server.sin_port = htons(PC_SERVER_PORT);
    server.sin_addr.s_addr = inet_addr("255.255.255.255");

    ret = setsockopt(IPCAM_CLIENT_FD, SOL_SOCKET, SO_BROADCAST, &on, sizeof(on));
    if (ret < 0) {
        debug_print("setsockopt fail");
        exit(errno);
    }

    ret = sendto(IPCAM_CLIENT_FD, &login_msg, sizeof(login_msg), 0,
                 (const struct sockaddr *)&server, 
                 (socklen_t)sizeof(server));
    debug_print("ret is %d", ret);

    return;
}

int main(int argc, char **argv)
{
    int ret;
    pthread_t deal_msg_pid;
 
    initsocket();
    broadcast_login_msg();

    ret = pthread_create(&deal_msg_pid, 0, 
                         deal_msg_func, NULL);
    if (ret) {
        debug_print("pthread_create failed");
        exit(errno);
    }

    pthread_join(deal_msg_pid, NULL);

    return 0;
}
