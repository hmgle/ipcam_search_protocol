#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#if _LINUX_
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#else
#include <windows.h>
#define sleep(n) Sleep(1000 * (n))
#endif
#include <pthread.h>
#include "socket_wrap.h"
#include "ipcam_message.h"
#include "ipcam_list.h"
#include "para_parse.h"
#include "debug_print.h"

#if _LINUX_
#include "ae.c"
#include "anet.c"
#endif

#ifndef LINE_MAX
#define LINE_MAX            2048
#endif

#define MAX_MSG_LEN         512
#define IPCAM_SERVER_PORT   6755
#define PC_SERVER_PORT      6756
#define CHECK_IPCAM_CYCLE   30      /* 30s */

static ipcam_link IPCAM_DEV;
static uint32_t SSRC;

static pthread_mutex_t IPCAM_DEV_MUTEX = PTHREAD_MUTEX_INITIALIZER;

void list_ipcam(ipcam_link ipcam_dev);
int run_cmd_by_string(char *cmd_string);
static void search_ipcam(void);
static char *get_line(char *s, size_t n, FILE *f);
void ctrl_c(int signo);
void deal_console_input_sig_init(void);
void release_exit(int signo);
void *deal_console_input(void *p);
void *recv_msg_from_ipcam(void *p);
static void clear_all_dev_online(ipcam_link IPCAM_DEV);
static void test_all_dev_online(ipcam_link IPCAM_DEV);
void *maintain_ipcam_link(void *p);
static void deal_msg_func(const struct ipcam_search_msg *msg, const struct sockaddr_in *from);
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
	char time_str[128] = {0};
	pipcam_node q = ipcam_dev->next;

#if _LINUX_
	printf("\033[01;33mALIVE?\tIP ADDR\t\tDEV NAME\t\tMAC\t\tSTARTUP TIME\n\033[0m");
#else
	printf("ALIVE?\tIP ADDR\t\tDEV NAME\t\tMAC\t\tSTARTUP TIME\n");
#endif
	while (q) {
		if (q->alive_flag & 1)
			printf("yes\t");
		else
			printf("no\t");

		printf("%s\t", inet_ntoa(q->node_info.ipaddr));
		printf("%s\t", q->node_info.ipcam_name);
		printf("%02x:%02x:%02x:%02x:%02x:%02x\t", 
			q->node_info.mac[0],
			q->node_info.mac[1],
			q->node_info.mac[2],
			q->node_info.mac[3],
			q->node_info.mac[4],
			q->node_info.mac[5]);
		strftime(time_str, sizeof(time_str), "%F %R:%S",
			 localtime((const time_t *)&(q->node_info.startup_time)));

		printf("%s\n", time_str);
		q = q->next;
	}

	return;
}

int run_cmd_by_string(char *cmd_string)
{
	int ret = -1;

	switch (cmd_string[0]) {
	case 'r': /* renew ipcam list */
		pthread_mutex_lock(&IPCAM_DEV_MUTEX);
		delete_ipcam_all_node(IPCAM_DEV);
		pthread_mutex_unlock(&IPCAM_DEV_MUTEX);
	case 's':   /* search ipcam dev */
		search_ipcam();
		sleep(1);
	case 'l':   /* list ipcam dev */
		pthread_mutex_lock(&IPCAM_DEV_MUTEX);
		list_ipcam(IPCAM_DEV);
		pthread_mutex_unlock(&IPCAM_DEV_MUTEX);
		break;
	case 'q':
		release_exit(0);
		break;
	case 'h':
	default:
		printf("\'s\': search ipcam dev\n");
		printf("\'r\': renew ipcam list\n");
		printf("\'l\': list all ipcam dev\n");
		printf("\'q\': quit\n");
		printf("setipcname?ip=ip&name=name\n");
		printf("\'h\': show this help\n");
		break;
	} /* switch (cmd_string[0]) */

	return ret;
}

static void search_ipcam(void)
{
	int ret;
	struct ipcam_search_msg send_msg;

	memset(&send_msg, 0, sizeof(send_msg));
	send_msg.type = 0x4;
	send_msg.ssrc = SSRC;
	ret = broadcast_msg(IPCAM_SERVER_PORT, &send_msg, sizeof(send_msg));
	if (ret < 0)
		debug_print("fail: sendto return %d", ret);

	return;
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
	fprintf(stdout, "\nGood-bye \n");
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
#if _LINUX_
	fprintf(stdout, "\033[01;32mipc_shell> \033[0m");
#else
	fprintf(stdout, "ipc_shell> ");
#endif
	while (get_line(line_buf, sizeof(line_buf), stdin)) {
		curr_cmd = strtok(line_buf, ";");
		if (curr_cmd) {
			ret = run_cmd_by_string(curr_cmd);
		}

		while ((curr_cmd = strtok(NULL, ";")) != NULL) {
			ret = run_cmd_by_string(curr_cmd);
			debug_print("run_cmd_by_string return %d", ret);
		}
#if _LINUX_
		fprintf(stdout, "\033[01;32mipc_shell> \033[0m");
#else
		fprintf(stdout, "ipc_shell> ");
#endif
	} /* while (get_line(line_buf, sizeof(line_buf), stdin)) */
	fprintf(stdout, "Good-bye \n");
	release_exit(0);
	return NULL;
} /* void *deal_console_input(void *p) */

