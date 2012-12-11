#ifndef _GET_MAC_H
#define _GET_MAC_H

#include <stdio.h>
#include <stdint.h>
#include <sys/ioctl.h>
#include <net/if.h> 
#include <unistd.h>
#include <netinet/in.h>
#include <string.h>

uint8_t *get_mac(uint8_t *str_mac);

#endif
