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
        serial_printf("ETH: Invalid parameters\n");
        return;
    }

    // Calculate total size including padding
    uint16_t total_len = sizeof(struct eth_header) + len;
    if (total_len < 60) total_len = 60;

    // Allocate frame buffer on stack
    uint8_t frame[total_len];
    memset(frame, 0, total_len);

    // Build Ethernet header
    struct eth_header *eth = (struct eth_header *)frame;
    memcpy(eth->dest_mac, dest_mac, 6);
    memcpy(eth->src_mac, nic.mac, 6);
    eth->ethertype = htons(ethertype);

    // Copy payload
    memcpy(frame + sizeof(struct eth_header), data, len);

    serial_printf("ETH: Sending frame to %02x:%02x:%02x:%02x:%02x:%02x type=0x%04x len=%d\n",
                 dest_mac[0], dest_mac[1], dest_mac[2],
                 dest_mac[3], dest_mac[4], dest_mac[5],
                 ethertype, total_len);

    rtl8139_send_packet(frame, total_len);
}


void eth_init()
{
    rtl8139_init();
    __asm__ volatile("sti");
    
    // Configure networking
    nic.ip_addr = (10 << 24) | (0 << 16) | (2 << 8) | 15;     // 10.0.2.15
    nic.netmask = (255 << 24) | (255 << 16) | (255 << 8) | 0; // 255.255.255.0
    nic.gateway_ip = (10 << 24) | (0 << 16) | (2 << 8) | 2;   // 10.0.2.2
    uint32_t broadcast_ip = (255 << 24) | (255 << 16) | (255 << 8) | 255;

    // First resolve gateway MAC
    // rtl8139_send_arp_request(&nic.ip_addr, &nic.gateway_ip);
    
    // Wait for ARP reply before sending ICMP
    // for(volatile int i = 0; i < 2000000; i++) {
    //     uint8_t mac[6];
    //     if (arp_lookup(nic.gateway_ip, mac)) {
    //         serial_printf("ETH: Gateway MAC resolved\n");
    //         break;
    //     }
    // }
}