#ifndef NS16C2552_H
#define NS16C2552_H

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

#if defined(MACHINE_COMET68K)
/* In a COMET68k machine, the on-board 16C2552 UART is accessible in a contiguous address space - it is not necessary
 * to access the registers on alternate bytes, therefore channel A is only offset 8 bytes from channel B. */
#define NS16C2552_CHA_OFFSET 8
#else /* defined(MACHINE_COMET68K) */
#define NS16C2552_CHA_OFFSET 16
#endif

struct ns16c2552_ier {
    union {
        struct {
            UBYTE :4;
            UBYTE MSTAT:1;
            UBYTE LSTAT:1;
            UBYTE TXEMPTY:1;
            UBYTE RXDAT:1;
        };
        struct {
            UBYTE u8;
        };
    };
};

struct ns16c2552_iir {
    union {
        struct {
            UBYTE FIFOEN1:1;
            UBYTE FIFOEN0:1;
            UBYTE :2;
            UBYTE IPEND3:1;
            UBYTE IPEND2:1;
            UBYTE IPEND1:1;
            UBYTE IPEND0:1;
        };
        struct {
            UBYTE FIFOEN:2;
            UBYTE :2;
            UBYTE IPEND:4;
        };
        struct {
            UBYTE u8;
        };
    };
};

struct ns16c2552_fcr {
    union {
        struct {
            UBYTE RXTRG1:1;
            UBYTE RXTRG0:1;
            UBYTE :2;
            UBYTE DMASEL:1;
            UBYTE TXRST:1;
            UBYTE RXRST:1;
            UBYTE EN:1;
        };
        struct {
            UBYTE RXTRG:2;
            UBYTE :6;
        };
        struct {
            UBYTE u8;
        };
    };
};

struct ns16c2552_lcr {
    union {
        struct {
            UBYTE DLAB:1;
            UBYTE TXBRK:1;
            UBYTE PFORCE:1;
            UBYTE PEVEN:1;
            UBYTE PEN:1;
            UBYTE SLEN:1;
            UBYTE WLEN1:1;
            UBYTE WLEN0:1;
        };
        struct {
            UBYTE :6;
            UBYTE WLEN:2;
        };
        struct {
            UBYTE u8;
        };
    };
};

struct ns16c2552_mcr {
    union {
        struct {
            UBYTE :2;
            UBYTE AUTOFLOW:1;
            UBYTE LOOP:1;
            UBYTE OP2:1;
            UBYTE OP1:1;
            UBYTE RTSOC:1;
            UBYTE DTROC:1;
        };
        struct {
            UBYTE u8;
        };
    };
};

struct ns16c2552_lsr {
    union {
        struct {
            UBYTE RXERR:1;
            UBYTE TXIDL:1;
            UBYTE THRE:1;
            UBYTE RXBRK:1;
            UBYTE FERR:1;
            UBYTE PERR:1;
            UBYTE OERR:1;
            UBYTE RXD:1;
        };
        struct {
            UBYTE u8;
        };
    };
};

struct ns16c2552_msr {
    union {
        struct {
            UBYTE CDSTAT:1;
            UBYTE RISTAT:1;
            UBYTE DSRSTAT:1;
            UBYTE CTSSTAT:1;
            UBYTE CDCHG:1;
            UBYTE RICHG:1;
            UBYTE DSRCHG:1;
            UBYTE CTSCHG:1;
        };
        struct {
            UBYTE u8;
        };
    };
};

struct nc16c2552_afr {
    union {
        struct {
            UBYTE :5;
            UBYTE MFSEL1:1;
            UBYTE MFSEL0:1;
            UBYTE BOTH:1;
        };
        struct {
            UBYTE :5;
            UBYTE MFSEL:2;
            UBYTE :1;
        };
        struct {
            UBYTE u8;
        };
    };
};

#endif /* NS16C2552_H */
