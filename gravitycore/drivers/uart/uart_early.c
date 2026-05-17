/*
 * GravityOS — Early UART Driver (16550)
 * First output device available. Used by panic, kernel log.
 * No interrupts needed — polled I/O only.
 *
 * Copyright (c) 2026 GravityOS Contributors — MIT License
 */
#include <gravity/kernel.h>

#define UART_COM1   0x3F8
#define UART_COM2   0x2F8

/* 16550 UART register offsets */
#define UART_DATA   0  /* TX/RX data */
#define UART_IER    1  /* Interrupt Enable */
#define UART_FCR    2  /* FIFO Control */
#define UART_LCR    3  /* Line Control */
#define UART_MCR    4  /* Modem Control */
#define UART_LSR    5  /* Line Status */
#define UART_MSR    6  /* Modem Status */

/* Line Status Register bits */
#define UART_LSR_DR   0x01  /* Data Ready */
#define UART_LSR_THRE 0x20  /* Transmit Holding Register Empty */

/* Port I/O */
static inline void outb(u16 port, u8 val) {
    __asm__ volatile("outb %0, %1" :: "a"(val), "Nd"(port));
}

static inline u8 inb(u16 port) {
    u8 val;
    __asm__ volatile("inb %1, %0" : "=a"(val) : "Nd"(port));
    return val;
}

static u16 uart_port = UART_COM1;

/* ═══════ Init ═══════ */
void grav_uart_init(u16 port) {
    uart_port = port;

    outb(port + UART_IER, 0x00);  /* Disable interrupts */
    outb(port + UART_LCR, 0x80);  /* Enable DLAB (set baud rate divisor) */
    outb(port + UART_DATA, 0x01); /* Divisor low: 115200 baud */
    outb(port + UART_IER, 0x00);  /* Divisor high */
    outb(port + UART_LCR, 0x03);  /* 8 bits, no parity, one stop bit */
    outb(port + UART_FCR, 0xC7);  /* Enable FIFO, 14-byte threshold */
    outb(port + UART_MCR, 0x0B);  /* IRQs enabled, RTS/DSR set */

    /* Test: loopback mode */
    outb(port + UART_MCR, 0x1E);  /* Set loopback */
    outb(port + UART_DATA, 0xAE); /* Send test byte */
    if (inb(port + UART_DATA) != 0xAE) {
        /* UART not functional — silently fail */
        return;
    }

    /* Normal operation */
    outb(port + UART_MCR, 0x0F);
}

/* ═══════ Write single byte ═══════ */
void grav_uart_putchar(char c) {
    /* Wait for transmit buffer to be empty */
    while (!(inb(uart_port + UART_LSR) & UART_LSR_THRE)) {}
    outb(uart_port + UART_DATA, (u8)c);
}

/* ═══════ Write string ═══════ */
void grav_uart_puts(const char *s) {
    while (*s) {
        if (*s == '\n') grav_uart_putchar('\r');
        grav_uart_putchar(*s++);
    }
}

/* ═══════ Read byte (blocking) ═══════ */
char grav_uart_getchar(void) {
    while (!(inb(uart_port + UART_LSR) & UART_LSR_DR)) {}
    return (char)inb(uart_port + UART_DATA);
}

/* ═══════ Check if data available ═══════ */
int grav_uart_data_ready(void) {
    return inb(uart_port + UART_LSR) & UART_LSR_DR;
}

/* ═══════ Kernel Log via UART ═══════ */
void grav_log(u32 level, const char *fmt, ...) {
    /* Log levels: 0=DEBUG, 1=INFO, 2=WARN, 3=ERROR, 4=PANIC */
    const char *level_prefix[] = {
        "[DBG] ", "[INF] ", "[WRN] ", "[ERR] ", "[PNC] "
    };
    if (level < 5) grav_uart_puts(level_prefix[level]);

    /* Simplified printf — supports %s, %d, %x, %u */
    va_list args;
    __builtin_va_start(args, fmt);
    while (*fmt) {
        if (*fmt == '%' && *(fmt + 1)) {
            fmt++;
            switch (*fmt) {
            case 's': {
                const char *s = __builtin_va_arg(args, const char *);
                if (s) grav_uart_puts(s); else grav_uart_puts("(null)");
                break;
            }
            case 'd': case 'u': {
                u64 val = __builtin_va_arg(args, u64);
                char buf[21]; int i = 20;
                buf[i] = '\0';
                if (val == 0) { grav_uart_putchar('0'); break; }
                while (val > 0 && i > 0) {
                    buf[--i] = '0' + (val % 10);
                    val /= 10;
                }
                grav_uart_puts(&buf[i]);
                break;
            }
            case 'x': {
                u64 val = __builtin_va_arg(args, u64);
                const char *hex = "0123456789abcdef";
                grav_uart_puts("0x");
                int started = 0;
                for (int i = 60; i >= 0; i -= 4) {
                    int digit = (val >> i) & 0xF;
                    if (digit || started || i == 0) {
                        grav_uart_putchar(hex[digit]);
                        started = 1;
                    }
                }
                break;
            }
            case '%': grav_uart_putchar('%'); break;
            default: grav_uart_putchar('%'); grav_uart_putchar(*fmt); break;
            }
        } else {
            if (*fmt == '\n') grav_uart_putchar('\r');
            grav_uart_putchar(*fmt);
        }
        fmt++;
    }
    __builtin_va_end(args);

    grav_uart_putchar('\r');
    grav_uart_putchar('\n');
}
