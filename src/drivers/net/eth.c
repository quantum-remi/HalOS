#include "eth.h"

#include "pci.h"
#include "serial.h"
#include "liballoc.h"
#include "rtl8139.h"
#include "ne2k.h"
#include "network.h"
#include "ipv4.h"
#include "icmp.h"
#include "tcp.h"

#include "arp.h"


void eth_send_frame(uint8_t *dest_mac, uint16_t ethertype, uint8_t *data, uint16_t len)
{
    if (!dest_mac || !data) {
        serial_printf("ETH: Invalid parameters\n");
        return;
    }

    uint16_t total_len = sizeof(struct eth_header) + len;
    if (total_len < 60) total_len = 60;

    static uint8_t frame[1518];
    memset(frame, 0, sizeof(frame));

    struct eth_header *eth = (struct eth_header *)frame;
    memcpy(eth->dest_mac, dest_mac, 6);
    memcpy(eth->src_mac, nic.mac, 6);
    eth->ethertype = htons(ethertype);

    memcpy(frame + sizeof(struct eth_header), data, len);

    rtl8139_send_packet(frame, total_len);
}

void eth_init()
{
    __asm__ volatile("sti");
    rtl8139_init();
    // dp83820_init(); // Removed undefined function call

    nic.ip_addr = inet_addr("10.0.2.15");
    nic.netmask = inet_addr("255.255.255.0");
    nic.gateway_ip = inet_addr("10.0.2.2");

    rtl8139_send_arp_request(&nic.ip_addr, &nic.gateway_ip);
    tcp_listen(8080);
}