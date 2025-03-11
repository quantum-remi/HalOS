#include "rtl8139.h"
#include "serial.h"
#include "arp.h"
#include "network.h"
#include "kernel.h"

#define TX_TIMEOUT_MS 2000

struct rtl8139_dev nic = {0};

static void read_mac_address() {
    uint32_t mac_low = inportl(nic.iobase + REG_MAC0);
    uint16_t mac_high = inportw(nic.iobase + REG_MAC0 + 4);
    memcpy(nic.mac, &mac_low, 4);
    memcpy(nic.mac + 4, &mac_high, 2);
}

void rtl8139_init() {
    pci_dev_t dev = pci_get_device(RTL8139_VENDOR_ID, RTL8139_DEVICE_ID, -1);
    if (dev.bits == dev_zero.bits) {
        serial_printf("RTL8139: Device not found\n");
        return;
    }

    // Enable bus mastering and I/O space
    uint32_t pci_cmd = pci_read(dev, PCI_COMMAND);
    pci_cmd |= (1 << 0) | (1 << 2);  // I/O space + Bus mastering
    pci_write(dev, PCI_COMMAND, pci_cmd);

    // Get base I/O address
    nic.iobase = pci_read(dev, PCI_BAR0) & 0xFFFC;
    nic.irq = pci_read(dev, PCI_INTERRUPT_LINE) & 0xFF;

    nic.rx_buffer = vmm_alloc_contiguous(RX_BUFFER_SIZE / PAGE_SIZE);
    nic.rx_phys = virt_to_phys(nic.rx_buffer);
    nic.tx_buffer = vmm_alloc_contiguous(NUM_TX_BUFFERS * TX_BUFFER_SIZE / PAGE_SIZE);
    nic.tx_phys = virt_to_phys(nic.tx_buffer);

    // Soft reset
    outportb(nic.iobase + REG_CONFIG1, 0x10);
    while (inportb(nic.iobase + REG_CONFIG1) & 0x10);

    // Configure registers
    outportl(nic.iobase + REG_RXBUF, nic.rx_phys);
    outportl(nic.iobase + REG_RCR, 0xF | (1 << 7));  // AAP + 8K buffer
    outportl(nic.iobase + 0x40, 0x03000700 | (0x7 << 8));  // TCR

    // Program TX buffers
    for(int i = 0; i < NUM_TX_BUFFERS; i++) {
        uint32_t tx_phys = nic.tx_phys + (i * TX_BUFFER_SIZE);
        outportl(nic.iobase + REG_TXADDR0 + (i * 4), tx_phys);
        outportl(nic.iobase + REG_TXSTATUS0 + (i * 4), 0);
    }

    // Enable interrupts
    outportw(nic.iobase + REG_IMR, 0x0005); // ROK + TOK
    outportb(nic.iobase + REG_CMD, 0x0C);    // TE + RE

    // Read MAC address
    read_mac_address();
    serial_printf("RTL8139: MAC %02x:%02x:%02x:%02x:%02x:%02x\n",
                 nic.mac[0], nic.mac[1], nic.mac[2],
                 nic.mac[3], nic.mac[4], nic.mac[5]);

    // Register IRQ handler
    isr_register_interrupt_handler(nic.irq + 32, rtl8139_irq_handler);
    pic8259_unmask(nic.irq);
}

void rtl8139_send_packet(uint8_t* data, uint16_t len) {
    serial_printf("RTL8139: Sending packet of length %d\n", len);

    if(len > TX_BUFFER_SIZE) {
        serial_printf("RTL8139: Packet too large (%d bytes)\n", len);
        return;
    }

    uint8_t tx_idx = nic.tx_current;
    uint8_t* tx_buf = nic.tx_buffer + (tx_idx * TX_BUFFER_SIZE);

    // Wait for buffer to be free
    uint32_t timeout = TX_TIMEOUT_MS;
    while(inportl(nic.iobase + REG_TXSTATUS0 + (tx_idx * 4)) & 0x2000) {
        if(--timeout == 0) {
            serial_printf("RTL8139: TX timeout\n");
            panic("RTL8139: TX timeout");
            return;
        }
        usleep(1000);
    }

    // Copy data and start transmission
    memcpy(tx_buf, data, len);
    outportl(nic.iobase + REG_TXSTATUS0 + (tx_idx * 4), len | 0x2000);
    nic.tx_current = (tx_idx + 1) % NUM_TX_BUFFERS;

    serial_printf("RTL8139: Packet sent, tx_idx=%d\n", tx_idx);
}

void rtl8139_irq_handler(REGISTERS *reg) {
    uint16_t status = inportw(nic.iobase + REG_ISR);
    outportw(nic.iobase + REG_ISR, status); // Acknowledge interrupts

    if(status & 0x01) { // Receive interrupt
        uint16_t *rx_header;
        uint16_t packet_len;
        
        while((nic.rx_ptr = inportw(nic.iobase + REG_CAPR)) != 0) {
            rx_header = (uint16_t*)(nic.rx_buffer + nic.rx_ptr);
            packet_len = rx_header[1];
            
            if(packet_len > 4) {
                uint8_t* packet = (uint8_t*)(rx_header + 2);
                net_process_packet(packet, packet_len - 4);
            }
            
            nic.rx_ptr = (nic.rx_ptr + packet_len + 4 + 3) & ~3;
            if(nic.rx_ptr >= RX_BUFFER_SIZE)
                nic.rx_ptr -= RX_BUFFER_SIZE;
            
            outportw(nic.iobase + REG_CAPR, nic.rx_ptr - 0x10);
        }
    }

    if(status & 0x04) { // Transmit OK
        // Just clear status, buffers are reused
    }

    if(status & 0x08) { // Receive error
        serial_printf("RTL8139: Receive error\n");
        // Reset receiver
        outportb(nic.iobase + REG_CMD, inportb(nic.iobase + REG_CMD) | 0x08);
    }
}