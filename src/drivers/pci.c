#include "pci.h"

uint32_t pci_size_map[100];
pci_dev_t dev_zero = {0};

pci_class_subclass_t pci_class_subclass_table[] = {
    {0x01, 0x01, "IDE Controller"},
    {0x01, 0x02, "Floppy Disk Controller"},
    {0x01, 0x03, "IPI Bus Controller"},
    {0x01, 0x04, "RAID Controller"},
    {0x01, 0x05, "ATA Controller"},
    {0x01, 0x06, "Serial ATA Controller"},
    {0x01, 0x07, "Serial Attached SCSI Controller"},
    {0x01, 0x08, "NVMe Controller"},
    {0x02, 0x00, "Ethernet Controller"},
    {0x02, 0x01, "Token Ring Controller"},
    {0x02, 0x02, "FDDI Controller"},
    {0x02, 0x03, "ATM Controller"},
    {0x02, 0x04, "ISDN Controller"},
    {0x02, 0x05, "WorldFip Controller"},
    {0x02, 0x06, "PICMG 2.14 Multi Computing"},
    {0x03, 0x00, "VGA Compatible Controller"},
    {0x03, 0x01, "XGA Controller"},
    {0x03, 0x02, "3D Controller"},
    {0x04, 0x00, "Multimedia Video Controller"},
    {0x04, 0x01, "Multimedia Audio Controller"},
    {0x04, 0x02, "Computer Telephony Device"},
    {0x04, 0x03, "Audio Device"},
    {0x05, 0x00, "RAM Controller"},
    {0x05, 0x01, "Flash Controller"},
    {0x06, 0x00, "Host Bridge"},
    {0x06, 0x01, "ISA Bridge"},
    {0x06, 0x02, "EISA Bridge"},
    {0x06, 0x03, "MCA Bridge"},
    {0x06, 0x04, "PCI-to-PCI Bridge"},
    {0x06, 0x05, "PCMCIA Bridge"},
    {0x06, 0x06, "NuBus Bridge"},
    {0x06, 0x07, "Cardbus Bridge"},
    {0x06, 0x08, "RACEway Bridge"},
    {0x06, 0x09, "PCI-to-PCI Bridge "},
    {0x06, 0x0A, "InfiniBand-to-PCI Host Bridge"},
    {0x06, 0x80, "Other Bridge"},
    {0x07, 0x00, "Serial Controller"},
    {0x07, 0x01, "Parallel Controller"},
    {0x07, 0x02, "Multiport Serial Controller"},
    {0x07, 0x03, "Modem"},
    {0x07, 0x04, "IEEE 488.1/2 (GPIB) Controller"},
    {0x07, 0x05, "Smart card"},
    {0x08, 0x00, "PIC"},
    {0x08, 0x01, "DMA Controller"},
    {0x08, 0x02, "Timer"},
    {0x08, 0x03, "RTC Controller"},
    {0x08, 0x04, "PCI Hot-Plug Controller"},
    {0x08, 0x05, "SD Host controller"},
    {0x08, 0x06, "IOMMU"},
    {0x09, 0x00, "Keyboard Controller"},
    {0x09, 0x01, "Digitizer Pen"},
    {0x09, 0x02, "Mouse Controller"},
    {0x09, 0x03, "Scanner Controller"},
    {0x09, 0x04, "Game Port Controller"},
    {0x0A, 0x00, "Docking Station Generic"},
    {0x0B, 0x00, "386"},
    {0x0B, 0x01, "486"},
    {0x0B, 0x02, "Pentium"},
    {0x0B, 0x03, "Pentium Pro"},
    {0x0B, 0x10, "Alpha"},
    {0x0B, 0x20, "PowerPC"},
    {0x0B, 0x30, "MIPS"},
    {0x0B, 0x40, "Co-Processor"},
    {0x0C, 0x00, "FireWire (IEEE 1394) Controller "},
    {0x0C, 0x01, "ACCESS Bus Controller"},
    {0x0C, 0x02, "SSA"},
    {0x0C, 0x03, "USB Controller"},
    {0x0C, 0x04, "Fibre Channel"},
    {0x0C, 0x05, "SMBus Controller"},
    {0x0C, 0x06, "Infiniband Controller"},
    {0x0C, 0x07, "IPMI Controller"},
    {0x0C, 0x08, "SERCOS Interface (IEC 61491)"},
    {0x0C, 0x09, "CANbus Controller"},
    // Add more entries for other class and subclass codes as needed
};

uint32_t pci_read(pci_dev_t dev, uint32_t field)
{
    dev.field = (field & 0xFC) >> 2;
    dev.enable = 1;
    outportl(PCI_CONFIG_ADDRESS, dev.bits);

    size_t size = pci_size_map[field];
    if (size == 1)
    {
        uint8_t t = inportb(PCI_CONFIG_DATA + (field & 3));
        return t;
    }
    else if (size == 2)
    {
        uint16_t t = inports(PCI_CONFIG_DATA + (field & 2));
        return t;
    }
    else if (size == 4)
    {
        uint32_t t = inportl(PCI_CONFIG_DATA);
        return t;
    }
    else
    {
        return 0xFFFFFFFF;
    }
}

void pci_write(pci_dev_t dev, uint32_t field, uint32_t value)
{
    dev.field = (field & 0xFC) >> 2;
    dev.enable = 1;
    outportl(PCI_CONFIG_ADDRESS, dev.bits);
    outportl(PCI_CONFIG_DATA, value);
}

uint32_t get_device_type(pci_dev_t dev)
{
    uint32_t t = pci_read(dev, PCI_CLASS) << 8;
    return t | pci_read(dev, PCI_SUBCLASS);
}

uint32_t get_secondary_bus(pci_dev_t dev)
{
    return pci_read(dev, PCI_SECONDARY_BUS);
}

