#ifndef _IPCAM_LIST_H
#define _IPCAM_LIST_H

#include <stdint.h>
#include <netinet/in.h>

typedef struct ipcam_info {
    struct in_addr ipaddr;
    uint8_t mac[6];
    uint8_t ipcam_name[64];
    uint32_t startup_time;
} ipcam_info_t;

struct ipcam_node {
    ipcam_info_t node_info;
    int alive_flag;     /* 0: not online, 1: online */
    struct ipcam_node *next;
};

typedef struct ipcam_node *pipcam_node;
typedef struct ipcam_node *ipcam_link;

ipcam_link create_empty_ipcam_link(void);
ipcam_link insert_ipcam_node(ipcam_link link, const pipcam_node insert_node);
int delete_ipcam_node_by_mac(ipcam_link link, const char *mac);
ipcam_link delete_this_ipcam_node(ipcam_link link, const pipcam_node this_node);
pipcam_node search_ipcam_node_by_mac(ipcam_link link, const char *mac);
int num_ipcam_node(ipcam_link link);
int insert_nodulp_ipcam_node(ipcam_link link, const pipcam_node insert_node);
void free_ipcam_link(ipcam_link link);

#endif
