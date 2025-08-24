#include "emutos.h"
#include "vectors.h"
#include "serport.h"
#include "asm.h"
#include "ikbd.h"
#include "ns16c2552.h"

#if defined(CONF_WITH_NS16C2552) && CONF_WITH_NS16C2552

/* Nasty global hacks to try and keep all NS16C2552 stuff in one place */
#define RS232_BUFSIZE 256
EXT_IOREC iorecA, iorecB;
UBYTE ibufA[RS232_BUFSIZE], obufA[RS232_BUFSIZE];
UBYTE ibufB[RS232_BUFSIZE], obufB[RS232_BUFSIZE];

/* Based on a 7.3728MHz clock */
static const WORD ns16c2552_timeconst[] = {
    /*  19200 */  24,
    /*   9600 */  48,
    /*   4800 */  96,
    /*   3600 */  128,
    /*   2400 */  192,
    /*   2000 */  230,      /* Actual: 2003, 1.15% error */
    /*   1800 */  256,
    /*   1200 */  384,
    /*    600 */  768,
    /*    300 */  1536,
    /* 230400 */  2,
    /* 115200 */  4,
    /*  57600 */  8,
    /*  38400 */  12,
    /* 153600 */  3,
    /*  78600 */  6         /* Actual: 76800, 2.34% error */
};

/* Forward decls */
static void interrupt(void);
// static void interrupt_ch_a(ULONG source);
static void interrupt_ch_b(ULONG source);

void ns16c2552_init(void)
{
    struct ns16c2552_ier *ier = (struct ns16c2552_ier *)(NS16C2552_BASE + NS16C2552_IER_REG);
    struct ns16c2552_lcr *lcr = (struct ns16c2552_lcr *)(NS16C2552_BASE + NS16C2552_LCR_REG);
    struct ns16c2552_fcr *fcr = (struct ns16c2552_fcr *)(NS16C2552_BASE + NS16C2552_FCR_REG);
    volatile UBYTE *dll = (UBYTE *)(NS16C2552_BASE + NS16C2552_DLL_REG);
    volatile UBYTE *dlm = (UBYTE *)(NS16C2552_BASE + NS16C2552_DLM_REG);

    /* Basic interface configuration - 8-bit, 1 stop bit, no parity */
    lcr->u8 = 0;
    lcr->WLEN = 3;

    /* Configure the default baud rate */
    const UWORD baud = ns16c2552_timeconst[DEFAULT_BAUDRATE];

    lcr->DLAB = 1;      /* Access alternate register set */
    *dll = baud;
    *dlm = baud >> 8;
    lcr->DLAB = 0;      /* Main register set */

    ier->u8 = 0;        /* Disable all interrupt sources */

    fcr->u8 = 0x07;     /* Reset FIFOs, RX interrupt trigger level = 1 byte, enable tx/rx */

    /* Interrupt setup */
    volatile PFVOID *vector_addr = &VEC_LEVEL1 + (CONF_NS16C2552_AUTOVECTOR - 1);
    *vector_addr = (PFVOID)interrupt;

    ier->RXDAT = 1;     /* Interrupt on RX data available */
}

static void __attribute__((interrupt))
interrupt(void)
{
    /* To hold interrupt ident bits */
    struct ns16c2552_iir iir_b;

    /* Get interrupt source */
    iir_b.u8 = *(UBYTE *)(NS16C2552_BASE + NS16C2552_IIR_REG);

    /* Check for channel B interrupts */
    if (!iir_b.INTSRC0) {
        interrupt_ch_b(iir_b.INTSRC);
    }
}

static void
interrupt_ch_b(ULONG source)
{
    struct ns16c2552_ier *ier = (struct ns16c2552_ier *)(NS16C2552_BASE + NS16C2552_IER_REG);
    struct ns16c2552_lsr *lsr = (struct ns16c2552_lsr *)(NS16C2552_BASE + NS16C2552_LSR_REG);

    /* Serves a dual purposes for reading and writing */
    volatile UBYTE *rbr = (UBYTE *)(NS16C2552_BASE + NS16C2552_RBR_REG);

    IOREC *out;

    switch (source) {
        case NS16C2552_INT_TXRDY:
            out = &iorecB.out;

            /* Enter critical section */
            WORD old_sr = set_sr(0x2700);

            /* If the iorec tail==head then the tx ring is empty. Disable interrupts and we're done */
            if (out->tail == out->head) {
                ier->TXEMPTY = 0;
            } else {
                /* Queue bytes until the TX FIFO is full, or the tx ring is empty */
                UWORD ctr = CONF_NS16C2552_FIFOSIZE;

                for (; ctr; ctr--) {
                    /* Increment head and wrap */
                    out->head++;

                    if (out->head >= out->size) {
                        out->head = 0;
                    }

                    /* Transmit byte at head */
                    *rbr = out->buf[out->head];

                    /* If tail==head, the ring is empty */
                    if (out->tail == out->head) {
                        break;
                    }
                }
            }

            /* Exit critical section */
            (void)set_sr(old_sr);

            break;

        case NS16C2552_INT_RXTIMEOUT:
        case NS16C2552_INT_RXRDY:
            while (lsr->RXRDY) {
                /* Check for error conditions, and also ignore break characters */
                if (lsr->OERR || lsr->PERR || lsr->FERR || lsr->RXBRK) {
                    /* Dummy read the RBR and move to the next received character */
                    (void)*rbr;

                    continue;
                }

#if !CONF_WITH_IKBD_NS16C2552
                push_serial_iorec(&iorecB.in, *rbr);
#else
                push_ascii_ikbdiorec(*rbr);
#endif
            }

            break;

        default:
            FATAL(0xFBEE); /* Unhandled */
    }
}

void
ns16c2552_tx(void *base, EXT_IOREC *iorec, UBYTE data)
{
    /* Assign pointers to registers */
    const struct ns16c2552_lsr *lsr = (struct ns16c2552_lsr *)(base + NS16C2552_LSR_REG);
    UBYTE *thr = base + NS16C2552_THR_REG;
    IOREC *out = &iorec->out;

    /* Enter critical section */
    WORD old_sr = set_sr(0x2700);

    /* If the UART transmitter is completely idle, queue directly with the UART - but only if the ring is empty */
    if (out->tail == out->head) {
        if (lsr->THRE) {
            *thr = data;

            goto done;
        }
    }

    /* Otherwise add the byte to the tx ring, and enable the TXEMPTY interrupt to handle this byte once the existing
     * transmission is complete */
    struct ns16c2552_ier *ier = base + NS16C2552_IER_REG;

    /* Wrap the tail if it exceeds the buffer size */
    WORD tail = out->tail;
    tail++;

    if (tail >= out->size) {
        tail = 0;
    }

    /* Queue the byte - dont need to check for tail==head since we only got here because the queue had room in the
     * first place */
    *(out->buf + tail) = data;
    out->tail = tail;

    /* Enable TX interrupt */
    ier->TXEMPTY = 1;

done:
    /* Exit critical section */
    (void)set_sr(old_sr);
}

ULONG
ns16c2552_rsconf(void *port, EXT_IOREC *iorec, WORD baud, WORD ctrl, WORD ucr, WORD rsr, WORD tsr, WORD scr)
{
    ULONG old = 0;

    return old;
}

#endif /* defined(CONF_WITH_NS16C2552) && CONF_WITH_NS16C2552 */
