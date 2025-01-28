#ifndef SERIAL_H
#define SERIAL_H

#include <stdint.h>

// COM1 port
#define COM1 0x3F8

// Serial port registers offsets
#define DATA_REG        0   // Data register
#define INT_ENABLE_REG  1   // Interrupt enable
#define FIFO_CTRL_REG   2   // FIFO control
#define LINE_CTRL_REG   3   // Line control
#define MODEM_CTRL_REG  4   // Modem control
#define LINE_STATUS_REG 5   // Line status

// Function declarations
void serial_init(void);
void serial_printf(const char* format, ...);
char serial_read(void);
int serial_received(void);
int serial_is_transmit_empty(void);
void _putchar(char character);
#endif