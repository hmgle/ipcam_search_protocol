#ifndef _GET_MAC_H
#define _GET_MAC_H

#include <stdio.h>
#include <stdint.h>
#if _LINUX_
#include <sys/ioctl.h>
#include <net/if.h> 
#include <netinet/in.h>
#else
#include <windows.h>
#include <Iphlpapi.h>
#include <Assert.h>
#define WIN32_LEAN_AND_MEAN
#endif
#include <unistd.h>
#include <string.h>

uint8_t *get_mac(uint8_t *str_mac);

#endif
