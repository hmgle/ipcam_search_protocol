#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include "ipcam_message.h"
#include "get_mac.h"
#include "debug_print.h"

#define IPCAM_SERVER_PORT   6755
#define PC_SERVER_PORT      6756
#define MAX_MSG_LEN         512

int IPCAM_SERVER_FD = -1;
int IPCAM_CLIENT_FD = -1;
int SSRC;
uint8_t buf[512];

int initsocket(void);
void *deal_msg_func(void *p);
void broadcast_login_msg(void);

int initsocket(void)
{
    int ret;
    struct sockaddr_in ipcam_server;

    IPCAM_CLIENT_FD = socket(AF_INET, SOCK_DGRAM, 0);
    if (IPCAM_CLIENT_FD == -1) {
        debug_log("socket fail");
        exit(errno);
    }

    IPCAM_SERVER_FD = socket(AF_INET, SOCK_DGRAM, 0);
    if (IPCAM_SERVER_FD == -1) {
        debug_log("socket fail");
        exit(errno);
    }

    ipcam_server.sin_family = AF_INET;
    ipcam_server.sin_port = htons(IPCAM_SERVER_PORT);
    ipcam_server.sin_addr.s_addr = htonl(INADDR_ANY);

    ret = bind(IPCAM_SERVER_FD, (struct sockaddr*)&ipcam_server,
               sizeof(ipcam_server));
    if (ret == -1) {
        debug_log("bind fail");
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

    uint8_t mac[6];
    get_mac(mac);
    debug_log("mac is %02x:%02x:%02x:%02x:%02x:%02x", 
                mac[0], 
                mac[1], 
                mac[2], 
                mac[3], 
                mac[4], 
                mac[5]);

    memset(&login_msg, 0, sizeof(login_msg));
    login_msg.type = IPCAMMSG_LOGIN;
    login_msg.ssrc = SSRC;
    strncpy(login_msg.ipcam_name, "ipcam_name0", sizeof(login_msg.ipcam_name));

    server.sin_family = AF_INET;
    server.sin_port = htons(PC_SERVER_PORT);
    server.sin_addr.s_addr = inet_addr("255.255.255.255");

    ret = setsockopt(IPCAM_CLIENT_FD, SOL_SOCKET, SO_BROADCAST, &on, sizeof(on));
    if (ret < 0) {
        debug_log("setsockopt fail");
        exit(errno);
    }

    memcpy(buf, &login_msg, sizeof(login_msg));
    memcpy(buf + sizeof(login_msg), mac, sizeof(mac));
    ((struct ipcam_search_msg *)buf)->exten_len = sizeof(mac);
    ret = sendto(IPCAM_CLIENT_FD, buf, sizeof(login_msg) + sizeof(mac), 
                 0, (const struct sockaddr *)&server, 
                 (socklen_t)sizeof(server));
    debug_log("sendto return %d", ret);

    return;
}

int main(int argc, char **argv)
{
    int ret;
    int devnullfd;
    struct sigaction sa;
    pthread_t deal_msg_pid;

    open_debug_log("./_ipcam_device_debug.log");

    /*
     * deamon init
     */
    ret = umask(0);
    if (ret == -1) {
        debug_print("umask fail");
        exit(errno);
    }

    devnullfd = open("/dev/null", 0);
    if (devnullfd == -1) {
        debug_print("open fail");
        exit(errno);
    }

    ret = dup2(devnullfd, STDIN_FILENO);
    if (ret == -1) {
        debug_log("dup2 fail");
        exit(errno);
    }

    ret = dup2(devnullfd, STDOUT_FILENO);
    if (ret == -1) {
        debug_log("dup2 fail");
        exit(errno);
    }

    switch (fork()) {
    case -1:
        debug_log("fork fail");
        exit(errno);
        break;
    case 0:
        break;
    default:
        exit(0);
        break;
    }
    setsid();

    sa.sa_handler = SIG_IGN;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(SIGHUP, &sa, NULL) < 0) {
        debug_log("sigaction fail");
        exit(errno);
    }

    if (chdir("/") < 0) {
        debug_log("chdir fail");
        exit(errno);
    }

    /*
     * server init
     */
    SSRC = getpid();
    initsocket();
    broadcast_login_msg();

    ret = pthread_create(&deal_msg_pid, 0, 
                         deal_msg_func, NULL);
    if (ret) {
        debug_log("pthread_create failed");
        exit(errno);
    }

    pthread_join(deal_msg_pid, NULL);

    close_debug_log();

    return 0;
}
