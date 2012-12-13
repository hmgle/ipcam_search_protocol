#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "ipcam_list.h"
#include "debug_print.h"

ipcam_link create_empty_ipcam_link(void)
{
    ipcam_link link0 = malloc(sizeof(struct ipcam_node));
    if (link0)
        link0->next = NULL;
    else {
        debug_print("out of space!");
        exit(errno);
    }

    return link0;
}

ipcam_link insert_ipcam_node(ipcam_link link, const pipcam_node insert_node)
{
    pipcam_node q = malloc(sizeof(struct ipcam_node));
    memcpy(q, insert_node, sizeof(struct ipcam_node));
    q->next = link->next;
    link->next = q;

    return link;
}

int delete_ipcam_node_by_mac(ipcam_link link, const char *mac)
{
    int ret = 0;
    if (link->next == NULL)
        return 0;

    pipcam_node q = link->next;
    pipcam_node p = link;

    do {
        if (!strncmp((const char *)q->node_info.mac, mac, 
                     sizeof(q->node_info.mac))) {
            p->next = q->next;
            if (q) {
                free(q);
                q = NULL;
            }
            ret++;
        }
        p = q;
        if (q)
            q = q->next;
        else
            break;
    } while (1);

    return ret;
}

ipcam_link delete_this_ipcam_node(ipcam_link link, const pipcam_node this_node)
{
    if (!link->next)
        return link;

    pipcam_node q = link->next;
    pipcam_node p = link;

    do {
        if (q == this_node) {
            free(q);
            q = NULL;
        }
        p = q;
        q = q->next;
    } while (q);

    return link;
}

pipcam_node search_ipcam_node_by_mac(ipcam_link link, const char *mac)
{
    pipcam_node q = link->next;

    while (q) {
        if (!strncmp((const char *)q->node_info.mac, mac, 
                     sizeof(q->node_info.mac)))
            return q;
        q = q->next;
    }

    return NULL;
}

int num_ipcam_node(ipcam_link link)
{
    int n = 0;

    while (link->next) {
        n++;
        link = link->next;
    }
    return n;
}

/*
 * return 0: fail
 * return 1: succeed
 */
int insert_nodulp_ipcam_node(ipcam_link link, const pipcam_node insert_node)
{
    int ret = 0;
    if (!search_ipcam_node_by_mac(link, 
                (const char *)insert_node->node_info.mac)) {
        insert_ipcam_node(link, insert_node);
        ret = 1;
    }
    return ret;
}

void free_ipcam_link(ipcam_link link)
{
    pipcam_node q = link->next;

    if (!link->next)
        return;
    else
        q = link->next->next;

    do {
        free(link->next);
        if (q) {
            link->next = q;
            q = q->next;
        } else
            break;
    } while (1);
    return;
}
