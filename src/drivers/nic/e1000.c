#include "e1000.h"

#include "pci.h"
#include "serial.h"
#include "vmm.h"
#include "paging.h"


extern pci_dev_t dev_zero;
extern pci_dev_t nic;
extern struct e1000_dev_t e1000_dev;


e1000_dev_t e1000_probe() {
    e1000_dev_t dev = {0};
    const uint16_t e1000_ids[] = {E1000_DEV, 0x1004, 0x100F, 0x1533, 0x1536};
    
    for (int i = 0; i < sizeof(e1000_ids)/sizeof(e1000_ids[0]); i++) {
        dev.pci_info = pci_get_device(PCI_VENDOR_INTEL, e1000_ids[i], 
                                     (PCI_CLASS_NETWORK << 8) | PCI_SUBCLASS_ETHERNET);
        if (dev.pci_info.bits) break;
    }
    
    if (dev.pci_info.bits) {
        uint32_t bar0 = pci_read(dev.pci_info, PCI_BAR0);
        dev.mmio_base = bar0 & ~0xF;
        dev.device_id = pci_read(dev.pci_info, PCI_DEVICE_ID);
    }
    uint8_t header_type = pci_read(dev.pci_info, PCI_HEADER_TYPE) & 0x7F;
    if (header_type != 0x00) {  // Standard endpoint header
        serial_printf("Invalid PCI header type: %x\n", header_type);
    }
    return dev;
}

void e1000_init()
{
    e1000_dev_t nic = e1000_probe();
    if (nic.pci_info.bits) {  // Now correct via e1000_dev_t structure
        void* mmio = vmm_map_mmio(nic.mmio_base, 0x20000, PAGE_UNCACHED);
        serial_printf("E1000 found at %x\n", nic.mmio_base);
        // Access E1000 registers through mmio
    }
    else {
        serial_printf("No supported NIC found\n");
    }
}
