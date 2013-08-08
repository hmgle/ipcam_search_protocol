#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include "socket_wrap.h"
#include "ipcam_message.h"
#include "ipcam_list.h"
#include "para_parse.h"
#include "already_running.h"
#include "debug_print.h"

#include "ae.c"
#include "anet.c"

#ifndef LINE_MAX
#define LINE_MAX            2048
#endif

#define MAX_MSG_LEN         512
#define IPCAM_SERVER_PORT   6755
#define PC_SERVER_PORT      6756
#define CHECK_IPCAM_CYCLE   30      /* 30s */

static ipcam_link IPCAM_DEV;
static uint32_t SSRC;

void list_ipcam(ipcam_link ipcam_dev);
int run_cmd_by_string(aeEventLoop *loop, char *cmd_string);
static void search_ipcam(void);
static char *get_line(char *s, size_t n, FILE *f);
void ctrl_c(int signo);
void deal_console_input_sig_init(void);
void release_exit(int signo);
static void clear_all_dev_online(ipcam_link IPCAM_DEV);
static void test_all_dev_online(ipcam_link IPCAM_DEV);
static void deal_msg_func(const struct ipcam_search_msg *msg, const struct sockaddr_in *from);
static int watch_ipcam_link_clear(struct aeEventLoop *loop, long long id, void *clientData);
static int watch_ipcam_link_test(struct aeEventLoop *loop, long long id, void *clientData);

static int callback_list_ipcam(struct aeEventLoop *loop, long long id, void *clientData);
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

	fprintf(stdout, "\033[01;33mALIVE?\tIP ADDR\t\tDEV NAME\t\tMAC\t\tSTARTUP TIME\n\033[0m");
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

