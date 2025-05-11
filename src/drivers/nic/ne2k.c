#include "pci.h"
#include "vmm.h"
#include "8259_pic.h"
#include "io.h"
#include "isr.h"
#include "serial.h"
#include "liballoc.h"
#include "network.h"
#include "ne2k.h"

#define NE2K_VENDOR_ID 0x10EC
#define NE2K_DEVICE_ID 0x8029

#define NE2K_IOPORT_SIZE 0x20

// NE2000 Registers (offsets from base I/O port)
#define NE2K_CR 0x00
#define NE2K_PSTART 0x01
#define NE2K_PSTOP 0x02
#define NE2K_BNRY 0x03
#define NE2K_TSR 0x04
#define NE2K_TPSR 0x04
#define NE2K_NCR 0x05
#define NE2K_TBCR0 0x05
#define NE2K_TBCR1 0x06
#define NE2K_ISR 0x07
#define NE2K_RSAR0 0x08
#define NE2K_RSAR1 0x09
#define NE2K_RBCR0 0x0A
#define NE2K_RBCR1 0x0B
#define NE2K_RCR 0x0C
#define NE2K_TCR 0x0D
#define NE2K_DCR 0x0E
#define NE2K_IMR 0x0F
#define NE2K_PAR0 0x01
#define NE2K_CURR 0x07
#define NE2K_MAR0 0x08

#define NE2K_RESET 0x1F

#define NE2K_CR_STA 0x02
#define NE2K_CR_STP 0x01
#define NE2K_CR_TXP 0x04
#define NE2K_CR_RD2 0x20
#define NE2K_CR_PAGE0 0x00
#define NE2K_CR_PAGE1 0x40

#define NE2K_DCR_INIT 0x49
#define NE2K_RCR_MON 0x20
#define NE2K_TCR_NORMAL 0x00

#define NE2K_TX_BUF 0x40
#define NE2K_RX_START 0x46
#define NE2K_RX_STOP 0x60

#define NE2K_ISR_PRX 0x01
#define NE2K_ISR_PTX 0x02
#define NE2K_ISR_RXE 0x04
#define NE2K_ISR_TXE 0x08
#define NE2K_ISR_OVW 0x10
#define NE2K_ISR_CNT 0x20
#define NE2K_ISR_RDC 0x40
#define NE2K_ISR_RST 0x80

static uint16_t ne2k_iobase = 0;
static uint8_t ne2k_mac[6] = {0};
static int ne2k_present = 0;
static uint8_t ne2k_irq = 0;

int ne2k_is_present()
{
    return ne2k_present;
}

static void ne2k_reset_chip()
{
    inportb(ne2k_iobase + NE2K_RESET);
    while (!(inportb(ne2k_iobase + NE2K_ISR) & 0x80))
        ;
    outportb(ne2k_iobase + NE2K_ISR, 0x80);
}

static void ne2k_read_mac(uint8_t *mac)
{
    outportb(ne2k_iobase + NE2K_CR, NE2K_CR_STA | NE2K_CR_RD2);

    outportb(ne2k_iobase + NE2K_RSAR0, 0x00);
    outportb(ne2k_iobase + NE2K_RSAR1, 0x00);

    outportb(ne2k_iobase + NE2K_RBCR0, 12);
    outportb(ne2k_iobase + NE2K_RBCR1, 0x00);

    outportb(ne2k_iobase + NE2K_CR, NE2K_CR_STA | NE2K_CR_RD2 | 0x08);

    for (int i = 0; i < 6; i++)
        mac[i] = inportb(ne2k_iobase + 0x10);

    for (int i = 0; i < 6; i++)
        (void)inportb(ne2k_iobase + 0x10);

    while (!(inportb(ne2k_iobase + NE2K_ISR) & 0x40))
        ;
    outportb(ne2k_iobase + NE2K_ISR, 0x40);
}

void ne2k_get_mac(uint8_t *mac)
{
    for (int i = 0; i < 6; i++)
        mac[i] = ne2k_mac[i];
}

