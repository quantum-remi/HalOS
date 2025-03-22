#include "rtl8139.h"
#include "serial.h"
#include "arp.h"
#include "network.h"
#include "kernel.h"
#include "8259_pic.h"

#define TX_TIMEOUT_MS 2000
#define TX_BUFFER_TIMEOUT 1000

struct rtl8139_dev nic = {0};

static void read_mac_address()
{
    uint32_t mac_low = inportl(nic.iobase + REG_MAC0);
    uint16_t mac_high = inportw(nic.iobase + REG_MAC0 + 4);
    memcpy(nic.mac, &mac_low, 4);
    memcpy(nic.mac + 4, &mac_high, 2);
}

void rtl8139_init()
{
    pci_dev_t dev = pci_get_device(RTL8139_VENDOR_ID, RTL8139_DEVICE_ID, -1);
    if (dev.bits == dev_zero.bits)
    {
        serial_printf("RTL8139: Device not found\n");
        return;
    }

    // Enable PCI features
    uint32_t pci_cmd = pci_read(dev, PCI_COMMAND);
    pci_cmd |= (1 << 0) | (1 << 2); // I/O space + Bus mastering
    pci_write(dev, PCI_COMMAND, pci_cmd);

    // Get base address and IRQ
    nic.iobase = pci_read(dev, PCI_BAR0) & 0xFFFC;
    nic.irq = pci_read(dev, PCI_INTERRUPT_LINE) & 0xFF;

    if (nic.iobase == 0)
    {
        serial_printf("RTL8139: Invalid I/O base\n");
        return;
    }

    if (nic.irq == 0)
    {
        serial_printf("RTL8139: Invalid IRQ\n");
        nic.rx_buffer = dma_alloc(RX_BUFFER_SIZE / PAGE_SIZE);
        if (!nic.rx_buffer)
        {
            serial_printf("RTL8139: Failed to allocate RX buffer\n");
            return;
        }
        return;
    }

    // Allocate DMA buffers
    nic.rx_buffer = dma_alloc(RX_BUFFER_PAGES);
    if (!nic.rx_buffer)
    {
        serial_printf("RTL8139: Failed to allocate RX buffer\n");
        return;
    }
    nic.rx_phys = virt_to_phys(nic.rx_buffer);
    serial_printf("RTL8139: RX buffer P=0x%x\n", nic.rx_phys);
    outportl(nic.iobase + REG_RXBUF, nic.rx_phys); // Program RX buffer address

    uint32_t tx_total_size = NUM_TX_BUFFERS * TX_BUFFER_SIZE;
    uint32_t tx_pages = (tx_total_size + PAGE_SIZE - 1) / PAGE_SIZE;
    nic.tx_buffer = dma_alloc(tx_pages);
    if (!nic.tx_buffer)
    {
        serial_printf("RTL8139: Failed to allocate TX buffer\n");
        dma_free(nic.rx_buffer, RX_BUFFER_PAGES); // Use DMA-free
        return;
    }
    nic.tx_phys = virt_to_phys(nic.tx_buffer);
    for (int i = 0; i < NUM_TX_BUFFERS; i++)
    {
        uint32_t tx_phys = nic.tx_phys + (i * TX_BUFFER_SIZE);
        outportl(nic.iobase + REG_TXADDR0 + (i * 4), tx_phys);  // Set TXADDR
        serial_printf("RTL8139: TX%d addr=0x%x\n", i, tx_phys); // Debug log
    }

    // Hardware reset
    outportb(nic.iobase + REG_CONFIG1, 0x10);
    while (inportb(nic.iobase + REG_CONFIG1) & 0x10)
        ;

    // Configure registers
    outportl(nic.iobase + REG_RXBUF, nic.rx_phys);
    outportl(nic.iobase + REG_RCR, 0xF | (1 << 7));
    outportl(nic.iobase + REG_TCR, 
        (0x3 << 8) |    // IFG
        (0x7 << 4) |    // MXDMA
        (1 << 3));       // APM (Auto Pad)
    // Initialize TX descriptors
    // const int TX_STEP_SIZE = 4;
    // for (int i = 0; i < NUM_TX_BUFFERS; i++)
    // {
    //     uint32_t tx_phys = nic.tx_phys + (i * TX_BUFFER_SIZE);
    //     outportl(nic.iobase + REG_TXADDR0 + (i * TX_STEP_SIZE), tx_phys);
    //     outportl(nic.iobase + REG_TXSTATUS0 + (i * TX_STEP_SIZE), 0);
    // }

    for (int i = 0; i < NUM_TX_BUFFERS; i++)
    {
        outportl(nic.iobase + REG_TXSTATUS0 + (i * 4), 0); // Clear OWN initially
    }

    // Enable interrupts and start chip
    outportw(nic.iobase + REG_IMR, 0x0005); // ROK + TOK
    outportb(nic.iobase + REG_CMD, 0x0C);

    // outportl(nic.iobase + 0x40,
    //          0x03000700 |     // MXDMA unlimited
    //              (0x3 << 8)); // IFG = 3 (96-bit inter-frame gap)

    // clear rx buffer
    nic.rx_ptr = 0;
    outportw(nic.iobase + REG_CAPR, 0);

    // pic8259_unmask(2);
    pic8259_unmask(nic.irq);
    // Read and print MAC
    isr_register_interrupt_handler(nic.irq + IRQ_BASE, rtl8139_irq_handler);
    // if (nic.irq >= 8) {
    //     pic8259_unmask(nic.irq - 8); // For IRQ 11: 11-8=3 → unmask Slave PIC IRQ 3
    // } else {
    //     pic8259_unmask(nic.irq);
    // }
    // Add debug logs to confirm unmasking:
    read_mac_address();
    serial_printf("RTL8139: MAC %02x:%02x:%02x:%02x:%02x:%02x\n",
                  nic.mac[0], nic.mac[1], nic.mac[2],
                  nic.mac[3], nic.mac[4], nic.mac[5]);

    serial_printf("TX Buffers:\n");
    for (int i = 0; i < 4; i++)
    {
        uint32_t tx_phys = nic.tx_phys + (i * TX_BUFFER_SIZE);
        serial_printf("  TX%d: V=0x%x P=0x%x\n",
                      i, nic.tx_buffer + (i * TX_BUFFER_SIZE), tx_phys);
    }
    serial_printf("TX Buffer Virtual: 0x%x → Physical: 0x%x\n",
                  nic.tx_buffer, virt_to_phys(nic.tx_buffer));

    serial_printf("RTL8139: Initialized\n");

    // serial_printf("RTL8139: TEST trigger interrupt\n");
    // outportb(nic.iobase + REG_ISR, 0x01);
}

