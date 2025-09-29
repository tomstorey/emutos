#ifndef COMET_CF_H
#define COMET_CF_H

#include "emutos.h"

#if CONF_WITH_COMET_CF

#define COMET_CF_CSR_OFFSET 0x20

struct comet_cf_csr {
    union {
        struct {
            volatile UBYTE IN_USE:1;
            volatile UBYTE FUNC1:1;
            volatile UBYTE T1:1;
            volatile UBYTE T0:1;
            volatile UBYTE RESET:1;
            volatile UBYTE VS2:1;
            volatile UBYTE VS1:1;
            volatile UBYTE CD:1;
        };
        struct {
            volatile UBYTE :2;
            volatile UBYTE T:2;
            volatile UBYTE :4;
        };
        volatile UBYTE u8;
    };
} __attribute__((packed));

void comet_cf_fast_read(void *interface, UBYTE *buffer, ULONG bufferlen, int need_byteswap);
void comet_cf_fast_write(void *interface, UBYTE *buffer, ULONG bufferlen, int need_byteswap);

#endif /* CONF_WITH_COMET_CF */

#endif //COMET_CF_H
