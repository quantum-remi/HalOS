#include "ipv4.h"
#include "arp.h"
#include "rtl8139.h"
#include "network.h"
#include "serial.h"
#include <string.h>
#include "liballoc.h"

#define IP_DEFAULT_TTL 64
#define INADDR_NONE 0xFFFFFFFF
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

uint32_t inet_addr(const char *ip_str)
{
    uint32_t result = 0;
    int value = 0;
    int part = 0;

    for (const char *p = ip_str; *p != '\0'; p++)
    {
        if (*p == '.')
        {
            if (value > 255)
                return INADDR_NONE;
            result = (result << 8) | value;
            value = 0;
            part++;
            if (part > 3)
                return INADDR_NONE;
        }
        else if (*p >= '0' && *p <= '9')
        {
            value = value * 10 + (*p - '0');
        }
        else
        {
            return INADDR_NONE;
        }
    }

    if (value > 255 || part != 3)
        return INADDR_NONE;
    result = (result << 8) | value;
    return result;
}
void net_send_ipv4_packet(uint32_t dst_ip, uint8_t protocol, uint8_t *payload, uint16_t payload_len)
{
    // Use static buffer instead of malloc
    static uint8_t packet_buffer[1518]; // Max Ethernet frame size
    uint16_t total_len = sizeof(ipv4_header_t) + payload_len;

    if (total_len > sizeof(packet_buffer)) {
        serial_printf("IPv4: Packet too large\n");
        return;
    }

    uint8_t dst_mac[6];
    uint32_t next_hop;

    // Determine if destination is on local network
    if ((dst_ip & nic.netmask) == (nic.ip_addr & nic.netmask)) {
        next_hop = dst_ip;
    } else {
        next_hop = nic.gateway_ip;
    }

    // serial_printf("IPv4: Routing to %d.%d.%d.%d via %d.%d.%d.%d\n",
    //              (dst_ip >> 24) & 0xFF, (dst_ip >> 16) & 0xFF,
    //              (dst_ip >> 8) & 0xFF, dst_ip & 0xFF,
    //              (next_hop >> 24) & 0xFF, (next_hop >> 16) & 0xFF,
    //              (next_hop >> 8) & 0xFF, next_hop & 0xFF);

    if (!arp_lookup(next_hop, dst_mac)) {
        queue_packet(dst_ip, protocol, payload, payload_len);
        rtl8139_send_arp_request(&nic.ip_addr, &next_hop);
        return;
    }

    ipv4_header_t *ip = (ipv4_header_t *)packet_buffer;
    ip->version_ihl = 0x45; // IPv4, 5 DWORDs
    ip->tos = 0;
    ip->total_length = htons(total_len);
    static uint16_t packet_id = 0;
    ip->id = htons(packet_id++);
    ip->frag_offset = 0;
    ip->ttl = 64;
    ip->protocol = protocol;
    ip->src_ip = htonl(nic.ip_addr);
    ip->dst_ip = htonl(dst_ip);
    ip->checksum = 0;
    ip->checksum = ip_checksum(ip, sizeof(ipv4_header_t));

    memcpy(packet_buffer + sizeof(ipv4_header_t), payload, payload_len);

    eth_send_frame(dst_mac, ETHERTYPE_IP, packet_buffer, total_len);
}