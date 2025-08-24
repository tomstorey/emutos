#ifdef MACHINE_COMET68K

# ifndef CONF_ATARI_HARDWARE
#  define CONF_ATARI_HARDWARE 0
# endif
# ifndef CONF_WITH_ADVANCED_CPU
#  define CONF_WITH_ADVANCED_CPU 1
# endif
# ifndef CONF_WITH_APOLLO_68080
#  define CONF_WITH_APOLLO_68080 0
# endif
# ifndef CONF_WITH_BUS_ERROR
#  define CONF_WITH_BUS_ERROR 1
# endif
# ifndef CONF_WITH_CACHE_CONTROL
#  define CONF_WITH_CACHE_CONTROL 0
# endif
# ifndef ALWAYS_SHOW_INITINFO
#  define ALWAYS_SHOW_INITINFO 1
# endif

#define CONF_WITH_NS16C2552 1
#define NS16C2552_BASE 0x00C20000
#define CONF_NS16C2552_AUTOVECTOR 5             /* Define and set to the IRQ level for autovectored UART interrupt */
#define CONF_NS16C2552_FIFOSIZE 16
#define CONF_WITH_IKBD_NS16C2552 1

#define CONF_WITH_DP8570_TIMER 1
#define DP8570_BASE 0x00C30000
#define CONF_DP8570_AUTOVECTOR 1                /* Define and set to the IRQ level for autovectored timer interrupt */

#define CONF_WITH_COMET_CF 1
#define COMET_CF_BASE 0x00C50000
#define COMET_CF_COUNT 4

/* COMET68k has 4MB on-board, but Im artificially limiting it to 3MB here to give me room to load the EmuTOS binary
 * into the top 1MB using my serial bootloader utility */
# ifndef CONF_STRAM_SIZE
#  define CONF_STRAM_SIZE 0x300000
# endif
# ifndef CONF_WITH_ALT_RAM
#  define CONF_WITH_ALT_RAM 0
# endif
# ifndef CONF_WITH_MFP
#  define CONF_WITH_MFP 0
# endif
# ifndef CONF_WITH_DUART
#  define CONF_WITH_DUART 0
# endif
# ifndef DUART_BASE
#  define DUART_BASE 0xFFAD0000UL
# endif
# ifndef CONF_WITH_DUART_EXTENDED_BAUD_RATES
#  define CONF_WITH_DUART_EXTENDED_BAUD_RATES 0
# endif
# ifndef CONF_WITH_DUART_CHANNEL_B
#  define CONF_WITH_DUART_CHANNEL_B 0
# endif
# ifndef CONF_WITH_IKBD_DUART
#  define CONF_WITH_IKBD_DUART 0
# endif
# ifndef CONF_DUART_TIMER_C
#  define CONF_DUART_TIMER_C 0
# endif
# ifndef DUART_DEBUG_PRINT
#  define DUART_DEBUG_PRINT 0
# endif
# ifndef RS232_DEBUG_PRINT
#  define RS232_DEBUG_PRINT 0
# endif
# ifndef NS16C2552_DEBUG_PRINT
#  define NS16C2552_DEBUG_PRINT 1
#  define ENABLE_KDEBUG 1
# endif

#ifndef CONF_WITH_FDC
# define CONF_WITH_FDC 0
#endif

#ifndef CONF_WITH_ACSI
# define CONF_WITH_ACSI 0
#endif

#ifndef CONF_WITH_SCSI
# define CONF_WITH_SCSI 0
#endif

# ifndef CONF_WITH_IDE
#  define CONF_WITH_IDE 1
# endif
# ifndef CONF_ATARI_IDE
#  define CONF_ATARI_IDE 0
# endif
# ifndef CONF_IDE_NO_RESET
#  define CONF_IDE_NO_RESET 1
# endif

# ifndef CONF_WITH_SDMMC
#  define CONF_WITH_SDMMC 0
# endif

# ifndef CONF_WITH_RESET
#  define CONF_WITH_RESET 0
# endif

# ifndef CONF_SERIAL_CONSOLE
#  define CONF_SERIAL_CONSOLE 1
# endif
# ifndef CONF_SERIAL_CONSOLE_ANSI
#  if CONF_SERIAL_CONSOLE
#   define CONF_SERIAL_CONSOLE_ANSI 1
#  else
#   define CONF_SERIAL_CONSOLE_ANSI 0
#  endif
# endif
# ifndef CONF_SERIAL_CONSOLE_POLLING_MODE
#  define CONF_SERIAL_CONSOLE_POLLING_MODE 0
# endif
#define DEFAULT_BAUDRATE B115200

# ifndef USE_STOP_INSN_TO_FREE_HOST_CPU
#  define USE_STOP_INSN_TO_FREE_HOST_CPU 0
# endif
# ifndef DETECT_NATIVE_FEATURES
#  define DETECT_NATIVE_FEATURES 0
# endif

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
