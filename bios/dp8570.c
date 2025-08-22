#include "emutos.h"
#include "vectors.h"
#include "dp8570.h"

/* Forward decls */
void dp8570_interrupt(void);

void
dp8570_init_system_timer(void)
{
    volatile UBYTE *reg = (UBYTE *)DP8570_BASE;

    /* Ensure we are accessing the first set of registers */
    *(reg + DP8570_MSR) &= ~0x40;

    /* Configure Timer 0 to produce a 200hz (5ms) interrupt */
    *(reg + DP8570_T0CR) = 0x02;                /* TCK clock, mode 1 (pulse generator), timer stopped */
    *(reg + DP8570_IRR) = 0x08;                 /* Interrupt sourced from Timer 0 */
    *(reg + DP8570_MSR) |= 0x40;                /* Access second set of registers */
    *(reg + DP8570_OMR) = 0x73;                 /* MFO pin is T0, MFO active high driven, INTR active low open drain */
    *(reg + DP8570_ICR0) = 0x40;                /* Timer 0 interrupt enable */
    *(reg + DP8570_ICR1) = 0;                   /* Disable alarms */
    *(reg + DP8570_MSR) &= ~0x40;               /* Back to first set of registers */

#if defined(MACHINE_COMET68K)
    *(reg + DP8570_T0LSB) = 0x35;               /* Timer 0 prescaler: 625KHz / 200 = 3125 = 0x0C35 */
    *(reg + DP8570_T0MSB) = 0x0C;
#else
#error "You need to define a prescaler for Timer 0 for your machine"
#endif

    /* Set the interrupt vector */
#ifdef CONF_DP8570_AUTOVECTOR
    volatile PFVOID *vector_addr = &VEC_LEVEL1 + (CONF_DP8570_AUTOVECTOR - 1);

    *vector_addr = (PFVOID)dp8570_interrupt;
#else
#error "TODO: vectored interrupt for DP8570"
#endif

    *(reg + DP8570_T0CR) = 0x03;                /* Start the timer */
}

void __attribute__((interrupt))
dp8570_interrupt(void)
{
    volatile UBYTE *reg = (UBYTE *)DP8570_BASE;
    static UWORD counter = 0;

    CHECKPOINT(counter);
    counter++;

    /* Get interrupt conditions */
    UBYTE conds = *(reg + DP8570_MSR);

    /* Write back to clear them */
    *(reg + DP8570_MSR) = conds;

    /* Check if timer 0 interrupt pending */
    if (conds & 0x01) {
        if (conds & 0x10) {
            /* Timer 0 interrupt, tick! */
            vector_5ms();
        }
    }
}
