#ifndef DP8570_H
#define DP8570_H

#include "emutos.h"

#if CONF_WITH_DP8570_TIMER || CONF_WITH_DP8570_RTC

#if defined(MACHINE_COMET68K)
# define DP8570_MSR 0

/* With RS = 0 */
# define DP8570_T0CR 1
# define DP8570_T1CR 2
# define DP8570_PFR 3
# define DP8570_IRR 4

/* With RS = 1 */
# define DP8570_RTMR 1
# define DP8570_OMR 2
# define DP8570_ICR0 3
# define DP8570_ICR1 4

/* With RS = X */
# define DP8570_T0LSB 0xF
# define DP8570_T0MSB 0x10
# define DP8570_T1LSB 0x11
# define DP8570_T1MSB 0x12
#else
#error "You need to specify an address map for the DP8570 in your machine"
#endif

struct dp8570_msr {
    union {
        struct {
            volatile UBYTE PS:1;
            volatile UBYTE RS:1;
            volatile UBYTE T1:1;
            volatile UBYTE T0:1;
            volatile UBYTE AL:1;
            volatile UBYTE PER:1;
            volatile UBYTE PF:1;
            volatile UBYTE INT:1;
        };
        volatile UBYTE u8;
    };
} __attribute__((packed));

struct dp8570_txcr {
    union {
        struct {
            volatile UBYTE CHG:1;
            volatile UBYTE RD:1;
            volatile UBYTE C2:1;
            volatile UBYTE C1:1;
            volatile UBYTE C0:1;
            volatile UBYTE M1:1;
            volatile UBYTE M0:1;
            volatile UBYTE TSS:1;
        };
        struct {
            volatile UBYTE :2;
            volatile UBYTE C:3;
            volatile UBYTE M:2;
            volatile UBYTE :1;
        };
        volatile UBYTE u8;
    };
} __attribute__((packed));

struct dp8570_pfr {
    union {
        struct {
            volatile UBYTE TM:1;
            volatile UBYTE OSF:1;
            volatile UBYTE PF1MS:1;
            volatile UBYTE PF10MS:1;
            volatile UBYTE PF100MS:1;
            volatile UBYTE PF1S:1;
            volatile UBYTE PF10S:1;
            volatile UBYTE PF1MIN:1;
        };
        struct {
            volatile UBYTE :1;
            volatile UBYTE SSUP:1;
            volatile UBYTE :6;
        };
        volatile UBYTE u8;
    };
} __attribute__((packed));

struct dp8570_irr {
    union {
        struct {
            volatile UBYTE TS:1;
            volatile UBYTE LB:1;
            volatile UBYTE PFD:1;
            volatile UBYTE T1R:1;
            volatile UBYTE TOR:1;
            volatile UBYTE ALR:1;
            volatile UBYTE PRR:1;
            volatile UBYTE PFR:1;
        };
        volatile UBYTE u8;
    };
} __attribute__((packed));

struct dp8570_rtmr {
    union {
        struct {
            volatile UBYTE XT1:1;
            volatile UBYTE XT0:1;
            volatile UBYTE TPF:1;
            volatile UBYTE IPF:1;
            volatile UBYTE CSS:1;
            volatile UBYTE H12:1;
            volatile UBYTE LY1:1;
            volatile UBYTE LY0:1;
        };
        struct {
            volatile UBYTE XT:2;
            volatile UBYTE :4;
            volatile UBYTE LY:2;
        };
        volatile UBYTE u8;
    };
} __attribute__((packed));

struct dp8570_omr {
    union {
        struct {
            volatile UBYTE MO:1;
            volatile UBYTE MT:1;
            volatile UBYTE MP:1;
            volatile UBYTE MH:1;
            volatile UBYTE IP:1;
            volatile UBYTE IH:1;
            volatile UBYTE TP:1;
            volatile UBYTE TH:1;
        };
        volatile UBYTE u8;
    };
} __attribute__((packed));

struct dp8570_icr0 {
    union {
        struct {
            volatile UBYTE ENT1:1;
            volatile UBYTE ENT0:1;
            volatile UBYTE EN1MS:1;
            volatile UBYTE EN10MS:1;
            volatile UBYTE EN100MS:1;
            volatile UBYTE EN1S:1;
            volatile UBYTE EN10S:1;
            volatile UBYTE EN1MIN:1;
        };
        volatile UBYTE u8;
    };
} __attribute__((packed));

struct dp8570_icr1 {
    union {
        struct {
            volatile UBYTE PF:1;
            volatile UBYTE AL:1;
            volatile UBYTE DOW:1;
            volatile UBYTE MO:1;
            volatile UBYTE DOM:1;
            volatile UBYTE HR:1;
            volatile UBYTE MN:1;
            volatile UBYTE SC:1;
        };
        volatile UBYTE u8;
    };
} __attribute__((packed));

void dp8570_detect_rtc(void);
void dp8570_init_system_timer(void);
void dp8570_init_clock(void);
LONG dp8570_getdt(void);

#endif /* CONF_WITH_DP8570_TIMER || CONF_WITH_DP8570_RTC */

#endif /* DP8570_H */
