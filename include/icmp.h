#ifndef ICMP_H
#define ICMP_H

#include "ipv4.h"

#pragma pack(push, 1)
typedef struct
{
    uint8_t type;
    uint8_t code;
    uint16_t checksum;
    uint16_t id;
    uint16_t seq;
} icmp_header_t;
#pragma pack(pop)

void icmp_handle_packet(ipv4_header_t* ip, uint8_t* payload, uint16_t len);
void icmp_send_echo_request(uint32_t dst_ip);

#endif