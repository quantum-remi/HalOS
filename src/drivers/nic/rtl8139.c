#include "rtl8139.h"
#include "serial.h"
#include "arp.h"
#include "network.h"
#include "kernel.h"
#include "8259_pic.h"

#define TX_TIMEOUT_MS 2000
#define TX_BUFFER_TIMEOUT 1000

struct rtl8139_dev nic = {0};
uint16_t rx_offset = 0;

static void read_mac_address()
{
    uint32_t mac_low = inportl(nic.iobase + REG_MAC0);
    uint16_t mac_high = inportw(nic.iobase + REG_MAC0 + 4);
    memcpy(nic.mac, &mac_low, 4);
    memcpy(nic.mac + 4, &mac_high, 2);
}

void rtl8139_init()
{
    // Correct PCI device detection approach
    pci_dev_t dev = pci_get_device(RTL8139_VENDOR_ID, RTL8139_DEVICE_ID, -1);
    if (dev.bits == dev_zero.bits)
    {
        serial_printf("RTL8139: Device not found\n");
        return;
    }

    // Correct: Enabling Bus Mastering and I/O Space access
    uint32_t pci_cmd = pci_read(dev, PCI_COMMAND);
    pci_cmd |= (1 << 0) | (1 << 2);
    pci_write(dev, PCI_COMMAND, pci_cmd);

    // Correct BAR and IRQ reading
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

    // Correct TX buffer setup
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

    outportb(nic.iobase + REG_CONFIG1, 0x0); 
    
    
    outportb(nic.iobase + REG_CMD, 0x10);
    while((inportb(nic.iobase + REG_CMD) & 0x10) != 0) {}
    
    outportl(nic.iobase + REG_RXBUF, nic.rx_phys);

    outportw(nic.iobase + REG_IMR, 0x0005);

    outportl(nic.iobase + REG_RCR, 0xf | (1 << 7));

    outportb(nic.iobase + REG_CMD, 0x0C);

    outportb(nic.iobase + REG_CONFIG1, 0x0);
    outportb(nic.iobase + REG_CONFIG1, 0x0);
    outportb(nic.iobase + REG_CONFIG1, 0x0);
    
    read_mac_address();
    pic8259_unmask(nic.irq);

    isr_register_interrupt_handler(nic.irq + IRQ_BASE, rtl8139_irq_handler);
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

    uint8_t *tx_buf = nic.tx_buffer + (nic.tx_current * TX_BUFFER_SIZE);
    memcpy(tx_buf, data, len);

    outportl(nic.iobase + REG_TXSTATUS0 + (nic.tx_current *4), len);

    nic.tx_current = (nic.tx_current + 1) % NUM_TX_BUFFERS;

    serial_printf("RTL8139: TX packet send\n");

}

void rtl8139_receive_packet() {
    uint16_t cbr;
    uint16_t rx_offset = nic.rx_ptr;

    cbr = inportw(nic.iobase + 0x3A) << 8;


    while (rx_offset != cbr) {
     
        uint16_t buffer_pos = rx_offset % RX_BUFFER_SIZE;

        uint32_t rx_status = *(uint32_t*)(nic.rx_buffer + buffer_pos);
        uint16_t packet_len = rx_status >> 16;


        if ((packet_len == 0) || (packet_len > 1514)) {
            serial_printf("RTL8139: Invalid packet length %d\n", packet_len);
            break;
        }


        uint8_t *packet_data = nic.rx_buffer + buffer_pos + 4;
        net_process_packet(packet_data, packet_len);


        rx_offset = (buffer_pos + packet_len + 4 + 3) & ~3;

        if (rx_offset >= RX_BUFFER_SIZE)
            rx_offset -= RX_BUFFER_SIZE;
    }


    nic.rx_ptr = rx_offset;
    outportw(nic.iobase + REG_CAPR, (nic.rx_ptr - 16) % RX_BUFFER_SIZE);
}

void rtl8139_irq_handler(REGISTERS *r)
{
    (void)r;
    serial_printf("RTL8139: IRQ %d\n", nic.irq);
    uint16_t status = inportw(nic.iobase + 0x3E);
    outportw(nic.iobase + 0x3E, 0x05);

    if (status & 0x01)
    {
        serial_printf("RTL8139: Receive OK\n");

        rtl8139_receive_packet();
    }

    if (status & 0x04)
    {
        for (int i = 0; i < NUM_TX_BUFFERS; i++)
        {
            uint32_t tsd = inportl(nic.iobase + REG_TXSTATUS0 + (i * 4));
            serial_printf("RTL8139: TX%d TSD=0x%x\n", i, tsd); 
        }
    }

    if (status & 0x10)
    {
        serial_printf("RTL8139: Rx Buffer Overflow - Resetting RX\n");
        uint8_t cmd = inportb(nic.iobase + REG_CMD);
        outportb(nic.iobase + REG_CMD, cmd & ~0x08);
        nic.rx_ptr = 0;
        outportw(nic.iobase + REG_CAPR, 0);
        outportl(nic.iobase + REG_RXBUF, nic.rx_phys);
        outportb(nic.iobase + REG_CMD, cmd | 0x08);
    }

    if (status & 0x08)
    {
        serial_printf("RTL8139: Transmit Error\n");
    }
    if (status & 0x02)
    {
        serial_printf("RTL8139: Receive Error\n");
    }

    pic8259_unmask(nic.irq);
    pic8259_eoi(nic.irq);
}