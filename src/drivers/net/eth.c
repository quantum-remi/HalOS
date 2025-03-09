#include "eth.h"

#include "pci.h"
#include "serial.h"
#include "vmm.h"
#include "paging.h"

extern pci_dev_t dev_zero;
extern pci_dev_t nic;
extern struct e1000_dev_t e1000_dev;

void eth_init()
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