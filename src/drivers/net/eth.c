#include "eth.h"

#include "pci.h"
#include "serial.h"
#include "vmm.h"
#include "paging.h"
#include "ne2k.h"

void eth_init()
{

    pci_dev_t dev = pci_get_device(NE2K_VENDOR_ID, NE2K_DEVICE_ID, 
        PCI_CLASS_NETWORK << 8 | PCI_SUBCLASS_ETHERNET);
    
    serial_printf("NE2000 found %d\n", dev);
    if(ne2k_init(dev)) {
        ne2k_device nic;
        nic.pci_dev = dev;
        nic.iobase = pci_read(dev, PCI_BAR0) & 0xFFFC;
        
        // Read MAC into struct
        for(int i = 0; i < 6; i++) {
            nic.mac[i] = inportb(nic.iobase + NE2K_DATA + i);
        }
        
        test_ne2k(&nic);  // Send test packet
    }

}