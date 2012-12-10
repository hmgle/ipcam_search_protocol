#ifndef _IPCAM_MESSAGE_H
#define _IPCAM_MESSAGE_H

#include <stdint.h>

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
    IPCAMMSG_LOGIN = 0x1,
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
