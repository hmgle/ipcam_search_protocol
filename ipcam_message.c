#include <string.h>
#include "ipcam_message.h"

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
