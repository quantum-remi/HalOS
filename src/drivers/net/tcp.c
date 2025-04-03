#include "tcp.h"
#include "network.h"
#include "serial.h"
#include "liballoc.h"
#include "string.h"
#include "timer.h"
#include "console.h"

#define DEFAULT_WINDOW_SIZE 5840
#define TCP_SYN_RETRANSMIT_TIMEOUT 3000
#define TCP_DATA_RETRANSMIT_TIMEOUT 3000
#define MAX_SYN_RETRIES 5
#define DEFAULT_MSS 1460

#define HTTP_PORT 8080
#define HTTP_RESPONSE             \
    "HTTP/1.1 200 OK\r\n"         \
    "Content-Type: text/html\r\n" \
    "Content-Length: 348\r\n"     \
    "Connection: close\r\n"       \
    "\r\n"                        \
    "<html><head><style>\r\n"     \
    "body { font-family: Arial; background: #f0f0f0; margin: 40px; }\r\n" \
    "h1 { color: #333; text-align: center; }\r\n" \
    "p { color: #666; text-align: center; }\r\n" \
    "</style></head>\r\n"         \
    "<body>\r\n"                  \
    "<h1>Welcome to HalOS!</h1>\r\n" \
    "<p>This is a minimal operating system with TCP/IP networking.</p>\r\n" \
    "<p>Server is up and running successfully.</p>\r\n" \
    "</body></html>"

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

    // Free retransmission queue
    retransmit_entry_t *entry = conn->retransmit_queue;
    while (entry)
    {
        retransmit_entry_t *next = entry->next;
        free(entry->data);
        free(entry);
        entry = next;
    }

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
    if (!new_port)
    {
        serial_printf("TCP: Failed to allocate memory for listening port\n");
        return;
    }
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
        remaining--;
        if (kind == 0)
            break;
        if (kind == 1)
        {
            // NOP option, no length
            continue;
        }

        if (remaining == 0)
            break;

        uint8_t length = *ptr++;
        remaining--;

        if (length < 2)
            break;

        if (kind == 2 && length == 4 && remaining >= 2)
        {
            // MSS option
            uint16_t mss = (ptr[0] << 8) | ptr[1];
            conn->mss = (mss < DEFAULT_MSS) ? mss : DEFAULT_MSS;
            ptr += 2;
            remaining -= 2;
        }
        else
        {
            // Skip unknown options
            uint8_t opt_len = length - 2;
            if (opt_len > remaining)
                break;
            ptr += opt_len;
            remaining -= opt_len;
        }
    }
}

void start_retransmission_timer(tcp_connection_t *conn, uint32_t timeout_ms)
{
    tcp_timer_t *timer = malloc(sizeof(tcp_timer_t));
    if (!timer)
    {
        serial_printf("TCP: Failed to allocate memory for retransmission timer\n");
        return;
    }
    timer->conn = conn;
    timer->timeout_ms = timeout_ms;
    timer->start_time = get_ticks();
    timer->retries = 0;
    timer->next = active_timers;
    active_timers = timer;
}

