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

// Move these from the header to here, and remove 'static'
void (*eth_send_packet_func)(uint8_t*, uint16_t) = NULL;
void (*eth_get_mac_func)(uint8_t*) = NULL;

void eth_send_frame(uint8_t *dest_mac, uint16_t ethertype, uint8_t *data, uint16_t len)
{
    if (!dest_mac || !data || !eth_send_packet_func) {
        serial_printf("ETH: Invalid parameters or no NIC\n");
        return;
    }

    uint16_t total_len = sizeof(struct eth_header) + len;
    if (total_len < 60) total_len = 60;

    uint8_t frame[1518]; // was static, now local
    memset(frame, 0, sizeof(frame));

    struct eth_header *eth = (struct eth_header *)frame;
    memcpy(eth->dest_mac, dest_mac, 6);

    if (eth_get_mac_func) {
        uint8_t mac[6];
        eth_get_mac_func(mac);
        memcpy(eth->src_mac, mac, 6);
    } else {
        memcpy(eth->src_mac, nic.mac, 6);
    }
    eth->ethertype = htons(ethertype);

    memcpy(frame + sizeof(struct eth_header), data, len);

    eth_send_packet_func(frame, total_len);
}

void eth_init()
{
    __asm__ volatile("sti");
    rtl8139_init();
    extern int rtl8139_present;
    if (rtl8139_present) {
        eth_send_packet_func = rtl8139_send_packet;
        eth_get_mac_func = NULL;
        serial_printf("ETH: Using RTL8139\n");
    } else if (ne2k_init() == 0 && ne2k_is_present()) {
        eth_send_packet_func = ne2k_send_packet;
        eth_get_mac_func = ne2k_get_mac;
        serial_printf("ETH: Using NE2K (RTL8029)\n");
    } else {
        serial_printf("ETH: No supported NIC found\n");
        eth_send_packet_func = NULL;
    }

    if (eth_send_packet_func == ne2k_send_packet) {
        serial_printf("ETH: eth_send_packet_func is set to ne2k_send_packet\n");
    }

    nic.ip_addr = inet_addr("10.0.2.15");
    nic.netmask = inet_addr("255.255.255.0");
    nic.gateway_ip = inet_addr("10.0.2.2");

    rtl8139_send_arp_request(&nic.ip_addr, &nic.gateway_ip);
    tcp_listen(8080);
}