static void ne2k_isr(REGISTERS *regs)
{
    (void)regs;
    uint8_t isr = inportb(ne2k_iobase + NE2K_ISR);
    serial_printf("NE2K: ISR triggered (0x%02x)\n", isr);

    uint8_t curr = inportb(ne2k_iobase + NE2K_CURR);
    outportb(ne2k_iobase + NE2K_CR, NE2K_CR_PAGE0 | NE2K_CR_STA);

    uint8_t bnry = inportb(ne2k_iobase + NE2K_BNRY);
    uint8_t page = bnry + 1;
    if (page >= NE2K_RX_STOP)
        page = NE2K_RX_START;

    if (page != curr)
    {
        serial_printf("NE2K: Processing packet (BNRY=0x%02x, CURR=0x%02x)\n", bnry, curr);
        uint16_t pkt_offset = page << 8;
        outportb(ne2k_iobase + NE2K_RSAR0, pkt_offset & 0xFF);
        outportb(ne2k_iobase + NE2K_RSAR1, (pkt_offset >> 8) & 0xFF);
        outportb(ne2k_iobase + NE2K_RBCR0, 4);
        outportb(ne2k_iobase + NE2K_RBCR1, 0);
        outportb(ne2k_iobase + NE2K_CR, NE2K_CR_STA | NE2K_CR_RD2 | 0x08);

        uint8_t header[4];
        for (int i = 0; i < 4; i++)
            header[i] = inportb(ne2k_iobase + 0x10);

        while (!(inportb(ne2k_iobase + NE2K_ISR) & NE2K_ISR_RDC))
            ;
        outportb(ne2k_iobase + NE2K_ISR, NE2K_ISR_RDC);

        uint8_t status = header[0];
        uint8_t next = header[1];
        uint16_t len = header[2] | (header[3] << 8);

        serial_printf("NE2K: RX packet status=0x%02x next=0x%02x len=%d\n",
                      status, next, len);

        if (len < 4)
            len = 4;
        len -= 4;
        if (len > 1514)
            len = 1514;

        uint16_t data_offset = ((page << 8) + 4) & 0xFFFF;
        outportb(ne2k_iobase + NE2K_RSAR0, data_offset & 0xFF);
        outportb(ne2k_iobase + NE2K_RSAR1, (data_offset >> 8) & 0xFF);
        outportb(ne2k_iobase + NE2K_RBCR0, len & 0xFF);
        outportb(ne2k_iobase + NE2K_RBCR1, len >> 8);
        outportb(ne2k_iobase + NE2K_CR, NE2K_CR_STA | NE2K_CR_RD2 | 0x08);

        uint8_t buf[1514];
        for (uint16_t i = 0; i < len; i++)
            buf[i] = inportb(ne2k_iobase + 0x10);

        while (!(inportb(ne2k_iobase + NE2K_ISR) & NE2K_ISR_RDC))
            ;
        outportb(ne2k_iobase + NE2K_ISR, NE2K_ISR_RDC);

        net_process_packet(buf, len);

        uint8_t new_bnry = (next == NE2K_RX_START) ? (NE2K_RX_STOP - 1) : (next - 1);
        outportb(ne2k_iobase + NE2K_BNRY, new_bnry);
    }
    else
    {
        serial_printf("NE2K: No new packets (BNRY=0x%02x, CURR=0x%02x)\n", bnry, curr);
    }

    outportb(ne2k_iobase + NE2K_ISR, isr & (NE2K_ISR_PRX | NE2K_ISR_RXE | NE2K_ISR_OVW | NE2K_ISR_PTX | NE2K_ISR_TXE | NE2K_ISR_RDC));
    pic8259_eoi(ne2k_irq);
    serial_printf("NE2K: ISR handled, isr=0x%02x\n", isr);
}

