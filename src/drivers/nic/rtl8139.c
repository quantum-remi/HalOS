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

    uint32_t pci_cmd = pci_read(dev, PCI_COMMAND);
    pci_cmd |= (1 << 0) | (1 << 2);
    pci_write(dev, PCI_COMMAND, pci_cmd);

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
        nic.rx_buffer = dma_alloc(RX_BUFFER_PAGES);
        if (!nic.rx_buffer)
        {
            serial_printf("RTL8139: Failed to allocate RX buffer\n");
            return;
        }
        return;
    }

    nic.rx_buffer = dma_alloc(RX_BUFFER_PAGES);
    if (!nic.rx_buffer)
    {
        serial_printf("RTL8139: Failed to allocate RX buffer\n");
        return;
    }
    nic.rx_phys = virt_to_phys(nic.rx_buffer);
    outportl(nic.iobase + REG_RXBUF, nic.rx_phys);

    uint32_t tx_total_size = NUM_TX_BUFFERS * TX_BUFFER_SIZE;
    uint32_t tx_pages = (tx_total_size + PAGE_SIZE - 1) / PAGE_SIZE;
    nic.tx_buffer = dma_alloc(tx_pages);
    if (!nic.tx_buffer)
    {
        serial_printf("RTL8139: Failed to allocate TX buffer\n");
        dma_free(nic.rx_buffer, RX_BUFFER_PAGES);
        return;
    }
    nic.tx_phys = virt_to_phys(nic.tx_buffer);
    for (int i = 0; i < NUM_TX_BUFFERS; i++)
    {
        uint32_t tx_phys = nic.tx_phys + (i * TX_BUFFER_SIZE);
        outportl(nic.iobase + REG_TXADDR0 + (i * 4), tx_phys);
    }

    // Hardware reset with timeout
    outportb(nic.iobase + REG_CONFIG1, 0x10);
    uint32_t reset_timeout = 1000;
    while ((inportb(nic.iobase + REG_CONFIG1) & 0x10) && reset_timeout--)
    {
        __asm__ volatile("pause");
    }
    if (reset_timeout == 0)
    {
        serial_printf("RTL8139: Reset timeout\n");
        return;
    }

    // Additional delay after reset
    for (volatile int i = 0; i < 10000; i++)
        __asm__ volatile("pause");

    // Configure buffers first
    outportl(nic.iobase + REG_RXBUF, nic.rx_phys);

    // Clear missed packet counter
    outportl(nic.iobase + 0x4C, 0);

    // Configure RX: Accept all packets
    outportl(nic.iobase + REG_RCR,
             (1 << 7) |  // WRAP
             (1 << 3) |  // AR - Accept All Packets
             (1 << 2) |  // AB - Accept Broadcast
             (1 << 1) |  // AM - Accept Multicast
             (1 << 0));  // APM - Accept Physical Match

    // Configure TX with better defaults
    outportl(nic.iobase + REG_TCR,
             (6 << 8) |      // IFG = 960ns (default)
             (0x3F << 16) |  // Maximum TX DMA burst
             (1 << 24));     // Clear abort on error

    // Initialize all TX descriptors
    for (int i = 0; i < NUM_TX_BUFFERS; i++)
    {
        outportl(nic.iobase + REG_TXADDR0 + (i * 4), nic.tx_phys + (i * TX_BUFFER_SIZE));
        outportl(nic.iobase + REG_TXSTATUS0 + (i * 4), 0);
    }

    nic.tx_current = 0;

    // Enable interrupts - ROK + TOK only initially
    outportw(nic.iobase + REG_IMR, 0x0005);

    // Start RX and TX
    outportb(nic.iobase + REG_CMD, 0x0C);

    // Reset pointers
    nic.rx_ptr = 0;
    outportw(nic.iobase + REG_CAPR, 0);

    // enable for qemu
    outportb(nic.iobase + REG_CONFIG1, 0x0);
    outportb(nic.iobase + REG_CONFIG1, 0x0);
    outportb(nic.iobase + REG_CONFIG1, 0x0);

    pic8259_unmask(nic.irq);

    isr_register_interrupt_handler(nic.irq + IRQ_BASE, rtl8139_irq_handler);
    read_mac_address();
    serial_printf("RTL8139: MAC %02x:%02x:%02x:%02x:%02x:%02x\n",
                  nic.mac[0], nic.mac[1], nic.mac[2],
                  nic.mac[3], nic.mac[4], nic.mac[5]);

    serial_printf("RTL8139: Initialized\n");
}

