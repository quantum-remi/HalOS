#include "tcp.h"
#include "network.h"
#include "serial.h"
#include "liballoc.h"
#include "string.h"
#include "timer.h"
#include "console.h"

#define DEFAULT_WINDOW_SIZE 5840
#define TCP_SYN_RETRANSMIT_TIMEOUT 3000
#define MAX_SYN_RETRIES 5
#define DEFAULT_MSS 1460

#define HTTP_PORT 80
#define HTTP_RESPONSE             \
    "HTTP/1.1 200 OK\r\n"         \
    "Content-Type: text/html\r\n" \
    "Content-Length: 45\r\n"      \
    "Connection: close\r\n"       \
    "\r\n"                        \
    "<html><body><h1>HalOS Works!</h1></body></html>"

static tcp_connection_t *connection_list = NULL;

struct listening_port
{
    uint16_t port;
    struct listening_port *next;
};

typedef struct tcp_timer
{
    tcp_connection_t *conn;
    uint32_t timeout_ms;
    uint32_t start_time;
    uint8_t retries;
    struct tcp_timer *next;
} tcp_timer_t;

static struct listening_port *listen_ports = NULL;
static tcp_timer_t *active_timers = NULL;

static uint32_t generate_secure_initial_seq()
{
    static uint32_t counter = 0;
    uint32_t tick = get_ticks();
    return (tick << 16) | (counter++ & 0xFFFF);
}

static tcp_connection_t *find_connection(uint32_t remote_ip, uint16_t remote_port,
                                         uint32_t local_ip, uint16_t local_port)
{
    tcp_connection_t *curr = connection_list;
    while (curr)
    {
        if (curr->remote_ip == remote_ip && curr->remote_port == remote_port &&
            curr->local_ip == local_ip && curr->local_port == local_port)
        {
            return curr;
        }
        curr = curr->next;
    }
    return NULL;
}

static void add_connection(tcp_connection_t *conn)
{
    conn->next = connection_list;
    connection_list = conn;
}

void cancel_retransmission_timer(tcp_connection_t *conn)
{
    tcp_timer_t **pp = &active_timers;
    while (*pp)
    {
        tcp_timer_t *curr = *pp;
        if (curr->conn == conn)
        {
            *pp = curr->next;
            free(curr);
            return;
        }
        pp = &(*pp)->next;
    }
}

void remove_connection(tcp_connection_t *conn)
{
    cancel_retransmission_timer(conn);

    tcp_connection_t **pp = &connection_list;
    while (*pp)
    {
        if (*pp == conn)
        {
            *pp = conn->next;
            free(conn);
            return;
        }
        pp = &(*pp)->next;
    }
}

void tcp_listen(uint16_t port)
{
    struct listening_port *new_port = malloc(sizeof(struct listening_port));
    new_port->port = port;
    new_port->next = listen_ports;
    listen_ports = new_port;
}

int is_listening_port(uint16_t port)
{
    struct listening_port *curr = listen_ports;
    while (curr)
    {
        if (curr->port == port)
            return 1;
        curr = curr->next;
    }
    return 0;
}

void process_tcp_options(tcp_connection_t *conn, uint8_t *options, uint8_t options_len)
{
    uint8_t *ptr = options;
    uint8_t remaining = options_len;

    while (remaining > 0)
    {
        uint8_t kind = *ptr++;
        if (kind == 0)
            break;
        if (kind == 1)
        {
            remaining--;
            continue;
        }

        if (kind == 2 && remaining >= 4)
        {
            uint8_t length = *ptr++;
            uint16_t mss = (ptr[0] << 8) | ptr[1];
            conn->mss = (mss < 1460) ? mss : 1460;
            ptr += 2;
            remaining -= 4;
        }
        else
        {
            uint8_t length = *ptr++;
            ptr += length - 2;
            remaining -= length;
        }
    }
}

void start_retransmission_timer(tcp_connection_t *conn, uint32_t timeout_ms)
{
    tcp_timer_t *timer = malloc(sizeof(tcp_timer_t));
    timer->conn = conn;
    timer->timeout_ms = timeout_ms;
    timer->start_time = get_ticks();
    timer->retries = 0;
    timer->next = active_timers;
    active_timers = timer;
}

