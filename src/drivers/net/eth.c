#include "eth.h"

#include "pci.h"
#include "serial.h"
#include "liballoc.h"
#include "rtl8139.h"

#include "arp.h"

void eth_init()
{
    __asm__ volatile("cli");  // Disable interrupts globally
    rtl8139_init();
    // __asm__ volatile("sti");  // Enable after setup
    
    // Send test ARP request
    uint8_t src_ip[4] = {10, 0, 2, 15};
    uint8_t target_ip[4] = {10, 0, 2, 255};
    rtl8139_send_arp_request(src_ip, target_ip);


}