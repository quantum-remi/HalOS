#include "icmp.h"
#include "serial.h"
#include "ipv4.h"
#include "network.h"
#include "liballoc.h"
#include "console.h"

#define ICMP_ECHO_REPLY        0
#define ICMP_ECHO_REQUEST      8
#define ICMP_PACKET_SIZE       64  // Standard ping packet size
#define ICMP_DATA_PATTERN_START sizeof(icmp_header_t)

void icmp_handle_packet(ipv4_header_t *ip, uint8_t *payload, uint16_t len) {
    if (!ip || !payload || len < sizeof(icmp_header_t)) {
        serial_printf("ICMP: Invalid packet parameters\n");
        return;
    }

    icmp_header_t *icmp = (icmp_header_t *)payload;
    serial_printf("ICMP: Received type=%d from %d.%d.%d.%d\n",
                 icmp->type,
                 (ntohl(ip->src_ip) >> 24) & 0xFF,
                 (ntohl(ip->src_ip) >> 16) & 0xFF,
                 (ntohl(ip->src_ip) >> 8) & 0xFF,
                 ntohl(ip->src_ip) & 0xFF);
    console_printf("\n");
    console_printf(" *** ICMP Packet ***\n");
    console_printf("Type: %d\n", icmp->type);
    console_printf("Code: %d\n", icmp->code);
    console_printf("ID: 0x%04x\n", ntohs(icmp->id));
    console_printf("Sequence: %d\n", ntohs(icmp->seq));
    console_printf("Checksum: 0x%04x\n", icmp->checksum);
    console_printf("Data length: %d bytes\n", len - sizeof(icmp_header_t));
    // Verify checksum
    uint16_t saved_checksum = icmp->checksum;
    icmp->checksum = 0;
    uint16_t calc_checksum = ip_checksum(icmp, len);
    
    if (saved_checksum != calc_checksum) {
        serial_printf("ICMP: Bad checksum (rcvd 0x%04x != calc 0x%04x)\n",
                     saved_checksum, calc_checksum);
        icmp->checksum = saved_checksum;  // Restore original
        return;
    }
    icmp->checksum = saved_checksum;

    if (icmp->type == ICMP_ECHO_REQUEST) {
        serial_printf("ICMP: Echo request (ID=0x%04x Seq=%d)\n",
                     ntohs(icmp->id), ntohs(icmp->seq));

        // Statically allocate response buffer
        static uint8_t response[1518]; // Max Ethernet frame size
        
        // Copy and modify the packet
        memcpy(response, payload, len);
        icmp_header_t *reply = (icmp_header_t *)response;
        
        reply->type = ICMP_ECHO_REPLY;
        reply->code = 0;
        reply->checksum = 0;
        reply->checksum = ip_checksum(reply, len);

        // Send response - no memory allocation needed
        net_send_ipv4_packet(ntohl(ip->src_ip), IP_PROTO_ICMP, response, len);
        serial_printf("ICMP: Sent echo reply to %d.%d.%d.%d\n",
                     (ntohl(ip->src_ip) >> 24) & 0xFF,
                     (ntohl(ip->src_ip) >> 16) & 0xFF,
                     (ntohl(ip->src_ip) >> 8) & 0xFF,
                     ntohl(ip->src_ip) & 0xFF);
    }
}

void icmp_send_echo_request(uint32_t dst_ip) {
    // Use static allocation instead of malloc
    static uint8_t packet[ICMP_PACKET_SIZE];
    
    // Initialize header
    icmp_header_t *icmp = (icmp_header_t *)packet;
    icmp->type = ICMP_ECHO_REQUEST;
    icmp->code = 0;
    icmp->checksum = 0;
    icmp->id = htons(1);     // Standard ID
    icmp->seq = htons(1);    // Sequence number

    // Fill payload with pattern
    for (int i = ICMP_DATA_PATTERN_START; i < ICMP_PACKET_SIZE; i++) {
        packet[i] = 'A' + (i % 26);
    }

    // Finalize checksum
    icmp->checksum = ip_checksum(packet, ICMP_PACKET_SIZE);

    // Resolve MAC or queue packet
    uint8_t mac[6];
    if (!arp_lookup(dst_ip, mac)) {
        serial_printf("ICMP: Queueing packet for ARP resolution\n");
        // Queue using static buffer
        queue_packet(dst_ip, IP_PROTO_ICMP, packet, ICMP_PACKET_SIZE);

        rtl8139_send_arp_request(&nic.ip_addr, &dst_ip);
        return;
    }

    // Direct send if MAC known
    net_send_ipv4_packet(dst_ip, IP_PROTO_ICMP, packet, ICMP_PACKET_SIZE);
    serial_printf("ICMP: Sent echo request to %d.%d.%d.%d\n",
                 (dst_ip >> 24) & 0xFF,
                 (dst_ip >> 16) & 0xFF,
                 (dst_ip >> 8) & 0xFF,
                 dst_ip & 0xFF);
}