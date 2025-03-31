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
        nic.rx_buffer = dma_alloc(RX_BUFFER_SIZE / PAGE_SIZE);
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

    // Hardware reset
    outportb(nic.iobase + REG_CONFIG1, 0x10);
    while (inportb(nic.iobase + REG_CONFIG1) & 0x10)
        ;

    outportl(nic.iobase + REG_RXBUF, nic.rx_phys);
    outportl(nic.iobase + REG_RCR, 0xF | (1 << 7));

    // Configure TX
    outportl(nic.iobase + REG_TCR,
             (0x03 << 24) |     // TX DMA burst unlimited
                 (0x00 << 16) | // Clear early TX threshold
                 (0x60 << 8) |  // IFG = normal
                 (0x00));       // Default retry count

    // Initialize TX descriptors
    for (int i = 0; i < NUM_TX_BUFFERS; i++)
    {
        outportl(nic.iobase + REG_TXADDR0 + (i * 4), nic.tx_phys + (i * TX_BUFFER_SIZE));
        outportl(nic.iobase + REG_TXSTATUS0 + (i * 4), 0);
    }

    // Enable interrupts and start chip
    outportw(nic.iobase + REG_IMR, 0x0005);
    outportb(nic.iobase + REG_CMD, 0x0C);

    nic.rx_ptr = 0;
    outportw(nic.iobase + REG_CAPR, 0);

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
        serial_printf("RTL8139: Packet too large\n");
        return;
    }

    uint8_t tx_idx = nic.tx_current;
    uint32_t status;

    // Wait for buffer to become available
    uint32_t timeout = TX_TIMEOUT_MS * 1000;
    do
    {
        status = inportl(nic.iobase + REG_TXSTATUS0 + (tx_idx * 4));
        if (!(status & (1 << 13)))
        { // Check if buffer is available
            break;
        }
        if (--timeout == 0)
        {
            serial_printf("RTL8139: TX timeout on buffer %d (status=0x%x)\n", tx_idx, status);
            return;
        }
        __asm__ volatile("pause");
    } while (1);

    // Copy data to TX buffer
    uint8_t *tx_buf = nic.tx_buffer + (tx_idx * TX_BUFFER_SIZE);
    memcpy(tx_buf, data, len);

    // Start transmission
    outportl(nic.iobase + REG_TXSTATUS0 + (tx_idx * 4),
             len & 0x1FFF); // Packet size

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

    // Clear interrupts first
    outportw(nic.iobase + REG_ISR, status);

    if (status & 0x01)
    { // ROK (Receive OK)
        uint16_t curr_rx = inportw(nic.iobase + REG_CAPR);

        while ((curr_rx + 16) % RX_BUFFER_SIZE != nic.rx_ptr)
        {
            uint8_t *rx_buf = nic.rx_buffer + nic.rx_ptr;
            uint16_t pkt_len = *(uint16_t *)(rx_buf + 2) & 0x1FFF;

            if (pkt_len < 14 || pkt_len > 1514 ||
                (nic.rx_ptr + pkt_len + 4) > RX_BUFFER_SIZE)
            {
                nic.rx_ptr = (nic.rx_ptr + 4 + 3) & ~3;
                nic.rx_ptr %= RX_BUFFER_SIZE;
                continue;
            }

            uint8_t *packet_data = rx_buf + 4;
            net_process_packet(packet_data, pkt_len);

            nic.rx_ptr = (nic.rx_ptr + pkt_len + 4 + 3) & ~3;
            nic.rx_ptr %= RX_BUFFER_SIZE;
            outportw(nic.iobase + REG_CAPR, (nic.rx_ptr - 16) % RX_BUFFER_SIZE);
            return;
        }
    }
    if (status & 0x04) { // TOK - Transmit OK
        for (int i = 0; i < NUM_TX_BUFFERS; i++) {
            uint32_t tsd = inportl(nic.iobase + REG_TXSTATUS0 + (i * 4));
            
            if (tsd & (1 << 15)) { // TOK bit - Transmit OK
                // Clear status and ownership
                outportl(nic.iobase + REG_TXSTATUS0 + (i * 4), 0);
                serial_printf("RTL8139: TX buffer %d completed\n", i);
            }
            // Remove the OWN bit check since we only care about completed transmissions
        }
        // Clear TOK interrupt
        outportw(nic.iobase + REG_ISR, 0x04);
    }

    if (status & 0x08)
    {
        serial_printf("RTL8139: Transmit Error\n");
    }
    if (status & 0x02)
    {
        serial_printf("RTL8139: Receive Error\n");
    }
    if (status & 0x10)
    {
        serial_printf("RTL8139: Rx Buffer Overflow\n");
        outportb(nic.iobase + REG_CMD, 0x04);
        nic.rx_ptr = 0;
        outportb(nic.iobase + REG_CMD, 0x0C);
    }
    outportw(nic.iobase + REG_CAPR, (nic.rx_ptr - 16) % RX_BUFFER_SIZE);

    pic8259_eoi(nic.irq);
}