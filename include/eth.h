#ifndef ETH_H
#define ETH_H

#include <stdint.h>
#include <stddef.h>

#define ETHERTYPE_ARP  0x0806
#define ETHERTYPE_IP   0x0800

void eth_send_frame(uint8_t *dest_mac, uint16_t ethertype, uint8_t *data, uint16_t len);

void eth_init();

extern void (*eth_send_packet_func)(uint8_t*, uint16_t);
extern void (*eth_get_mac_func)(uint8_t*);

#endif