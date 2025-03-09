#include "ne2k.h"
#include "serial.h"
// #include "pci.h" // Ensure pci.h is included for pci_get_device and pci_dev_t

static struct ne2k_dev dev;

// Static 64KB buffer (aligned to 4KB for QEMU)
#define PCI_CLASS_NETWORK     0x02
#define PCI_SUBCLASS_ETHERNET 0x00
#define NE2K_BUF_PAGES 16
static uint8_t nic_buffer[NE2K_BUF_PAGES * 256] __attribute__((aligned(4096)));

void ne2k_init() {
    serial_printf("Initializing NE2000...\n");

    pci_dev_t pci_dev = pci_get_device(NE2K_VENDOR, NE2K_DEVICE, PCI_CLASS_NETWORK << 8 | PCI_SUBCLASS_ETHERNET);
    if (pci_dev.bits == 0) {
        serial_printf("NE2000 not found\n");
        return;
    }
    serial_printf("NE2000 found. Configuring...\n");

    // Enable I/O and Bus Mastering
    pci_write(pci_dev, PCI_COMMAND, 0x05);

    // Get I/O base
    uint32_t bar0 = pci_read(pci_dev, PCI_BAR0);
    dev.io_base = bar0 & 0xFFFC; // Align to 4-byte boundary
    serial_printf("I/O base: 0x%x (BAR0=0x%x)\n", dev.io_base, bar0);

    // QEMU Workaround: Reset sequence
    outportb(dev.io_base + NE2K_CMD, NE2K_CMD_STOP);
    for (volatile int i = 0; i < 1000; i++); // Delay
    outportb(dev.io_base + NE2K_CMD, NE2K_CMD_START);

    // Set up receive buffer
    outportb(dev.io_base + NE2K_PSTART, 0x40);
    outportb(dev.io_base + NE2K_PSTOP, 0x80);
    outportb(dev.io_base + NE2K_BNRY, 0x40);
    serial_printf("RX buffer configured\n");

    // Configure NIC
    outportb(dev.io_base + NE2K_RCR, RCR_MON); // Promiscuous mode
    outportb(dev.io_base + NE2K_TCR, 0x00);    // Normal TX
    outportb(dev.io_base + NE2K_DCR, 0x49);    // 16-bit DMA, FIFO=8

    // Read MAC address via Remote DMA (correct method)
    outportb(dev.io_base + NE2K_CMD, NE2K_CMD_STOP | 0x00); // Page 0, STOP mode

    // Set Remote DMA start address to 0x0000 (PROM location)
    outportb(dev.io_base + NE2K_RSAR0, 0x00);
    outportb(dev.io_base + NE2K_RSAR1, 0x00);
    outportb(dev.io_base + NE2K_RBCR0, 6); // Read 6 bytes (MAC length)
    outportb(dev.io_base + NE2K_RBCR1, 0x00);

    // Start DMA read
    outportb(dev.io_base + NE2K_CMD, NE2K_CMD_STOP | 0x00 | 0x08); // RDMA read

    // Read MAC from data port (0x10)
    for (int i = 0; i < 6; i++) {
        dev.mac[i] = inportb(dev.io_base + 0x10); // Read from data port
    }

    serial_printf("MAC: %02x:%02x:%02x:%02x:%02x:%02x\n",
                  dev.mac[0], dev.mac[1], dev.mac[2], dev.mac[3], dev.mac[4], dev.mac[5]);
    // Enable NIC
    outportb(dev.io_base + NE2K_IMR, 0x1F); // Enable interrupts
    outportb(dev.io_base + NE2K_CMD, NE2K_CMD_START);
    serial_printf("NE2000 initialized\n");
}

int ne2k_send_packet(uint8_t* data, uint16_t len) {
    // Wait for TX buffer
    if (inportb(dev.io_base + NE2K_CMD) & 0x04) { // Check TX busy
        return -1;
    }

    // Set up DMA transfer
    outportb(dev.io_base + NE2K_RSAR0, 0x00);     // Copy to NIC's TX buffer (page 0)
    outportb(dev.io_base + NE2K_RSAR1, 0x00);
    outportb(dev.io_base + NE2K_RBCR0, len & 0xFF);
    outportb(dev.io_base + NE2K_RBCR1, (len >> 8) & 0xFF);

    // Write data via DMA
    for (uint16_t i = 0; i < len; i++) {
        outportb(dev.io_base + 0x10, data[i]);
    }

    // Start transmission
    outportb(dev.io_base + NE2K_TPSR, 0x00);      // TX starts at page 0
    outportb(dev.io_base + NE2K_TBCR0, len & 0xFF);
    outportb(dev.io_base + NE2K_TBCR1, (len >> 8) & 0xFF);
    outportb(dev.io_base + NE2K_CMD, 0x22);       // Start TX

    return 0;
}

void ne2k_handle_interrupt() {
    uint8_t isr = inportb(dev.io_base + NE2K_ISR);

    if (isr & ISR_PRX) {  // Packet received
        uint8_t curr = inportb(dev.io_base + NE2K_BNRY);
        // Read packet from buffer (simplified)
        // ... (process data from nic_buffer[curr * 256])
        outportb(dev.io_base + NE2K_BNRY, curr + 1); // Advance boundary
        outportb(dev.io_base + NE2K_ISR, ISR_PRX);   // Clear interrupt
    }

    // Clear other interrupts
    outportb(dev.io_base + NE2K_ISR, isr);
}