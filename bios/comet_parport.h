#ifndef COMET_PARPORT_H
#define COMET_PARPORT_H

#if defined(CONF_WITH_COMET_PARPORT) && CONF_WITH_COMET_PARPORT

#include "portab.h"

void comet_parport_init(void);
LONG comet_parport_bcostat(void);
LONG comet_parport_prnout(WORD c);
LONG comet_parport_bconout0(WORD dev, WORD c);

#endif /* defined(CONF_WITH_COMET_PARPORT) && CONF_WITH_COMET_PARPORT */

#endif /* COMET_PARPORT_H */