static void deal_msg_func(const struct ipcam_search_msg *msg, 
						  const struct sockaddr_in *from)
{
	int ret;
	ipcam_info_t remote_ipcam_info;
	struct ipcam_node new_ipcam_node;
	pipcam_node tmp_node;

	memset(&remote_ipcam_info, 0, sizeof(remote_ipcam_info));

	switch (msg->type) {
	case IPCAMMSG_LOGIN:
		debug_print("new ipcam login");
		memcpy(&remote_ipcam_info.ipaddr, &from->sin_addr, 
			   (size_t)sizeof(struct in_addr));
		memcpy(remote_ipcam_info.mac, msg->exten_msg, msg->exten_len);
		memcpy(remote_ipcam_info.ipcam_name, msg->ipcam_name, 
				(size_t)sizeof(remote_ipcam_info.ipcam_name));
		remote_ipcam_info.startup_time = msg->timestamp;
		debug_print("ipaddr is %s", inet_ntoa(remote_ipcam_info.ipaddr));
		debug_print("mac is %02x:%02x:%02x:%02x:%02x:%02x", 
			    remote_ipcam_info.mac[0],
			    remote_ipcam_info.mac[1],
			    remote_ipcam_info.mac[2],
			    remote_ipcam_info.mac[3],
			    remote_ipcam_info.mac[4],
			    remote_ipcam_info.mac[5]);
		debug_print("ipcam_name is %s\n", remote_ipcam_info.ipcam_name);
		debug_print("timestamp is %d\n", remote_ipcam_info.startup_time);
		memset(&new_ipcam_node, 0, sizeof(new_ipcam_node));
		new_ipcam_node.node_info = remote_ipcam_info;
		new_ipcam_node.alive_flag = 1;

		pthread_mutex_lock(&IPCAM_DEV_MUTEX);
		ret = insert_nodulp_ipcam_node(IPCAM_DEV, &new_ipcam_node);
		if (!ret) {
			tmp_node 
			    = search_ipcam_node_by_mac(IPCAM_DEV, 
						       remote_ipcam_info.mac);
			tmp_node->alive_flag = 1;
		}
		debug_print("insert return %d", ret);
		pthread_mutex_unlock(&IPCAM_DEV_MUTEX);

		break;
	case IPCAMMSG_LOGOUT:
		debug_print("recv IPCAMMSG_LOGOUT msg");
		pthread_mutex_lock(&IPCAM_DEV_MUTEX);
		ret = delete_ipcam_node_by_mac(IPCAM_DEV, msg->exten_msg);
		if (!ret)
			debug_print("it work badly");

		pthread_mutex_unlock(&IPCAM_DEV_MUTEX);
		break;
	case IPCAMMSG_HEARTBEAT:
		memcpy(&remote_ipcam_info.ipaddr, 
		       &from->sin_addr, 
		       (size_t)sizeof(struct in_addr));
		memcpy(remote_ipcam_info.mac, 
		       msg->exten_msg, 
		       sizeof(remote_ipcam_info.mac));
		memcpy(remote_ipcam_info.ipcam_name, 
		       msg->ipcam_name, 
		       (size_t)sizeof(remote_ipcam_info.ipcam_name));
		memcpy(&remote_ipcam_info.startup_time, 
		       msg->exten_msg + sizeof(remote_ipcam_info.mac), 
		       sizeof(remote_ipcam_info.startup_time));
		memset(&new_ipcam_node, 0, sizeof(new_ipcam_node));
		new_ipcam_node.node_info = remote_ipcam_info;
		new_ipcam_node.alive_flag = 1;

		pthread_mutex_lock(&IPCAM_DEV_MUTEX);
		ret = insert_nodulp_ipcam_node(IPCAM_DEV, &new_ipcam_node);
		if (!ret) {
			tmp_node = search_ipcam_node_by_mac(IPCAM_DEV, 
												remote_ipcam_info.mac);
			tmp_node->alive_flag = 1;
		}
		pthread_mutex_unlock(&IPCAM_DEV_MUTEX);

		break;
	case IPCAMMSG_ACK_ALIVE:
		memcpy(&remote_ipcam_info.ipaddr, &from->sin_addr, 
			   (size_t)sizeof(struct in_addr));
		memcpy(remote_ipcam_info.mac, msg->exten_msg, 
			   sizeof(remote_ipcam_info.mac));
		memcpy(remote_ipcam_info.ipcam_name, msg->ipcam_name, 
			   (size_t)sizeof(remote_ipcam_info.ipcam_name));
		memcpy(&remote_ipcam_info.startup_time, 
				msg->exten_msg + sizeof(remote_ipcam_info.mac), 
				sizeof(remote_ipcam_info.startup_time));
		debug_print("ipaddr is %s", inet_ntoa(remote_ipcam_info.ipaddr));
		debug_print("mac is %02x:%02x:%02x:%02x:%02x:%02x", 
			remote_ipcam_info.mac[0],
			remote_ipcam_info.mac[1],
			remote_ipcam_info.mac[2],
			remote_ipcam_info.mac[3],
			remote_ipcam_info.mac[4],
			remote_ipcam_info.mac[5]);
		debug_print("ipcam_name is %s\n", remote_ipcam_info.ipcam_name);
		debug_print("timestamp is %d\n", remote_ipcam_info.startup_time);
		memset(&new_ipcam_node, 0, sizeof(new_ipcam_node));
		new_ipcam_node.node_info = remote_ipcam_info;
		new_ipcam_node.alive_flag = 1;

		pthread_mutex_lock(&IPCAM_DEV_MUTEX);
		ret = insert_nodulp_ipcam_node(IPCAM_DEV, &new_ipcam_node);
		if (!ret) {
			tmp_node 
			    = search_ipcam_node_by_mac(IPCAM_DEV,
						       remote_ipcam_info.mac);
			tmp_node->alive_flag = 1;
		}
		pthread_mutex_unlock(&IPCAM_DEV_MUTEX);

		break;
	case IPCAMMSG_ACK_DHCP:
		break;
	case IPCAMMSG_ACK_IP:
		break;
	default:
		debug_print("i am confused by the type of %d ", msg->type);
	} /* switch (msg->type) */

	return;
} /* static void deal_msg_func() */

