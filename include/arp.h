#ifndef ARP_H
#define ARP_H

#include <stdint.h>
#include "rtl8139.h"

#pragma pack(push, 1) // Disable struct padding

#define ARP_REQUEST 1
#define ARP_REPLY   2

// Ethernet header
struct eth_header {
    uint8_t dest_mac[6];
    uint8_t src_mac[6];
    uint16_t ethertype;
};

// ARP payload
struct arp_packet {
    uint16_t hw_type;
    uint16_t proto_type;
    uint8_t hw_size;
    uint8_t proto_size;
    uint16_t opcode;
    uint8_t sender_mac[6];
    uint8_t sender_ip[4];
    uint8_t target_mac[6];
    uint8_t target_ip[4];
};

#pragma pack(pop)

void rtl8139_send_arp_request(uint8_t *src_ip, uint8_t *target_ip);

#endif // ARP_H