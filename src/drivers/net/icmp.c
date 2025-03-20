#include "icmp.h"

#include "serial.h"
#include "ipv4.h"
#include "network.h"

#define ICMP_ECHO_REPLY 0
#define ICMP_ECHO_REQUEST 8

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

void icmp_handle_packet(ipv4_header_t *ip, uint8_t *payload, uint16_t len)
{
    if (!ip || !payload || len < sizeof(icmp_header_t)) {
        serial_printf("ICMP: Invalid packet parameters\n");
        return;
    }

    icmp_header_t *icmp = (icmp_header_t *)payload;
    serial_printf("ICMP: Received type=%d, code=%d from %d.%d.%d.%d\n",
                 icmp->type, icmp->code,
                 (ntohl(ip->src_ip) >> 24) & 0xFF,
                 (ntohl(ip->src_ip) >> 16) & 0xFF,
                 (ntohl(ip->src_ip) >> 8) & 0xFF,
                 ntohl(ip->src_ip) & 0xFF);

    if (icmp->type == ICMP_ECHO_REQUEST) {
        serial_printf("ICMP: Sending echo reply (id=%d, seq=%d)\n",
                     ntohs(icmp->id), ntohs(icmp->seq));
        
        // Swap src/dst IP and send reply
        icmp->type = ICMP_ECHO_REPLY;
        icmp->checksum = 0;
        icmp->checksum = ip_checksum(icmp, len);
        net_send_ipv4_packet(ntohl(ip->src_ip), IP_PROTO_ICMP, (uint8_t *)icmp, len);
    }
}

void icmp_send_echo_request(uint32_t dst_ip)
{
    uint8_t payload[64] = {0};  // Zero initialize
    icmp_header_t* icmp = (icmp_header_t*)payload;

    // Build ICMP header
    icmp->type = ICMP_ECHO_REQUEST;
    icmp->code = 0;
    icmp->checksum = 0;
    icmp->id = htons(0x1234);    // Use a fixed ID for now
    icmp->seq = htons(0x0001);   // Start with sequence 1

    // Fill payload with recognizable pattern using size_t for array indexing
    for (size_t i = sizeof(icmp_header_t); i < sizeof(payload); i++) {
        payload[i] = i & 0xFF;
    }

    serial_printf("ICMP: Building echo request to %d.%d.%d.%d (id=0x%04x seq=%d)\n",
                 (dst_ip >> 24) & 0xFF, (dst_ip >> 16) & 0xFF,
                 (dst_ip >> 8) & 0xFF, dst_ip & 0xFF,
                 ntohs(icmp->id), ntohs(icmp->seq));

    // Calculate checksum over entire ICMP message
    icmp->checksum = ip_checksum(payload, sizeof(payload));
    
    // Send via IP layer
    net_send_ipv4_packet(dst_ip, IP_PROTO_ICMP, payload, sizeof(payload));
}