void check_tcp_timers()
{
    tcp_timer_t **pp = &active_timers;
    uint32_t current_time = get_ticks();

    while (*pp)
    {
        tcp_timer_t *curr = *pp;
        uint32_t elapsed = current_time - curr->start_time;

        if (elapsed >= curr->timeout_ms)
        {
            tcp_connection_t *conn = curr->conn;

            if (curr->retries < MAX_SYN_RETRIES)
            {
                if (conn->state == TCP_SYN_SENT || conn->state == TCP_SYN_RECEIVED)
                {
                    tcp_send_segment(conn, TCP_SYN | TCP_ACK, NULL, 0);
                    curr->start_time = current_time;
                    curr->retries++;
                    curr->timeout_ms *= 2;
                    pp = &(*pp)->next;
                }
            }
            else
            {
                serial_printf("TCP: Connection timed out after %d retries\n", MAX_SYN_RETRIES);
                remove_connection(curr->conn);
                *pp = curr->next;
                free(curr);
            }
        }
        else
        {
            pp = &(*pp)->next;
        }
    }
}

void tcp_send_reset(ipv4_header_t *ip, tcp_header_t *tcp)
{
    tcp_connection_t temp_conn = {
        .local_ip = ntohl(ip->dst_ip),
        .remote_ip = ntohl(ip->src_ip),
        .local_port = ntohs(tcp->dest_port),
        .remote_port = ntohs(tcp->src_port),
        .next_seq = ntohl(tcp->ack),
    };

    tcp_send_segment(&temp_conn, TCP_RST, NULL, 0);
}

uint16_t tcp_checksum(ipv4_header_t *ip, tcp_header_t *tcp, uint16_t tcp_len)
{
    struct pseudo_header
    {
        uint32_t src_ip;
        uint32_t dest_ip;
        uint8_t zero;
        uint8_t protocol;
        uint16_t tcp_length;
    } ph;

    ph.src_ip = ip->src_ip;
    ph.dest_ip = ip->dst_ip;
    ph.zero = 0;
    ph.protocol = IP_PROTO_TCP;
    ph.tcp_length = htons(tcp_len);

    uint32_t sum = 0;
    uint16_t *ptr = (uint16_t *)&ph;
    for (size_t i = 0; i < sizeof(ph) / 2; i++)
    {
        sum += ptr[i];
    }

    ptr = (uint16_t *)tcp;
    while (tcp_len > 1)
    {
        sum += *ptr++;
        tcp_len -= 2;
    }
    if (tcp_len == 1)
    {
        sum += *(uint8_t *)ptr;
    }

    sum = (sum >> 16) + (sum & 0xFFFF);
    sum += (sum >> 16);
    return (uint16_t)~sum;
}

void tcp_send_segment(tcp_connection_t *conn, uint8_t flags, uint8_t *data, uint16_t data_len)
{
    uint8_t options[4] = {0};
    uint8_t options_len = 0;

    if (flags & TCP_SYN)
    {
        options[0] = 2;
        options[1] = 4;
        options[2] = (DEFAULT_MSS >> 8) & 0xFF;
        options[3] = DEFAULT_MSS & 0xFF;
        options_len = 4;
    }

    uint16_t header_len = sizeof(tcp_header_t) + options_len;
    uint8_t packet[header_len + data_len];
    tcp_header_t *tcp = (tcp_header_t *)packet;

    tcp->src_port = htons(conn->local_port);
    tcp->dest_port = htons(conn->remote_port);
    tcp->seq = htonl(conn->next_seq);
    tcp->ack = htonl(conn->expected_ack);
    tcp->data_offset = (sizeof(tcp_header_t) / 4) << 4;
    tcp->flags = flags;
    tcp->window = htons(DEFAULT_WINDOW_SIZE);
    tcp->checksum = 0;
    tcp->urgent_ptr = 0;

    if (options_len > 0)
    {
        memcpy(packet + sizeof(tcp_header_t), options, options_len);
        tcp->data_offset = ((header_len / 4) << 4);
    }

    if (data_len > 0)
    {
        memcpy(packet + header_len, data, data_len);
        conn->next_seq += data_len;
    }

    if (flags & (TCP_SYN | TCP_FIN))
    {
        conn->next_seq++;
    }

    ipv4_header_t ip_dummy = {
        .src_ip = htonl(conn->local_ip),
        .dst_ip = htonl(conn->remote_ip)};
    tcp->checksum = tcp_checksum(&ip_dummy, tcp, header_len + data_len);

    net_send_ipv4_packet(conn->remote_ip, IP_PROTO_TCP, packet, header_len + data_len);
}

static void handle_http_request(tcp_connection_t *conn)
{
    tcp_send_segment(conn, TCP_PSH | TCP_ACK,
                     (uint8_t *)HTTP_RESPONSE, strlen(HTTP_RESPONSE));
    tcp_send_segment(conn, TCP_FIN | TCP_ACK, NULL, 0);
    remove_connection(conn);
}

