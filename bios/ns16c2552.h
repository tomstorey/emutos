#ifndef NS16C2552_H
#define NS16C2552_H

#if defined(CONF_WITH_NS16C2552) && CONF_WITH_NS16C2552

#include "emutos.h"
#include "serport.h"

#define NS16C2552_RBR_REG 0             /* Receiver Buffer Register (r) */
#define NS16C2552_THR_REG 0             /* Transmitter Holding Register (w) */
#define NS16C2552_IER_REG 1             /* Interrupt Enable Register (r/w) */
#define NS16C2552_IIR_REG 2             /* Interrupt Ident Register (r) */
#define NS16C2552_FCR_REG 2             /* FIFO Control Register (w) */
#define NS16C2552_LCR_REG 3             /* Line Control Register (r/w) */
#define NS16C2552_MCR_REG 4             /* Modem Control Register (r/w) */
#define NS16C2552_LSR_REG 5             /* Line Status Register (r/w) */
#define NS16C2552_MSR_REG 6             /* Modem Status Register (r/w) */
#define NS16C2552_SCR_REG 7             /* Scratch Register (r/w) */

#define NS16C2552_DLL_REG 0             /* LSB of divisor (r/w) */
#define NS16C2552_DLM_REG 1             /* MSB of divisor (r/w) */
#define NS16C2552_AFR_REG 2             /* Alternate Function Register (r/w) */

#define NS16C2552_INT_MSR 0
#define NS16C2552_INT_TXRDY 0x02
#define NS16C2552_INT_RXRDY 0x04
#define NS16C2552_INT_LSR 0x06
#define NS16C2552_INT_RXTIMEOUT 0x0C
#define NS16C2552_INT_XOFF 0x10
#define NS16C2552_INT_FLOW 0x20

#if defined(MACHINE_COMET68K)
/* In a COMET68k machine, the on-board 16C2552 UART is accessible in a contiguous address space - it is not necessary
 * to access the registers on alternate bytes, therefore channel A is only offset 8 bytes from channel B. */
#define NS16C2552_CHA_OFFSET 8
#else
#define NS16C2552_CHA_OFFSET 16
#endif

struct ns16c2552_ier {
    union {
        struct {
            volatile UBYTE CTSINTEN:1;
            volatile UBYTE RTSINTEN:1;
            volatile UBYTE XOFFINTEN:1;
            volatile UBYTE SLEEP:1;
            volatile UBYTE MSTAT:1;
            volatile UBYTE RXLSTAT:1;
            volatile UBYTE TXEMPTY:1;
            volatile UBYTE RXDAT:1;
        };
        volatile UBYTE u8;
    };
} __attribute__((packed));

struct ns16c2552_iir {
    union {
        struct {
            volatile UBYTE FIFOEN1:1;
            volatile UBYTE FIFOEN0:1;
            volatile UBYTE INTSRC5:1;
            volatile UBYTE INTSRC4:1;
            volatile UBYTE INTSRC3:1;
            volatile UBYTE INTSRC2:1;
            volatile UBYTE INTSRC1:1;
            volatile UBYTE INTSRC0:1;
        };
        struct {
            volatile UBYTE FIFOEN:2;
            volatile UBYTE INTSRC:6;
        };
        volatile UBYTE u8;
    };
} __attribute__((packed));

struct ns16c2552_fcr {
    union {
        struct {
            volatile UBYTE RXTRG1:1;
            volatile UBYTE RXTRG0:1;
            volatile UBYTE :2;
            volatile UBYTE DMASEL:1;
            volatile UBYTE TXRST:1;
            volatile UBYTE RXRST:1;
            volatile UBYTE FIFOEN:1;
        };
        struct {
            volatile UBYTE RXTRG:2;
            volatile UBYTE :6;
        };
        volatile UBYTE u8;
    };
} __attribute__((packed));

struct ns16c2552_lcr {
    union {
        struct {
            volatile UBYTE DLAB:1;
            volatile UBYTE TXBRK:1;
            volatile UBYTE PFORCE:1;
            volatile UBYTE PODD:1;
            volatile UBYTE PEN:1;
            volatile UBYTE SLEN:1;
            volatile UBYTE WLEN1:1;
            volatile UBYTE WLEN0:1;
        };
        struct {
            volatile UBYTE :6;
            volatile UBYTE WLEN:2;  /* Word length: 00=5 bits, 01=6 bits, 10=7 bits, 11=8 bits */
        };
        volatile UBYTE u8;
    };
} __attribute__((packed));

struct ns16c2552_mcr {
    union {
        /* NS16C2552 */
        struct {
            volatile UBYTE CLKDIVSEL:1;
            volatile UBYTE IRMODE:1;
            volatile UBYTE XONANY:1;
            volatile UBYTE LOOP:1;
            volatile UBYTE OP2:1;
            volatile UBYTE OP1:1;
            volatile UBYTE RTSOC:1;
            volatile UBYTE DTROC:1;
        };
        /* TL16C2552 */
        struct {
            volatile UBYTE :2;
            volatile UBYTE AUTOFLOW:1;
            volatile UBYTE :1;
            volatile UBYTE INTEN:1;
            volatile UBYTE :3;
        };
        volatile UBYTE u8;
    };
} __attribute__((packed));

struct ns16c2552_lsr {
    union {
        struct {
            volatile UBYTE RXERR:1;
            volatile UBYTE TXIDL:1;
            volatile UBYTE THRE:1;
            volatile UBYTE RXBRK:1;
            volatile UBYTE FERR:1;
            volatile UBYTE PERR:1;
            volatile UBYTE OERR:1;
            volatile UBYTE RXRDY:1;
        };
        struct {
            volatile UBYTE :4;
            volatile UBYTE XERR:3;
            volatile UBYTE :1;
        };
        volatile UBYTE u8;
    };
} __attribute__((packed));

struct ns16c2552_msr {
    union {
        struct {
            volatile UBYTE CDSTAT:1;
            volatile UBYTE RISTAT:1;
            volatile UBYTE DSRSTAT:1;
            volatile UBYTE CTSSTAT:1;
            volatile UBYTE CDCHG:1;
            volatile UBYTE RICHG:1;
            volatile UBYTE DSRCHG:1;
            volatile UBYTE CTSCHG:1;
        };
        volatile UBYTE u8;
    };
} __attribute__((packed));

struct nc16c2552_afr {
    union {
        struct {
            volatile UBYTE :5;
            volatile UBYTE MFSEL1:1;
            volatile UBYTE MFSEL0:1;
            volatile UBYTE BOTH:1;
        };
        struct {
            volatile UBYTE :5;
            volatile UBYTE MFSEL:2;
            volatile UBYTE :1;
        };
        volatile UBYTE u8;
    };
} __attribute__((packed));

void ns16c2552_detect(void);
void ns16c2552_init(void);
void ns16c2552_tx(void *base, EXT_IOREC *iorec, UBYTE data);
ULONG ns16c2552_rsconf(void *base, EXT_IOREC *iorec, WORD baud, WORD ctrl, WORD ucr, WORD rsr, WORD tsr, WORD scr);

#endif /* defined(CONF_WITH_NS16C2552) && CONF_WITH_NS16C2552 */

#endif /* NS16C2552_H */
