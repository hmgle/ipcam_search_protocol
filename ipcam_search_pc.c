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

#ifndef LINE_MAX
#define LINE_MAX            2048
#endif

#define MAX_MSG_LEN         512
#define IPCAM_SERVER_PORT   6755
#define PC_SERVER_PORT      6756

int run_cmd_by_string(char *cmd_string);
static char *get_line(char *s, size_t n, FILE *f);
void ctrl_c(int signo);
void deal_console_input_sig_init(void);
void release_exit(int signo);
void *deal_console_input(void *p);
void *deal_msg_func(void *p);
int parse_msg(const char *msg, int size, struct ipcam_search_msg *save_msg);
#if 0
struct ipcam_search_msg {
    uint8_t     type;
    uint8_t     deal_id;
    uint8_t     flag;
    uint8_t     exten_len;
    uint32_t    ssrc;
    uint32_t    timestamp;
    uint32_t    heartbeat_num;
    char        ipcam_name[64];
    char        exten_msg[0];
} __attribute ((packed));

enum {
    IPCAMMSG_LOGIN      = 0x1,
    IPCAMMSG_LOGOUT,
    IPCAMMSG_HEARTBEAT,
    IPCAMMSG_QUERY_ALIVE,
    IPCAMMSG_ACK_ALIVE,
    IPCAMMSG_DHCP,
    IPCAMMSG_ACK_DHCP,
    IPCAMMSG_SET_IP,
    IPCAMMSG_ACK_IP,
} ipcam_search_msg_type;
#endif

int run_cmd_by_string(char *cmd_string)
{
    int ret = -1;
    debug_print("cmd is %s", cmd_string);
    return ret;
}

static char *get_line(char *s, size_t n, FILE *f)
{
    char *p = fgets(s, n, f);

    if (p) {
        size_t last = strlen(s) - 1;

        if (s[last] == '\n')
            s[last] = '\0';
    }
    return p;
}

void ctrl_c(int signo)
{
    release_exit(0);
    return;
}

void release_exit(int signo)
{
    /*
     * release resources
     */

    /*
     * exit
     */
    exit(signo);
}

void deal_console_input_sig_init(void)
{
    signal(SIGINT, ctrl_c);
    return;
}

void *deal_console_input(void *p)
{
    char line_buf[LINE_MAX] = {0};
    char *curr_cmd = NULL;
    int ret;

    deal_console_input_sig_init();
    fprintf(stdout, "Hello, This is ipc_shell\n"
            "type \"help\" for help\n");
    fprintf(stdout, "\033[01;32mipc_shell> \033[0m");
    while (get_line(line_buf, sizeof(line_buf), stdin)) {
        fprintf(stdout, "\033[01;32mipc_shell> \033[0m");
        curr_cmd = strtok(line_buf, ";");
        if (curr_cmd) {
            ret = run_cmd_by_string(curr_cmd);
            debug_print("run_cmd_by_string return %d", ret);
        }

        while ((curr_cmd = strtok(NULL, ";")) != NULL) {
            ret = run_cmd_by_string(curr_cmd);
            debug_print("run_cmd_by_string return %d", ret);
        }
    } /* while (get_line(line_buf, sizeof(line_buf), stdin)) */
    fprintf(stdout, "Good-bye \n");
    release_exit(0);
    return NULL;
} /* void *deal_console_input(void *p) */

int parse_msg(const char *msg, int size, struct ipcam_search_msg *save_msg)
{
    int exten_len;

    exten_len = ((struct ipcam_search_msg *)msg)->exten_len;
    save_msg->type = ((struct ipcam_search_msg *)msg)->type;
    save_msg->deal_id = ((struct ipcam_search_msg *)msg)->deal_id;
    save_msg->flag = ((struct ipcam_search_msg *)msg)->flag;
    save_msg->exten_len = exten_len;
    save_msg->ssrc = ((struct ipcam_search_msg *)msg)->ssrc;
    save_msg->timestamp = ((struct ipcam_search_msg *)msg)->timestamp;
    save_msg->heartbeat_num = ((struct ipcam_search_msg *)msg)->heartbeat_num;
    strncpy(save_msg->ipcam_name, 
            ((struct ipcam_search_msg *)msg)->ipcam_name, 
            sizeof(save_msg->ipcam_name));
    if (exten_len) {
        strncpy(save_msg->exten_msg, 
                ((struct ipcam_search_msg *)msg)->exten_msg, 
                sizeof(save_msg->exten_msg));
    }

    return 0;
}

void *deal_msg_func(void *p)
{
    int pc_server_fd;
    struct sockaddr_in pc_server;
    struct sockaddr_in peer;
    char buf[MAX_MSG_LEN];
    struct ipcam_search_msg *msg_buf;
    int len;
    int ret;

    pc_server_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (pc_server_fd == -1) {
        debug_print("socket fail");
        exit(errno);
    }

    pc_server.sin_family = AF_INET;
    pc_server.sin_port = htons(PC_SERVER_PORT);
    pc_server.sin_addr.s_addr = htonl(INADDR_ANY);

    ret = bind(pc_server_fd, (struct sockaddr*)&pc_server,
               sizeof(pc_server));
    if (ret == -1) {
        debug_print("bind fail");
        exit(errno);
    }

    while (1) {
        ret = recvfrom(pc_server_fd, buf, sizeof(buf), 0, 
                       (struct sockaddr *)&peer, 
                       (socklen_t *)&len);
        if (ret < 0) {
            debug_print("recvfrom fail");
            continue;
        }
        msg_buf = malloc(sizeof(struct ipcam_search_msg) 
                         + ((struct ipcam_search_msg *)buf)->exten_len);
        parse_msg(buf, ret, msg_buf);
        debug_print("msg from %s", msg_buf->ipcam_name);

        if (msg_buf) {
            free(msg_buf);
            msg_buf = NULL;
        }
    } /* while (1) */

    return NULL;
} /* void *deal_msg_func(void *p) */

int main(int argc, char **argv)
{
    int ret;
    pthread_t deal_msg_pid;
    pthread_t deal_console_input_pid;

    ret = pthread_create(&deal_console_input_pid, 0, 
                         deal_console_input, NULL);
    if (ret) {
        debug_print("pthread_create failed");
        exit(errno);
    }

    ret = pthread_create(&deal_msg_pid, 0, 
                         deal_msg_func, NULL);
    if (ret) {
        debug_print("pthread_create failed");
        exit(errno);
    }

    pthread_join(deal_msg_pid, NULL);
    pthread_join(deal_console_input_pid, NULL);

    return 0;
}
