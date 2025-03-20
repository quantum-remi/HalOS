#ifndef ARP_H
#define ARP_H

#include <stdint.h>
#include <stdbool.h>
#include "rtl8139.h"

#pragma pack(push, 1) // Disable struct padding

#define ARP_REQUEST 1
#define ARP_REPLY   2
#define ARP_CACHE_SIZE 32
#define ARP_CACHE_TIMEOUT 30000 
#define MAX_PENDING_PACKETS 5

// Ethernet header
struct eth_header {
    uint8_t dest_mac[6];
    uint8_t src_mac[6];
    uint16_t ethertype;
};

// ARP payload
struct arp_packet {
    uint16_t hw_type;
    uint16_t proto_type;
    uint8_t hw_size;
    uint8_t proto_size;
    uint16_t opcode;
    uint8_t sender_mac[6];
    uint8_t sender_ip[4];
    uint8_t target_mac[6];
    uint8_t target_ip[4];
};

struct arp_cache_entry {
    uint32_t ip;
    uint8_t mac[6];
    uint32_t timestamp;
};

struct pending_packet {
    uint32_t dst_ip;
    uint8_t protocol;
    uint8_t *payload;
    uint16_t payload_len;
    uint32_t timestamp;
};
#pragma pack(pop)

extern struct arp_cache_entry arp_cache[ARP_CACHE_SIZE];
extern struct pending_packet pending_queue[MAX_PENDING_PACKETS];
extern int pending_count;

bool arp_lookup(uint32_t ip, uint8_t* mac);
void arp_cache_update(uint32_t ip, uint8_t* mac);
void rtl8139_send_arp_request(uint32_t *src_ip, uint32_t *target_ip);
void queue_packet(uint32_t dst_ip, uint8_t protocol, uint8_t* payload, uint16_t payload_len);
void retry_pending_packets();

#endif // ARP_H