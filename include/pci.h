#ifndef PCI_H
#define PCI_H

#include "console.h"
#include "types.h"
#include "io.h"
#include "string.h"
#include "serial.h"

#define PCI_CONFIG_ADDRESS 0xCF8
#define PCI_CONFIG_DATA 0xCFC

uint32 pci_read(uint8 bus, uint8 slot, uint8 func, uint8 offset);
void pci_show();

#endif // PCI_H