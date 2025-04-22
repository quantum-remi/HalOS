#ifndef NE2K_H
#define NE2K_H
#include <stdint.h>

int ne2k_init();
int ne2k_is_present();

void ne2k_get_mac(uint8_t* mac);
void ne2k_send_packet(uint8_t* data, uint16_t length);

#endif /* NE2K_H */