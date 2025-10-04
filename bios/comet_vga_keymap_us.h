#ifndef COMET_VGA_KEYMAP_H
#define COMET_VGA_KEYMAP_H

/* A 0 in any positions means no translation is required, and the raw keyboard code may be queued with ikbdraw().
 *
 * -1 means the code should be consumed and not queued with ikbdraw().
 *
 * Any other value is the translated code that is to be queued with ikbdraw().
 */

/* Map for normal key presses - numlock disengaged */
static const UBYTE ps2_scancode_map[] = {
    -1,                             0,                              0,                              0,
    0,                              0,                              0,                              0,
    0,                              0,                              0,                              0,
    0,                              0,                              0,                              0,

    0,                              0,                              0,                              0,
    0,                              0,                              0,                              0,
    0,                              0,                              0,                              0,
    0,                              0,                              0,                              0,

    0,                              0,                              0,                              0,
    0,                              0,                              0,                              0,
    0,                              0,                              0,                              0,
    0,                              0,                              0,                              0,

    0,                              0,                              0,                              0,
    0,                              0,                              0,                              -1 /* Numpad * */,
    0,                              0,                              0,                              0,
    0,                              0,                              0,                              0,

    0,                              0,                              0,                              0,
    0,                              0,                              0,                              0 /* Numpad Home */,
    0 /* Numpad Up */,              0 /* Numpad Pg Up */,           0,                              0 /* Numpad Left */,
    -1,                             0 /* Numpad Right */,           0,                              0 /* Numpad End */,

    0 /* Numpad Down */,            0 /* Numpad Pg Dn */,           0 /* Numpad Insert */,          0,
    -1,                             -1,                             -1,                             0,
    0,                              -1,                             -1,                             -1,
    -1,                             -1,                             -1,                             -1,

    -1,                             -1,                             -1,                             -1,
    -1,                             -1,                             -1,                             -1,
    -1,                             -1,                             -1,                             -1,
    -1,                             -1,                             -1,                             -1,

    -1,                             -1,                             -1,                             -1,
    -1,                             -1,                             -1,                             -1,
    -1,                             -1,                             -1,                             -1,
    -1,                             -1,                             -1,                             -1
};

/* Map for normal key presses - numlock engaged */
static const UBYTE ps2_scancode_map_numlock[] = {
    -1,                             0,                              0,                              0,
    0,                              0,                              0,                              0,
    0,                              0,                              0,                              0,
    0,                              0,                              0,                              0,

    0,                              0,                              0,                              0,
    0,                              0,                              0,                              0,
    0,                              0,                              0,                              0,
    0,                              0,                              0,                              0,

    0,                              0,                              0,                              0,
    0,                              0,                              0,                              0,
    0,                              0,                              0,                              0,
    0,                              0,                              0,                              0,

    0,                              0,                              0,                              0,
    0,                              0,                              0,                              -1 /* Numpad * */,
    0,                              0,                              0,                              0,
    0,                              0,                              0,                              0,

    0,                              0,                              0,                              0,
    0,                              0,                              0,                              0x08 /* Numpad 7 */,
    0x09 /* Numpad 8 */,            0x0A /* Numpad 9 */,            0,                              0x05 /* Numpad 4 */,
    0x06 /* Numpad 5 */,            0x07 /* Numpad 6 */,            0,                              0x02 /* Numpad 1 */,

    0x03 /* Numpad 2 */,            0x04 /* Numpad 3 */,            0x0B /* Numpad 0 */,            0x34 /* Numpad . */,
    -1,                             -1,                             -1,                             0,
    0,                              -1,                             -1,                             -1,
    -1,                             -1,                             -1,                             -1,

    -1,                             -1,                             -1,                             -1,
    -1,                             -1,                             -1,                             -1,
    -1,                             -1,                             -1,                             -1,
    -1,                             -1,                             -1,                             -1,

    -1,                             -1,                             -1,                             -1,
    -1,                             -1,                             -1,                             -1,
    -1,                             -1,                             -1,                             -1,
    -1,                             -1,                             -1,                             -1
};

