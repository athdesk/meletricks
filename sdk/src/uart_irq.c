#include "uart_irq.h"

/* Vector table sits in SRAM at 0x11000000 (firmware relocates it there at
 * boot and points VTOR to it). Slot 25 (byte offset 0x64) is the UART0 IRQ
 * handler. Writing a new function pointer there with the Thumb bit set
 * redirects future UART0 interrupts to our handler. */
#define NVIC_VECTOR_TABLE       ((volatile u32 *)0x11000000u)
#define UART0_IRQ_VECTOR_INDEX  25

irq_handler_t install_uart_isr(irq_handler_t handler)
{
    irq_handler_t old_handler = (irq_handler_t)NVIC_VECTOR_TABLE[UART0_IRQ_VECTOR_INDEX];

    NVIC_VECTOR_TABLE[UART0_IRQ_VECTOR_INDEX] = ((u32)handler) | 1u;
    /* Make the write visible before any subsequent IRQ entry, and clear
     * any prefetched instruction state. */
    __asm__ volatile("dsb 0xF" ::: "memory");
    __asm__ volatile("isb 0xF" ::: "memory");
    return old_handler;
}
