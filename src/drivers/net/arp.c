#include <rtl8139.h>
#include <arp.h>

// ARP constants
#define ARP_REQUEST 0x0001
#define ETHERTYPE_ARP 0x0806
#define ETHERTYPE_IP  0x0800

void create_arp_packet(uint8_t* buffer, uint8_t* src_mac, uint8_t* src_ip, uint8_t* target_ip) {
    // Ethernet header (14 bytes)
    memset(buffer, 0xFF, 6);                 // Broadcast destination MAC
    memcpy(buffer + 6, src_mac, 6);           // Source MAC
    buffer[12] = ETHERTYPE_ARP >> 8;          // EtherType high byte
    buffer[13] = ETHERTYPE_ARP & 0xFF;        // EtherType low byte

    // ARP payload (28 bytes)
    buffer[14] = 0x00;                        // Hardware type (Ethernet)
    buffer[15] = 0x01;
    buffer[16] = ETHERTYPE_IP >> 8;           // Protocol type (IP)
    buffer[17] = ETHERTYPE_IP & 0xFF;
    buffer[18] = 6;                           // Hardware size
    buffer[19] = 4;                           // Protocol size
    buffer[20] = ARP_REQUEST >> 8;            // Opcode (request)
    buffer[21] = ARP_REQUEST & 0xFF;
    memcpy(buffer + 22, src_mac, 6);          // Sender MAC
    memcpy(buffer + 28, src_ip, 4);           // Sender IP
    memset(buffer + 32, 0x00, 6);             // Target MAC (unknown)
    memcpy(buffer + 38, target_ip, 4);        // Target IP
}

void rtl8139_send_arp_request(uint8_t* src_ip, uint8_t* target_ip) {
    uint8_t arp_packet[42];  // 14 Ethernet + 28 ARP
    create_arp_packet(arp_packet, nic.mac, src_ip, target_ip);
    rtl8139_send_packet(arp_packet, sizeof(arp_packet));
    
    serial_printf("Sent ARP request for ");
    serial_printf("%d.%d.%d.%d\n", 
                 target_ip[0], target_ip[1],
                 target_ip[2], target_ip[3]);
}