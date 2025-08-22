#ifdef MACHINE_COMET68K

#define CONF_ATARI_HARDWARE 0
#define CONF_WITH_ADVANCED_CPU 1
#define CONF_WITH_APOLLO_68080 0
#define CONF_WITH_BUS_ERROR 1
#define CONF_WITH_CACHE_CONTROL 0
#define ALWAYS_SHOW_INITINFO 1

/* COMET68k has 4MB on-board, but Im artificially limiting it to 3MB here to give me room to load the EmuTOS binary
 * into the top 1MB using my serial bootloader utility */
#define CONF_STRAM_SIZE 0x300000
#define CONF_WITH_ALT_RAM 0
#define CONF_WITH_MFP 0
#define CONF_WITH_DUART 0

#define CONF_WITH_NS16C2552 1
#define NS16C2552_BASE 0x00C20000

#define CONF_WITH_DP8570_TIMER 1
#define DP8570_BASE 0x00C30000
#define CONF_DP8570_AUTOVECTOR 1                /* Define and set to the IRQ level for autovectored timer interrupt */

#define RS232_DEBUG_PRINT 0

#define CONF_WITH_FDC 0
#define CONF_WITH_ACSI 0
#define CONF_WITH_SCSI 0
#define CONF_WITH_SDMMC 0

#define CONF_WITH_IDE 1
#define CONF_ATARI_IDE 1
#undef CONF_IDE_NO_RESET

#define CONF_WITH_RESET 0

#define CONF_DUART_TIMER_C 0

#define CONF_SERIAL_CONSOLE 1
#ifndef CONF_SERIAL_CONSOLE_ANSI
# if CONF_SERIAL_CONSOLE
#  define CONF_SERIAL_CONSOLE_ANSI 1
# else
#  define CONF_SERIAL_CONSOLE_ANSI 0
# endif
#endif
#define CONF_SERIAL_CONSOLE_POLLING_MODE 0
#define DEFAULT_BAUDRATE B115200

#define CHECKPOINT(v) { *(volatile UWORD *)(0xC00000) = (v); }
#define FATAL(v) { *(volatile UWORD *)(0xC00000) = (v); HCF(); }

/* Halt and catch fire! Halts the processor in a state where only an NMI would wake it, but
 * loops endlessly in this state. */
#define HCF()                                                                                   \
{                                                                                               \
    for (;;) {                                                                                  \
        asm volatile(                                                                           \
            "stop #0x2700                                   \n\t"                               \
        );                                                                                      \
    }                                                                                           \
}

#endif /* MACHINE_COMET68K */
