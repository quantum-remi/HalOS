#ifndef RTL8139_H
#define RTL8139_H

#include <stdint.h>
#include "pci.h"
#include "vmm.h"
#include "pmm.h"
#include "isr.h"
#include "io.h"
#include "timer.h"
#include "paging.h"
#include "8259_pic.h"

#define RTL8139_VENDOR_ID  0x10EC
#define RTL8139_DEVICE_ID  0x8139
#define ARP_CACHE_TIMEOUT    600   // 6 seconds @ 100Hz tick rate
#define RX_BUFFER_PAGES 4
#define RX_BUFFER_SIZE (RX_BUFFER_PAGES * PAGE_SIZE)
#define NUM_TX_BUFFERS     4
#define TX_BUFFER_SIZE     1792  // Max size per RTL8139 datasheet
#define TX_PACKET_ALIGN    4     // Align packets to 4 bytes

// Register offsets
enum RTL8139Registers {
    REG_MAC0        = 0x00,     // MAC address
    REG_MAR0        = 0x08,     // Multicast filter
    REG_TXSTATUS0   = 0x10,     // Transmit status (4 registers)
    REG_TXADDR0     = 0x20,     // Transmit address (4 registers)
    REG_RXBUF       = 0x30,     // Receive buffer start address
    REG_CMD         = 0x37,     // Command register
    REG_CAPR        = 0x38,     // Current read pointer
    REG_IMR         = 0x3C,     // Interrupt mask
    REG_ISR         = 0x3E,     // Interrupt status
    REG_TCR         = 0x40,     // Transmit configuration
    REG_RCR         = 0x44,     // Receive configuration
    REG_CONFIG1     = 0x52      // Configuration 1
};

struct rtl8139_dev {
    uint16_t iobase;
    uint8_t  irq;
    uint8_t  mac[6];
    uint8_t* rx_buffer;
    uint32_t rx_phys;
    uint16_t rx_ptr;
    uint8_t  tx_current;
    uint8_t* tx_buffer;
    uint32_t tx_phys;
    uint32_t ip_addr;
    uint32_t netmask;
    uint32_t gateway_ip;
};

extern struct rtl8139_dev nic;

void rtl8139_init();
void rtl8139_send_packet(uint8_t* data, uint16_t len);
void rtl8139_irq_handler(REGISTERS *reg);

#endif