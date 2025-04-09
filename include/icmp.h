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

// Extended ICMP types
#define ICMP_ECHO_REPLY          0
#define ICMP_DEST_UNREACHABLE    3
#define ICMP_SOURCE_QUENCH       4
#define ICMP_REDIRECT            5
#define ICMP_ECHO_REQUEST        8
#define ICMP_TIME_EXCEEDED      11
#define ICMP_PARAMETER_PROBLEM  12
#define ICMP_TIMESTAMP         13
#define ICMP_TIMESTAMP_REPLY   14

#define ICMP_PACKET_SIZE       64
#define ICMP_DATA_PATTERN_START sizeof(icmp_header_t)

void icmp_handle_packet(ipv4_header_t* ip, uint8_t* payload, uint16_t len);
void icmp_send_echo_request(uint32_t dst_ip);

#endif