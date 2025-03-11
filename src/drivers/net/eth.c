#include "eth.h"

#include "pci.h"
#include "serial.h"
#include "liballoc.h"
#include "rtl8139.h"

#include "arp.h"

void eth_init()
{

    rtl8139_init();

    uint8_t src_ip[4] = {10, 0, 2, 15};       // Configure these
    uint8_t target_ip[4] = {10, 0, 2, 255};    // Broadcast address
    rtl8139_send_arp_request(src_ip, target_ip);


}