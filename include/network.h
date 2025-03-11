#ifndef NETWORK_H
#define NETWORK_H

#include <stdint.h>
#include "rtl8139.h"
#include "arp.h"

void net_process_packet(uint8_t* data, uint16_t len);

#endif // NETWORK_H