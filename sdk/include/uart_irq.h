/*
 * uart_irq.h — FR8000U UART0 ISR hijack.
 *
 * Replaces the firmware's UART0 RX handler with a caller-supplied function.
 * After install_uart_isr(), the firmware's stock byte-FSM stops running for UART0.
 * The handler we install owns every byte that arrives on UART0.
 *
 * Handlers run in IRQ context; AAPCS callee-saves and PC/LR/xPSR are
 * stacked by hardware so a plain C function works. The handler must drain
 * the FIFO before returning or the IRQ will keep firing.
 *
 * UART0 hardware register offsets (8250-style, base 0x50050000):
 *   +0x00  THR — RX data (read) / TX holding (write)
 *   +0x04  IER — interrupt enables
 *   +0x08  IIR — interrupt identification (read)
 *   +0x14  LSR — line status; bit 0 = RX FIFO not empty
 *
 * TODO: calling yield from ISR will probably destroy everything,
 * find a way to either prevent this at compilation time or to
 * detect we are in an ISR and bail without yielding.
 */
#ifndef UART_IRQ_H
#define UART_IRQ_H

#include "fr_types.h"

#define UART0_BASE  0x50050000u
#define UART0_THR   (*(volatile u32 *)(UART0_BASE + 0x00))
#define UART0_IER   (*(volatile u32 *)(UART0_BASE + 0x04))
#define UART0_IIR   (*(volatile u32 *)(UART0_BASE + 0x08))
#define UART0_LSR   (*(volatile u32 *)(UART0_BASE + 0x14))

typedef void (*irq_handler_t)(void);

// Installs a handler for UART0 RX interrupts.
// Returns the previous handler, so it can be safely restored later.
irq_handler_t install_uart_isr(irq_handler_t handler);

#endif