int run_cmd_by_string(aeEventLoop *loop, char *cmd_string)
{
	int ret = -1;

	switch (cmd_string[0]) {
	case 'r': /* renew ipcam list */
		delete_ipcam_all_node(IPCAM_DEV);
	case 's':   /* search ipcam dev */
		search_ipcam();
		// sleep(1);
		aeCreateTimeEvent(loop, 1 * 1000, callback_list_ipcam, NULL, NULL);
		break;
	case 'l':   /* list ipcam dev */
		list_ipcam(IPCAM_DEV);
		fprintf(stdout, "\033[01;32mipc_shell> \033[0m");
		fflush(stdout);
		break;
	case 'q':
		release_exit(0);
		fprintf(stdout, "\033[01;32mipc_shell> \033[0m");
		fflush(stdout);
		break;
	case 'h':
	default:
		printf("\'s\': search ipcam dev\n");
		printf("\'r\': renew ipcam list\n");
		printf("\'l\': list all ipcam dev\n");
		printf("\'q\': quit\n");
		printf("setipcname?ip=ip&name=name\n");
		printf("\'h\': show this help\n");
		fprintf(stdout, "\033[01;32mipc_shell> \033[0m");
		fflush(stdout);
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

		ret = insert_nodulp_ipcam_node(IPCAM_DEV, &new_ipcam_node);
		if (!ret) {
			tmp_node 
			    = search_ipcam_node_by_mac(IPCAM_DEV, 
						       remote_ipcam_info.mac);
			tmp_node->alive_flag = 1;
		}
		debug_print("insert return %d", ret);

		break;
	case IPCAMMSG_LOGOUT:
		debug_print("recv IPCAMMSG_LOGOUT msg");
		ret = delete_ipcam_node_by_mac(IPCAM_DEV, msg->exten_msg);
		if (!ret)
			debug_print("it work badly");

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

		ret = insert_nodulp_ipcam_node(IPCAM_DEV, &new_ipcam_node);
		if (!ret) {
			tmp_node = search_ipcam_node_by_mac(IPCAM_DEV, 
					remote_ipcam_info.mac);
			tmp_node->alive_flag = 1;
		}

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

		ret = insert_nodulp_ipcam_node(IPCAM_DEV, &new_ipcam_node);
		if (!ret) {
			tmp_node 
			    = search_ipcam_node_by_mac(IPCAM_DEV,
						       remote_ipcam_info.mac);
			tmp_node->alive_flag = 1;
		}
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

static int watch_ipcam_link_clear(struct aeEventLoop *loop, long long id, void *clientData)
{
	debug_print("clear");
	clear_all_dev_online(IPCAM_DEV);
	aeCreateTimeEvent(loop, CHECK_IPCAM_CYCLE * 1000, watch_ipcam_link_test, NULL, NULL);
	return -1;
}

static int watch_ipcam_link_test(struct aeEventLoop *loop, long long id, void *clientData)
{
	debug_print("test");
	test_all_dev_online(IPCAM_DEV);

	clear_all_dev_online(IPCAM_DEV);

	return CHECK_IPCAM_CYCLE * 1000;
}

static int callback_list_ipcam(struct aeEventLoop *loop, long long id, void *clientData)
{
	list_ipcam(IPCAM_DEV);
	fprintf(stdout, "\033[01;32mipc_shell> \033[0m");
	fflush(stdout);
	return -1;
}

static void dealcmd(aeEventLoop *loop, int fd, void *privdata, int mask)
{
	char line_buf[LINE_MAX] = {0};
	char *curr_cmd = NULL;
	int ret;

	get_line(line_buf, sizeof(line_buf), stdin);
	curr_cmd = strtok(line_buf, ";");
	if (curr_cmd) {
		ret = run_cmd_by_string(loop, curr_cmd);
	}

	while ((curr_cmd = strtok(NULL, ";")) != NULL) {
		ret = run_cmd_by_string(loop, curr_cmd);
		debug_print("run_cmd_by_string return %d", ret);
	}
}

static int init_server_UDP_fd(int port, char *bindaddr)
{
	int pc_server_fd;
	struct sockaddr_in pc_server;
	int ret;
	int sock_opt;

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
	pc_server.sin_port = htons(port);
	pc_server.sin_addr.s_addr = htonl(INADDR_ANY);
	if (bindaddr && inet_aton(bindaddr, &pc_server.sin_addr) == 0) {
		debug_print("invalid bind address");
		close(pc_server_fd);
		return -1;
	}

	ret = bind(pc_server_fd, (struct sockaddr*)&pc_server,
			   sizeof(pc_server));
	if (ret == -1) {
		debug_print("bind fail");
		exit(errno);
	}
	return pc_server_fd;
}

static void dealnet(aeEventLoop *loop, int fd, void *privdata, int mask)
{
	struct sockaddr_in peer;
	char buf[MAX_MSG_LEN];
	struct ipcam_search_msg *msg_buf;
	socklen_t len;
	int ret;

	len = sizeof(struct sockaddr_in);
	ret = recvfrom(fd, buf, sizeof(buf), 0, 
		   	(struct sockaddr *)&peer, (socklen_t *)&len);
	if (ret < 0) {
		debug_print("recvfrom fail");
		return;
	}
	msg_buf = malloc(sizeof(struct ipcam_search_msg) 
				 + ((struct ipcam_search_msg *)buf)->exten_len);
	parse_msg(buf, ret, msg_buf);
	deal_msg_func(msg_buf, &peer);

	if (msg_buf) {
		free(msg_buf);
		msg_buf = NULL;
	}
}

int main(int argc, char **argv)
{
	int ret;
	aeEventLoop *loop;

	if (already_running()) {
		fprintf(stderr, "already running!\n");
		exit(1);
	}

	SSRC = getpid();
	IPCAM_DEV = create_empty_ipcam_link();

	int pc_server_fd;

	pc_server_fd = init_server_UDP_fd(PC_SERVER_PORT, "0.0.0.0");
	assert(pc_server_fd > 0);
	loop = aeCreateEventLoop();
	fprintf(stdout, "\033[01;32mipc_shell> \033[0m");
	fflush(stdout);
	ret = aeCreateFileEvent(loop, STDIN_FILENO, AE_READABLE, dealcmd, NULL);
	assert(ret != AE_ERR);
	ret = aeCreateFileEvent(loop, pc_server_fd, AE_READABLE, dealnet, NULL);
	assert(ret != AE_ERR);
	aeCreateTimeEvent(loop, CHECK_IPCAM_CYCLE * 1000, watch_ipcam_link_clear, NULL, NULL);
	aeMain(loop);
	return 0;
}
