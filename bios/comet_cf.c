#include "emutos.h"
#include "asm.h"
#include "comet_cf.h"

#if defined(CONF_WITH_COMET_CF) && CONF_WITH_COMET_CF

#define ide_get_and_incr(src, dst) asm volatile("move.l (%1), (%0)+" : "=a"(dst): "a"(src), "0"(dst));
#define ide_put_and_incr(src, dst) asm volatile("move.l (%0)+, (%1)" : "=a"(src): "a"(dst), "0"(src));

void
comet_cf_fast_read(void *interface, UBYTE *buffer, ULONG bufferlen, int need_byteswap)
{
    KDEBUG(("comet_cf_fast_read(): interface=%p  buffer=%p  bufferlen=%u  need_byteswap=%02X\n", interface, buffer, (unsigned int)bufferlen, need_byteswap));

    ULONG *data = interface + 0x200;
    ULONG temp;
    ULONG *dst = (ULONG *)buffer;

    /* Loop counter */
    ULONG ctr;

    if (need_byteswap) {
        /* Byte swaps are a little ugly, do 4 at a time so the code isnt too crazy */
        ctr = bufferlen >> 4;

        for (; ctr; --ctr) {
            temp = *data;
            swpw2(temp);
            *dst++ = temp;

            temp = *data;
            swpw2(temp);
            *dst++ = temp;

            temp = *data;
            swpw2(temp);
            *dst++ = temp;

            temp = *data;
            swpw2(temp);
            *dst++ = temp;
        }
    } else {
        /* Go for broke and do 16 transfers per iteration of the loop */
        ctr = bufferlen >> 6;

        for (; ctr; --ctr) {
            ide_get_and_incr(data, dst);
            ide_get_and_incr(data, dst);
            ide_get_and_incr(data, dst);
            ide_get_and_incr(data, dst);

            ide_get_and_incr(data, dst);
            ide_get_and_incr(data, dst);
            ide_get_and_incr(data, dst);
            ide_get_and_incr(data, dst);

            ide_get_and_incr(data, dst);
            ide_get_and_incr(data, dst);
            ide_get_and_incr(data, dst);
            ide_get_and_incr(data, dst);

            ide_get_and_incr(data, dst);
            ide_get_and_incr(data, dst);
            ide_get_and_incr(data, dst);
            ide_get_and_incr(data, dst);
        }
    }
}

void
comet_cf_fast_write(void *interface, UBYTE *buffer, ULONG bufferlen, int need_byteswap)
{
    KDEBUG(("comet_cf_fast_write(): interface=%p  buffer=%p  bufferlen=%u  need_byteswap=%02X\n", interface, buffer, (unsigned int)bufferlen, need_byteswap));

    ULONG *data = interface + 0x200;
    ULONG temp;
    ULONG *src = (ULONG *)buffer;

    /* Loop counter */
    ULONG ctr;

    if (need_byteswap) {
        /* Byte swaps are a little ugly, do 4 at a time so the code isnt too crazy */
        ctr = bufferlen >> 4;

        for (; ctr; --ctr) {
            temp = *src++;
            swpw2(temp);
            *data = temp;

            temp = *src++;
            swpw2(temp);
            *data = temp;

            temp = *src++;
            swpw2(temp);
            *data = temp;

            temp = *src++;
            swpw2(temp);
            *data = temp;
        }
    } else {
        /* Go for broke and do 16 transfers per iteration of the loop */
        ctr = bufferlen >> 6;

        for (; ctr; --ctr) {
            ide_put_and_incr(src, data);
            ide_put_and_incr(src, data);
            ide_put_and_incr(src, data);
            ide_put_and_incr(src, data);

            ide_put_and_incr(src, data);
            ide_put_and_incr(src, data);
            ide_put_and_incr(src, data);
            ide_put_and_incr(src, data);

            ide_put_and_incr(src, data);
            ide_put_and_incr(src, data);
            ide_put_and_incr(src, data);
            ide_put_and_incr(src, data);

            ide_put_and_incr(src, data);
            ide_put_and_incr(src, data);
            ide_put_and_incr(src, data);
            ide_put_and_incr(src, data);
        }
    }
}

#endif /* defined(CONF_WITH_COMET_CF) && CONF_WITH_COMET_CF */
