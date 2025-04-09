#include "icmp.h"
#include "serial.h"
#include "ipv4.h"
#include "network.h"
#include "liballoc.h"
#include "console.h"
#include "timer.h"

// Track sequence numbers
static uint16_t next_seq = 1;
static uint16_t next_id = 1;

uint32_t prev_id = 0;

// Helper function to print ICMP type as string
static const char* icmp_type_to_string(uint8_t type) {
    switch(type) {
        case ICMP_ECHO_REPLY: return "Echo Reply";
        case ICMP_DEST_UNREACHABLE: return "Destination Unreachable";
        case ICMP_SOURCE_QUENCH: return "Source Quench";
        case ICMP_REDIRECT: return "Redirect";
        case ICMP_ECHO_REQUEST: return "Echo Request";
        case ICMP_TIME_EXCEEDED: return "Time Exceeded";
        case ICMP_PARAMETER_PROBLEM: return "Parameter Problem";
        case ICMP_TIMESTAMP: return "Timestamp";
        case ICMP_TIMESTAMP_REPLY: return "Timestamp Reply";
        default: return "Unknown";
    }
}

void icmp_handle_packet(ipv4_header_t *ip, uint8_t *payload, uint16_t len) {
    if (!ip || !payload || len < sizeof(icmp_header_t)) {
        serial_printf("ICMP: Invalid packet parameters\n");
        return;
    }

    
    icmp_header_t *icmp = (icmp_header_t *)payload;
    if(prev_id >= ntohs(icmp->id)) // dont process same packet two times
    {
        return;
    }
    serial_printf("ICMP: Received %s from %d.%d.%d.%d\n",
                 icmp_type_to_string(icmp->type),
                 (ntohl(ip->src_ip) >> 24) & 0xFF,
                 (ntohl(ip->src_ip) >> 16) & 0xFF,
                 (ntohl(ip->src_ip) >> 8) & 0xFF,
                 ntohl(ip->src_ip) & 0xFF);


    // Verify checksum
    uint16_t saved_checksum = icmp->checksum;
    icmp->checksum = 0;
    uint16_t calc_checksum = ip_checksum(icmp, len);
    
    if (saved_checksum != calc_checksum) {
        serial_printf("ICMP: Checksum verification failed (received: 0x%04x, calculated: 0x%04x)\n",
                     saved_checksum, calc_checksum);
        icmp->checksum = saved_checksum;
        return;
    }
    icmp->checksum = saved_checksum;

    // Handle different ICMP types
    switch(icmp->type) {
        case ICMP_ECHO_REQUEST: {
            serial_printf("ICMP: Processing echo request (ID=0x%04x Seq=%d)\n",
                         ntohs(icmp->id), ntohs(icmp->seq));
        
            const uint16_t max_icmp_len = 1518 - sizeof(ipv4_header_t);
            if (len > max_icmp_len) {
                serial_printf("ICMP: Truncating Echo Reply from %d to %d bytes\n", len, max_icmp_len);
                len = max_icmp_len;
            }
        
            static uint8_t response[1518];
            memcpy(response, payload, len);
            icmp_header_t *reply = (icmp_header_t *)response;
            
            reply->type = ICMP_ECHO_REPLY;
            reply->code = 0;
            reply->checksum = 0;
            reply->checksum = ip_checksum(reply, len);
        
            net_send_ipv4_packet(ntohl(ip->src_ip), IP_PROTO_ICMP, response, len);
            break;
        }
        case ICMP_ECHO_REPLY:
            serial_printf("ICMP: Echo reply received (ID=0x%04x Seq=%d)\n",
                         ntohs(icmp->id), ntohs(icmp->seq));
            console_printf("\n === ICMP Packet Details ===\n");
            console_printf("Type: %d (%s)\n", icmp->type, icmp_type_to_string(icmp->type));
            console_printf("Code: %d\n", icmp->code);
            console_printf("ID: 0x%04x\n", ntohs(icmp->id));
            console_printf("Sequence: %d\n", ntohs(icmp->seq));
            console_printf("Checksum: 0x%04x\n", icmp->checksum);
            console_printf("Data length: %d bytes\n", len - sizeof(icmp_header_t));
            prev_id = ntohs(icmp->id);
            break;

        case ICMP_DEST_UNREACHABLE:
            serial_printf("ICMP: Destination unreachable message received\n");
            break;

        default:
            serial_printf("ICMP: Unhandled ICMP type %d\n", icmp->type);
            break;
    }
}

void icmp_send_echo_request(uint32_t dst_ip) {
    static uint8_t packet[ICMP_PACKET_SIZE];
    
    icmp_header_t *icmp = (icmp_header_t *)packet;
    icmp->type = ICMP_ECHO_REQUEST;
    icmp->code = 0;
    icmp->checksum = 0;
    icmp->id = htons(next_id++);
    icmp->seq = htons(next_seq++);

    // Fill payload with timestamp and pattern
    uint32_t timestamp = htonl(get_ticks());
    memcpy(packet + ICMP_DATA_PATTERN_START, &timestamp, sizeof(timestamp));
    
    for (int i = ICMP_DATA_PATTERN_START + sizeof(timestamp); i < ICMP_PACKET_SIZE; i++) {
        packet[i] = 'A' + (i % 26);
    }

    icmp->checksum = ip_checksum(packet, ICMP_PACKET_SIZE);

    uint8_t mac[6];
    if (!arp_lookup(dst_ip, mac)) {
        serial_printf("ICMP: Initiating ARP resolution for %d.%d.%d.%d\n",
                     (dst_ip >> 24) & 0xFF,
                     (dst_ip >> 16) & 0xFF,
                     (dst_ip >> 8) & 0xFF,
                     dst_ip & 0xFF);
        queue_packet(dst_ip, IP_PROTO_ICMP, packet, ICMP_PACKET_SIZE);
        rtl8139_send_arp_request(&nic.ip_addr, &dst_ip);
        return;
    }

    net_send_ipv4_packet(dst_ip, IP_PROTO_ICMP, packet, ICMP_PACKET_SIZE);
}