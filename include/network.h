#ifndef NETWORK_H
#define NETWORK_H

#include <stdint.h>
#include "rtl8139.h"
#include "arp.h"

#define htons(x) ((((x) >> 8) & 0xFF) | (((x) & 0xFF) << 8))
#define ntohs(x) htons(x)
#define htonl(x) ((((x) >> 24) & 0xFF) | (((x) >> 8) & 0xFF00) | (((x) << 8) & 0xFF0000) | ((x) << 24))
#define ntohl(x) htonl(x)

void net_process_packet(uint8_t* data, uint16_t len);

#endif // NETWORK_H