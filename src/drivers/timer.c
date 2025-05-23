#include "timer.h"

#include "console.h"
#include "idt.h"
#include "io.h"
#include "8259_pic.h"
#include "isr.h"
#include "string.h"
#include "serial.h"
#include <stdint.h>
#include <stddef.h>

unsigned int seed = 0;
volatile uint32_t g_ticks = 0; 
uint16_t g_freq_hz = 0;
TIMER_FUNCTION_MANAGER g_timer_function_manager;

void timer_set_frequency(uint16_t f)
{
    g_freq_hz = f;
    uint16_t divisor = TIMER_INPUT_CLOCK_FREQUENCY / f;
    outportb(TIMER_COMMAND_PORT, 0b00110110);
    outportb(TIMER_CHANNEL_0_DATA_PORT, divisor & 0xFF);
    outportb(TIMER_CHANNEL_0_DATA_PORT, (divisor >> 8) & 0xFF);
}

void srand(uint32_t new_seed)
{
    seed = new_seed ^ (g_ticks << 16);
}

void timer_handler(REGISTERS *r)
{
    (void)r;
    size_t i;
    TIMER_FUNC_ARGS *args = NULL;
    g_ticks++;
    for (i = 0; i < MAXIMUM_TIMER_FUNCTIONS; i++)
    {
        args = &g_timer_function_manager.func_args[i];
        if (args->timeout == 0)
            continue;
        if ((g_ticks % args->timeout) == 0)
        {
            g_timer_function_manager.functions[i](args);
        }
    }
    pic8259_eoi(IRQ_BASE);
}

void timer_register_function(TIMER_FUNCTION function, TIMER_FUNC_ARGS *args)
{
    size_t index = 0;
    if (function == NULL || args == NULL)
    {
        console_printf("ERROR: failed to register timer function %x\n", (uintptr_t)function);
        return;
    }
    index = (++g_timer_function_manager.current_index) % MAXIMUM_TIMER_FUNCTIONS;
    g_timer_function_manager.current_index = index;
    g_timer_function_manager.functions[index] = function;
    memcpy(&g_timer_function_manager.func_args[index], args, sizeof(TIMER_FUNC_ARGS));
}

void timer_init()
{
    memset(&g_timer_function_manager, 0, sizeof(g_timer_function_manager));
    timer_set_frequency(100);
    isr_register_interrupt_handler(IRQ_BASE, timer_handler);
    pic8259_unmask(0);
    __asm__ volatile("sti");
}

void sleep(int sec)
{
    uint32_t end = g_ticks + sec * g_freq_hz;
    while (g_ticks < end) {
        __asm__ volatile ("sti; hlt");
    }
}

void usleep(int usec)
{
    uint32_t end = g_ticks + (usec * g_freq_hz) / 1000000;
    while (g_ticks < end)
        ;
}

void uptime()
{
    console_printf("uptime: %d seconds\n", g_ticks / g_freq_hz);
}

int rand(void)
{
    seed = (seed * 1103515245 + 12345) & 0x7fffffff;
    seed ^= seed << 13;
    seed ^= seed >> 17;
    seed ^= seed << 5;
    seed = (seed + g_ticks) ^ ((seed * g_ticks) >> 16);
    seed = ((seed ^ 0x5DEECE66D) + (g_ticks * 69069)) & 0x7fffffff;
    return (seed >> 16) & 0x7FFF;
}
uint32_t get_ticks(void)
{
    return g_ticks;
}