/* Key map for single escaped keys */
static const UBYTE ps2_extended_scancode_map[] = {
    -1,                             -1,                             -1,                             -1,
    -1,                             -1,                             -1,                             -1,
    -1,                             -1,                             -1,                             -1,
    -1,                             -1,                             -1,                             -1,

    -1,                             -1,                             -1,                             -1,
    -1,                             -1,                             -1,                             -1,
    -1,                             -1,                             -1,                             -1,
    0 /* Numpad Enter */,           0 /* Right ctrl */,             -1,                             -1,

    -1,                             -1,                             -1,                             -1,
    -1,                             -1,                             -1,                             -1,
    -1,                             -1,                             -1,                             -1,
    -1,                             -1,                             -1,                             -1,

    -1,                             -1,                             -1,                             -1,
    -1,                             0 /* Numpad / */,               -1,                             -1,
    -1,                             -1,                             -1,                             -1,
    -1,                             -1,                             -1,                             -1,

    -1,                             -1,                             -1,                             -1,
    -1,                             -1,                             -1,                             0 /* Home */,
    0 /* Up arrow */,               0 /* Pg Up */,                  -1,                             0 /* Left Arrow */,
    -1,                             0 /* Right arrow */,            -1,                             0 /* End */,

    0 /* Down arrow */,             0 /* Pg Dn */,                  0 /* Insert */,                 0 /* Delete */,
    -1,                             -1,                             -1,                             -1,
    -1,                             -1,                             -1,                             -1,
    -1,                             -1,                             -1,                             -1,

    -1,                             -1,                             -1,                             -1,
    -1,                             -1,                             -1,                             -1,
    -1,                             -1,                             -1,                             -1,
    -1,                             -1,                             -1,                             -1,

    -1,                             -1,                             -1,                             -1,
    -1,                             -1,                             -1,                             -1,
    -1,                             -1,                             -1,                             -1,
    -1,                             -1,                             -1,                             -1
};

/* Key map for double escaped keys */
static const UBYTE ps2_extended2_scancode_map[] = {
    -1,                             -1,                             -1,                             -1,
    -1,                             -1,                             -1,                             -1,
    -1,                             -1,                             -1,                             -1,
    -1,                             -1,                             -1,                             -1,

    -1,                             -1,                             -1,                             -1,
    -1,                             -1,                             -1,                             -1,
    -1,                             -1,                             -1,                             -1,
    -1,                             -1,                             -1,                             -1,

    -1,                             -1,                             -1,                             -1,
    -1,                             -1,                             -1,                             -1,
    -1,                             -1,                             -1,                             -1,
    -1,                             -1,                             -1,                             -1,

    -1,                             -1,                             -1,                             -1,
    -1,                             -1,                             -1,                             -1,
    -1,                             -1,                             -1,                             -1,
    -1,                             -1,                             -1,                             -1,

    -1,                             -1,                             -1,                             -1,
    -1,                             -1,                             -1,                             -1,
    -1,                             -1,                             -1,                             -1,
    -1,                             -1,                             -1,                             -1,

    -1,                             -1,                             -1,                             -1,
    -1,                             -1,                             -1,                             -1,
    -1,                             -1,                             -1,                             -1,
    -1,                             -1,                             -1,                             -1,

    -1,                             -1,                             -1,                             -1,
    -1,                             -1,                             -1,                             -1,
    -1,                             -1,                             -1,                             -1,
    -1,                             -1,                             -1,                             -1,

    -1,                             -1,                             -1,                             -1,
    -1,                             -1,                             -1,                             -1,
    -1,                             -1,                             -1,                             -1,
    -1,                             -1,                             -1,                             -1
};

#endif /* COMET_VGA_KEYMAP_H */
