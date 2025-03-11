#include "eth.h"

#include "pci.h"
#include "serial.h"
#include "liballoc.h"
#include "rtl8139.h"


void eth_init()
{

    rtl8139_init();

}