void check_tcp_timers(void)
{
    tcp_timer_t **pp = &active_timers;
    uint32_t current_time = get_ticks();

    while (*pp)
    {
        tcp_timer_t *timer = *pp;
        if (current_time - timer->start_time >= timer->timeout_ms)
        {
            if (timer->retries < MAX_SYN_RETRIES)
            {
                // Retransmit the segment
                serial_printf("TCP: Retransmitting SEQ=%u\n", timer->conn->next_seq);
                tcp_send_segment(timer->conn, TCP_ACK, NULL, 0);
                timer->start_time = current_time;
                timer->timeout_ms *= 2;
                timer->retries++;
                pp = &(*pp)->next;
            }
            else
            {
                // Max retries reached: abort connection
                serial_printf("TCP: Retransmission failed, closing connection\n");
                remove_connection(timer->conn);
                tcp_timer_t *to_free = *pp;
                *pp = to_free->next;
                free(to_free);
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
        sum += (uint16_t)(*(uint8_t *)ptr) << 8;
    }

    sum = (sum >> 16) + (sum & 0xFFFF);
    sum += (sum >> 16);
    return (uint16_t)~sum;
}

void tcp_send_segment(tcp_connection_t *conn, uint8_t flags, uint8_t *data, uint16_t data_len)
{
    uint8_t options[4] = {0};
    uint8_t options_len = 0;
    uint32_t original_seq = conn->next_seq;

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
    tcp->seq = htonl(original_seq);
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

    // Compute checksum
    ipv4_header_t ip_dummy = {
        .src_ip = htonl(conn->local_ip),
        .dst_ip = htonl(conn->remote_ip)};
    tcp->checksum = tcp_checksum(&ip_dummy, tcp, header_len + data_len);

    // Send the packet
    net_send_ipv4_packet(conn->remote_ip, IP_PROTO_TCP, packet, header_len + data_len);

    // Add to retransmit queue if needed
    if (flags & (TCP_SYN | TCP_FIN) || data_len > 0)
    {
        retransmit_entry_t *entry = malloc(sizeof(retransmit_entry_t));
        if (!entry)
        {
            serial_printf("TCP: Retransmit entry allocation failed\n");
            return;
        }

        entry->seq = original_seq;
        entry->length = data_len;
        entry->data = NULL;

        if (data_len > 0)
        {
            entry->data = malloc(data_len);
            if (!entry->data)
            {
                free(entry);
                serial_printf("TCP: Data buffer allocation failed\n");
                return;
            }
            memcpy(entry->data, data, data_len);
        }

        entry->timeout_ms = TCP_DATA_RETRANSMIT_TIMEOUT;
        entry->retries = 0;
        entry->start_time = get_ticks();
        entry->next = conn->retransmit_queue;
        conn->retransmit_queue = entry;
    }
}

static void handle_http_request(tcp_connection_t *conn)
{
    serial_printf("HTTP: Handling GET request\n");
    tcp_send_segment(conn, TCP_PSH | TCP_ACK, (uint8_t *)HTTP_RESPONSE, strlen(HTTP_RESPONSE));
}

static int is_http_get_request(uint8_t *data, uint16_t len)
{
    return (len >= 4 && data[0] == 'G' && data[1] == 'E' && data[2] == 'T' && data[3] == ' ');
}

static void handle_established_state(tcp_connection_t *conn, tcp_header_t *tcp, uint8_t *data, uint16_t len)
{
    uint32_t seq = ntohl(tcp->seq);
    uint32_t ack = ntohl(tcp->ack);
    uint16_t data_len = len - (tcp->data_offset >> 4) * 4;

    // Handle duplicate ACKs
    if (ack < conn->next_seq)
    {
        conn->dup_ack_count++;
        if (conn->dup_ack_count == 3)
        {
            retransmit_entry_t *entry = conn->retransmit_queue;
            if (entry && entry->seq == conn->next_seq - entry->length)
            {
                serial_printf("TCP: Fast retransmit triggered for SEQ=%u\n", entry->seq);
                tcp_send_segment(conn, TCP_ACK, entry->data, entry->length);
                entry->start_time = get_ticks();
                entry->retries++;
            }
        }
    }
    else
    {
        conn->dup_ack_count = 0;
    }

    // Process ACKs and retransmission queue
    retransmit_entry_t **pp = &conn->retransmit_queue;
    while (*pp)
    {
        retransmit_entry_t *entry = *pp;
        if (ack >= entry->seq + entry->length)
        {
            *pp = entry->next;
            free(entry->data);
            free(entry);
        }
        else
        {
            pp = &(*pp)->next;
        }
    }

    if (seq != conn->expected_ack)
    {
        tcp_send_segment(conn, TCP_ACK, NULL, 0);
        return;
    }

    if (data_len > 0)
    {
        conn->expected_ack = seq + data_len;
        tcp_send_segment(conn, TCP_ACK, NULL, 0);
        uint8_t *payload = data + (tcp->data_offset >> 4) * 4;
        if (is_http_get_request(payload, data_len))
        {
            handle_http_request(conn);
            conn->state = TCP_WAIT_FOR_ACK;
        }
    }

    // Handle FIN
    if (tcp->flags & TCP_FIN)
    {
        conn->expected_ack++;
        conn->state = TCP_CLOSE_WAIT;
        tcp_send_segment(conn, TCP_ACK, NULL, 0);
        tcp_send_segment(conn, TCP_FIN | TCP_ACK, NULL, 0);
        conn->state = TCP_LAST_ACK;
    }
}

static void handle_wait_for_ack_state(tcp_connection_t *conn, tcp_header_t *tcp)
{
    uint32_t ack = ntohl(tcp->ack);

    if (ack < conn->next_seq)
    {
        conn->dup_ack_count++;
        if (conn->dup_ack_count == 3)
        {
            serial_printf("TCP: Fast retransmit for HTTP response\n");
            tcp_send_segment(conn, TCP_PSH | TCP_ACK, (uint8_t *)HTTP_RESPONSE, strlen(HTTP_RESPONSE));
            conn->dup_ack_count = 0;
        }
        return;
    }

    if (ack >= conn->next_seq)
    {
        cancel_retransmission_timer(conn);
        conn->state = TCP_LAST_ACK;
        tcp_send_segment(conn, TCP_FIN | TCP_ACK, NULL, 0);
    }
}

static void handle_last_ack_state(tcp_connection_t *conn, tcp_header_t *tcp)
{
    if (tcp->flags & TCP_ACK && ntohl(tcp->ack) == conn->next_seq)
    {
        remove_connection(conn);
    }
}

void tcp_handle_packet(ipv4_header_t *ip, uint8_t *data, uint16_t len)
{
    tcp_header_t *tcp = (tcp_header_t *)data;
    uint8_t flags = tcp->flags;

    // Validate data_offset to prevent underflow
    uint8_t data_offset = tcp->data_offset >> 4;
    if (data_offset < 5)
    {
        serial_printf("TCP: Invalid data offset %u\n", data_offset);
        tcp_send_reset(ip, tcp);
        return;
    }

    // Checksum verification
    uint16_t received_checksum = tcp->checksum;
    tcp->checksum = 0;
    uint16_t calculated_checksum = tcp_checksum(ip, tcp, len);
    if (received_checksum != calculated_checksum)
    {
        serial_printf("TCP: Invalid checksum (got 0x%04x, expected 0x%04x)\n", received_checksum, calculated_checksum);
        return;
    }
    tcp->checksum = received_checksum;

    uint16_t src_port = ntohs(tcp->src_port);
    uint16_t dest_port = ntohs(tcp->dest_port);

    // Find existing connection
    tcp_connection_t *conn = find_connection(ntohl(ip->src_ip), src_port, ntohl(ip->dst_ip), dest_port);

    if (flags & TCP_RST)
    {
        if (conn)
        {
            serial_printf("TCP: RST received, closing connection\n");
            remove_connection(conn);
        }
        return;
    }

    // Handle SYN
    if (flags & TCP_SYN && !conn)
    {
        if (!is_listening_port(dest_port))
        {
            tcp_send_reset(ip, tcp);
            return;
        }

        conn = malloc(sizeof(tcp_connection_t));
        if (!conn)
        {
            serial_printf("TCP: Memory allocation failed\n");
            return;
        }

        memset(conn, 0, sizeof(tcp_connection_t));
        conn->local_ip = ntohl(ip->dst_ip);
        conn->remote_ip = ntohl(ip->src_ip);
        conn->local_port = dest_port;
        conn->remote_port = src_port;
        conn->next_seq = generate_secure_initial_seq();
        conn->expected_ack = ntohl(tcp->seq) + 1;
        conn->state = TCP_SYN_RECEIVED;
        conn->mss = DEFAULT_MSS;

        uint8_t options_len = (tcp->data_offset >> 4) * 4 - sizeof(tcp_header_t);
        if (options_len > 0)
        {
            process_tcp_options(conn, (uint8_t *)(tcp + 1), options_len);
        }

        add_connection(conn);
        tcp_send_segment(conn, TCP_SYN | TCP_ACK, NULL, 0);
        start_retransmission_timer(conn, TCP_SYN_RETRANSMIT_TIMEOUT);
        return;
    }

    if (!conn)
    {
        tcp_send_reset(ip, tcp);
        return;
    }

    switch (conn->state)
    {
    case TCP_SYN_RECEIVED:
        if (flags & TCP_ACK && ntohl(tcp->ack) == conn->next_seq)
        {
            conn->state = TCP_ESTABLISHED;
            cancel_retransmission_timer(conn);
            serial_printf("TCP: Connection established\n");
        }
        break;

    case TCP_ESTABLISHED:
        handle_established_state(conn, tcp, data, len);
        break;

    case TCP_WAIT_FOR_ACK:
        handle_wait_for_ack_state(conn, tcp);
        break;

    case TCP_LAST_ACK:
        handle_last_ack_state(conn, tcp);
        break;

    default:
        break;
    }
}