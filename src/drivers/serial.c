#include "serial.h"
#include "io.h"
#include "printf.h"

void serial_init() {
    // Disable interrupts
    outportb(COM1 + INT_ENABLE_REG, 0x00);
    
    // Set baud rate to 38400
    outportb(COM1 + LINE_CTRL_REG, 0x80);    // Enable DLAB
    outportb(COM1 + 0, 0x03);                // Low byte
    outportb(COM1 + 1, 0x00);                // High byte
    
    // 8 bits, no parity, one stop bit
    outportb(COM1 + LINE_CTRL_REG, 0x03);
    
    // Enable FIFO, clear it, with 14-byte threshold
    outportb(COM1 + FIFO_CTRL_REG, 0xC7);
    
    // IRQs enabled, RTS/DSR set
    outportb(COM1 + MODEM_CTRL_REG, 0x0B);
}

int serial_received() {
    return inportb(COM1 + LINE_STATUS_REG) & 1;
}

char serial_read() {
    while (serial_received() == 0);
    return inportb(COM1);
}

int serial_is_transmit_empty() {
    return inportb(COM1 + LINE_STATUS_REG) & 0x20;
}

void serial_putchar(char c) {
    while (serial_is_transmit_empty() == 0);
    outportb(COM1, c);
}

void serial_printf(const char* format, ...) {
    va_list args;
    va_start(args, format);
    vprintf_(format, args);
    va_end(args);
}

void _putchar(char character) {
    serial_putchar(character);
}