#ifndef NE2K_H
#define NE2K_H

#include "pci.h"
#include <stdint.h>
#include <stdbool.h>

#define NE2K_VENDOR_ID 0x10EC
#define NE2K_DEVICE_ID 0x8029

/* NE2000 Registers */
enum NE2KRegisters {
    NE2K_CR      = 0x00,  // Command Register
    NE2K_PSTART  = 0x01,  // Page Start Register
    NE2K_PSTOP   = 0x02,  // Page Stop Register
    NE2K_BNRY    = 0x03,  // Boundary Pointer
    NE2K_TSR     = 0x04,  // Transmit Status Register
    NE2K_TPSR    = 0x04,  // Transmit Page Start
    NE2K_TBCR0   = 0x05,  // Transmit Byte Count 0
    NE2K_TBCR1   = 0x06,  // Transmit Byte Count 1
    NE2K_ISR     = 0x07,  // Interrupt Status Register
    NE2K_RSAR0   = 0x08,  // Remote Start Address 0
    NE2K_RSAR1   = 0x09,  // Remote Start Address 1
    NE2K_RBCR0   = 0x0A,  // Remote Byte Count 0
    NE2K_RBCR1   = 0x0B,  // Remote Byte Count 1
    NE2K_RCR     = 0x0C,  // Receive Configuration
    NE2K_TCR     = 0x0D,  // Transmit Configuration
    NE2K_DCR     = 0x0E,  // Data Configuration
    NE2K_IMR     = 0x0F,  // Interrupt Mask
    NE2K_DATA    = 0x10   // Data Port
};

typedef struct {
    pci_dev_t pci_dev;
    uint16_t iobase;
    uint8_t mac[6];
} ne2k_device;

// Driver interface
bool ne2k_probe(pci_dev_t dev);
bool ne2k_init(pci_dev_t dev);
void ne2k_send(ne2k_device *dev, void *data, uint16_t len);
void test_ne2k(ne2k_device *nic);

#endif