static int is_http_get_request(uint8_t *data, uint16_t len)
{
    return (len >= 4 &&
            data[0] == 'G' &&
            data[1] == 'E' &&
            data[2] == 'T' &&
            data[3] == ' ');
}
static void handle_established_state(tcp_connection_t *conn, tcp_header_t *tcp, uint8_t *data, uint16_t len)
{
    uint32_t seq = ntohl(tcp->seq);
    uint32_t ack = ntohl(tcp->ack);
    uint16_t data_len = len - (tcp->data_offset >> 4) * 4;

    if (seq != conn->expected_ack)
    {
        tcp_send_segment(conn, TCP_ACK, NULL, 0);
        return;
    }

    if (data_len > 0)
    {
        conn->expected_ack = seq + data_len;
        tcp_send_segment(conn, TCP_ACK, NULL, 0);
        serial_printf("TCP: Received data: %.32s%s\n", data, (data_len > 32 ? "..." : ""));
        // if (is_http_get_request(data, data_len))
        // {
        //     serial_printf("HTTP: Received GET request\n");
        //     handle_http_request(conn);
        // }
    }

    // if(data_len > 0 && is_http_get_request(data, data_len)) {
    // }
    // Handle FIN flag
    if (tcp->flags & TCP_FIN)
    {
        conn->expected_ack++;
        conn->state = TCP_CLOSE_WAIT;
        tcp_send_segment(conn, TCP_ACK, NULL, 0);
    }
}

static void handle_syn_received(tcp_connection_t *conn, tcp_header_t *tcp)
{
    if (!(tcp->flags & TCP_ACK))
    {
        return;
    }

    uint32_t ack = ntohl(tcp->ack);
    if (ack == conn->next_seq)
    {
        conn->state = TCP_ESTABLISHED;
        cancel_retransmission_timer(conn);
        serial_printf("TCP: Connection established\n");
    }
}