void rtl8139_send_packet(uint8_t *data, uint16_t len)
{
    if (len > TX_BUFFER_SIZE)
    {
        serial_printf("RTL8139: Packet too large (%d bytes)\n", len);
        return;
    }

    // Find a free TX descriptor with timeout
    uint32_t start_time = get_ticks();
    uint8_t tx_idx = nic.tx_current;
    uint32_t status;
    bool buffer_found = false;

    while ((get_ticks() - start_time) < TX_TIMEOUT_MS)
    {
        for (int i = 0; i < NUM_TX_BUFFERS; i++)
        {
            tx_idx = (nic.tx_current + i) % NUM_TX_BUFFERS;
            status = inportl(nic.iobase + REG_TXSTATUS0 + (tx_idx * 4));
            if (!(status & (1 << 13)))
            { // Check OWN bit
                buffer_found = true;
                break;
            }
        }
        if (buffer_found)
            break;
        __asm__ volatile("pause");
    }

    if (!buffer_found)
    {
        serial_printf("RTL8139: TX timeout - resetting TX\n");
        // Reset TX ring
        uint8_t cmd = inportb(nic.iobase + REG_CMD);
        outportb(nic.iobase + REG_CMD, cmd & ~0x04);
        outportb(nic.iobase + REG_CMD, cmd | 0x04);

        // Reset descriptors
        for (int i = 0; i < NUM_TX_BUFFERS; i++)
        {
            outportl(nic.iobase + REG_TXSTATUS0 + (i * 4), 0);
        }

        tx_idx = 0;
        nic.tx_current = 0;
    }

    // Copy packet to TX buffer with proper alignment
    uint8_t *tx_buf = nic.tx_buffer + (tx_idx * TX_BUFFER_SIZE);
    memcpy(tx_buf, data, len);

    // Set descriptor with correct flags
    outportl(nic.iobase + REG_TXSTATUS0 + (tx_idx * 4),
             (0x3F << 16) |  // All segments + maximum early TX threshold
             (len & 0x1FFF)); // Length

    nic.tx_current = (tx_idx + 1) % NUM_TX_BUFFERS;
}

void rtl8139_receive_packet(uint8_t *data, uint16_t len)
{
    (void)data;
    (void)len;
}

void rtl8139_irq_handler(REGISTERS *r)
{
    (void)r;
    uint16_t status = inportw(nic.iobase + REG_ISR);

    // Clear interrupts but keep track of what we're processing
    outportw(nic.iobase + REG_ISR, status);

    if (status & 0x01)
    { // ROK (Receive OK)
        serial_printf("RTL8139: Receive OK\n");

        // Keep processing packets until we catch up with the NIC
        while (1)
        {
            uint8_t *rx_buf = nic.rx_buffer + nic.rx_ptr;
            uint32_t rx_status = *(uint32_t *)rx_buf;
            uint16_t pkt_len = rx_status >> 16;

            // Check if we have a complete packet
            if (!(rx_status & 0x1))
            {
                break; // No more packets
            }

            pkt_len &= 0x1FFF; // Mask length to 13 bits

            if (pkt_len < 64 || pkt_len > 1514)
            {
                serial_printf("RTL8139: Invalid packet length %d\n", pkt_len);
                break;
            }

            // Process the actual packet data (skip status dword)
            uint8_t *packet_data = rx_buf + 4;
            net_process_packet(packet_data, pkt_len);

            // Update receive pointer with 4-byte alignment
            nic.rx_ptr = (nic.rx_ptr + pkt_len + 4 + 3) & ~3;
            nic.rx_ptr %= RX_BUFFER_SIZE;

            // Update CAPR
            outportw(nic.iobase + REG_CAPR, (nic.rx_ptr - 16) % RX_BUFFER_SIZE);
        }
    }

    if (status & 0x04)
    { // TOK - Transmit OK
        for (int i = 0; i < NUM_TX_BUFFERS; i++)
        {
            uint32_t tsd = inportl(nic.iobase + REG_TXSTATUS0 + (i * 4));

            if (tsd & (1 << 15))
            { // TOK bit - Transmit OK
                // Clear status and ownership
                outportl(nic.iobase + REG_TXSTATUS0 + (i * 4), 0);
                serial_printf("RTL8139: TX buffer %d completed\n", i);
            }
        }
        // Clear TOK interrupt
        outportw(nic.iobase + REG_ISR, 0x04);
    }

    if (status & 0x10)
    { // Rx buffer overflow
        serial_printf("RTL8139: Rx Buffer Overflow - Resetting RX\n");
        uint8_t cmd = inportb(nic.iobase + REG_CMD);
        outportb(nic.iobase + REG_CMD, cmd & ~0x08); // Stop RX
        nic.rx_ptr = 0;
        outportw(nic.iobase + REG_CAPR, 0);
        outportl(nic.iobase + REG_RXBUF, nic.rx_phys); // Reset buffer address
        outportb(nic.iobase + REG_CMD, cmd | 0x08);    // Restart RX
    }

    if (status & 0x08)
    {
        serial_printf("RTL8139: Transmit Error\n");
    }
    if (status & 0x02)
    {
        serial_printf("RTL8139: Receive Error\n");
    }
    outportw(nic.iobase + REG_CAPR, (nic.rx_ptr - 16) % RX_BUFFER_SIZE);

    pic8259_eoi(nic.irq);
}