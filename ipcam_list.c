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

int delete_ipcam_all_node(ipcam_link link)
{
    int ret = 0;
    pipcam_node *curr = &link;
    pipcam_node entry;

    while (*curr) {
	    if ((entry = (*curr)->next) == NULL)
		    break;
	    (*curr)->next = entry->next;
	    curr = &(entry->next);
	    free(entry);
	    ret++;
    }
    return ret;
}

int delete_ipcam_node_by_mac(ipcam_link link, const char *mac)
{
    int ret = 0;
    pipcam_node *curr;
    pipcam_node entry;

    for (curr = &link; (*curr)->next; ) {
	    entry = (*curr)->next;
	    if (!strncmp((const char *)entry->node_info.mac, mac,
			 sizeof(entry->node_info.mac))) {
		    *curr = entry->next;
		    free(entry);
		    ret++;
	    } else
		    curr = &entry->next;
    }
    return ret;
}

ipcam_link delete_this_ipcam_node(ipcam_link link, const pipcam_node this_node)
{
    pipcam_node *curr;
    pipcam_node entry;

    for (curr = &link; (*curr)->next; ) {
	    entry = (*curr)->next;
	    if (entry == this_node) {
		    *curr = entry->next;
		    free(entry);
	    } else
		    curr = &entry->next;
    }
    return link;
}

int strvalncmp(const uint8_t *s1, const uint8_t *s2, size_t n)
{
    int i;
    int ret = 0;

    for (i = 0; i < n; i++) {
        ret = s1[i] - s2[i];
        if (ret)
            return ret;
    }
    return 0;
}

pipcam_node search_ipcam_node_by_mac(ipcam_link link, const uint8_t *mac)
{
    pipcam_node q = link->next;

    while (q) {
        if (!strvalncmp(q->node_info.mac, mac, 
                     sizeof(q->node_info.mac)))
            return q;
        q = q->next;
    }

    return NULL;
}

int num_ipcam_node(const ipcam_link link)
{
    int n = 0;
    ipcam_link *curr;

    for (curr = (ipcam_link *)&link; (*curr)->next; curr = &((*curr)->next))
        n++;
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
                    insert_node->node_info.mac)) {
        debug_print();
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
