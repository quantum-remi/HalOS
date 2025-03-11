#include "network.h"

void net_process_packet(uint8_t* data, uint16_t len) {
    serial_printf("Received packet (%d bytes):\n", len);
    
    // Print first 64 bytes of the packet
    for (int i = 0; i < (len > 64 ? 64 : len); i++) {
        serial_printf("%02x ", data[i]);
        if ((i+1) % 16 == 0) serial_printf("\n");
    }
    serial_printf("\n");
    
}