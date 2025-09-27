#include "emutos.h"
#include "vectors.h"
#include "biosbind.h"
#include "asm.h"
#include "delay.h"
#include "dp8570.h"

#if CONF_WITH_DP8570_TIMER || CONF_WITH_DP8570_RTC

/* Globals which signal that a DP8570 timer and/or RTC are present */
int has_dp8570_timer;
int has_dp8570_rtc;

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
static void basic_config(void);
static void clock_restart(void);
static UBYTE int2bcd(UWORD a);
static UWORD bcd2int(UBYTE a);

void
dp8570_detect_rtc(void)
{
    has_dp8570_rtc = 0;

    /* Try to read the seconds register to avoid disturbing any flags set in other registers */
    if (check_read_byte(DP8570_BASE + DP8570_SEC)) {
        has_dp8570_rtc = 1;
    }

    KDEBUG(("has_dp8570_rtc = %d\n", has_dp8570_rtc));
}

void
dp8570_init_system_timer(void)
{
    KDEBUG(("dp8570_init_system_timer()\n"));

    /* Apply basic configuration */
    basic_config();

    /* Perform configuration specific to the system timer */
    struct dp8570_msr *msr = (void *)DP8570_BASE + DP8570_MSR;
    struct dp8570_txcr *t0cr = (void *)DP8570_BASE + DP8570_T0CR;
    struct dp8570_icr0 *icr0 = (void *)DP8570_BASE + DP8570_ICR0;
    volatile UBYTE *t0lsb = (void *)DP8570_BASE + DP8570_T0LSB;
    volatile UBYTE *t0msb = (void *)DP8570_BASE + DP8570_T0MSB;

    /* Ensure we are accessing the first set of registers */
    msr->u8 = 0;

    /* Configure Timer 0 to produce a 200hz (5ms) interrupt */
    t0cr->u8 = 0x04;                            /* TCK clock, mode 2 (square wave), timer stopped */

#if defined(MACHINE_COMET68K)
    *t0lsb = 0x1A;                              /* Timer 0 prescaler: 625KHz / 200 = 3125 = 0x0C35 */
    *t0msb = 0x06;
#else
#error "You need to define a prescaler for Timer 0 for your machine"
#endif

    /* Set the interrupt vector and fill in the IRQ chain handler */
    next_vec = Setexc((24 + CONF_DP8570_AUTOVECTOR), (ULONG)&irq_chain);

    irq_chain[3] = (UWORD)((ULONG)&interrupt >> 16); /* Address of our ISR */
    irq_chain[4] = (UWORD)((ULONG)&interrupt);

    irq_chain[8] = (UWORD)(next_vec >> 16);     /* Address of next ISR */
    irq_chain[9] = (UWORD)next_vec;

    /* Start the timer */
    t0cr->TSS = 1;

    /* Access bank 1 */
    msr->u8 = 0x40;

    /* Enable Timer 0 interrupt */
    icr0->ENT0 = 1;

    /* Leave in bank 0 */
    msr->u8 = 0;
}

void
dp8570_init_clock(void)
{
    KDEBUG(("dp8570_init_clock()\n"));

    /* Apply basic configuration */
    basic_config();

    /* Check for oscillator fail and restart if necessary */
    clock_restart();
}

ULONG
dp8570_getdt(void)
{
    const struct dp8570_pfr *pfr = (void *)DP8570_BASE + DP8570_PFR;
    const volatile UBYTE *rtc = (UBYTE *)DP8570_BASE;

    UWORD seconds;
    UWORD minutes;
    UWORD hours;
    UWORD days;
    UWORD months;
    UWORD years;
    UWORD date, time;
    ULONG dt = 0;

    /* Dummy read the Periodic Flag register to clear all flags */
    (void)pfr->u8;

    /* Read registers, and re-read them if the seconds flag is set (rollover event) */
    do {
        seconds = *(rtc + DP8570_SEC);
        minutes = *(rtc + DP8570_MIN);
        hours = *(rtc + DP8570_HR);
        days = *(rtc + DP8570_DAY);
        months = *(rtc + DP8570_MON);
        years = *(rtc + DP8570_YR);
    } while (pfr->PF1S);

    seconds = bcd2int(seconds);
    minutes = bcd2int(minutes);
    hours = bcd2int(hours);
    days = bcd2int(days);
    months = bcd2int(months);
    years = bcd2int(years);

    /* Portions borrowed from amiga_dogetdate() and amiga_dogettime() */

    if (years >= 78) {
        years += 1900;
    } else {
        years += 2000;
    }

    KDEBUG(("dp8570_getdt(): %04d/%02d/%02d %02d:%02d:%02d\n", years, months, days, hours, minutes, seconds));

    /* Packed bit format: YYYYYYYMMMMDDDDD */
    date = (days & 0x1F) | (months & 0xF) << 5 | ((years - 1980) & 0x7F) << 9;

    /* Packed bit format: HHHHHMMMMMMSSSSS */
    time = (seconds & 0x3F) >> 1 | (minutes & 0x3F) << 5 | (hours & 0x1F) << 11;

    dt = MAKE_ULONG(date, time);

    // /* Packed bit format: YYYYYYYMMMMDDDDDHHHHHMMMMMMSSSSS */
    // /* Gives "left shift count >= width of type" warnings ... */
    // dt = (seconds & 0x3F) >> 1 | (minutes & 0x3F) << 5 | (hours & 0x1F) << 11 |
    //      (days & 0x1F) << 16 | (months & 0xF) << 21 | ((years - 1980) & 0x7F) << 24;

    return dt;
}

