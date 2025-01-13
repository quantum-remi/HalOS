#include "pci.h"


uint32 pci_read(uint8 bus, uint8 slot, uint8 func, uint8 offset) 
{
    uint32 address;
    uint32 lbus = (uint32)bus;
    uint32 lslot = (uint32)slot;
    uint32 lfunc = (uint32)func;
    uint32 tmp = 0;

    address = (uint32)((lbus << 16) | (lslot << 11) |
                      (lfunc << 8) | (offset & 0xFC) | ((uint32)0x80000000));
    
    outportl(PCI_CONFIG_ADDRESS, address);

    tmp = inportl(PCI_CONFIG_DATA);
    return tmp;
}

void pci_show()
{
    for (uint32 device = 0; device < 32; device++) 
    {
        for (uint32 function = 0; function < 8; function++)
        {
            uint32 data = pci_read(0, device, function, 0);
            uint32 vendor_id = (uint16)(data & 0xFFFF);
            uint32 device_id = (uint16)(data >> 16);

            if (vendor_id != 0xFFFF) 
            {
                // Device was found
                serial_printf("PCI Device found at %d:%d\n", device, function);
                console_printf("PCI Device found at %d:%d\n", device, function);
                console_printf("Vendor ID: %x\n", vendor_id);
                console_printf("Device ID: %x\n", device_id);
                
            }
        }
    }
}