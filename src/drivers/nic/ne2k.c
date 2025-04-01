#include "pci.h"
#include "vmm.h"
#include "8259_pic.h"
#include "io.h"
#include "isr.h"
#include "serial.h"
#include "liballoc.h"
#include "network.h"

#define DP83820_VENDOR_ID  0x100B
#define DP83820_DEVICE_ID  0x0002

/* DP83820 Registers */
typedef struct {
    uint32_t cr;         /* Control Register */
    uint32_t cfg;        /* Configuration Register */
    uint32_t mear;       /* MII Management */
    uint32_t ptr;        /* PCI Test */
    uint32_t isr;        /* Interrupt Status */
    uint32_t imr;        /* Interrupt Mask */
    uint32_t rcr;        /* Receive Configuration */
    uint32_t tcr;        /* Transmit Configuration */
    uint32_t mpc;        /* Missed Packet Counter */
    uint32_t rdesc;      /* Receive Descriptor Base */
    uint32_t rbuf;       /* Receive Buffer Base */
    uint32_t tdesc;      /* Transmit Descriptor Base */
} dp83820_regs_t;

/* Descriptor Structure */
typedef struct {
    uint32_t status;
    uint32_t bufptr;
    uint32_t next;
    uint32_t control;
} dp83820_desc_t;

/* Driver State */
typedef struct {
    dp83820_regs_t* regs;
    uint32_t irq;
    uint8_t mac[6];
    
    /* Transmit */
    dp83820_desc_t* tx_ring;
    uint32_t tx_cur;
    uint8_t* tx_buffers[32];
    
    /* Receive */
    dp83820_desc_t* rx_ring;
    uint32_t rx_cur;
    uint8_t* rx_buffers[32];
} dp83820_driver_t;

static dp83820_driver_t nic2k;

/* Register Operations */
#define REG_READ(reg) (*(volatile uint32_t*)((uintptr_t)nic2k.regs + reg))
#define REG_WRITE(reg, val) (*(volatile uint32_t*)((uintptr_t)nic2k.regs + reg) = val)

/* Descriptor Status Flags */
#define DESC_OWN    (1 << 31)
#define DESC_EOR    (1 << 30)
#define DESC_FS     (1 << 29)
#define DESC_LS     (1 << 28)
#define DESC_TX_OK  (1 << 21)

/* Interrupt Flags */
#define ISR_RX_OK   (1 << 0)
#define ISR_TX_OK   (1 << 1)
#define ISR_RX_ERR  (1 << 2)
#define ISR_TX_ERR  (1 << 3)

static void dp83820_reset();
static void dp83820_isr(REGISTERS* regs);

/* Interrupt Service Routine */
static void dp83820_isr(REGISTERS* regs) {
    uint32_t status = REG_READ(0x10); /* ISR */
    REG_WRITE(0x10, status); /* Clear interrupts */

    if (status & ISR_RX_OK) {
        while (!(nic2k.rx_ring[nic2k.rx_cur].status & DESC_OWN)) {
            uint16_t length = nic2k.rx_ring[nic2k.rx_cur].status & 0x1FFF;
            net_process_packet(nic2k.rx_buffers[nic2k.rx_cur], length);
            
            /* Recycle descriptor */
            nic2k.rx_ring[nic2k.rx_cur].status = DESC_OWN;
            nic2k.rx_cur = (nic2k.rx_cur + 1) % 32;
        }
    }

    if (status & ISR_TX_OK) {
        /* Free completed TX buffers */
        while (nic2k.tx_ring[nic2k.tx_cur].status & DESC_TX_OK) {
            nic2k.tx_ring[nic2k.tx_cur].status = 0;
            nic2k.tx_cur = (nic2k.tx_cur + 1) % 32;
        }
    }

    pic8259_eoi(nic2k.irq);
}
/* Initialize the DP83820 NIC */
int dp83820_init() {
    pci_dev_t dev = pci_get_device(DP83820_VENDOR_ID, DP83820_DEVICE_ID, PCI_TYPE_ETHERNET);
    if (dev.bits == dev_zero.bits) {
        serial_printf("DP83820: Device not found\n");
        return -1;
    }

    /* Enable Bus Mastering and MMIO */
    uint32_t command = pci_read(dev, PCI_COMMAND);
    command |= (1 << 1) | (1 << 2); /* Enable MMIO and Bus Master */
    pci_write(dev, PCI_COMMAND, command);

    /* Map MMIO Space */
    uint32_t bar0 = pci_read(dev, PCI_BAR0);
    nic2k.regs = (dp83820_regs_t*)vmm_map_mmio(bar0 & ~0xF, 0x100, 0);

    /* Get MAC Address */
    for (int i = 0; i < 6; i++)
        nic2k.mac[i] = pci_read(dev, 0x10 + i) >> 24;

    /* Get IRQ */
    nic2k.irq = pci_read(dev, PCI_INTERRUPT_LINE) & 0xFF;
    pic8259_unmask(nic2k.irq);
    
    /* Setup ISR */
    isr_register_interrupt_handler(nic2k.irq, dp83820_isr);
    
    /* Initialize NIC */
    dp83820_reset();
    return 0;
}

