#include "rtl8139.h"
#include "serial.h"
#include "arp.h"
#include "network.h"
#include "kernel.h"
#include "8259_pic.h"

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

    // Enable PCI features
    uint32_t pci_cmd = pci_read(dev, PCI_COMMAND);
    pci_cmd |= (1 << 0) | (1 << 2);  // I/O space + Bus mastering
    pci_write(dev, PCI_COMMAND, pci_cmd);

    // Get base address and IRQ
    nic.iobase = pci_read(dev, PCI_BAR0) & 0xFFFC;
    nic.irq = pci_read(dev, PCI_INTERRUPT_LINE) & 0xFF;

    if (nic.iobase == 0 || nic.irq == 0) {
        serial_printf("RTL8139: Invalid I/O base or IRQ\n");
        return;
    }

    // Allocate DMA buffers
    nic.rx_buffer = vmm_alloc_contiguous(RX_BUFFER_SIZE / PAGE_SIZE);
    if (!nic.rx_buffer) {
        serial_printf("RTL8139: Failed to allocate RX buffer\n");
        return;
    }
    nic.rx_phys = virt_to_phys(nic.rx_buffer);
    
    nic.tx_buffer = vmm_alloc_contiguous(NUM_TX_BUFFERS * TX_BUFFER_SIZE / PAGE_SIZE);
    if (!nic.tx_buffer) {
        serial_printf("RTL8139: Failed to allocate TX buffer\n");
        vmm_free_contiguous(nic.rx_buffer, RX_BUFFER_SIZE / PAGE_SIZE);
        return;
    }
    nic.tx_phys = virt_to_phys(nic.tx_buffer);

    // Hardware reset
    outportb(nic.iobase + REG_CONFIG1, 0x10);
    while (inportb(nic.iobase + REG_CONFIG1) & 0x10);

    // Configure registers
    outportl(nic.iobase + REG_RXBUF, nic.rx_phys);
    outportl(nic.iobase + REG_RCR, 0xF | (1 << 7));  // Receive configuration
    outportl(nic.iobase + REG_TCR, 0x03000700 | (0x7 << 8));  // Transmit configuration

    // Initialize TX descriptors
    for(int i = 0; i < NUM_TX_BUFFERS; i++) {
        uint32_t tx_phys = nic.tx_phys + (i * TX_BUFFER_SIZE);
        outportl(nic.iobase + REG_TXADDR0 + (i * 4), tx_phys);
        outportl(nic.iobase + REG_TXSTATUS0 + (i * 4), 0);
    }

    // Enable interrupts and start chip
    outportw(nic.iobase + REG_IMR, 0x0005);  // ROK + TOK
    outportw(nic.iobase + 0x3C, 0x0005);  // IMR
    outportb(nic.iobase + REG_CMD, 0x0C);     // TE + RE
    outportl(nic.iobase + 0x40, 
        0x03000700 |  // MXDMA unlimited
        (0x3 << 8));  // IFG = 3 (96-bit inter-frame gap)

    // Read and print MAC
    read_mac_address();
    serial_printf("RTL8139: MAC %02x:%02x:%02x:%02x:%02x:%02x\n",
                 nic.mac[0], nic.mac[1], nic.mac[2],
                 nic.mac[3], nic.mac[4], nic.mac[5]);


    serial_printf("TX Buffers:\n");
    for(int i=0; i<4; i++) {
        uint32_t tx_phys = nic.tx_phys + (i * TX_BUFFER_SIZE);
        serial_printf("  TX%d: V=0x%x P=0x%x\n", 
                    i, nic.tx_buffer + (i * TX_BUFFER_SIZE), tx_phys);
    }
    serial_printf("Registering IRQ %d (PCI INT %d)\n", nic.irq + 32, nic.irq);
    isr_register_interrupt_handler(43, rtl8139_irq_handler);  // Register for vector 43
    pic8259_unmask(11);  // Unmask IRQ 11 on PIC
    serial_printf("Slave PIC IMR: 0x%x\n", inportb(PIC2_DATA));
    serial_printf("TX Buffer Virtual: 0x%x → Physical: 0x%x\n",
        nic.tx_buffer, virt_to_phys(nic.tx_buffer));
}

void rtl8139_send_packet(uint8_t* data, uint16_t len) {
    if(len > TX_BUFFER_SIZE) {
        serial_printf("RTL8139: Packet too large (%d bytes)\n", len);
        return;
    }

    // if (len < 64) {
    //     memset(data + len, 0, 64 - len);
    //     len = 64;
    // }
    uint8_t tx_idx = nic.tx_current;
    uint8_t* tx_buf = nic.tx_buffer + (tx_idx * TX_BUFFER_SIZE);

    // Wait for buffer to be free
    uint32_t timeout = 1000;
    while(inportl(nic.iobase + REG_TXSTATUS0 + (tx_idx * 4)) & 0x2000) {
        if(--timeout == 0) {
            serial_printf("TX buffer %d never freed!\n", tx_idx);
            return;
        }
        usleep(1000);
    }

    // Program descriptor and send
    memcpy(tx_buf, data, len);
    outportl(nic.iobase + REG_TXSTATUS0 + (tx_idx * 4), len | 0x2000);
    nic.tx_current = (tx_idx + 1) % NUM_TX_BUFFERS;

    serial_printf("TX Buffer %d Phys: 0x%x\n", 
        tx_idx, nic.tx_phys + (tx_idx * TX_BUFFER_SIZE));

    serial_printf("RTL8139: Sent %d bytes via buffer %d (TSD: 0x%x)\n",
                 len, tx_idx, inportl(nic.iobase + REG_TXSTATUS0 + (tx_idx * 4)));
}

void rtl8139_irq_handler(REGISTERS *reg) {
    uint16_t status = inportw(nic.iobase + 0x3E);
    outportw(nic.iobase + 0x3E, status); // Acknowledge

    serial_printf("RTL8139: ISR 0x%x\n", status);

    if(status & 0x01) { // Receive interrupt
        uint16_t curr_rx = inportw(nic.iobase + REG_CAPR);
        while(nic.rx_ptr != curr_rx) {
            uint8_t *rx_buf = nic.rx_buffer + (nic.rx_ptr % RX_BUFFER_SIZE);
            uint16_t pkt_status = *(uint16_t*)(rx_buf);
            uint16_t pkt_len = *(uint16_t*)(rx_buf + 2);

            if((pkt_status & 0x01) && (pkt_len > 4)) {
                net_process_packet(rx_buf + 4, pkt_len - 4);
            }

            nic.rx_ptr = (nic.rx_ptr + pkt_len + 4 + 3) & ~3;
            if(nic.rx_ptr >= RX_BUFFER_SIZE)
                nic.rx_ptr -= RX_BUFFER_SIZE;
            
            outportw(nic.iobase + REG_CAPR, nic.rx_ptr - 0x10);
        }
    }

    if(status & 0x04) { // Transmit OK
        serial_printf("RTL8139: TX completed\n");
    }

    if(status & 0x08) { // Receive error
        serial_printf("RTL8139: RX error, resetting\n");
        outportb(nic.iobase + REG_CMD, 0x10);
        while(inportb(nic.iobase + REG_CMD) & 0x10);
        outportb(nic.iobase + REG_CMD, 0x0C);
    }
}