#ifndef NE2K_H
#define NE2K_H

#include <stdint.h>
#include "pci.h"
#include "io.h"

#define NE2K_VENDOR   0x10EC  // Realtek (for QEMU's NE2000)
#define NE2K_DEVICE   0x8029  // QEMU's NE2000 PCI ID

// NE2000 Registers
#define NE2K_CMD      0x00
#define NE2K_PSTART   0x01    // Receive buffer start page
#define NE2K_PSTOP    0x02    // Receive buffer end page
#define NE2K_BNRY     0x03    // Boundary pointer
#define NE2K_TPSR     0x04    // Transmit page start
#define NE2K_TBCR0    0x05    // Transmit byte count (low)
#define NE2K_TBCR1    0x06    // Transmit byte count (high)
#define NE2K_ISR      0x07    // Interrupt status
#define NE2K_RSAR0    0x08    // Remote DMA start (low)
#define NE2K_RSAR1    0x09    // Remote DMA start (high)
#define NE2K_RBCR0    0x0A    // Remote DMA byte count (low)
#define NE2K_RBCR1    0x0B    // Remote DMA byte count (high)
#define NE2K_RCR      0x0C    // Receive configuration
#define NE2K_TCR      0x0D    // Transmit configuration
#define NE2K_DCR      0x0E    // Data configuration
#define NE2K_IMR      0x0F    // Interrupt mask

// Commands
#define NE2K_CMD_STOP  0x01   // Bit 0: Stop NIC
#define NE2K_CMD_START 0x02   // Bit 1: Start NIC
#define NE2K_CMD_RDMA_READ 0x08 // Bit 3: Remote DMA read

// Receive Configuration
#define RCR_MON        0x20   // Monitor mode (accept all packets)

// Interrupt Flags
#define ISR_PRX        0x01   // Packet received

struct ne2k_dev {
    uint16_t io_base;    // I/O port base
    uint8_t  mac[6];     // MAC address
};

void ne2k_init();
int ne2k_send_packet(uint8_t* data, uint16_t len);
void ne2k_handle_interrupt();

#endif