void *recv_msg_from_ipcam(void *p)
{
	int pc_server_fd;
	struct sockaddr_in pc_server;
	struct sockaddr_in peer;
	char buf[MAX_MSG_LEN];
	struct ipcam_search_msg *msg_buf;
	socklen_t len;
	int ret;
	int sock_opt;

#if !_LINUX_
	WSADATA wsadata;
	if (WSAStartup(MAKEWORD(1, 1), &wsadata) == SOCKET_ERROR) {
		debug_print("WSAStartup() fail\n");
		exit(errno);
	}
#endif

	pc_server_fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (pc_server_fd == -1) {
		debug_print("socket fail");
		exit(errno);
	}

	sock_opt = 1;
	ret = setsockopt(pc_server_fd, SOL_SOCKET, SO_REUSEADDR, 
			 &sock_opt, sizeof(sock_opt));
	if (ret < 0) {
		debug_print("setsockopt fail");
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

static void clear_all_dev_online(ipcam_link IPCAM_DEV)
{
	pipcam_node q = IPCAM_DEV->next;

	while (q) {
		q->alive_flag |= 1 << 1;
		q = q->next;
	}
}

static void test_all_dev_online(ipcam_link IPCAM_DEV)
{
	pipcam_node q = IPCAM_DEV->next;

	while (q) {
		if (q->alive_flag != 1)
			q->alive_flag = 0;
		q = q->next;
	}
}

void *maintain_ipcam_link(void *p)
{
	while (1) {
		pthread_mutex_lock(&IPCAM_DEV_MUTEX);
		clear_all_dev_online(IPCAM_DEV);
		pthread_mutex_unlock(&IPCAM_DEV_MUTEX);

		sleep(CHECK_IPCAM_CYCLE);

		pthread_mutex_lock(&IPCAM_DEV_MUTEX);
		test_all_dev_online(IPCAM_DEV);
		pthread_mutex_unlock(&IPCAM_DEV_MUTEX);
	}

	return NULL;
}

int main(int argc, char **argv)
{
	int ret;
	pthread_t deal_msg_pid;
	pthread_t deal_console_input_pid;
	pthread_t maintain_ipcam_link_pid;
#if _LINUX_
	aeEventLoop *loop;
#endif

	SSRC = getpid();
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

	ret = pthread_create(&maintain_ipcam_link_pid, 0, 
						 maintain_ipcam_link, NULL);
	if (ret) {
		debug_print("pthread_create failed");
		exit(errno);
	}

	pthread_join(maintain_ipcam_link_pid, NULL);
	pthread_join(deal_msg_pid, NULL);
	pthread_join(deal_console_input_pid, NULL);

#if _LINUX_
	loop = aeCreateEventLoop();
#endif
	return 0;
}
