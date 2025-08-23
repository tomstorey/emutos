#ifndef DP8570_H
#define DP8570_H

#define DP8570_MSR 0

/* With RS = 0 */
#define DP8570_T0CR 1
#define DP8570_T1CR 2
#define DP8570_PFR 3
#define DP8570_IRR 4

/* With RS = 1 */
#define DP8570_RTMR 1
#define DP8570_OMR 2
#define DP8570_ICR0 3
#define DP8570_ICR1 4

/* With RS = X */
#define DP8570_T0LSB 0xF
#define DP8570_T0MSB 0x10
#define DP8570_T1LSB 0x11
#define DP8570_T1MSB 0x12

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
};

void dp8570_init_system_timer(void);

#endif /* DP8570_H */
