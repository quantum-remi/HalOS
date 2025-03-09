#include "eth.h"

#include "pci.h"
#include "serial.h"
#include "vmm.h"
#include "paging.h"
#include "e1000.h"

void eth_init()
{
    e1000_init();

}