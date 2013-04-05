#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/stat.h>
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
#include <time.h>
#include "ipcam_message.h"
#include "socket_wrap.h"
#include "get_mac.h"
#include "config_ipcam_info.h"
#include "debug_print.h"

#define IPCAM_SERVER_PORT   6755
#define PC_SERVER_PORT	  6756
#define MAX_MSG_LEN		 512
#define HEARTBEAT_CYCLE	 10	  /* 10s */

static int IPCAM_SERVER_FD = -1;
static int IPCAM_CLIENT_FD = -1;
static time_t STARTUP_TIME;
static uint32_t SSRC;
static uint8_t IPCAM_MAC[6];

int initsocket(void);
static void replay_alive_msg(const struct ipcam_search_msg *recv_msg, const struct sockaddr_in *from);
static void ipcam_deal_msg_func(const struct ipcam_search_msg *msg, const struct sockaddr_in *from);
static void *recv_msg_from_pc(void *p);
void broadcast_login_msg(void);
static void send_heartbeat_msg(void);
static void send_logout_msg(void);
static void release_exit(int signo);

int initsocket(void)
{
	int ret;
	struct sockaddr_in ipcam_server;

#if !_LINUX_
	WSADATA wsadata;
	if (WSAStartup(MAKEWORD(1, 1), &wsadata) == SOCKET_ERROR) {
		debug_print("WSAStartup() fail\n");
		exit(errno);
	}
#endif

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

static void replay_alive_msg(const struct ipcam_search_msg *recv_msg, const struct sockaddr_in *from)
{
	struct sockaddr_in remote_sockaddr;
	struct ipcam_search_msg send_msg = {0};
	uint8_t buf[MAX_MSG_LEN];

	send_msg.type = IPCAMMSG_ACK_ALIVE;
	send_msg.deal_id = recv_msg->deal_id;
	send_msg.ssrc = SSRC;
	send_msg.timestamp = time(NULL);
	get_ipcam_name(send_msg.ipcam_name);
	memcpy(buf, &send_msg, sizeof(send_msg));
	memcpy(buf + sizeof(send_msg), IPCAM_MAC, sizeof(IPCAM_MAC));
	memcpy(buf + sizeof(send_msg) + sizeof(IPCAM_MAC), &STARTUP_TIME, 
			sizeof(STARTUP_TIME));
	((struct ipcam_search_msg *)buf)->exten_len = sizeof(IPCAM_MAC)
						+ sizeof(STARTUP_TIME);

	memcpy(&remote_sockaddr, from, sizeof(remote_sockaddr));
	remote_sockaddr.sin_port = htons(PC_SERVER_PORT);
	send_msg_by_sockaddr(buf, sizeof(send_msg) + 
				((struct ipcam_search_msg *)buf)->exten_len, &remote_sockaddr);

	return;
}

static void ipcam_deal_msg_func(const struct ipcam_search_msg *msg, const struct sockaddr_in *from)
{
	switch (msg->type) {
	case IPCAMMSG_QUERY_ALIVE:
		replay_alive_msg(msg, from);
		break;
	default:
		break;
	}

	return;
}

static void *recv_msg_from_pc(void *p)
{
	int ret;
	struct sockaddr_in peer;
	uint8_t buf[MAX_MSG_LEN];
	struct ipcam_search_msg *msg_buf;
	socklen_t len;

	while (1) {
		len = sizeof(struct sockaddr_in);
		ret = recvfrom(IPCAM_SERVER_FD, buf, sizeof(buf), 0, 
					   (struct sockaddr *)&peer, 
					   (socklen_t *)&len);
		if (ret < 0) {
			debug_log("recvfrom fail");
			continue;
		}
		msg_buf = malloc(sizeof(struct ipcam_search_msg) 
				 + ((struct ipcam_search_msg *)buf)->exten_len);
		parse_msg((const char *)buf, ret, msg_buf);
		ipcam_deal_msg_func(msg_buf, &peer);

		if (msg_buf) {
			free(msg_buf);
			msg_buf = NULL;
		}
	}
	return NULL;
}

void broadcast_login_msg(void)
{
	int ret;
	struct ipcam_search_msg login_msg;
	uint8_t buf[MAX_MSG_LEN];

	debug_log("mac is %02x:%02x:%02x:%02x:%02x:%02x", 
			IPCAM_MAC[0], 
			IPCAM_MAC[1], 
			IPCAM_MAC[2], 
			IPCAM_MAC[3], 
			IPCAM_MAC[4], 
			IPCAM_MAC[5]);

	memset(&login_msg, 0, sizeof(login_msg));
	login_msg.type = IPCAMMSG_LOGIN;
	login_msg.ssrc = SSRC;
	login_msg.timestamp = time(NULL);
	get_ipcam_name(login_msg.ipcam_name);

	memcpy(buf, &login_msg, sizeof(login_msg));
	memcpy(buf + sizeof(login_msg), IPCAM_MAC, sizeof(IPCAM_MAC));
	((struct ipcam_search_msg *)buf)->exten_len = sizeof(IPCAM_MAC);
	ret = broadcast_msg(PC_SERVER_PORT, buf, sizeof(login_msg) + sizeof(IPCAM_MAC));
	debug_log("sendto return %d", ret);

	return;
}

static void send_logout_msg(void)
{
	int ret;
	struct ipcam_search_msg logout_msg;
	uint8_t buf[MAX_MSG_LEN];

	memset(&logout_msg, 0, sizeof(logout_msg));

	logout_msg.type = IPCAMMSG_LOGOUT;
	logout_msg.ssrc = SSRC;
	logout_msg.timestamp = time(NULL);
	get_ipcam_name(logout_msg.ipcam_name);

	memcpy(buf, &logout_msg, sizeof(logout_msg));
	memcpy(buf + sizeof(logout_msg), IPCAM_MAC, sizeof(IPCAM_MAC));

	memcpy(buf + sizeof(logout_msg) + sizeof(IPCAM_MAC), &STARTUP_TIME, 
			sizeof(STARTUP_TIME));
	((struct ipcam_search_msg *)buf)->exten_len = sizeof(IPCAM_MAC)
						+ sizeof(STARTUP_TIME);

	ret = broadcast_msg(PC_SERVER_PORT, buf, 
			sizeof(logout_msg) + ((struct ipcam_search_msg *)buf)->exten_len);
	if (ret < 0)
		debug_log("broadcast_msg fail");
}

static void send_heartbeat_msg(void)
{
	int ret;
	struct ipcam_search_msg heartbeat_msg;
	uint8_t buf[MAX_MSG_LEN];

	memset(&heartbeat_msg, 0, sizeof(heartbeat_msg));

	heartbeat_msg.type = IPCAMMSG_HEARTBEAT;
	heartbeat_msg.ssrc = SSRC;
	heartbeat_msg.timestamp = time(NULL);
	heartbeat_msg.heartbeat_num = 0;
	get_ipcam_name(heartbeat_msg.ipcam_name);

	memcpy(buf, &heartbeat_msg, sizeof(heartbeat_msg));
	memcpy(buf + sizeof(heartbeat_msg), IPCAM_MAC, sizeof(IPCAM_MAC));
	memcpy(buf + sizeof(heartbeat_msg) + sizeof(IPCAM_MAC), &STARTUP_TIME, 
			sizeof(STARTUP_TIME));
	((struct ipcam_search_msg *)buf)->exten_len = sizeof(IPCAM_MAC)
						+ sizeof(STARTUP_TIME);

	while (1) {
		ret = broadcast_msg(PC_SERVER_PORT, buf, 
				sizeof(heartbeat_msg) + ((struct ipcam_search_msg *)buf)->exten_len);
		if (ret < 0)
			debug_log("broadcast_msg fail");

		((struct ipcam_search_msg *)buf)->heartbeat_num++;
		sleep(HEARTBEAT_CYCLE);
	} /* while (1) */
}

static void release_exit(int signo)
{
   send_logout_msg();

	/*
	 * release resources
	 */
	close(IPCAM_SERVER_FD);
	close(IPCAM_CLIENT_FD);
	close_debug_log();

	/*
	 * exit
	 */
	exit(signo);
}

int main(int argc, char **argv)
{
	int ret;
#if _LINUX_
	int devnullfd;
	struct sigaction sa;
#endif
	pthread_t deal_msg_pid;

	signal(SIGINT, release_exit);
	signal(SIGTERM, release_exit);
	open_debug_log("./_ipcam_device_debug.log");

	STARTUP_TIME = time(NULL);
	get_mac(IPCAM_MAC);

	/*
	 * deamon init
	 */
	ret = umask(0);
	if (ret == -1) {
		debug_print("umask fail");
		exit(errno);
	}

#if _LINUX_
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
#endif

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

	ret = pthread_create(&deal_msg_pid, 0, recv_msg_from_pc, NULL);
	if (ret) {
		debug_log("pthread_create failed");
		exit(errno);
	}

	send_heartbeat_msg();

	pthread_join(deal_msg_pid, NULL);
	close_debug_log();

	return 0;
}
