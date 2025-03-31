#include "ipv4.h"
#include "arp.h"
#include "rtl8139.h"
#include "network.h"
#include "serial.h"
#include <string.h>
#include "liballoc.h"

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
    uint8_t dst_mac[6];
    if (!arp_lookup(dst_ip, dst_mac)) {
        serial_printf("IPv4: No MAC found for %d.%d.%d.%d\n",
                     (dst_ip >> 24) & 0xFF, (dst_ip >> 16) & 0xFF,
                     (dst_ip >> 8) & 0xFF, dst_ip & 0xFF);
        return;
    }

    uint16_t total_len = sizeof(ipv4_header_t) + payload_len;
    uint8_t *packet = malloc(total_len);
    if (!packet) return;

    ipv4_header_t *ip = (ipv4_header_t *)packet;
    ip->version_ihl = 0x45;  // IPv4, 5 DWORDs
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

    memcpy(packet + sizeof(ipv4_header_t), payload, payload_len);

    eth_send_frame(dst_mac, ETHERTYPE_IP, packet, total_len);
    free(packet);
}