/* Reset and Configure NIC */
static void dp83820_reset() {
    /* Soft Reset */
    REG_WRITE(0x00, 0x80000000);
    while (REG_READ(0x00) & 0x80000000);

    /* Allocate DMA Buffers */
    nic2k.tx_ring = (dp83820_desc_t*)dma_alloc(32 * sizeof(dp83820_desc_t));
    nic2k.rx_ring = (dp83820_desc_t*)dma_alloc(32 * sizeof(dp83820_desc_t));

    /* Initialize TX Descriptors */
    for (int i = 0; i < 32; i++) {
        nic2k.tx_ring[i].status = 0;
        nic2k.tx_ring[i].bufptr = virt_to_phys(nic2k.tx_buffers[i] = (uint8_t*)dma_alloc(2048));
        nic2k.tx_ring[i].control = 0;
        nic2k.tx_ring[i].next = virt_to_phys(&nic2k.tx_ring[(i + 1) % 32]);
    }
    REG_WRITE(0x28, virt_to_phys(nic2k.tx_ring)); /* TDESC */

    /* Initialize RX Descriptors */
    for (int i = 0; i < 32; i++) {
        nic2k.rx_ring[i].status = DESC_OWN;
        nic2k.rx_ring[i].bufptr = virt_to_phys(nic2k.rx_buffers[i] = (uint8_t*)dma_alloc(2048));
        nic2k.rx_ring[i].control = 2048;
        nic2k.rx_ring[i].next = virt_to_phys(&nic2k.rx_ring[(i + 1) % 32]);
    }
    REG_WRITE(0x20, virt_to_phys(nic2k.rx_ring)); /* RDESC */

    /* Configure Receive */
    REG_WRITE(0x18, 0x0000E00F); /* RCR: Accept Broadcast, Multicast, Promiscuous */

    /* Enable Interrupts */
    REG_WRITE(0x14, ISR_RX_OK | ISR_TX_OK); /* IMR */
    REG_WRITE(0x00, 0x00000003); /* CR: Enable RX/TX */
}


/* Send Ethernet Packet */
void dp83820_send_packet(uint8_t* data, uint16_t length) {
    /* Wait for free TX descriptor */
    while (nic2k.tx_ring[nic2k.tx_cur].status & DESC_OWN);

    /* Copy packet to buffer */
    if (length > 2048) length = 2048;
    memcpy(nic2k.tx_buffers[nic2k.tx_cur], data, length);

    /* Setup descriptor */
    nic2k.tx_ring[nic2k.tx_cur].control = length;
    nic2k.tx_ring[nic2k.tx_cur].status = DESC_OWN | DESC_FS | DESC_LS;

    /* Start transmission */
    REG_WRITE(0x00, 0x00000001); /* TX Poll */
    nic2k.tx_cur = (nic2k.tx_cur + 1) % 32;
}

/* Get MAC Address */
void dp83820_get_mac(uint8_t* mac) {
    memcpy(mac, nic.mac, 6);
}
