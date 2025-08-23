#include "emutos.h"
#include "vectors.h"
#include "dp8570.h"

/* Forward decls */
static void interrupt(void);

void
dp8570_init_system_timer(void)
{
    volatile UBYTE *base = (UBYTE *)DP8570_BASE;

    /* Ensure we are accessing the first set of registers */
    *(base + DP8570_MSR) &= ~0x40;

    /* Configure Timer 0 to produce a 200hz (5ms) interrupt */
    *(base + DP8570_T0CR) = 0x04;               /* TCK clock, mode 1 (pulse generator), timer stopped */
    *(base + DP8570_IRR) = ~0x08;               /* Interrupt sourced from Timer 0 */

    *(base + DP8570_MSR) |= 0x40;               /* Access second set of registers */
    *(base + DP8570_OMR) = 0x73;                /* MFO pin is T0, MFO active high driven, INTR active low open drain */
    *(base + DP8570_ICR0) = 0x40;               /* Timer 0 interrupt enable */
    *(base + DP8570_ICR1) = 0;                  /* Disable alarms */

    *(base + DP8570_MSR) &= ~0x40;              /* Back to first set of registers */

#if defined(MACHINE_COMET68K)
    *(base + DP8570_T0LSB) = 0x1A;              /* Timer 0 prescaler: 625KHz / 200 = 3125 = 0x0C35 */
    *(base + DP8570_T0MSB) = 0x06;
#else
#error "You need to define a prescaler for Timer 0 for your machine"
#endif

    /* Set the interrupt vector */
#ifdef CONF_DP8570_AUTOVECTOR
    volatile PFVOID *vector_addr = &VEC_LEVEL1 + (CONF_DP8570_AUTOVECTOR - 1);

    *vector_addr = (PFVOID)interrupt;
#else
#error "TODO: vectored interrupt for DP8570"
#endif

    *(base + DP8570_T0CR) = 0x05;               /* Start the timer */
}

static void __attribute__((interrupt))
interrupt(void)
{
    static UWORD counter = 0;

    CHECKPOINT(0x8570);
    CHECKPOINT(++counter);

    /* Get interrupt conditions */
    volatile UBYTE *msr = (UBYTE *)DP8570_BASE + DP8570_MSR;
    struct dp8570_msr conds = { .u8 = *msr };

    /* Write back to clear them */
    *msr = conds.u8;

    /* Check if timer 0 interrupt pending */
    if (conds.INT) {
        if (conds.T0) {
            /* Timer 0 interrupt, tick! */

            /* vector_5ms() is a function pointer to int_timerc() which performs an RTE, so we need to create a fake
             * exception stack frame with a return address to ourself, and then JMP to vector_5ms */
            asm volatile(
                "   .extern _longframe                      \n\t"
                "   tst.w   _longframe                      \n\t"
                "   beq     1f                              \n\t"
                "   move.w  #0, -(sp)                       \n\t"
                "1: pea     2f                              \n\t"
                "   move.w  sr, -(sp)                       \n\t"
                "   .extern _vector_5ms                     \n\t"
                "   move.l  _vector_5ms, a0                 \n\t"
                "   jmp     (a0)                            \n\t"
                "2:                                         \n\t"
                :
                :
                :
            );
        }
    }
}
