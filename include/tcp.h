#ifndef TCP_H
#define TCP_H

#include "ipv4.h"

#define IP_PROTO_TCP 6

// TCP Flags
#define TCP_FIN 0x01
#define TCP_SYN 0x02
#define TCP_RST 0x04
#define TCP_PSH 0x08
#define TCP_ACK 0x10
#define TCP_URG 0x20

typedef enum
{
    TCP_CLOSED,
    TCP_LISTEN,
    TCP_SYN_SENT,
    TCP_SYN_RECEIVED,
    TCP_ESTABLISHED,
    TCP_WAIT_FOR_ACK,
    TCP_FIN_WAIT_1,
    TCP_FIN_WAIT_2,
    TCP_CLOSE_WAIT,
    TCP_CLOSING,
    TCP_LAST_ACK,
    TCP_TIME_WAIT
} tcp_state_t;

typedef struct retransmit_entry
{
    uint32_t seq;
    uint16_t length;
    uint8_t *data;
    uint32_t start_time;
    uint8_t flags;
    uint8_t retries; // Track retry attempts
    struct retransmit_entry *next;
} retransmit_entry_t;

#pragma pack(push, 1)
typedef struct
{
    uint16_t src_port;
    uint16_t dest_port;
    uint32_t seq;
    uint32_t ack;
    uint8_t data_offset; // 4 bits offset, 4 bits reserved
    uint8_t flags;
    uint16_t window;
    uint16_t checksum;
    uint16_t urgent_ptr;
} tcp_header_t;
#pragma pack(pop)

// TCP Control Block (TCB)
typedef struct tcp_connection
{
    uint32_t local_ip;
    uint32_t remote_ip;
    uint16_t local_port;
    uint16_t remote_port;
    uint16_t mss;
    uint32_t next_seq;     // Next sequence number to send
    uint32_t expected_ack; // Last ACK received
    uint8_t dup_ack_count; // Track duplicate ACKs
    tcp_state_t state;
    retransmit_entry_t *retransmit_queue;
    uint8_t recv_buffer[4096]; // Buffer for incoming data
    uint16_t recv_buffer_len;  // Current data length in buffer
    uint8_t send_buffer[4096]; // Buffer for outgoing data
    uint16_t send_buffer_len;
    uint16_t window_size;
    uint32_t last_ack;
    struct tcp_connection *next;
} tcp_connection_t;

void tcp_handle_packet(ipv4_header_t *ip, uint8_t *data, uint16_t len);
uint16_t tcp_checksum(ipv4_header_t *ip, tcp_header_t *tcp, uint16_t tcp_len);
void tcp_send_segment(tcp_connection_t *conn, uint8_t flags, uint8_t *data, uint16_t data_len);
void tcp_listen(uint16_t port);
void check_tcp_timers(void);
tcp_connection_t *tcp_connect(uint32_t remote_ip, uint16_t remote_port);
void remove_connection(tcp_connection_t *conn);

#endif