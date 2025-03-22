#include <rtl8139.h>
#include <arp.h>
#include "liballoc.h"
#include "network.h"
#include "serial.h"
#include "ipv4.h"

struct arp_cache_entry arp_cache[ARP_CACHE_SIZE];
// ARP constants
#define ETHERTYPE_ARP 0x0806
#define ETHERTYPE_IP 0x0800

struct pending_packet pending_queue[MAX_PENDING_PACKETS];
int pending_count = 0;

void create_arp_packet(uint8_t *buffer, uint8_t *src_mac, uint32_t *src_ip, uint32_t *target_ip)
{
    // Convert IPs to network byte order
    uint32_t src_ip_net = htonl(*src_ip);
    uint32_t target_ip_net = htonl(*target_ip);

    // Ethernet header (14 bytes)
    memset(buffer, 0xFF, 6);           // Broadcast destination MAC
    memcpy(buffer + 6, src_mac, 6);    // Source MAC
    buffer[12] = ETHERTYPE_ARP >> 8;   // EtherType high byte
    buffer[13] = ETHERTYPE_ARP & 0xFF; // EtherType low byte

    // ARP payload (28 bytes)
    buffer[14] = 0x00; // Hardware type (Ethernet)
    buffer[15] = 0x01;
    buffer[16] = ETHERTYPE_IP >> 8; // Protocol type (IP)
    buffer[17] = ETHERTYPE_IP & 0xFF;
    buffer[18] = 6;                         // Hardware size
    buffer[19] = 4;                         // Protocol size
    buffer[20] = 0x00;                      // Opcode high byte (request)
    buffer[21] = 0x01;                      // Opcode low byte
    memcpy(buffer + 22, src_mac, 6);        // Sender MAC
    memcpy(buffer + 28, &src_ip_net, 4);    // Sender IP in network byte order
    memset(buffer + 32, 0x00, 6);           // Target MAC (unknown)
    memcpy(buffer + 38, &target_ip_net, 4); // Target IP in network byte order
}

void rtl8139_send_arp_request(uint32_t *src_ip, uint32_t *target_ip)
{
    if (!src_ip || !target_ip)
    {
        serial_printf("ARP: Invalid parameters for request\n");
        return;
    }

    uint8_t arp_packet[60] = {0}; // Zero-initialize to 60 bytes
    create_arp_packet(arp_packet, nic.mac, src_ip, target_ip);
    rtl8139_send_packet(arp_packet, 60); // Send 60 bytes (NIC pads to 64)
}

bool arp_lookup(uint32_t ip, uint8_t *mac)
{
    if (!mac)
    {
        serial_printf("ARP: Invalid MAC output buffer\n");
        return false;
    }

    for (int i = 0; i < ARP_CACHE_SIZE; i++)
    {
        if (arp_cache[i].ip == ip &&
            (get_ticks() - arp_cache[i].timestamp) < ARP_CACHE_TIMEOUT)
        {
            memcpy(mac, arp_cache[i].mac, 6);
            serial_printf("ARP: Cache hit for IP %d.%d.%d.%d\n",
                          (ip >> 24) & 0xFF, (ip >> 16) & 0xFF,
                          (ip >> 8) & 0xFF, ip & 0xFF);
            return true;
        }
    }

    serial_printf("ARP: Cache miss for IP %d.%d.%d.%d\n",
                  (ip >> 24) & 0xFF, (ip >> 16) & 0xFF,
                  (ip >> 8) & 0xFF, ip & 0xFF);
    return false;
}

void arp_cache_update(uint32_t ip, uint8_t *mac)
{
    if (!mac)
    {
        serial_printf("ARP: Invalid MAC for cache update\n");
        return;
    }

    serial_printf("ARP: Updating cache for IP %d.%d.%d.%d -> %02x:%02x:%02x:%02x:%02x:%02x\n",
                  (ip >> 24) & 0xFF, (ip >> 16) & 0xFF,
                  (ip >> 8) & 0xFF, ip & 0xFF, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    // Find existing or empty slot
    for (int i = 0; i < ARP_CACHE_SIZE; i++)
    {
        if (arp_cache[i].ip == ip || arp_cache[i].ip == 0)
        {
            arp_cache[i].ip = ip;
            memcpy(arp_cache[i].mac, mac, 6);
            arp_cache[i].timestamp = get_ticks();
            return;
        }
    }
}

void queue_packet(uint32_t dst_ip, uint8_t protocol, uint8_t *payload, uint16_t payload_len)
{
    if (pending_count >= MAX_PENDING_PACKETS)
    {
        serial_printf("ARP: Packet queue full\n");
        return;
    }
    struct pending_packet *pkt = &pending_queue[pending_count];
    pkt->dst_ip = dst_ip;
    pkt->protocol = protocol;
    pkt->payload = malloc(payload_len);
    memcpy(pkt->payload, payload, payload_len);
    pkt->payload_len = payload_len;
    pkt->timestamp = get_ticks();
    pending_count++;
}

void retry_pending_packets()
{
    for (int i = 0; i < pending_count; i++)
    {
        struct pending_packet *pkt = &pending_queue[i];
        uint8_t dst_mac[6];
        if (arp_lookup(pkt->dst_ip, dst_mac))
        {
            // Rebuild the IPv4 packet with current source IP
            net_send_ipv4_packet(pkt->dst_ip, pkt->protocol, pkt->payload, pkt->payload_len);
            free(pkt->payload);
            pkt->payload = NULL;
            if (i != pending_count - 1)
            {
                struct pending_packet temp = pending_queue[i];
                pending_queue[i] = pending_queue[pending_count - 1];
                pending_queue[pending_count - 1] = temp;
            }
            pending_count--;
            i--;
        }
    }
}