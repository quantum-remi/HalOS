#include "network.h"

#include "serial.h"
#include "rtl8139.h"
#include "ipv4.h"
#include "arp.h"
#include "icmp.h"

void net_process_packet(uint8_t *data, uint16_t len)
{
    if (!data || len < sizeof(struct eth_header)) {
        return;
    }
    serial_printf("NET: Processing packet of length %d\n", len);

    struct eth_header *eth = (struct eth_header *)data;
    uint16_t ethertype = ntohs(eth->ethertype);

    // Only process IP and ARP packets
    // if (ethertype != ETHERTYPE_IP && ethertype != ETHERTYPE_ARP) {
    //     return;
    // }

    // Log first 16 bytes for debug
    serial_printf("NET: Packet type=0x%04x len=%d data: ", ethertype, len);
    for (int i = 0; i < 16 && i < len; i++) {
        serial_printf("%02x ", data[i]);
    }
    serial_printf("\n");

    switch (ethertype)
    {
    case ETHERTYPE_ARP:
        struct arp_packet *arp = (struct arp_packet *)(data + sizeof(struct eth_header));
        uint16_t opcode = ntohs(arp->opcode);

        // Handle ARP replies
        if (opcode == ARP_REPLY)
        {
            // Extract sender IP and MAC from the ARP reply
            uint32_t sender_ip = ntohl(*(uint32_t *)arp->sender_ip);

            arp_cache_update(sender_ip, arp->sender_mac);
            retry_pending_packets(); // Retry queued packets now that MAC is resolved
        }
        // Handle ARP requests
        else if (opcode == ARP_REQUEST)
        {
            uint32_t target_ip = (arp->target_ip[0] << 24) |
                                 (arp->target_ip[1] << 16) |
                                 (arp->target_ip[2] << 8) |
                                 arp->target_ip[3];
            target_ip = ntohl(target_ip); // Convert to host byte order

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
        break;

    case ETHERTYPE_IP:
    {
        if (len < sizeof(struct eth_header) + sizeof(ipv4_header_t)) {
            serial_printf("NET: IP packet too short\n");
            break;
        }

        ipv4_header_t *ip = (ipv4_header_t *)(data + sizeof(struct eth_header));
        uint8_t version = (ip->version_ihl >> 4) & 0xF;
        uint8_t ihl = ip->version_ihl & 0xF;
        uint16_t total_length = ntohs(ip->total_length);

        // Validate IP header
        if (version != 4 || ihl < 5 || total_length < (ihl * 4)) {
            serial_printf("NET: Invalid IP header\n");
            break;
        }

        // Verify packet fits in received frame
        if (len < sizeof(struct eth_header) + total_length) {
            serial_printf("NET: IP packet truncated (len %d < %d)\n",
                        len, sizeof(struct eth_header) + total_length);
            break;
        }

        // Reject fragmented packets for ICMP
        if ((ntohs(ip->frag_offset) & 0x1FFF) || (ntohs(ip->frag_offset) & 0x2000)) {
            serial_printf("NET: Fragmented packet, ignoring\n");
            break;
        }

        // Check if packet is for us
        uint32_t dst_ip = ntohl(ip->dst_ip);
        if (dst_ip != nic.ip_addr && dst_ip != 0xFFFFFFFF) {
            serial_printf("NET: IP packet not for us (dst=%d.%d.%d.%d)\n",
                         (dst_ip >> 24) & 0xFF, (dst_ip >> 16) & 0xFF,
                         (dst_ip >> 8) & 0xFF, dst_ip & 0xFF);
            break;
        }

        // Process ICMP
        if (ip->protocol == IP_PROTO_ICMP) {
            uint16_t header_len = ihl * 4;
            uint16_t icmp_len = total_length - header_len;
            
            if (icmp_len < sizeof(icmp_header_t)) {
                serial_printf("NET: ICMP packet too small (%d bytes)\n", icmp_len);
                break;
            }

            uint8_t *icmp_data = (uint8_t *)ip + header_len;
            serial_printf("NET: Processing ICMP packet len=%d\n", icmp_len);
            icmp_handle_packet(ip, icmp_data, icmp_len);
        }
        break;
    }

    default:
        break;
    }
}