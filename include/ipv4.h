#ifndef IPV4_H
#define IPV4_H

#include <stdint.h>
#include "eth.h"

#define IP_PROTO_ICMP  1
#define IP_PROTO_TCP   6
#define IP_PROTO_UDP   17

#pragma pack(push, 1)
typedef struct {
    uint8_t  ihl : 4;
    uint8_t  version : 4;
    uint8_t  tos;
    uint16_t total_length;
    uint16_t id;
    uint16_t frag_offset;
    uint8_t  ttl;
    uint8_t  protocol;
    uint16_t checksum;
    uint32_t src_ip;
    uint32_t dst_ip;
} ipv4_header_t;
#pragma pack(pop)

uint16_t ip_checksum(void* data, uint16_t len);
void net_send_ipv4_packet(uint32_t dst_ip, uint8_t protocol, uint8_t* payload, uint16_t payload_len);

#endif