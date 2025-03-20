#include "ipv4.h"
#include "arp.h"
#include "rtl8139.h"
#include "network.h"
#include "serial.h"
#include <string.h>

#define IP_DEFAULT_TTL 64

uint16_t ip_checksum(void *data, uint16_t len)
{
    uint32_t sum = 0;
    uint16_t *ptr = (uint16_t *)data;
    for (; len > 1; len -= 2)
        sum += *ptr++;
    if (len == 1)
        sum += *(uint8_t *)ptr;
    sum = (sum >> 16) + (sum & 0xFFFF);
    sum += (sum >> 16);
    return (uint16_t)(~sum);
}

void net_send_ipv4_packet(uint32_t dst_ip, uint8_t protocol, uint8_t *payload, uint16_t payload_len)
{
    if (!payload && payload_len > 0)
    {
        serial_printf("IPv4: Invalid payload parameters\n");
        return;
    }

    if (payload_len > 1500 - sizeof(ipv4_header_t))
    {
        serial_printf("IPv4: Payload too large (%d bytes)\n", payload_len);
        return;
    }

    // Resolve MAC via ARP and send
    uint8_t dst_mac[6];
    if (arp_lookup(dst_ip, dst_mac))
    {
        // Build IPv4 packet dynamically
        uint8_t packet[sizeof(ipv4_header_t) + payload_len];
        ipv4_header_t *ip = (ipv4_header_t *)packet;

        // Fill IPv4 header
        ip->version = 4;
        ip->ihl = 5;
        ip->tos = 0;
        ip->total_length = htons(sizeof(ipv4_header_t) + payload_len);
        ip->id = htons(0x1234);
        ip->frag_offset = htons(0x4000);
        ip->ttl = IP_DEFAULT_TTL;
        ip->protocol = protocol;
        ip->src_ip = htonl(nic.ip_addr); // Current IP
        ip->dst_ip = htonl(dst_ip);
        ip->checksum = 0;
        ip->checksum = ip_checksum(ip, sizeof(ipv4_header_t));

        // Copy payload
        memcpy(packet + sizeof(ipv4_header_t), payload, payload_len);

        // Send the packet
        eth_send_frame(dst_mac, ETHERTYPE_IP, packet, sizeof(packet));
    }
    else
    {
        // Queue parameters (payload + metadata)
        queue_packet(dst_ip, protocol, payload, payload_len);
        // Send ARP request
        uint32_t src_ip = nic.ip_addr;
        rtl8139_send_arp_request(&src_ip, &dst_ip);
    }
}