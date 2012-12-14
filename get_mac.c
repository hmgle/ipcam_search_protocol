#include <stdlib.h>
#include <errno.h>
#include "get_mac.h"
#include "debug_print.h"

uint8_t *get_mac(uint8_t *str_mac)
{
#if _LINUX_
    int sock;
    struct ifreq ifr;
    struct ifconf ifc;
    uint8_t buf[1024];
    int success = 0;

    sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock == -1) {
        debug_print("socket fail");
        exit(errno);
    }
    memset(&ifr, 0, sizeof(ifr));
    ifc.ifc_len = sizeof(buf);
    ifc.ifc_buf = (char *)buf;
    if (ioctl(sock, SIOCGIFCONF, &ifc) == -1) { /* handle error */ 
        debug_print("ioctl fail");
        exit(errno);
    }

    struct ifreq *it = ifc.ifc_req;
    const struct ifreq * const end = it 
            + (ifc.ifc_len / sizeof(struct ifreq));

    for (; it != end; ++it) {
        strcpy(ifr.ifr_name, it->ifr_name);
        if (!ioctl(sock, SIOCGIFFLAGS, &ifr)) {
            if (!(ifr.ifr_flags & IFF_LOOPBACK)) { // don't count loopback
                if (!ioctl(sock, SIOCGIFHWADDR, &ifr)) {
                    success = 1;
                    break;
                }
            }
        } else { /* handle error */ 
            debug_print("ioctl fail");
            exit(errno);
        }
    }

    if (success) {
        memcpy(str_mac, ifr.ifr_hwaddr.sa_data, 6);
        return str_mac;
    } else
        return NULL;
#else
	IP_ADAPTER_INFO AdapterInfo[16];
	DWORD dwBufLen = sizeof(AdapterInfo);

	DWORD dwStatus = GetAdaptersInfo( AdapterInfo, &dwBufLen);
	assert(dwStatus == ERROR_SUCCESS);

	PIP_ADAPTER_INFO pAdapterInfo = AdapterInfo;
    memcpy(str_mac, pAdapterInfo->Address, 6);

    return str_mac;
#endif
}

