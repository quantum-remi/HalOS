#include "eth.h"

#include "pci.h"
#include "serial.h"
#include "liballoc.h"
#include "rtl8139.h"
#include "network.h"
#include "ipv4.h"
#include "icmp.h"

#include "arp.h"

void eth_send_frame(uint8_t *dest_mac, uint16_t ethertype, uint8_t *data, uint16_t len)
{
    if (!dest_mac || !data) {
        serial_printf("ETH: Invalid parameters for frame transmission\n");
        return;
    }
    uint16_t min_frame_size = 60; // 14 (header) + 46 (payload)
    if (len < min_frame_size - 14) { // Adjust payload requirement
        len = min_frame_size - 14;   // Pad payload to 46 bytes
    }

    uint8_t frame[14 + len]; // Ethernet header (14B) + payload
    struct eth_header *eth = (struct eth_header *)frame;

    // Build Ethernet header
    memcpy(eth->dest_mac, dest_mac, 6);
    memcpy(eth->src_mac, nic.mac, 6);
    eth->ethertype = htons(ethertype);

    // Copy payload
    memcpy(frame + 14, data, len);

    serial_printf("ETH: Sending frame to %02x:%02x:%02x:%02x:%02x:%02x (type 0x%04x, len %d)\n",
                 dest_mac[0], dest_mac[1], dest_mac[2], dest_mac[3], dest_mac[4], dest_mac[5],
                 ethertype, len);

    rtl8139_send_packet(frame, sizeof(frame));
}

void eth_init()
{
    rtl8139_init();
    __asm__ volatile("sti"); // Enable interrupts
    
    // Set IP before attempting any network operations
    // nic.ip_addr = 0x0A00020F;   // 10.0.2.15
    
    
    // Wait a bit before sending first packet
    // usleep(100000); // 100ms delay
    
    // uint32_t src_ip[4] = {10, 0, 2, 15};
    // uint32_t target_ip[4] = {10, 0, 2, 2}; // Fix: Target gateway IP
    // rtl8139_send_arp_request(src_ip, target_ip);
    // rtl8139_send_arp_request(src_ip, target_ip);
    // net_send_ipv4_packet(0x0A000202, IP_PROTO_ICMP, (uint8_t *)"Hello, world!", 13);
    // net_send_ipv4_packet(0x0A000202, IP_PROTO_ICMP, (uint8_t *)"Hello", 5);
    //
    //
    icmp_send_echo_request(0x0A000202);
}