void
dp8570_setdt(ULONG time)
{
    const struct dp8570_pfr *pfr = (void *)DP8570_BASE + DP8570_PFR;
    struct dp8570_rtmr *rtmr = (void *)DP8570_BASE + DP8570_RTMR;
    volatile UBYTE *rtc = (UBYTE *)DP8570_BASE;

    UBYTE seconds;
    UBYTE minutes;
    UBYTE hours;
    UBYTE days;
    UBYTE months;
    UWORD years;
    UBYTE leap;

    /* Packed bit format: YYYYYYYMMMMDDDDDHHHHHMMMMMMSSSSS */
    seconds = (time & 0x1F) << 1;
    minutes = (time >> 5) & 0x3F;
    hours = (time >> 11) & 0x1F;
    days = (time >> 16) & 0x1F;
    months = (time >> 21) & 0xF;
    years = 1980 + ((time >> 25) & 0x7F);

    KDEBUG(("dp8570_setdt(): %04d/%02d/%02d %02d:%02d:%02d\n", years, months, days, hours, minutes, seconds));

    /* The DP8570 needs to be told when the last leap year was. The range of our clock is 1978 (Amiga epoch) to 2077,
     * and since 2000 was a leap year (divisible by 400) the calculation is pretty simple. */
    leap = years % 4;

    if (years >= 2000 && years <= 2077) {
        years -= 2000;
    } else {
        years -= 1900;
    }

    seconds = int2bcd(seconds);
    minutes = int2bcd(minutes);
    hours = int2bcd(hours);
    days = int2bcd(days);
    months = int2bcd(months);
    years = int2bcd(years);

    /* The datasheet suggests an algorithm for updating the clock registers without stopping the clock.
     * This seems like it may be necessary, because stopping the clock once the timer has been started
     * appears to affect its frequency, and it'll be a bit of a pain to re-initialise everything again.
     *
     * The algorithm is to wait for the 10ms periodic flag, then an additional 15uS. So we'll wait for
     * the 10ms flag, then delay 1ms and then write to the registers. */

    /* Dummy read Periodic Flags to reset them */
    (void)pfr->u8;

    /* Wait for the 10ms flag to be set */
    while (!pfr->PF10MS) {}

    /* Then delay 1ms */
    delay_loop(loopcount_1_msec);

    /* Then write new time */
    *(rtc + DP8570_SEC) = seconds;
    *(rtc + DP8570_MIN) = minutes;
    *(rtc + DP8570_HR) = hours;
    *(rtc + DP8570_DAY) = days;
    *(rtc + DP8570_MON) = months;
    *(rtc + DP8570_YR) = years;
    rtmr->LY = leap;
}

ULONG
dp8570_1ms_loop_calibration(void)
{
    const struct dp8570_pfr *pfr = (void *)DP8570_BASE + DP8570_PFR;
    ULONG result;

    /* For saving the CPU Status Register and IPL when entering/exiting critical sections */
    WORD old_sr;

    /* Check for oscillator fail and restart if necessary */
    clock_restart();

    /* Enter critical section - disable interrupts so they don't interfere with the measurement - max ~2ms */
    old_sr = set_sr(0x2700);

    /* The DP8570 contains a variety of flags which signal various periods of time, one of which
     * is a 1ms interval. Use this flag to calibrate the 1ms software loop. */
    asm volatile (
        "   move.b  (%1), d0            \n\t"   /* Dummy read Periodic Flags to reset them */
        "   moveq.l #0, d1              \n\t"   /* Clear D1 to use as a counter */

        "0: btst    #5, (%1)            \n\t"   /* Wait for the 1ms flag to set */
        "   beq     0b                  \n\t"

        "1: addq.l  #1, d1              \n\t"   /* Increment until the 1ms flag is set again */
        "   btst    #5, (%1)            \n\t"
        "   beq     1b                  \n\t"

        /* Attempt to refine the loop count - based on a 68000 @ 20MHz */
        "   move.l  d1, d2              \n\t"
        "   lsr.l   #1, d2              \n\t"   /* Add 1/8 */
        "   add.l   d2, d1              \n\t"
        "   lsr.l   #1, d2              \n\t"   /* Add 1/4 */
        "   add.l   d2, d1              \n\t"
        "   lsr.l   #1, d2              \n\t"   /* Subtract 1/8 */
        "   sub.l   d2, d1              \n\t"
        "   lsr.l   #1, d2              \n\t"   /* Subtract 1/16 */
        "   sub.l   d2, d1              \n\t"
        "   lsr.l   #1, d2              \n\t"   /* Add 1/32 */
        "   add.l   d2, d1              \n\t"
        "   lsr.l   #1, d2              \n\t"   /* Add 1/64 */
        "   add.l   d2, d1              \n\t"

        // "   lsl.l   #1, d1              \n\t"   /* Compensate for two loop instructions */

        "   move.l  d1, %0              \n\t"
        :"=mr"(result)
        :"a"(pfr)
        :"d0", "d1", "d2"
    );

    /* Exit critical section */
    (void)set_sr(old_sr);

    KDEBUG(("dp8570_1ms_loop_calibration(): %lu\n", result));

    return result;
}

