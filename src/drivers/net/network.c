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

    struct eth_header *eth = (struct eth_header *)data;
    uint16_t ethertype = ntohs(eth->ethertype);

    // Only process IP and ARP packets
    if (ethertype != ETHERTYPE_IP && ethertype != ETHERTYPE_ARP) {
        return;
    }

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
            break;
        }

        ipv4_header_t *ip = (ipv4_header_t *)(data + sizeof(struct eth_header));
        
        // Extract version and IHL fields correctly
        uint8_t version = (ip->version_ihl >> 4) & 0xF;
        uint8_t ihl = ip->version_ihl & 0xF;

        if (version != 4 || ihl < 5) {
            break;
        }

        uint16_t total_length = ntohs(ip->total_length);

        // Process ICMP
        if (ip->protocol == IP_PROTO_ICMP) {
            uint16_t header_len = ihl * 4;  // IHL is in 4-byte units
            if (total_length >= header_len + sizeof(icmp_header_t)) {
                uint8_t *icmp_data = data + sizeof(struct eth_header) + header_len;
                uint16_t icmp_len = total_length - header_len;
                
                icmp_header_t *icmp = (icmp_header_t *)icmp_data;
                
                icmp_handle_packet(ip, icmp_data, icmp_len);
            }
        }
        break;
    }

    default:
        break;
    }
}