void tcp_handle_packet(ipv4_header_t *ip, uint8_t *data, uint16_t len)
{
    tcp_header_t *tcp = (tcp_header_t *)data;
    uint8_t flags = tcp->flags;

    uint16_t received_checksum = tcp->checksum;
    tcp->checksum = 0;
    uint16_t calculated_checksum = tcp_checksum(ip, tcp, len);
    if (received_checksum != calculated_checksum)
    {
        serial_printf("TCP: Invalid checksum (got 0x%04x, expected 0x%04x)\n",
                      received_checksum, calculated_checksum);
        return;
    }
    tcp->checksum = received_checksum;

    uint16_t src_port = ntohs(tcp->src_port);
    uint16_t dest_port = ntohs(tcp->dest_port);
    uint32_t seq = ntohl(tcp->seq);
    uint32_t ack = ntohl(tcp->ack);

    if (flags & TCP_RST)
    {
        serial_printf("TCP: RST received from %d.%d.%d.%d:%d\n",
                      (ntohl(ip->src_ip) >> 24) & 0xFF,
                      (ntohl(ip->src_ip) >> 16) & 0xFF,
                      (ntohl(ip->src_ip) >> 8) & 0xFF,
                      ntohl(ip->src_ip) & 0xFF,
                      src_port);
        tcp_connection_t *conn = find_connection(ntohl(ip->src_ip), src_port,
                                                 ntohl(ip->dst_ip), dest_port);
        if (conn)
        {
            serial_printf("TCP: Closing connection due to RST\n");
            remove_connection(conn);
            free(conn);
        }
        return;
    }

    if (flags & TCP_URG)
    {
        uint16_t urgent_ptr = ntohs(tcp->urgent_ptr);
        serial_printf("TCP: URG flag set, urgent pointer: %d\n", urgent_ptr);
    }

    if (flags & TCP_SYN)
    {
        if (len < sizeof(tcp_header_t))
        {
            serial_printf("TCP: Dropped malformed SYN (too short)\n");
            return;
        }

        uint32_t client_seq = ntohl(tcp->seq);
        serial_printf("TCP: SYN from %d.%d.%d.%d:%d (SEQ=%u)\n",
                      (ntohl(ip->src_ip) >> 24) & 0xFF,
                      (ntohl(ip->src_ip) >> 16) & 0xFF,
                      (ntohl(ip->src_ip) >> 8) & 0xFF,
                      ntohl(ip->src_ip) & 0xFF,
                      src_port, client_seq);

        tcp_connection_t *existing = find_connection(
            ntohl(ip->src_ip), src_port,
            ntohl(ip->dst_ip), dest_port);
        if (existing)
        {
            if (existing->state == TCP_SYN_RECEIVED)
            {
                serial_printf("TCP: Retransmitted SYN detected, resending SYN-ACK\n");
                tcp_send_segment(existing, TCP_SYN | TCP_ACK, NULL, 0);
                return;
            }
            else
            {
                serial_printf("TCP: Unexpected SYN on existing connection (state=%d)\n",
                              existing->state);
                tcp_send_reset(ip, tcp);
                return;
            }
        }

        if (!is_listening_port(dest_port))
        {
            serial_printf("TCP: SYN to non-listening port %d\n", dest_port);
            tcp_send_reset(ip, tcp);
            return;
        }

        tcp_connection_t *conn = malloc(sizeof(tcp_connection_t));
        if (!conn)
        {
            serial_printf("TCP: Memory error, dropping SYN\n");
            return;
        }

        conn->local_ip = ntohl(ip->dst_ip);
        conn->remote_ip = ntohl(ip->src_ip);
        conn->local_port = dest_port;
        conn->remote_port = src_port;
        conn->next_seq = generate_secure_initial_seq();
        conn->expected_ack = client_seq + 1;
        conn->state = TCP_SYN_RECEIVED;

        uint8_t options_len = (tcp->data_offset >> 4) * 4 - sizeof(tcp_header_t);
        if (options_len > 0)
        {
            process_tcp_options(conn, (uint8_t *)(tcp + 1), options_len);
        }

        add_connection(conn);

        serial_printf("TCP: Sending SYN-ACK (SEQ=%u, ACK=%u)\n",
                      conn->next_seq, conn->expected_ack);
        tcp_send_segment(conn, TCP_SYN | TCP_ACK, NULL, 0);
        conn->next_seq++;
        start_retransmission_timer(conn, TCP_SYN_RETRANSMIT_TIMEOUT);
    }

    tcp_connection_t *conn = find_connection(ntohl(ip->src_ip), src_port,
                                             ntohl(ip->dst_ip), dest_port);

    if (conn)
    {
        switch (conn->state)
        {
        case TCP_SYN_RECEIVED:
            handle_syn_received(conn, tcp);
            break;

        case TCP_ESTABLISHED:
            handle_established_state(conn, tcp, data, len);
            break;

        default:
            break;
        }
    }

    if (flags & TCP_ACK && conn && conn->state == TCP_SYN_RECEIVED)
    {
        if (ack == conn->next_seq)
        {
            conn->state = TCP_ESTABLISHED;
            serial_printf("TCP: Connection established (SEQ=%u ACK=%u)\n",
                          conn->next_seq, conn->expected_ack);
            cancel_retransmission_timer(conn);
        }
        else
        {
            serial_printf("TCP: Invalid ACK %u (expected %u)\n",
                          ack, conn->next_seq);
            tcp_send_reset(ip, tcp);
            remove_connection(conn);
        }
    }

    if (flags & TCP_ACK && !(flags & TCP_SYN))
    {
        tcp_connection_t *conn = find_connection(ntohl(ip->src_ip), src_port,
                                                 ntohl(ip->dst_ip), dest_port);

        if (conn)
        {
            if (conn->state == TCP_SYN_SENT)
            {
                if (ack == conn->next_seq)
                {
                    conn->state = TCP_ESTABLISHED;
                    conn->expected_ack = seq + 1;
                    cancel_retransmission_timer(conn);
                    return;
                }
                else
                {
                    tcp_send_segment(conn, TCP_RST, NULL, 0);
                    remove_connection(conn);
                }
            }
        }
    }

    if (flags & TCP_PSH)
    {
        tcp_connection_t *conn = find_connection(ntohl(ip->src_ip), src_port,
                                                 ntohl(ip->dst_ip), dest_port);
        uint8_t *payload = data + (tcp->data_offset >> 4) * 4;
        uint16_t payload_len = len - (tcp->data_offset >> 4) * 4;

        console_printf(payload);
        if (is_http_get_request(payload, payload_len))
        {
            serial_printf("HTTP: Received GET request\n");
            handle_http_request(conn);
        }

        serial_printf("TCP: Received %d bytes of data\n", payload_len);
    }

    if (flags & TCP_FIN)
    {
        tcp_connection_t *conn = find_connection(ntohl(ip->src_ip), src_port,
                                                 ntohl(ip->dst_ip), dest_port);
        if (conn)
        {
            serial_printf("TCP: FIN received, closing connection\n");
            tcp_send_segment(conn, TCP_ACK, NULL, 0);
            remove_connection(conn);
            free(conn);
        }
    }
}