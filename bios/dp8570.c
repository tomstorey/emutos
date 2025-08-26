#include "emutos.h"
#include "vectors.h"
#include "bios.h"
#include "ikbd.h"
#include "dp8570.h"

#if CONF_WITH_DP8570_TIMER || CONF_WITH_DP8570_RTC

/* Globals which signal that a DP8570 timer and/or RTC are present */
int has_dp8570_timer;
int has_dp8570_rtc;

/* Forward decls */
static void interrupt(void);
static void basic_config(void);
static UWORD get_date(void);
static UWORD get_time(void);
static UBYTE int2bcd(UWORD a);
static UWORD bcd2int(UBYTE a);

void
dp8570_detect_rtc(void)
{
    has_dp8570_rtc = 0;

    /* Try to read the seconds register to avoid disturbing any flags set in other registers */
    if (check_read_byte(DP8570_BASE + 6)) {
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

    /* Set the interrupt vector */
#ifdef CONF_DP8570_AUTOVECTOR
    volatile PFVOID *vector_addr = &VEC_LEVEL1 + (CONF_DP8570_AUTOVECTOR - 1);

    *vector_addr = (PFVOID)interrupt;
#else
#error "TODO: vectored interrupt for DP8570"
#endif

    t0cr->TSS = 1;                              /* Start the timer */

    /* Access bank 1 */
    msr->u8 = 0x40;

    icr0->ENT0 = 1;                             /* Enable Timer 0 interrupt */

    /* Leave in bank 0 */
    msr->u8 = 0;
}

void
dp8570_init_clock(void)
{
    KDEBUG(("dp8570_init_clock()\n"));

    /* Apply basic configuration */
    basic_config();

    struct dp8570_msr *msr = (void *)DP8570_BASE + DP8570_MSR;
    const struct dp8570_pfr *pfr = (void *)DP8570_BASE + DP8570_PFR;
    struct dp8570_rtmr *rtmr = (void *)DP8570_BASE + DP8570_RTMR;
    const volatile UBYTE *rtc = (UBYTE *)DP8570_BASE;

    ULONG ctr;

    /* Ensure we are accessing the first set of registers */
    msr->u8 = 0;

    if (pfr->OSF) {
        KDEBUG(("dp8570_init_clock(): oscillator fail event, attempting to start clock\n"));

        msr->u8 = 0x40;
        rtmr->CSS = 1;

        /* Fortunately the DP8570 has a register that counts 1/100's of a second, so we should be able to observe
         * this register before and after some delay to see if it changes, and this may indicate that the clock is
         * running again */
        const UBYTE before = *(rtc + 5);

        for (ctr = 0xFFFFFF; ctr; ctr--) {
            if (*(rtc + 5) != before) {
                break;
            }
        }

        if (ctr == 0) {
            KDEBUG(("dp8570_init_clock(): clock does not appear to be running\n"));
            has_dp8570_rtc = 0;
        } else {
            KDEBUG(("dp8570_init_clock(): clock is running\n"));
        }

        /* Leave in bank 0 */
        msr->u8 = 0;
    }
}

LONG
dp8570_getdt(void)
{
    return MAKE_ULONG(get_date(), get_time());
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

static void __attribute__((interrupt))
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

static UWORD
get_date(void)
{
    const volatile UBYTE *rtc = (UBYTE *)DP8570_BASE;

    UWORD days;
    UWORD months;
    UWORD years;
    UWORD date;

    do {
        days = *(rtc + DP8570_DAY);
        months = *(rtc + DP8570_MON);
        years = *(rtc + DP8570_YR);
    } while (days != *(rtc + DP8570_DAY));

    days = bcd2int(days);
    months = bcd2int(months);
    years = bcd2int(years);

    KDEBUG(("dp8570 get_date() %02d/%02d/%02d\n", years, months, days));

    /* Borrowed from amiga_dogetdate() */

    if (years >= 78) {
        years += 1900;
    } else {
        years += 2000;
    }

    if (years < 1980) {
        /* This date can't be represented in BDOS format. */
        return HIWORD(DEFAULT_DATETIME);
    }

    /* Packed bit format: YYYYYYYMMMMDDDDD */
    date = (days & 0x1F) | (months & 0xF) << 5 | ((years - 1980) & 0x7F) << 9;

    return date;
}

static UWORD
get_time(void)
{
    const volatile UBYTE *rtc = (UBYTE *)DP8570_BASE;

    UWORD seconds;
    UWORD minutes;
    UWORD hours;
    UWORD time;

    do {
        seconds = *(rtc + DP8570_SEC);
        minutes = *(rtc + DP8570_MIN);
        hours = *(rtc + DP8570_HR);
    } while (seconds != *(rtc + DP8570_SEC));

    seconds = bcd2int(seconds);
    minutes = bcd2int(minutes);
    hours = bcd2int(hours);

    KDEBUG(("dp8570 get_time() %02d:%02d:%02d\n", hours, minutes, seconds));

    /* Borrowed from amiga_dogettime() */

    /* Packed bit format: HHHHHMMMMMMSSSSS */
    time = ((seconds >> 1) & 0x1F) | (minutes & 0x3F) << 5 | (hours & 0x1F) << 11;

    return time;
}

/* int2bcd() is borrowed from clock.c because it is declared static there */
static UBYTE
int2bcd(UWORD a)
{
    return (a % 10) + ((a / 10) << 4);
}

/* bcd2int() is borrowed from clock.c because it is declared static there */
static UWORD
bcd2int(UBYTE a)
{
    return (a & 0xf) + ((a >> 4) * 10);
}

#endif /* CONF_WITH_DP8570_TIMER || CONF_WITH_DP8570_RTC */
