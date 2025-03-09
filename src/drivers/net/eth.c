#include "eth.h"

#include "pci.h"
#include "serial.h"
#include "vmm.h"
#include "paging.h"
#include "ne2k.h"

void eth_init()
{
    ne2k_init();
    ne2k_handle_interrupt();
    ne2k_send_packet("Hello, World!", 13);

}