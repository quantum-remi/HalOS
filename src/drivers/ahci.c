#include "ahci.h"

static inline uint32_t mmio_read32(volatile void *addr)
{
    return *(volatile uint32_t *)addr;
}

static inline void mmio_write32(volatile void *addr, uint32_t value)
{
    *(volatile uint32_t *)addr = value;
}

static pci_dev_t find_ahci_controller()
{
    pci_dev_t dev = {0};

    // Scan all PCI buses
    for (uint16_t bus = 0; bus < 256; bus++)
    {
        for (uint8_t device = 0; device < DEVICE_PER_BUS; device++)
        {
            for (uint8_t function = 0; function < FUNCTION_PER_DEVICE; function++)
            {
                dev.bus_num = bus;
                dev.device_num = device;
                dev.function_num = function;

                if (pci_read(dev, PCI_VENDOR_ID) == PCI_NONE)
                    continue;

                // Read class/subclass/progif
                uint32_t class_reg = pci_read(dev, PCI_CLASS);
                uint8_t class_code = (class_reg >> 16) & 0xFF;
                uint8_t subclass_code = (class_reg >> 8) & 0xFF;
                uint8_t prog_if = class_reg & 0xFF;

                // Check for AHCI controller
                if (class_code == PCI_CLASS_STORAGE &&
                    subclass_code == PCI_SUBCLASS_SATA &&
                    prog_if == PCI_PROGIF_AHCI)
                {
                    return dev;
                }
            }
        }
    }
    return dev_zero;
}

int ahci_init(AHCI_DRIVER *drv)
{
    // Initialize PCI subsystem
    pci_init();

    // Find AHCI controller
    pci_dev_t ahci_dev = find_ahci_controller();
    if (!ahci_dev.bits)
    {
        // Handle error
        return -1;
    }

    // Get ABAR (BAR5)
    uint32_t bar_low = pci_read(ahci_dev, PCI_BAR5);
    uint32_t bar_high = pci_read(ahci_dev, PCI_BAR5 + 4);
    uint64_t abar = ((uint64_t)bar_high << 32) | bar_low;

    // Mask out flags
    abar &= ~0xFFF;

    // Continue with original AHCI initialization
    drv->mem = (HBA_MEM *)(uintptr_t)abar;

    // Reset controller
    mmio_write32(&drv->mem->ghc, mmio_read32(&drv->mem->ghc) | 0x80000000);
    while (mmio_read32(&drv->mem->ghc) & 0x80000000)
        ;

    // Enable AHCI mode
    mmio_write32(&drv->mem->ghc, mmio_read32(&drv->mem->ghc) | 0x1);

    // Get implemented ports
    uint32_t pi = mmio_read32(&drv->mem->pi);
    drv->num_ports = 0;

    for (int i = 0; i < AHCI_MAX_PORTS; i++)
    {
        if (pi & (1 << i))
        {
            HBA_PORT *port = &drv->mem->ports[i];
            uint32_t ssts = mmio_read32(&port->ssts);

            if ((ssts & 0xF) == 3)
            { // Device detected and PHY active
                if (ahci_port_init(drv, i) == 0)
                {
                    drv->num_ports++;
                }
            }
        }
    }

    return drv->num_ports > 0 ? 0 : -1;
}

int ahci_port_init(AHCI_DRIVER *drv, int port_num)
{
    AHCI_PORT *port = &drv->ports[port_num];
    HBA_PORT *regs = &drv->mem->ports[port_num];

    // Allocate command list (1K aligned)
    port->cl = dma_alloc(1024, &port->cl_phys);
    if (!port->cl)
        return -1;

    // Allocate FIS (256B aligned)
    port->fis = dma_alloc(256, &port->fis_phys);
    if (!port->fis)
        return -1;

    // Allocate command tables (128B aligned)
    port->ct = dma_alloc(sizeof(HBA_CMD_TABLE) * AHCI_CMD_SLOTS, &port->ct_phys);
    if (!port->ct)
        return -1;

    // Configure port registers
    mmio_write32(&regs->clb, port->cl_phys);
    mmio_write32(&regs->clbu, 0);
    mmio_write32(&regs->fb, port->fis_phys);
    mmio_write32(&regs->fbu, 0);

    // Enable FIS receive
    uint32_t cmd = mmio_read32(&regs->cmd);
    cmd |= 0x10; // FRE
    mmio_write32(&regs->cmd, cmd);

    // Start port
    cmd |= 0x1; // ST
    mmio_write32(&regs->cmd, cmd);

    return 0;
}

int ahci_read(AHCI_DRIVER *drv, int port_num, uint64_t lba, void *buffer, size_t count)
{
    AHCI_PORT *port = &drv->ports[port_num];
    HBA_PORT *regs = &drv->mem->ports[port_num];

    // Find free command slot
    port->slot = 0;
    while (port->slot < AHCI_CMD_SLOTS)
    {
        if (!(mmio_read32(&regs->ci) & (1 << port->slot)))
            break;
        port->slot++;
    }
    if (port->slot >= AHCI_CMD_SLOTS)
        return -1; // No free slots

    HBA_CMD_HEADER *cmd_header = (HBA_CMD_HEADER *)port->cl + port->slot;
    HBA_CMD_TABLE *cmd_table = &port->ct[port->slot];

    // Setup command FIS
    FIS_REG_H2D *fis = (FIS_REG_H2D *)cmd_table->cfis;
    *fis = (FIS_REG_H2D){
        .fis_type = FIS_TYPE_REG_H2D,
        .c = 1,
        .command = 0x25, // READ DMA EXT
        .lba0 = (lba >> 0) & 0xFF,
        .lba1 = (lba >> 8) & 0xFF,
        .lba2 = (lba >> 16) & 0xFF,
        .lba3 = (lba >> 24) & 0xFF,
        .lba4 = (lba >> 32) & 0xFF,
        .lba5 = (lba >> 40) & 0xFF,
        .device = 0x40, // LBA mode
        .countl = (count >> 0) & 0xFF,
        .counth = (count >> 8) & 0xFF,
    };

    // Setup PRDT
    cmd_header->prdtl = 1;
    cmd_table->prdt[0] = (HBA_PRDT_ENTRY){
        .dba = (uint32_t)(uintptr_t)buffer,
        .dbc = (count * 512) - 1,
        .i = 1};

    // Point command header to table
    cmd_header->ctba = port->ct_phys + port->slot * sizeof(HBA_CMD_TABLE);
    cmd_header->prdbc = 0;

    // Issue command
    mmio_write32(&regs->ci, 1 << port->slot);

    // Wait for completion
    while (mmio_read32(&regs->ci) & (1 << port->slot))
        ;

    // Check for errors
    if (mmio_read32(&regs->is) & 0x1)
    {
        mmio_write32(&regs->is, 0x1); // Clear error
        return -1;
    }

    return 0;
}