uint32_t pci_reach_end(pci_dev_t dev)
{
    uint32_t t = pci_read(dev, PCI_HEADER_TYPE);
    return !t;
}

pci_dev_t pci_scan_function(uint16_t vendor_id, uint16_t device_id, uint32_t bus, uint32_t device, uint32_t function, int device_type)
{
    pci_dev_t dev = {0};
    dev.bus = bus;
    dev.device = device;
    dev.function = function;

    // Check if this is a bridge first
    if (get_device_type(dev) == PCI_TYPE_BRIDGE)
    {
        uint32_t secondary_bus = get_secondary_bus(dev);
        pci_scan_bus(vendor_id, device_id, secondary_bus, device_type);
    }

    // Check device match
    if (device_type == -1 || device_type == get_device_type(dev))
    {
        uint32_t dev_id = pci_read(dev, PCI_DEVICE_ID);
        uint32_t vend_id = pci_read(dev, PCI_VENDOR_ID);
        if (vend_id == vendor_id && dev_id == device_id)
        {
            return dev;
        }
    }

    return dev_zero;
}

pci_dev_t pci_scan_device(uint16_t vendor_id, uint16_t device_id, uint32_t bus, uint32_t device, int device_type)
{
    pci_dev_t dev = {0};
    dev.bus = bus;
    dev.device = device;

    if (pci_read(dev, PCI_VENDOR_ID) == PCI_NONE)
    {
        return dev_zero;
    }

    for (int function = 0; function < FUNCTION_PER_DEVICE; function++)
    {
        dev.function = function;

        if (pci_read(dev, PCI_VENDOR_ID) != PCI_NONE)
        {
            pci_dev_t t = pci_scan_function(vendor_id, device_id, bus, device, function, device_type);
            if (t.bits)
            {
                return t;
            }
        }
    }

    return dev_zero;
}

pci_dev_t pci_scan_bus(uint16_t vendor_id, uint16_t device_id, uint32_t bus, int device_type) {
    for (int device = 0; device < DEVICE_PER_BUS; device++) {
        pci_dev_t t = pci_scan_device(vendor_id, device_id, bus, device, device_type);
        if (t.bits) {
            // Recursively scan bridges
            if (get_device_type(t) == PCI_TYPE_BRIDGE) {
                uint32_t secondary = get_secondary_bus(t);
                pci_scan_bus(vendor_id, device_id, secondary, device_type);
            }
            return t;
        }
    }
    return dev_zero;
}

pci_dev_t pci_get_device(uint16_t vendor_id, uint16_t device_id, int device_type)
{
    // Check main bus hierarchy first
    pci_dev_t t = pci_scan_bus(vendor_id, device_id, 0, device_type);
    if (t.bits)
        return t;

    // Check other buses if present
    for (int bus = 1; bus < 256; bus++)
    {
        t = pci_scan_bus(vendor_id, device_id, bus, device_type);
        if (t.bits)
            return t;
    }

    return dev_zero;
}

void pci_init()
{
    // Init size map
    pci_size_map[PCI_VENDOR_ID] = 2;
    pci_size_map[PCI_DEVICE_ID] = 2;
    pci_size_map[PCI_COMMAND] = 2;
    pci_size_map[PCI_STATUS] = 2;
    pci_size_map[PCI_SUBCLASS] = 1;
    pci_size_map[PCI_CLASS] = 1;
    pci_size_map[PCI_CACHE_LINE_SIZE] = 1;
    pci_size_map[PCI_LATENCY_TIMER] = 1;
    pci_size_map[PCI_HEADER_TYPE] = 1;
    pci_size_map[PCI_BIST] = 1;
    pci_size_map[PCI_BAR0] = 4;
    pci_size_map[PCI_BAR1] = 4;
    pci_size_map[PCI_BAR2] = 4;
    pci_size_map[PCI_BAR3] = 4;
    pci_size_map[PCI_BAR4] = 4;
    pci_size_map[PCI_BAR5] = 4;
    pci_size_map[PCI_INTERRUPT_LINE] = 1;
    pci_size_map[PCI_SECONDARY_BUS] = 1;
}
void pci_print_devices()
{
    pci_dev_t dev = {0};
    if (get_device_type(dev) == PCI_TYPE_BRIDGE) {
        uint32_t sec_bus = get_secondary_bus(dev);
        pci_scan_bus(0, 0, sec_bus, -1); // Print devices on secondary bus
    }

    for (uint16_t bus = 0; bus < 256; bus++)
    {
        for (uint8_t device = 0; device < DEVICE_PER_BUS; device++)
        {
            for (uint8_t function = 0; function < FUNCTION_PER_DEVICE; function++)
            {
                dev.bus = bus;
                dev.device = device;
                dev.function = function;
                if (pci_read(dev, PCI_VENDOR_ID) != PCI_NONE)
                {
                    uint32_t class_code = get_device_type(dev);
                    uint32_t subclass_code = pci_read(dev, PCI_SUBCLASS);
                    console_printf("PCI Device: %x:%x:%x, Class: %x, Subclass: %x (%s)\n",
                                   dev.bus, dev.device, dev.function, class_code, subclass_code,
                                   get_subclass_name(class_code, subclass_code));
                }
            }
        }
    }
}

const char *get_subclass_name(uint32_t class_code, uint32_t subclass_code)
{
    for (size_t i = 0; i < sizeof(pci_class_subclass_table) / sizeof(pci_class_subclass_table[0]); i++)
    {
        if (pci_class_subclass_table[i].class_code == (class_code >> 8) && pci_class_subclass_table[i].subclass_code == (subclass_code & 0xFF))
        {
            return pci_class_subclass_table[i].name;
        }
    }
    return "Unknown";
}