void rtl8139_send_packet(uint8_t *data, uint16_t len)
{
    if (len > TX_BUFFER_SIZE)
    {
        serial_printf("RTL8139: Packet too large (%d bytes)\n", len);
        return;
    }

    uint8_t tx_idx = nic.tx_current;
    uint8_t *tx_buf = nic.tx_buffer + (tx_idx * TX_BUFFER_SIZE);

    // Wait for buffer to be free
    uint32_t timeout = TX_TIMEOUT_MS * 1000;
    while ((inportl(nic.iobase + REG_TXSTATUS0 + (tx_idx * 4)) & 0x2000))
    {
        usleep(1); // Add a small delay to prevent CPU hogging

        if (--timeout == 0)
        {
            rtl8139_init(); // Reinitialize NIC
            continue;
        }
        __asm__ volatile("pause"); // Prevent CPU hogging
    }

    // Align packet to 4 bytes
    uint16_t aligned_len = (len + 3) & ~3;
    memcpy(tx_buf, data, len);
    memset(tx_buf + len, 0, aligned_len - len); // Zero-pad

    // Program descriptor with OWN bit
    uint32_t tsd_value = (len & 0x1FFF) | (1 << 13); // OWN = bit 13
    outportl(nic.iobase + REG_TXSTATUS0 + (tx_idx * 4), tsd_value);
    nic.tx_current = (tx_idx + 1) % NUM_TX_BUFFERS;

    serial_printf("RTL8139: Sent %d bytes via buffer %d (aligned=%d)\n",
                  len, tx_idx, aligned_len);
}

