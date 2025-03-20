#include "network.h"

#include "serial.h"
#include "rtl8139.h"
#include "ipv4.h"
#include "arp.h"
#include "icmp.h"

void net_process_packet(uint8_t *data, uint16_t len)
{
    if (!data || len < sizeof(struct eth_header))
    {
        // serial_printf("NET: Invalid packet received (data=%p len=%d)\n", data, len);
        return;
    }

    struct eth_header *eth = (struct eth_header *)data;
    uint16_t ethertype = ntohs(eth->ethertype);

    if (ethertype == ETHERTYPE_ARP)
    {
        struct arp_packet *arp = (struct arp_packet *)(data + sizeof(struct eth_header));
        uint16_t opcode = ntohs(arp->opcode);
        serial_printf("NET: ARP packet opcode=%d\n", opcode);

        // Handle ARP replies
        if (opcode == ARP_REPLY)
        {
            // Extract sender IP and MAC from the ARP reply
            uint32_t sender_ip = ntohl(*(uint32_t *)arp->sender_ip);

            arp_cache_update(sender_ip, arp->sender_mac);
            serial_printf("NET: ARP reply received for %d.%d.%d.%d → %02x:%02x:%02x:%02x:%02x:%02x\n",
                          arp->sender_ip[0], arp->sender_ip[1], arp->sender_ip[2], arp->sender_ip[3],
                          arp->sender_mac[0], arp->sender_mac[1], arp->sender_mac[2],
                          arp->sender_mac[3], arp->sender_mac[4], arp->sender_mac[5]);
            retry_pending_packets(); // Retry queued packets now that MAC is resolved
        }
        // Handle ARP requests (existing code)
        else if (opcode == ARP_REQUEST)
        {
            struct arp_packet *arp = (struct arp_packet *)(data + sizeof(struct eth_header));
            uint16_t opcode = ntohs(arp->opcode);

            serial_printf("NET: ARP packet opcode=%d\n", opcode);

            // Check if this ARP is for us
            uint32_t target_ip = (arp->target_ip[0] << 24) |
                                 (arp->target_ip[1] << 16) |
                                 (arp->target_ip[2] << 8) |
                                 arp->target_ip[3];

            if (target_ip == nic.ip_addr)
            {
                uint32_t sender_ip = (arp->sender_ip[0] << 24) |
                                     (arp->sender_ip[1] << 16) |
                                     (arp->sender_ip[2] << 8) |
                                     arp->sender_ip[3];

                // Always cache the sender's MAC
                arp_cache_update(sender_ip, arp->sender_mac);

                if (opcode == ARP_REQUEST)
                {
                    // Send ARP reply
                    serial_printf("NET: Sending ARP reply to %d.%d.%d.%d\n",
                                  arp->sender_ip[0], arp->sender_ip[1],
                                  arp->sender_ip[2], arp->sender_ip[3]);

                    // Swap addresses and send reply
                    struct arp_packet reply = *arp;
                    reply.opcode = htons(ARP_REPLY);
                    memcpy(reply.target_mac, arp->sender_mac, 6);
                    memcpy(reply.target_ip, arp->sender_ip, 4);
                    memcpy(reply.sender_mac, nic.mac, 6);
                    memcpy(reply.sender_ip, arp->target_ip, 4);

                    eth_send_frame(arp->sender_mac, ETHERTYPE_ARP, (uint8_t *)&reply, sizeof(reply));
                }
            }
            retry_pending_packets();
        }
    }

    else if (ethertype == ETHERTYPE_IP)
    {
        // Handle IPv4 packets
        if (len < sizeof(struct eth_header) + sizeof(ipv4_header_t))
        {
            serial_printf("NET: IP packet too short\n");
            return;
        }

        ipv4_header_t *ip = (ipv4_header_t *)(data + sizeof(struct eth_header));
        uint16_t checksum = ip->checksum;
        ip->checksum = 0;

        if (checksum != ip_checksum(ip, sizeof(ipv4_header_t)))
        {
            serial_printf("NET: IPv4 checksum error (got 0x%04x)\n", checksum);
            return;
        }

        serial_printf("NET: IPv4 packet from %d.%d.%d.%d to %d.%d.%d.%d (proto=%d)\n",
                      (ip->src_ip >> 24) & 0xFF, (ip->src_ip >> 16) & 0xFF,
                      (ip->src_ip >> 8) & 0xFF, ip->src_ip & 0xFF,
                      (ip->dst_ip >> 24) & 0xFF, (ip->dst_ip >> 16) & 0xFF,
                      (ip->dst_ip >> 8) & 0xFF, ip->dst_ip & 0xFF,
                      ip->protocol);

        // Handle ICMP packets
        if (ip->protocol == IP_PROTO_ICMP)
        {
            uint8_t *icmp_data = data + sizeof(struct eth_header) + sizeof(ipv4_header_t);
            uint16_t icmp_len = ntohs(ip->total_length) - sizeof(ipv4_header_t);
            icmp_handle_packet(ip, icmp_data, icmp_len);
        }
    }
    else
    {
        // serial_printf("NET: Unknown ethertype 0x%04x\n", ntohs(eth->ethertype));
    }
}
// Debug dump first few bytes
// serial_printf("NET: Packet dump: ");
// for (int i = 0; i < 16 && i < len; i++)
// {
//     serial_printf("%02x ", data[i]);
// }
// serial_printf("\n");

// serial_printf("NET: Received packet from %02x:%02x:%02x:%02x:%02x:%02x (type 0x%04x)\n",
//               eth->src_mac[0], eth->src_mac[1], eth->src_mac[2],
//               eth->src_mac[3], eth->src_mac[4], eth->src_mac[5],
//               ntohs(eth->ethertype));

