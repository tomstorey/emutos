#include "emutos.h"
#include "vectors.h"
#include "biosbind.h"
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
static const UWORD ns16c2552_timeconst[] = {
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

/* Global that indicates that an NS16C2552 UART has been detected */
int has_ns16c2552;

/* Holds the original interrupt vector for autovectored IRQ chaining */
static ULONG next_vec = 0;

/* A buffer that is built into a small IRQ chain handler - so that a jump can be made to the next handler in the chain
 * without clobbering any registers along the way */
static volatile UWORD irq_chain[10] = {
    0x48e7, 0xc0c0,                 /* movem.l %d0-%d1/%a0-%a1, %sp@-       Save temporaries */
    0x4eb9, 0x0000, 0x0000,         /* jsr     interrupt                    Run our ISR */
    0x4cdf, 0x0303,                 /* movem.l %sp@+, %d0-%d1/%a0-%a1       Restore temporaries */
    0x4ef9, 0x0000, 0x0000          /* jmp     ...                          Jump to next ISR in the chain */
};

/* Forward decls */
static void interrupt(void);
static void interrupt_ch(ULONG source, void *base, EXT_IOREC *iorec);

void ns16c2552_detect(void)
{
    has_ns16c2552 = 0;

    /* The NS16C2552 and compatibles have two identical channels, and both contain a scratch byte. Try to read this
     * byte from both channels to determine if a 16C2552 is present. First channel B then channel A. */
    if (check_read_byte(NS16C2552_BASE + NS16C2552_SCR_REG)) {
        if (check_read_byte(NS16C2552_BASE + NS16C2552_CHA_OFFSET + NS16C2552_SCR_REG)) {
            has_ns16c2552 = 1;
        }
    }

    KDEBUG(("ns16c2552_detect(): has_ns16c2552 = %d\n", has_ns16c2552));
}

void ns16c2552_init(void)
{
    KDEBUG(("ns16c2552_init()\n"));

    struct ns16c2552_ier *ier_a = (struct ns16c2552_ier *)(NS16C2552_BASE + NS16C2552_CHA_OFFSET + NS16C2552_IER_REG);
    struct ns16c2552_ier *ier_b = (struct ns16c2552_ier *)(NS16C2552_BASE + NS16C2552_IER_REG);

    /* Disable all interrupt sources */
    ier_a->u8 = 0;
    ier_b->u8 = 0;

    /* Interrupt vector setup - channels A and B share the same interrupt - chain the interrupt handler in.
     *
     * Cant use Setexc here because TRAP 13 has not been initialised at the stage of boot where this code executes */
    volatile void *vector_addr = &VEC_LEVEL1 + (CONF_NS16C2552_AUTOVECTOR - 1);
    next_vec = *(ULONG *)vector_addr;
    *(ULONG *)vector_addr = (ULONG)&irq_chain;

    irq_chain[3] = (UWORD)((ULONG)&interrupt >> 16); /* Address of our ISR */
    irq_chain[4] = (UWORD)((ULONG)&interrupt);

    irq_chain[8] = (UWORD)(next_vec >> 16);     /* Address of next ISR */
    irq_chain[9] = (UWORD)next_vec;

    /* Initialise channel B */
    (void)ns16c2552_rsconf((void *)NS16C2552_BASE, &iorecB, DEFAULT_BAUDRATE, -1, 0, -1, -1, -1);

    /* Interrupt on RX data available */
    ier_b->RXDAT = 1;

#if !NS16C2552_DEBUG_PRINT
    /* Conditionally initialise channel A - if used for debug printing, it should be setup prior to starting EmuTOS */
    (void)ns16c2552_rsconf((void *)NS16C2552_BASE, &iorecB, DEFAULT_BAUDRATE, -1, 0, -1, -1, -1);

    ier_a->RXDAT = 1;     /* Interrupt on RX data available */
#endif
}

static void
interrupt(void)
{
    /* To hold interrupt ident bits */
    struct ns16c2552_iir iir_b;
    struct ns16c2552_iir iir_a;

    /* Get interrupt source */
    iir_b.u8 = *(UBYTE *)(NS16C2552_BASE + NS16C2552_IIR_REG);
    iir_a.u8 = *(UBYTE *)(NS16C2552_BASE + NS16C2552_CHA_OFFSET + NS16C2552_IIR_REG);

    /* Handle channel B interrupts */
    if (!iir_b.INTSRC0) {
        interrupt_ch(iir_b.INTSRC, (void *)NS16C2552_BASE, &iorecB);
    }

    /* Handle channel A interrupts */
    if (!iir_a.INTSRC0) {
        interrupt_ch(iir_a.INTSRC, (void *)NS16C2552_BASE + NS16C2552_CHA_OFFSET, &iorecA);
    }
}

static void
interrupt_ch(ULONG source, void *base, EXT_IOREC *iorec)
{
    struct ns16c2552_ier *ier = base + NS16C2552_IER_REG;
    struct ns16c2552_mcr *mcr = base + NS16C2552_MCR_REG;
    struct ns16c2552_lsr *lsr = base + NS16C2552_LSR_REG;

    /* Serves a dual purposes for reading and writing */
    volatile UBYTE *rbr = (UBYTE *)base + NS16C2552_RBR_REG;

    /* For saving a copy of the LSR when checking error conditions */
    struct ns16c2552_lsr saved_lsr;

    /* For saving the CPU Status Register and IPL when entering/exiting critical sections */
    WORD old_sr;

    IOREC *out;

    switch (source) {
        case NS16C2552_INT_TXRDY:
            out = &iorec->out;

            /* Enter critical section */
            old_sr = set_sr(0x2700);

            /* If the iorec tail==head then the tx ring is empty. Disable interrupts and we're done */
            if (out->tail == out->head) {
                ier->TXEMPTY = 0;
            } else {
                /* Queue bytes until the TX FIFO is full, or the tx ring is empty, or CTS is negated */
                UWORD ctr = CONF_NS16C2552_FIFOSIZE;

                for (; ctr; ctr--) {
                    /* Increment head and wrap */
                    out->head++;

                    if (out->head >= out->size) {
                        out->head = 0;
                    }

                    /* Transmit byte at head */
                    *rbr = out->buf[out->head] & iorec->datamask;

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
            /* Enter critical section */
            old_sr = set_sr(0x2700);

            if (source == NS16C2552_INT_RXRDY || NS16C2552_DEBUG_PRINT) {
                /* Negate RTS to give us time to clear our buffer */
                mcr->RTSOC = 0;
            }

            for (;;) {
                /* Save the LSR to a temporary register so that we dont lose any error condition bits */
                saved_lsr.u8 = lsr->u8;

                if (!saved_lsr.RXRDY) {
                    break;
                }

                /* Check for error conditions, and also ignore break characters */
                if (saved_lsr.XERR || saved_lsr.RXBRK) {
                    /* Dummy read the RBR and move to the next received character */
                    (void)*rbr;

                    continue;
                }

                /* If the character is received from Channel B, and if we're operating with a serial based
                 * console, the character should be used as a keystroke. Otherwise it should be queued in
                 * the iorec for this channel. */
                if (base == (void *)NS16C2552_BASE) {
#if !CONF_WITH_IKBD_NS16C2552
                    push_serial_iorec(&iorec->in, *rbr);
#else
                    push_ascii_ikbdiorec(*rbr);
#endif
                } else {
                    push_serial_iorec(&iorec->in, *rbr);
                }
            }

            /* Assert RTS to allow transmission to continue */
            mcr->RTSOC = 1;

            /* Exit critical section */
            (void)set_sr(old_sr);

            break;

        default:
           ; /* Unhandled */
    }
}

void
ns16c2552_tx(void *base, EXT_IOREC *iorec, UBYTE data)
{
    /* Assign pointers to registers */
    const struct ns16c2552_lsr *lsr = base + NS16C2552_LSR_REG;
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
ns16c2552_rsconf(void *base, EXT_IOREC *iorec, WORD baud, WORD ctrl, WORD ucr, WORD rsr, WORD tsr, WORD scr)
{
    (void)ctrl;
    (void)rsr;
    (void)scr;

    if (baud == -2) {
        /* Return the current baud rate */
        return iorec->baudrate;
    }

    struct ns16c2552_fcr *fcr = base + NS16C2552_FCR_REG;
    struct ns16c2552_lcr *lcr = base + NS16C2552_LCR_REG;
    struct ns16c2552_mcr *mcr = base + NS16C2552_MCR_REG;
    const struct ns16c2552_lsr *lsr = base + NS16C2552_LSR_REG;
    volatile UBYTE *thr = base + NS16C2552_THR_REG;
    volatile UBYTE *dll = base + NS16C2552_DLL_REG;
    volatile UBYTE *dlm = base + NS16C2552_DLM_REG;

    BOOL changes = FALSE;

    /* Set up return value */
    ULONG old = (ULONG)iorec->ucr << 24;

    if (lcr->TXBRK) {
        old |= 0x0800;
    }

    /* Configure the baud rate */
    if (baud >= MIN_BAUDRATE_CODE && baud <= MAX_BAUDRATE_CODE) {
        const UWORD timeconst = ns16c2552_timeconst[baud];

        lcr->DLAB = 1;
        *dll = timeconst;
        *dlm = timeconst >> 8;
        lcr->DLAB = 0;

        iorec->baudrate = baud;

        changes = TRUE;
    }

    /* TODO: flow control configuration */

    if (ucr >= 0) {
        /* Format of the MFP UCR register:
         *
         *  7   6   5   4   3   2   1   0
         * CLK CL1 CL0 ST1 ST0 PE  E/O  *
         */

        /* Word size - MFP is inverse of NS16C2552 */
        const UBYTE ws = ((ucr >> 5) & 0x3) ^ 0x03;

        /* Byte mask based on word size - default to 8 bit */
        UBYTE mask = 0xFF;

        /* New LCR value to be applied */
        volatile struct ns16c2552_lcr new_lcr = { .u8 = 0 };

        /* Set word size */
        new_lcr.WLEN = ws;

        /* Determine new byte mask */
        switch (ws) {
            case 0: mask = 0x1F; break; /* 5-bit */
            case 1: mask = 0x3F; break; /* 6-bit */
            case 2: mask = 0x7F; break; /* 7-bit */
            default: ; /* 8-bit */
        }

        if (ucr & 0x4) {

            /* Enable parity */
            new_lcr.PEN = 1;

            if (!(ucr & 0x2)) {
                /* If bit 2 is clear, enable odd parity - inverse of MFP */
                new_lcr.PODD = 1;
            }
        }

        if (ucr & 0x10) {
            /* Set 1.5 or 2 stop bits based on word size:
             *
             * 5 = 1.5 stop bits
             * 6, 7, 8 = 2 stop bits
             *
             * Otherwise 1 stop bit.
             *
             * Not directly compatible with MFP. */
            new_lcr.SLEN = 1;
        }

        lcr->u8 = new_lcr.u8;

        iorec->ucr = ucr;
        iorec->datamask = mask;

        changes = TRUE;
    }

    if (tsr >= 0) {
        /* Format of the MFP TSR register:
         *
         *  7   6   5   4   3   2   1   0
         * BE  UE  AT  END  B   H   L  TE
         *
         * The only applicable bit is B - send break
         */
        if (tsr & 0x8) {
            /* Request to send a break */
            if (!lcr->TXBRK) {
                /* First, wait for the FIFO to be empty */
                while (!lsr->THRE) {}

                /* Then send a 0 byte */
                *thr = '\0';

                /* Again wait for the FIFO to be empty */
                while (!lsr->THRE) {}

                /* Set the TX break enable bit */
                lcr->TXBRK = 1;

                /* Now wait for the transmitter to be idle */
                while (!lsr->TXIDL) {}
            }
        } else {
            if (lcr->TXBRK) {
                /* Clear TX break enable bit */
                lcr->TXBRK = 0;
            }
        }
    }

    if (changes != FALSE) {
        /* Enable FIFOs and reset them, set the RX trigger level to 8 bytes */
        fcr->u8 = 0x01;
        fcr->u8 = 0x87;

        /* Assert RTS/DTR to allow comms */
        mcr->u8 = 0x03;
    }

    return old;
}

#endif /* defined(CONF_WITH_NS16C2552) && CONF_WITH_NS16C2552 */
