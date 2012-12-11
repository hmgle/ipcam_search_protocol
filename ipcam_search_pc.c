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
#include "ipcam_list.h"
#include "debug_print.h"

#ifndef LINE_MAX
#define LINE_MAX            2048
#endif

#define MAX_MSG_LEN         512
#define IPCAM_SERVER_PORT   6755
#define PC_SERVER_PORT      6756

static ipcam_link IPCAM_DEV;

void list_ipcam(ipcam_link ipcam_dev);
int run_cmd_by_string(char *cmd_string);
static char *get_line(char *s, size_t n, FILE *f);
void ctrl_c(int signo);
void deal_console_input_sig_init(void);
void release_exit(int signo);
void *deal_console_input(void *p);
void *recv_msg_from_ipcam(void *p);
void deal_msg_func(const struct ipcam_search_msg *msg, const struct sockaddr_in *from);
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

void list_ipcam(ipcam_link ipcam_dev)
{
    pipcam_node q = ipcam_dev->next;

    while (q) {
        debug_print("ipcam_name is %s", q->node_info.ipcam_name);
        q = q->next;
    }

    return;
}

int run_cmd_by_string(char *cmd_string)
{
    int ret = -1;

    debug_print("cmd is %s", cmd_string);
    switch (cmd_string[0]) {
    case 'l':   /* test */
        list_ipcam(IPCAM_DEV);
        break;
    default:
        break;
    } /* switch (cmd_string[0]) */

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
    free_ipcam_link(IPCAM_DEV);

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
    memcpy(save_msg->ipcam_name, 
            ((struct ipcam_search_msg *)msg)->ipcam_name, 
            sizeof(save_msg->ipcam_name));
    if (exten_len) {
        memcpy(save_msg->exten_msg, 
               ((struct ipcam_search_msg *)msg)->exten_msg, 
               exten_len);
    }

    return 0;
}

void deal_msg_func(const struct ipcam_search_msg *msg, const struct sockaddr_in *from)
{
    ipcam_info_t remote_ipcam_info;
    struct ipcam_node new_ipcam_node;

    switch (msg->type) {
    case IPCAMMSG_LOGIN:
        debug_print("new ipcam login");
        memset(&remote_ipcam_info, 0, sizeof(remote_ipcam_info));
        memcpy(&remote_ipcam_info.ipaddr, &from->sin_addr, 
               (size_t)sizeof(struct in_addr));
        memcpy(remote_ipcam_info.mac, msg->exten_msg, msg->exten_len);
        memcpy(remote_ipcam_info.ipcam_name, msg->ipcam_name, 
                (size_t)sizeof(remote_ipcam_info.ipcam_name));
        debug_print("ipaddr is %s", inet_ntoa(remote_ipcam_info.ipaddr));
        debug_print("mac is %02x:%02x:%02x:%02x:%02x:%02x\n", 
            remote_ipcam_info.mac[0],
            remote_ipcam_info.mac[1],
            remote_ipcam_info.mac[2],
            remote_ipcam_info.mac[3],
            remote_ipcam_info.mac[4],
            remote_ipcam_info.mac[5]);
        debug_print("ipcam_name is %s", remote_ipcam_info.ipcam_name);
        memset(&new_ipcam_node, 0, sizeof(new_ipcam_node));
        new_ipcam_node.node_info = remote_ipcam_info;
        new_ipcam_node.alive_flag = 1;
        insert_ipcam_node(IPCAM_DEV, &new_ipcam_node);
        break;
    case IPCAMMSG_LOGOUT:
        break;
    case IPCAMMSG_HEARTBEAT:
        break;
    case IPCAMMSG_ACK_ALIVE:
        break;
    case IPCAMMSG_ACK_DHCP:
        break;
    case IPCAMMSG_ACK_IP:
        break;
    default:
        debug_print("type is  %d ", msg->type);
    } /* switch (msg->type) */

    return;
}

void *recv_msg_from_ipcam(void *p)
{
    int pc_server_fd;
    struct sockaddr_in pc_server;
    struct sockaddr_in peer;
    char buf[MAX_MSG_LEN];
    struct ipcam_search_msg *msg_buf;
    socklen_t len;
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
        len = sizeof(struct sockaddr_in);
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

        deal_msg_func(msg_buf, &peer);

        if (msg_buf) {
            free(msg_buf);
            msg_buf = NULL;
        }
    } /* while (1) */

    return NULL;
} /* void *recv_msg_from_ipcam(void *p) */

int main(int argc, char **argv)
{
    int ret;
    pthread_t deal_msg_pid;
    pthread_t deal_console_input_pid;

    IPCAM_DEV = create_empty_ipcam_link();
    ret = pthread_create(&deal_console_input_pid, 0, 
                         deal_console_input, NULL);
    if (ret) {
        debug_print("pthread_create failed");
        exit(errno);
    }

    ret = pthread_create(&deal_msg_pid, 0, 
                         recv_msg_from_ipcam, NULL);
    if (ret) {
        debug_print("pthread_create failed");
        exit(errno);
    }

    pthread_join(deal_msg_pid, NULL);
    pthread_join(deal_console_input_pid, NULL);

    return 0;
}
