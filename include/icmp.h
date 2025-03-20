#ifndef ICMP_H
#define ICMP_H

#include "ipv4.h"

void icmp_handle_packet(ipv4_header_t* ip, uint8_t* payload, uint16_t len);
void icmp_send_echo_request(uint32_t dst_ip);

#endif