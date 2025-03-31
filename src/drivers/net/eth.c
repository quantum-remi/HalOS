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
    
    nic.ip_addr = (10 << 24) | (0 << 16) | (2 << 8) | 15;
    uint32_t gateway_ip = (10 << 24) | (0 << 16) | (2 << 8) | 2;

    serial_printf("ETH: Sending ARP request to gateway\n");
    rtl8139_send_arp_request(&nic.ip_addr, &gateway_ip);

    icmp_send_echo_request(gateway_ip);
    serial_printf("ETH: Sent ICMP echo request to %d.%d.%d.%d\n",
                  (gateway_ip >> 24) & 0xFF, (gateway_ip >> 16) & 0xFF,
                  (gateway_ip >> 8) & 0xFF, gateway_ip & 0xFF);
}