int ne2k_init()
{
    pci_dev_t dev = pci_get_device(NE2K_VENDOR_ID, NE2K_DEVICE_ID, -1);
    if (dev.bits == dev_zero.bits)
    {
        serial_printf("NE2K: Device not found\n");
        ne2k_present = 0;
        return -1;
    }
    uint32_t bar0 = pci_read(dev, PCI_BAR0) & ~0x3;
    ne2k_iobase = (uint16_t)bar0;
    if (ne2k_iobase == 0)
    {
        serial_printf("NE2K: Invalid I/O base\n");
        ne2k_present = 0;
        return -1;
    }

    ne2k_irq = pci_read(dev, PCI_INTERRUPT_LINE) & 0xFF;

    ne2k_reset_chip();

    outportb(ne2k_iobase + NE2K_DCR, NE2K_DCR_INIT);
    outportb(ne2k_iobase + NE2K_CR, NE2K_CR_STP | NE2K_CR_RD2);
    outportb(ne2k_iobase + NE2K_RCR, 0x0C);
    outportb(ne2k_iobase + NE2K_TCR, NE2K_TCR_NORMAL);
    outportb(ne2k_iobase + NE2K_PSTART, NE2K_RX_START);
    outportb(ne2k_iobase + NE2K_PSTOP, NE2K_RX_STOP);
    outportb(ne2k_iobase + NE2K_BNRY, NE2K_RX_START);
    outportb(ne2k_iobase + NE2K_CR, NE2K_CR_PAGE1 | NE2K_CR_STA);
    outportb(ne2k_iobase + NE2K_CURR, NE2K_RX_START + 1);
    outportb(ne2k_iobase + NE2K_CR, NE2K_CR_PAGE0 | NE2K_CR_STA);

    outportb(ne2k_iobase + NE2K_IMR, NE2K_ISR_PRX | NE2K_ISR_PTX | NE2K_ISR_RXE | NE2K_ISR_TXE | NE2K_ISR_OVW);

    ne2k_read_mac(ne2k_mac);
    serial_printf("NE2K: MAC %02x:%02x:%02x:%02x:%02x:%02x\n",
                  ne2k_mac[0], ne2k_mac[1], ne2k_mac[2], ne2k_mac[3], ne2k_mac[4], ne2k_mac[5]);

    memcpy(nic.mac, ne2k_mac, 6);

    outportb(ne2k_iobase + NE2K_CR, NE2K_CR_STA | NE2K_CR_RD2);

    pic8259_unmask(ne2k_irq);
    isr_register_interrupt_handler(ne2k_irq + IRQ_BASE, ne2k_isr);

    ne2k_present = 1;
    serial_printf("NE2K: Initialized at I/O 0x%x\n", ne2k_iobase);
    return 0;
}

void ne2k_send_packet(uint8_t *data, uint16_t length)
{
    if (!ne2k_present)
    {
        serial_printf("NE2K: Not present, cannot send\n");
        return;
    }
    if (length < 60)
        length = 60;
    if (length > 1514)
        length = 1514;

    outportb(ne2k_iobase + NE2K_CR, NE2K_CR_PAGE0 | NE2K_CR_STA | NE2K_CR_RD2);
    int txp_timeout = 1000;
    while ((inportb(ne2k_iobase + NE2K_CR) & NE2K_CR_TXP) && --txp_timeout > 0)
        ;
    if (txp_timeout == 0)
    {
        serial_printf("NE2K: TX FIFO timeout!\n");
        return;
    }

    outportb(ne2k_iobase + NE2K_TPSR, NE2K_TX_BUF);
    outportb(ne2k_iobase + NE2K_TBCR0, length & 0xFF);
    outportb(ne2k_iobase + NE2K_TBCR1, length >> 8);

    uint16_t tx_offset = NE2K_TX_BUF << 8;
    outportb(ne2k_iobase + NE2K_RSAR0, tx_offset & 0xFF);
    outportb(ne2k_iobase + NE2K_RSAR1, (tx_offset >> 8) & 0xFF);
    outportb(ne2k_iobase + NE2K_RBCR0, length & 0xFF);
    outportb(ne2k_iobase + NE2K_RBCR1, length >> 8);

    outportb(ne2k_iobase + NE2K_CR, NE2K_CR_STA | NE2K_CR_RD2 | 0x10);

    uint16_t i = 0;
    for (; i + 1 < length; i += 2)
    {
        uint16_t word = data[i] | (data[i + 1] << 8);
        outportw(ne2k_iobase + 0x10, word);
    }
    if (i < length)
    {
        outportw(ne2k_iobase + 0x10, data[i]);
    }

    while (!(inportb(ne2k_iobase + NE2K_ISR) & NE2K_ISR_RDC))
        ;
    outportb(ne2k_iobase + NE2K_ISR, NE2K_ISR_RDC);

    outportb(ne2k_iobase + NE2K_CR, NE2K_CR_STA | NE2K_CR_TXP | NE2K_CR_RD2);

    int timeout = 100000;
    while (!(inportb(ne2k_iobase + NE2K_ISR) & NE2K_ISR_PTX) && --timeout > 0)
        ;

    if (inportb(ne2k_iobase + NE2K_ISR) & NE2K_ISR_PTX)
        outportb(ne2k_iobase + NE2K_ISR, NE2K_ISR_PTX);

    uint8_t tsr = inportb(ne2k_iobase + NE2K_TSR);
    if (timeout == 0)
    {
        serial_printf("NE2K: TX timeout!\n");
    }
    else if (tsr & 0x01)
    {
        serial_printf("NE2K: TX OK (%u bytes)\n", length);
    }
    else
    {
        serial_printf("NE2K: TX error, TSR=0x%02x\n", tsr);
    }
}
