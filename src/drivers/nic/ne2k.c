#include "ne2k.h"
#include "io.h"
#include "serial.h"

#define NE2K_RX_START 0x46
#define NE2K_RX_END   0x60
#define NE2K_TX_START 0x40

bool ne2k_probe(pci_dev_t dev) {
    uint32_t vendor = pci_read(dev, PCI_VENDOR_ID);
    uint32_t device = pci_read(dev, PCI_DEVICE_ID);
    
    serial_printf("Probing device: vendor=0x%x, device=0x%x\n", vendor, device);
    
    bool result = (vendor == NE2K_VENDOR_ID) && (device == NE2K_DEVICE_ID);
    
    serial_printf("Probe result: %s\n", result ? "success" : "failure");
    
    return result;
}

bool ne2k_init(pci_dev_t pci_dev) {
    if(!ne2k_probe(pci_dev)) return false;

    ne2k_device dev;
    dev.pci_dev = pci_dev;
    
    // Enable I/O and Bus Mastering
    pci_write(pci_dev, PCI_COMMAND, (1 << 0) | (1 << 2));

    // Get I/O base
    uint32_t bar0 = pci_read(pci_dev, PCI_BAR0);
    dev.iobase = bar0 & 0xFFFC;
    
    // Reset sequence
    outportb(dev.iobase + NE2K_CR, 0x01);  // STOP
    for(volatile int i = 0; i < 10000; i++); // Delay
    outportb(dev.iobase + NE2K_CR, 0x02);  // START

    // Configure registers
    outportb(dev.iobase + NE2K_DCR, 0x49); 
    outportb(dev.iobase + NE2K_RCR, 0x04);  // Accept broadcast
    outportb(dev.iobase + NE2K_TCR, 0x00);  // Normal TX
    
    // Set up receive buffer
    outportb(dev.iobase + NE2K_PSTART, NE2K_RX_START);
    outportb(dev.iobase + NE2K_PSTOP, NE2K_RX_END);
    outportb(dev.iobase + NE2K_BNRY, NE2K_RX_START);

    // Read MAC address
    outportb(dev.iobase + NE2K_RSAR0, 0x00);
    outportb(dev.iobase + NE2K_RSAR1, 0x00);
    outportb(dev.iobase + NE2K_RBCR0, 6);
    outportb(dev.iobase + NE2K_RBCR1, 0);
    outportb(dev.iobase + NE2K_CR, 0x0A);  // Start DMA read
    
    for(int i = 0; i < 6; i++) {
        dev.mac[i] = inportb(dev.iobase + NE2K_DATA);
    }

    serial_printf("NE2000 initialized at 0x%x\n", dev.iobase);
    serial_printf("MAC: %02x:%02x:%02x:%02x:%02x:%02x\n",
                 dev.mac[0], dev.mac[1], dev.mac[2],
                 dev.mac[3], dev.mac[4], dev.mac[5]);
    
    return true;
}

void ne2k_send(ne2k_device *dev, void *data, uint16_t len) {
    // 1. Stop NIC
    outportb(dev->iobase + NE2K_CR, 0x01); // STOP command
    
    // 2. Setup DMA
    outportb(dev->iobase + NE2K_RSAR0, 0x00);
    outportb(dev->iobase + NE2K_RSAR1, 0x00);
    outportb(dev->iobase + NE2K_RBCR0, len & 0xFF);
    outportb(dev->iobase + NE2K_RBCR1, (len >> 8) & 0xFF);
    
    // 3. Start DMA write
    outportb(dev->iobase + NE2K_CR, 0x12); // STRT + RDMA_WRITE
    
    // 4. Write data
    uint8_t *p = data;
    for(uint16_t i = 0; i < len; i++) {
        outportb(dev->iobase + NE2K_DATA, p[i]);
    }
    
    // 5. Wait for DMA completion
    while((inportb(dev->iobase + NE2K_ISR) & 0x40) == 0);
    outportb(dev->iobase + NE2K_ISR, 0x40); // Clear DMA complete flag

    // 6. Configure transmission
    outportb(dev->iobase + NE2K_TPSR, NE2K_TX_START);
    outportb(dev->iobase + NE2K_TBCR0, len & 0xFF);
    outportb(dev->iobase + NE2K_TBCR1, (len >> 8) & 0xFF);
    
    // 7. Start transmission
    outportb(dev->iobase + NE2K_CR, 0x06); // STRT + TXP
    
    // 8. Wait for TX completion
    while((inportb(dev->iobase + NE2K_ISR) & 0x40) == 0);
    outportb(dev->iobase + NE2K_ISR, 0x40); // Clear TX complete flag
}

void test_ne2k(ne2k_device *nic) {
    /* Ethernet + ARP Packet (Broadcast) */
    uint8_t arp_packet[] = {
        // Ethernet Header (14 bytes)
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // Destination MAC (Broadcast)
        nic->mac[0], nic->mac[1], nic->mac[2], // Source MAC
        nic->mac[3], nic->mac[4], nic->mac[5], // (from NIC)
        0x08, 0x06,                         // EtherType = ARP

        // ARP Payload (28 bytes)
        0x00, 0x01,                         // Hardware Type (Ethernet)
        0x08, 0x00,                         // Protocol Type (IPv4)
        0x06, 0x04,                         // MAC/IP Lengths
        0x00, 0x01,                         // Operation (Request)
        nic->mac[0], nic->mac[1], nic->mac[2], // Sender MAC
        nic->mac[3], nic->mac[4], nic->mac[5], 
        0xC0, 0xA8, 0x01, 0x02,             // Sender IP (192.168.1.2)
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // Target MAC
        0xC0, 0xA8, 0x01, 0x01              // Target IP (192.168.1.1)
    };


    
    ne2k_send(nic, arp_packet, sizeof(arp_packet));
    uint8_t isr = inportb(nic->iobase + NE2K_ISR);
    // After transmission
    uint8_t tsr = inportb(nic->iobase + NE2K_TSR);
    serial_printf("TX Status: %s%s%s\n",
                (tsr & 0x01) ? "Collision " : "",
                (tsr & 0x20) ? "FIFO Underrun " : "",
                (tsr & 0x80) ? "OK" : "Failed");
    serial_printf("ISR: 0x%x\n", isr); // Should show 0x40 after TX
    serial_printf("ARP request sent!\n");
}