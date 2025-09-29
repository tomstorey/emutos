#include "emutos.h"
#include "vectors.h"
#include "comet_parport.h"

#if CONF_WITH_COMET_PARPORT

static volatile UBYTE *base = NULL;

WORD
comet_parport_init(void)
{
    WORD ret = -1;

    /* Pointer to the COMET parallel printer interface */
    base = (UBYTE *)COMET_PARPORT_BASE;

    /* Attempt to read from the data buffer */
    KDEBUG(("comet_parport_init(): check for COMET parallel printer interface at %p", base));

    if (check_read_byte((long)base)) {
        /* Set default pin states */
        *(base + 2) = 0x8C;

        /* Printer is available */
        ret = 0;

        KDEBUG((", card present\n"));
    } else {
        /* Reset pointer */
        base = NULL;

        KDEBUG((", controller absent\n"));
    }

    return ret;
}

LONG
comet_parport_bcostat(void)
{
    KDEBUG(("comet_parport_bcostat()\n"));

    const volatile UBYTE *status = base + 3;

    if (base != NULL) {
        if (*status & 0x80) {
            /* Printer is available */
            return -1;
        }
    }

    /* Printer not available */
    return 0;
}

LONG
comet_parport_prnout(WORD c)
{
    KDEBUG(("comet_parport_prnout() TODO\n"));

    return 1L;
}

LONG
comet_parport_bconout0(WORD dev, WORD c)
{
    KDEBUG(("comet_parport_bconout0() TODO\n"));

    return 0L;
}

#endif /* CONF_WITH_COMET_PARPORT */

