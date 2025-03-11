#ifndef RTL8139_H
#define RTL8139_H

#include <stdint.h>
#include <stddef.h>
#include "pci.h"
#include "io.h"
#include "serial.h"
#include "liballoc.h"
#include "vmm.h"
#include "isr.h"

#define RTL8139_VENDOR_ID 0x10EC
#define RTL8139_DEVICE_ID 0x8139

struct rtl8139_dev {
    uint32_t base_addr;    // I/O base address
    uint8_t irq;           // Interrupt line
    uint8_t mac[6];        // MAC address
    uint8_t* rx_buffer;    // Receive buffer (virtual address)
    uint32_t rx_phys;      // Receive buffer (physical address)
    uint16_t cur_rx;       // Current RX read position
    uint8_t tx_current;    // Current TX buffer index
    uint8_t* tx_buffers[4];// TX buffer array
    uint32_t tx_phys[4];   // Physical addresses of TX buffers
};

extern struct rtl8139_dev nic;

void rtl8139_init();
void rtl8139_send_packet(uint8_t* data, uint16_t len);

#endif // RTL8139_H