/* Apply basic config to the DP8570, such as clock/oscillator sources, output modes, etc */
static void
basic_config(void)
{
    /* Run basic config only once */
    static UBYTE configured = 0;

    if (configured) {
        return;
    }

    configured = 1;

    struct dp8570_msr *msr = (void *)DP8570_BASE + DP8570_MSR;
    struct dp8570_pfr *pfr = (void *)DP8570_BASE + DP8570_PFR;
    struct dp8570_irr *irr = (void *)DP8570_BASE + DP8570_IRR;
    struct dp8570_rtmr *rtmr = (void *)DP8570_BASE + DP8570_RTMR;
    struct dp8570_omr *omr = (void *)DP8570_BASE + DP8570_OMR;
    struct dp8570_icr0 *icr0 = (void *)DP8570_BASE + DP8570_ICR0;
    struct dp8570_icr1 *icr1 = (void *)DP8570_BASE + DP8570_ICR1;

    /* Access bank 0 */
    msr->u8 = 0;

    /* Configure for battery backed mode */
    pfr->u8 = 0;

    /* INTR shall be sourced from timer 0 - all other sources routed to MFO pin */
    irr->u8 = ~0x08;

    /* Access bank 1 */
    msr->u8 = 0x40;

    /* The clock should be in 24 hour mode, and running from a 32768Hz crystal. Interrupts and timers do not
     * function in the standby state. Keep the state of the CSS bit. */
    rtmr->u8 &= 0x08;

    /* Configure the output modes for T1, INTR and MFO pins:
     *
     *  T1 drives the speaker, and is active high push-pull
     *  INTR is active low open drain
     *  MFO is active high push-pull */
    omr->u8 = 0xB3;

    /* Disable all interrupt sources, maintaining Timer 0 if enabled */
    icr0->u8 &= 0x40;

    /* Disable alarms */
    icr1->u8 = 0;

    /* Leave in bank 0 */
    msr->u8 = 0;
}

static void
clock_restart(void)
{
    struct dp8570_msr *msr = (void *)DP8570_BASE + DP8570_MSR;
    const struct dp8570_pfr *pfr = (void *)DP8570_BASE + DP8570_PFR;
    struct dp8570_rtmr *rtmr = (void *)DP8570_BASE + DP8570_RTMR;
    const volatile UBYTE *rtc = (UBYTE *)DP8570_BASE;

    ULONG ctr;

    /* Ensure we are accessing the first set of registers */
    msr->u8 = 0;

    if (pfr->OSF) {
        KDEBUG(("dp8570 clock_restart(): oscillator fail event, attempting to start clock\n"));

        msr->u8 = 0x40;
        rtmr->CSS = 1;

        /* Fortunately the DP8570 has a register that counts 1/100's of a second, so we should be able to observe
         * this register before and after some delay to see if it changes, and this may indicate that the clock is
         * running again */
        const UBYTE before = *(rtc + DP8570_FRAC);

        for (ctr = 0xFFFFFF; ctr; ctr--) {
            if (*(rtc + DP8570_FRAC) != before) {
                break;
            }
        }

        if (ctr == 0) {
            KDEBUG(("dp8570 clock_restart(): clock does not appear to be running\n"));
            has_dp8570_rtc = 0;
        } else {
            KDEBUG(("dp8570 clock_restart(): clock is running\n"));
        }

        /* Leave in bank 0 */
        msr->u8 = 0;
    }
}

static void
interrupt(void)
{
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
                :"a0"
            );
        }
    }
}

/* int2bcd() is borrowed from clock.c because it is declared static there */
static UBYTE
int2bcd(const UWORD a)
{
    return (a % 10) + ((a / 10) << 4);
}

/* bcd2int() is borrowed from clock.c because it is declared static there */
static UWORD
bcd2int(const UBYTE a)
{
    return (a & 0xf) + ((a >> 4) * 10);
}

#endif /* CONF_WITH_DP8570_TIMER || CONF_WITH_DP8570_RTC */
