#include "rtl8139.h"

struct rtl8139_dev nic = {0};

void net_process_packet(uint8_t* data, uint16_t len) {
    // Stub implementation
    serial_printf("Packet received (%d bytes)\n", len);
}

void rtl8139_irq_handler(REGISTERS *reg) {
    (void)reg;
    uint16_t status = inportw(nic.base_addr + 0x3E);
    
    // Handle RX
    if (status & 0x01) {
        while (!(inportb(nic.base_addr + 0x37) & 0x01)) { // Check ROK bit
            uint16_t cur_rx = nic.cur_rx;
            uint8_t *rx_buf = nic.rx_buffer + cur_rx;
            uint16_t rx_status = *(uint16_t*)rx_buf;
            uint16_t rx_len = *(uint16_t*)(rx_buf + 2);
            
            // Validate packet
            if ((rx_status & 0x1) && !(rx_status & 0x1E)) {
                uint8_t *packet = rx_buf + 4;
                net_process_packet(packet, rx_len - 4);
            }
            
            // Update RX position
            cur_rx = (cur_rx + rx_len + 4 + 3) & ~3;
            if (cur_rx > 8192) cur_rx -= 8192;
            nic.cur_rx = cur_rx;
            
            // Update CAPR
            outportw(nic.base_addr + 0x38, cur_rx - 16);
        }
    }
    
    // Handle TX complete
    if (status & 0x04) {
        // Clear TX complete status
        outportw(nic.base_addr + 0x3E, 0x04);
    }
    
    // Handle errors
    if (status & 0x0E) {
        serial_printf("RTL8139: Error status: 0x%x\n", status);
        // Reset the controller on severe errors
        if (status & 0x08) {
            rtl8139_init();
            return;
        }
    }
    
    // Acknowledge interrupts
    outportw(nic.base_addr + 0x3E, status);
}

void rtl8139_send_packet(uint8_t* data, uint16_t len) {
    // Select next TX buffer (4 available: 0x20-0x34)
    uint8_t tx_idx = nic.tx_current;
    
    // Ensure packet size is valid
    if (len > 1792) { // Maximum size supported by RTL8139
        serial_printf("RTL8139: Packet too large (%d bytes)\n", len);
        return;
    }

    // Wait until buffer is free (check TOK bit)
    uint32_t timeout = 1000;
    while (!(inportl(nic.base_addr + 0x10) & (1 << (tx_idx + 2)))) {
        if (--timeout == 0) {
            serial_printf("RTL8139: TX timeout\n");
            return;
        }
    }

    // Copy data to TX DMA buffer
    memcpy(nic.tx_buffers[tx_idx], data, len);
    
    // Clear OWN bit and set TX status
    outportl(nic.base_addr + 0x20 + (tx_idx * 4), len);
    
    // Start transmission
    outportl(nic.base_addr + 0x10, (1 << tx_idx));
    
    // Update TX buffer index
    nic.tx_current = (tx_idx + 1) % 4;
}

void rtl8139_init() {
    // Find PCI device
    pci_dev_t dev = pci_get_device(RTL8139_VENDOR_ID, RTL8139_DEVICE_ID, -1);
    if (dev.bits == dev_zero.bits) {
        serial_printf("RTL8139: Device not found\n");
        return;
    }

    // Enable I/O and Bus Mastering
    uint32_t command = pci_read(dev, PCI_COMMAND);
    command |= (1 << 0) | (1 << 2);  // I/O Space + Bus Master
    pci_write(dev, PCI_COMMAND, command);

    // Get I/O base address
    nic.base_addr = pci_read(dev, PCI_BAR0) & 0xFFFC;
    serial_printf("RTL8139: Base I/O port: 0x%x\n", nic.base_addr);

    // Perform software reset
    outportb(nic.base_addr + 0x52, 0x10);
    while ((inportb(nic.base_addr + 0x52) & 0x10) != 0);

    // Allocate RX buffer (8KB + 16 bytes padding)
    nic.rx_buffer = malloc(8192 + 16);
    nic.rx_phys = virt_to_phys((uint32_t)nic.rx_buffer);  // Implement this in mm.h
    
    // Allocate TX buffers
    for (int i = 0; i < 4; i++) {
        nic.tx_buffers[i] = malloc(1792 + 16);
        if (!nic.tx_buffers[i]) {
            serial_printf("RTL8139: Failed to allocate TX buffer %d\n", i);
            return;
        }
        nic.tx_phys[i] = virt_to_phys((uint32_t)nic.tx_buffers[i]);
    }
    nic.cur_rx = 0;
    
    // Configure receive buffer (now using DMA properly)
    outportl(nic.base_addr + 0x30, virt_to_phys((uint32_t)nic.rx_buffer));

    // Set interrupt mask
    outportw(nic.base_addr + 0x3C, 0x0005);  // Enable RX/TX interrupts

    // Configure receive mode
    outportl(nic.base_addr + 0x44, 0x0000F | (1 << 7));  // Accept broadcast + error packets

    // Enable transmitter/receiver
    outportb(nic.base_addr + 0x37, 0x0C);  // TE + RE bits

    // Read MAC address
    for(int i = 0; i < 6; i++) {
        nic.mac[i] = inportb(nic.base_addr + i);
    }
    serial_printf("RTL8139: MAC %02x:%02x:%02x:%02x:%02x:%02x\n",
                 nic.mac[0], nic.mac[1], nic.mac[2],
                 nic.mac[3], nic.mac[4], nic.mac[5]);

    // Enable PCI interrupt
    uint16_t pci_cmd = pci_read(dev, PCI_COMMAND);
    pci_write(dev, PCI_COMMAND, pci_cmd | (1 << 10)); // Interrupt Disable bit OFF
    
    // Configure Receive Configuration Register
    outportl(nic.base_addr + 0x44, 0xF | (1 << 7)); // Accept all packets
    
    // Set Rx Buffer Size (9346 bytes + 16)
    outportb(nic.base_addr + 0xDA, 0x02); // RCR_RBLEN_8K
    // Register IRQ handler
    nic.irq = pci_read(dev, PCI_INTERRUPT_LINE) & 0xFF;
    isr_register_interrupt_handler(nic.irq + 32, rtl8139_irq_handler);
    serial_printf("RTL8139: Initialized on IRQ %d\n", nic.irq);
}