void rtl8139_receive_packet(uint8_t *data, uint16_t len)
{
    (void)data;
    (void)len;
}

void rtl8139_irq_handler(REGISTERS *r)
{
    (void)r;
    uint16_t status = inportw(nic.iobase + 0x3e);
    outportw(nic.iobase + 0x3E, 0x05);
    // uint16_t status = inportw(nic.iobase + REG_ISR);
    // outportw(nic.iobase + REG_ISR, status); // Clear interrupts

    serial_printf("RTL8139: IRQ Triggered\n");

    serial_printf("RTL8139: ISR=0x%x (ROK=%d, TOK=%d)\n",
                  status, (status & 0x01), (status & 0x04));

    if (status & 0x01)
    { // ROK (Receive OK)
        uint16_t capr = inportw(nic.iobase + REG_CAPR);
        uint16_t curr_rx = (capr + 16) % RX_BUFFER_SIZE;

        while (nic.rx_ptr != curr_rx)
        {
            uint8_t *rx_buf = nic.rx_buffer + nic.rx_ptr;
            uint16_t pkt_status = *(uint16_t *)(rx_buf);
            uint16_t pkt_len = *(uint16_t *)(rx_buf + 2) & 0x1FFF; // Mask 13 bits
            if (pkt_len < 14 || pkt_len > 1514)
            {
                // serial_printf("Invalid packet length: %d. Skipping.\n", pkt_len);
                nic.rx_ptr = (nic.rx_ptr + 4) % RX_BUFFER_SIZE; // Skip header
                continue;
            }

            // Process valid packet
            uint8_t *packet_data = rx_buf + 4; // Skip header
            net_process_packet(packet_data, pkt_len);
            // Update pointer with alignment
            nic.rx_ptr = (nic.rx_ptr + pkt_len + 4 + 3) & ~3; // Align to 4 bytes
            nic.rx_ptr %= RX_BUFFER_SIZE;                     // Wrap around circular buffer

            // Update CAPR
            outportw(nic.iobase + REG_CAPR, (nic.rx_ptr - 16) % RX_BUFFER_SIZE);
        }
    }
    if (status & 0x04)
    { // TOK (Transmit OK)
        for (int i = 0; i < NUM_TX_BUFFERS; i++)
        {
            uint32_t tsd = inportl(nic.iobase + REG_TXSTATUS0 + (i * 4));
            if (tsd & 0x8000)
            { // Check TOK bit (bit 15)
                // Clear TOK by writing 1 to it (per datasheet)
                outportl(nic.iobase + REG_TXSTATUS0 + (i * 4), tsd | 0x8000);
                serial_printf("TX buffer %d completed (TSD=0x%x)\n", i, tsd);
            }
        }
    }
    // Handle other interrupts (TX, errors)
    if (status & 0x08)
    { // Transmit error
        serial_printf("RTL8139: Transmit Error\n");
    }
    if (status & 0x02)
    { // Receive error
        serial_printf("RTL8139: Receive Error\n");
    }
    if (status & 0x10)
    { // Rx buffer overflow
        serial_printf("RTL8139: Rx Buffer Overflow\n");
        outportb(nic.iobase + REG_CMD, 0x04); // Reset rx
        nic.rx_ptr = 0;
        outportb(nic.iobase + REG_CMD, 0x0C); // Re-enable rx/tx
    }
    outportw(nic.iobase + REG_CAPR, (nic.rx_ptr - 16) % RX_BUFFER_SIZE);
    if (status & 0x01 || status & 0x04)
    {
        if (nic.irq >= 8)
        {
            pic8259_eoi(nic.irq - 8); // Slave
            pic8259_eoi(2);           // Master (cascade)
        }
        else
        {
            pic8259_eoi(nic.irq); // Master
        }
    }
}