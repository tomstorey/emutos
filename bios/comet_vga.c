#define ENABLE_KDEBUG

#include "emutos.h"

#if CONF_WITH_COMET_VGA

#include "vectors.h"
#include "conout.h"
#include "lineavars.h"
#include "tosvars.h"
#include "string.h"
#include "ikbd.h"
#include "comet_vga.h"
#include "comet_vga_font1.h"
#include "comet_vga_keymap_us_set2.h"

/* Macros for reading and writing hardware registers */
#define CRTC_WR_CSR0(v) (*(volatile UWORD *)(COMET_VGA_BASE + COMET_VGA_REG_FILE) = (v))
#define CRTC_RD_CSR0() (*(volatile UWORD *)(COMET_VGA_BASE + COMET_VGA_REG_FILE))
#define CRTC_WR_CSR1(v) (*(volatile UWORD *)(COMET_VGA_BASE + COMET_VGA_REG_FILE + COMET_VGA_REG_FILE_CSR1) = (v))
#define CRTC_RD_CSR1() (*(volatile UWORD *)(COMET_VGA_BASE + COMET_VGA_REG_FILE + COMET_VGA_REG_FILE_CSR1))
#define CRTC_WR(v, r) (*(volatile UWORD *)(COMET_VGA_BASE + COMET_VGA_REG_FILE + (r)) = (v))
#define RAMDAC_WR(v, r) (*(volatile UBYTE *)(COMET_VGA_BASE + COMET_VGA_RAMDAC + (r)) = (v))

#define I8042_RD(r) (*(volatile UBYTE *)(COMET_VGA_BASE + COMET_VGA_I8042 + (r)))
#define I8042_WR(v, r) (*(volatile UBYTE *)(COMET_VGA_BASE + COMET_VGA_I8042 + (r)) = (v))

/* Keyboard/mouse controller Status register bit masks */
#define VT82C42_STATUS_OBF 0x01
#define VT82C42_STATUS_IBF 0x02
#define VT82C42_STATUS_MS_DATA 0x20

/* Keyboard/mouse controller Command register bit masks */
#define VT82C42_CMD_KB_OBF_INT 0x01
#define VT82C42_CMD_MS_OBF_INT 0x02
#define VT82C42_CMD_KB_DISABLE 0x10
#define VT82C42_CMD_MS_DISABLE 0x20
#define VT82C42_CMD_PC_COMPAT 0     /* Set to 0x40 to enable scan code translation */

/* Keyboard LED bit masks */
#define KB_LED_SCROLL 0x01
#define KB_LED_NUM 0x02
#define KB_LED_CAPS 0x04

/* Used to specify which port a byte of data is destined for on the keyboard/mouse controller */
enum VT82C42_port {
    VT82C42_KB = 0,
    VT82C42_MS = 1
};

/* Keyboard state machine states */
enum key_state {
    KEY_STATE_DEFAULT = 0,
    KEY_STATE_UNTIL_BREAK,
    KEY_STATE_ESCAPE,
    KEY_STATE_PAUSE_BREAK
};

/* Struct of the CSR0 register in the CRTC */
struct csr0 {
    union {
        struct {
            volatile UWORD IN_USE:1;
            volatile UWORD :2;
            volatile UWORD V_BLANK:1;
            volatile UWORD FONT:1;
            volatile UWORD :1;
            volatile UWORD BANK_SEL:1;
            volatile UWORD REGEN_EN:1;
            volatile UWORD INTENSITY:1;
            volatile UWORD MODE2:1;
            volatile UWORD MODE1:1;
            volatile UWORD MODE0:1;
            volatile UWORD V_POL:1;
            volatile UWORD H_POL:1;
            volatile UWORD BLANK:1;
            volatile UWORD RUN:1;
        };
        struct {
            volatile UWORD :9;
            volatile UWORD MODE:3;
            volatile UWORD :4;
        };
        volatile UWORD u16;
    };
};

/* Struct of the CSR1 register in the CRTC */
struct csr1 {
    union {
        struct {
            volatile UWORD I:1;      /* Interrupt active (K OR C OR S) */
            volatile UWORD K:1;      /* Keyboard/mouse interrupt active */
            volatile UWORD C:1;      /* CRTC V blank interrupt active */
            volatile UWORD S:1;      /* I2C controller interrupt active */
            volatile UWORD :1;
            volatile UWORD K_EN:1;   /* Keyboard/mouse interrupt enable */
            volatile UWORD C_EN:1;   /* CRTC V blank interrupt enable */
            volatile UWORD S_EN:1;   /* I2C controller interrupt enable */
            volatile UWORD V:8;      /* Vector base address */
        };
        volatile UWORD u16;
    };
};

/* Palette map */
#define PAL_BLACK          0x00
#define PAL_BLUE           0x01
#define PAL_GREEN          0x02
#define PAL_CYAN           0x03
#define PAL_RED            0x04
#define PAL_MAGENTA        0x05
#define PAL_BROWN          0x06
#define PAL_WHITE          0x07
#define PAL_GREY           0x08
#define PAL_LTBLUE         0x09
#define PAL_LTGREEN        0x0A
#define PAL_LTCYAN         0x0B
#define PAL_LTRED          0x0C
#define PAL_LTMAGENTA      0x0D
#define PAL_LTYELLOW       0x0E
#define PAL_LTWHITE        0x0F

static const UWORD palette_map[] = {
    PAL_LTWHITE, PAL_RED, PAL_GREEN, PAL_BROWN,
    PAL_BLUE, PAL_MAGENTA, PAL_CYAN, PAL_WHITE,
    PAL_GREY, PAL_LTRED, PAL_LTGREEN, PAL_LTYELLOW,
    PAL_LTBLUE, PAL_LTMAGENTA, PAL_LTCYAN, PAL_BLACK
};

/* Text mode configuration - 80x25 screen with 9x16 characters, 16fg, 8bg, blinking text */
static const UWORD text_mode1_cfg[15] = {
    0x8187, (0x0200 | COMET_VGA_VECTOR_BASE), 0x0063, 0x0050, 0x0053, 0x0F06, 0x001B, 0x0002,
    0x0019, 0x001A, 0x000F, 0x600F, 0x0000, 0x0000, 0x0050
};

/* Default 16 colour palette */
static const UBYTE palette_16[48] = {
    0x00, 0x00, 0x00,   /* Black */
    0x00, 0x00, 0xAA,   /* Blue */
    0x00, 0xAA, 0x00,   /* Green */
    0x00, 0xAA, 0xAA,   /* Cyan */
    0xAA, 0x00, 0x00,   /* Red */
    0xAA, 0x00, 0xAA,   /* Magenta */
    0xAA, 0x55, 0x00,   /* Brown */
    0xAA, 0xAA, 0xAA,   /* White */
    0x55, 0x55, 0x55,   /* Grey */
    0x55, 0x55, 0xFF,   /* Lt Blue */
    0x55, 0xFF, 0x55,   /* Lt Green */
    0x55, 0xFF, 0xFF,   /* Lt Cyan */
    0xFF, 0x55, 0x55,   /* Lt Red */
    0xFF, 0x55, 0xFF,   /* Lt Magenta */
    0xFF, 0xFF, 0x55,   /* Lt Yellow */
    0xFF, 0xFF, 0xFF    /* Lt White */
};

/* A "pointer" to the location in regen memory representing the top left corner of the display. The value is stored as
 * an integer, from which pointers into regen memory can be created.
 *
 * COMET VGA regen buffers are 128Kbyte each, but 16-bit wide to store a character and its attributes, hence 65536
 * possible character positions. */
static UWORD regen_start = 0;

/* A "pointer" to the current position of the cursor in regen memory */
static UWORD cursor_pos = 0;

/* Indicates whether a peripheral has been detected */
static BOOL have_video = FALSE;
static BOOL have_vt82c42 = FALSE;

/* Holds the state of the keyboard LEDs, which can also be used to determine whether Num/Caps/Scroll locks have been
 * engaged or not */
static UBYTE kb_leds = 0;

/* Forward decls */
static void init_linea_vars(void);
static void set_text_mode(void);
static void load_palette(void);
static void load_font1(void);
static void interrupt_v_blank(void);
static void vt82c42_write_wait(UBYTE data, UBYTE reg);
static UBYTE vt82c42_cmd_data_polled(UBYTE cmd);
static UBYTE vt82c42_data_data_polled(UBYTE data);
static UBYTE vt82c42_data_polled(void);
static BOOL vt82c42_data_polled_timeout(UBYTE *data);
static UBYTE vt82c42_send_device_cmd(enum VT82C42_port port, UBYTE cmd);
static UBYTE vt82c42_get_cmd_byte(void);
static void vt82c42_set_cmd_byte(UBYTE cmd);
static BOOL device_keyboard_init(void);
static BOOL device_keyboard_reset(void);
static void device_keyboard_led_animate(void);
static BOOL device_mouse_init(void);
static BOOL device_mouse_reset(void);
static BOOL device_mouse_configure(void);
static void interrupt_vt82c42(void);
static void vt82c42_handle_key(UBYTE code);
static void vt82c42_handle_mouse(UBYTE code);

void
comet_vga_screen_init(void)
{
    KDEBUG(("comet_vga_screen_init()\n"));

    /* Reset have flag */
    have_video = FALSE;

    if (!check_read_word(COMET_VGA_BASE + COMET_VGA_REG_FILE)) {
        KDEBUG(("comet_vga_screen_init(): COMET VGA card not detected at %p\n", (void *)(COMET_VGA_BASE + COMET_VGA_REG_FILE)));

        return;
    }

    /* We have a card */
    have_video = TRUE;

    /* Reset vars */
    regen_start = 0;
    cursor_pos = 0;

    set_text_mode();
    init_linea_vars();
}

static void
init_linea_vars(void)
{
    /* Screen address */
    v_bas_ad = (UBYTE *)COMET_VGA_REGEN_ADDR;

    /* Fake 640x400x2 video mode (ST high) */
    sshiftmod = 2;

    /* Line A vars */
    /* Number of bitplanes - set to 16 to disable */
    v_planes = 16;

    /* Bytes per scan-line */
    BYTES_LIN = 80;
}

static void
set_text_mode(void)
{
    UWORD i;

    volatile UWORD *reg = (volatile UWORD *)(COMET_VGA_BASE + COMET_VGA_REG_FILE);

    /* Reset CSR0 - disables the display, resets the CRTC, selects Font 1, etc etc */
    CRTC_WR_CSR0(0);

    load_palette();
    load_font1();

    /* Configure CRTC registers */
    for (i = 2; i < 15; i++) {
        *(reg + i) = text_mode1_cfg[i];
    }

    /* Configure V Blank interrupt vector */
    PFVOID *v_blank = (PFVOID *)((COMET_VGA_VECTOR_BASE + COMET_VGA_V_BLANK_VECTOR) << 2);
    *v_blank = interrupt_v_blank;

    /* Configure CSR1, which will enable interrupts */
    CRTC_WR_CSR1(text_mode1_cfg[1]);

    /* Configure CSR0, which will enable the CRTC and display */
    CRTC_WR_CSR0(text_mode1_cfg[0]);
}

static void __attribute__((noinline))
load_palette(void)
{
    UWORD i;

    /* Set pixel mask for 16 colour palette */
    RAMDAC_WR(0x0F, COMET_VGA_RAMDAC_MASK);

    /* Set address register to 0 to start writing palette data */
    RAMDAC_WR(0, COMET_VGA_RAMDAC_ADDR_WR);

    /* Load palette - 16 RGB triplets */
    for (i = 0; i < 16 * 3; i++) {
        RAMDAC_WR(palette_16[i] >> 2, COMET_VGA_RAMDAC_PALRAM);
    }
}

static void
load_font1(void)
{
    volatile UBYTE *ram = (volatile UBYTE *)(COMET_VGA_BASE + COMET_VGA_FONTRAM);

    UWORD i;

    /* Load font into RAM */
    for (i = 0; i < 256 * 16; i++) {
        *ram = comet_font1_dat_table[i];

        /* Font RAM is only available on the upper half of the data bus, therefore increment pointer by 2 */
        ram += 2;
    }
}

static UWORD
bcd_count(UWORD val)
{
    if ((val & 0x000F) == 0x000A) {
        val &= 0xFFF0;
        val += 0x0010;
    }

    if ((val & 0x00F0) == 0x00A0) {
        val &= 0xFF0F;
        val += 0x0100;
    }

    if ((val & 0x0F00) == 0x0A00) {
        val &= 0xF0FF;
        val += 0x1000;
    }

    if ((val & 0xF000) == 0xA000) {
        val &= 0x0FFF;
    }

    return val;
}

static void __attribute__((interrupt))
interrupt_v_blank(void)
{
    static UWORD scaler = 0;
    static UWORD count = 0;

    const struct csr1 int_src = { .u16 = CRTC_RD_CSR1() };

    if (int_src.C) {
        /* Clear the interrupt condition */
        CRTC_WR_CSR1(int_src.u16 & ~0x0200);
        CRTC_WR_CSR1(int_src.u16);

        scaler++;

        if (scaler == 70) {
            scaler = 0;

            count = bcd_count(count + 1);

            CLEAR_DEBUG();
            CHECKPOINT(count);
        }
    }
}

void
ascii_out(const int ch)
{
    if (!have_video) {
        /* Dont do any video operations if we dont have a card */
        return;
    }

    /* Take working copies of cursor X and Y */
    UWORD x = v_cur_cx;
    UWORD y = v_cur_cy;

    /* First off, the character can be written to the current cursor position as that has already been calculated
     * prior to this character write */
    UWORD *this = (UWORD *)COMET_VGA_REGEN_ADDR;
    this += cursor_pos;

    if (v_stat_0 & M_REVID) {
        /* Reverse fg/bg colours */
        *this = (ch & 0xFF) << 8 | palette_map[v_col_bg & 0xF] | palette_map[v_col_fg & 0xF] << 4;
    } else {
        /* Normal fg/bg colours */
        *this = (ch & 0xFF) << 8 | palette_map[v_col_bg & 0xF] << 4 | palette_map[v_col_fg & 0xF];
    }

    /* Advance the cursor X position and wrap */
    x++;

    if (x > v_cel_mx) {
        /* Back to start of line */
        x = 0;

        /* Advance cursor Y and wrap */
        y++;

        if (y > v_cel_my) {
            /* Keep at bottom of display area */
           y = v_cel_my;

            /* Regen start should be advanced through memory */
            regen_start += BYTES_LIN;

            /* Blank bottom row of display area */
            blank_out(0, y, v_cel_mx, y);
        }
    }

    /* Update CRTC regen start */
    CRTC_WR(regen_start, COMET_VGA_REG_FILE_REGEN_START);

    /* Move the cursor to the new X and Y, which will update all required background variables */
    move_cursor(x, y);
}

void
move_cursor(int x, int y)
{
    if (!have_video) {
        /* Dont do any video operations if we dont have a card */
        return;
    }

    /* Take a copy of the regen starting address (i.e. top left corner) */
    UWORD work = regen_start;

    /* Bounds check supplied X and Y */
    if (x < 0) {
        x = 0;
    } else if (x > v_cel_mx) {
        x = v_cel_mx;
    }

    if (y < 0) {
        y = 0;
    } else if (y > v_cel_my) {
        y = v_cel_my;
    }

    /* Update position vars */
    v_cur_cx = x;
    v_cur_cy = y;

    /* Calculate new cursor position */
    work += y * BYTES_LIN;
    work += x;

    /* Save the new cursor position */
    cursor_pos = work;

    /* Write new cursor position to CRTC */
    CRTC_WR(work, COMET_VGA_REG_FILE_CURSOR_ADDR);
}

void
blank_out(const int topx, const int topy, const int botx, const int boty)
{
    if (!have_video) {
        /* Dont do any video operations if we dont have a card */
        return;
    }

    /* Take a copy of the regen starting address (i.e. top left corner) */
    UWORD work = regen_start;
    size_t x, y;

    /* Figure out the first location to be blanked */
    work += topy * BYTES_LIN;
    work += topx;

    for (y = topy; y <= boty; y++) {
        /* Make a pointer into regen memory */
        UWORD *this = (UWORD *)COMET_VGA_REGEN_ADDR;
        this += work;

        for (x = topx; x <= botx; x++) {
            *this = 0x0000 | palette_map[v_col_bg & 0xF] << 4 | palette_map[v_col_fg & 0xF];
            this++;
        }

        /* Move to the same topx position on the next row */
        work += BYTES_LIN;
    }
}

void
invert_cell(int x, int y)
{
    if (!have_video) {
        /* Dont do any video operations if we dont have a card */
        return;
    }

    /* invert_cell() seems to be related to a software cursor, which is not necessary with COMET VGA, since it has a
     * hardware cursor.
     *
     * The code below does work to invert a character cell, but it is being left commented out because a hardware
     * cursor is implemented instead. */

    // KDEBUG(("comet_vga invert_cell() x=%d y=%d\n", x, y));
    //
    // v_stat_0 |= M_CRIT;                 /* start of critical section. */
    //
    // /* Take a copy of the regen starting address (i.e. top left corner) */
    // UWORD work = regen_start;
    //
    // /* Figure out the location to be inverted */
    // work += y * BYTES_LIN;
    // work += x;
    //
    // /* Make a pointer into regen memory */
    // UWORD *this = (UWORD *)COMET_VGA_REGEN_ADDR;
    // this += work;
    //
    // /* Get the char/attr pair from regen memory */
    // UWORD data = *this;
    //
    // /* Swap fg/bg color indexes */
    // data = data & 0xFF00 | (data & 0x00F0) >> 4 | (data & 0x000F) << 4;
    //
    // /* Put it back */
    // *this = data;
    //
    // v_stat_0 &= ~M_CRIT;                /* end of critical section. */
}

void
scroll_up(const UWORD top_line)
{
    if (!have_video) {
        /* Dont do any video operations if we dont have a card */
        return;
    }

    /* If top_line is 0, the whole screen can be scrolled by adjusting the regen start address. For any other value, a
     * memmove() will be needed as only some portion of the screen is being scrolled. */
    if (top_line == 0) {
        /* Move 1 row further into regen memory */
        regen_start += BYTES_LIN;

        /* Update CRTC regen start register */
        CRTC_WR(regen_start, COMET_VGA_REG_FILE_REGEN_START);

        /* Move the cursor to its current X and Y, which will update all required background variables */
        move_cursor(v_cur_cx, v_cur_cy);
    } else {
        UBYTE *src, *dst;
        ULONG count;
        UWORD work;

        /* Figure out the destination address */
        work = regen_start + (top_line * BYTES_LIN);
        dst = (UBYTE *)COMET_VGA_REGEN_ADDR + (work * 2);

        /* The source address is one row below */
        work += BYTES_LIN;
        src = (UBYTE *)COMET_VGA_REGEN_ADDR + (work * 2);

        /* The number of bytes to copy is doubled due to regen memory being comprised of pairs of character and
         * attribute */
        count = BYTES_LIN * 2 * (v_cel_my - top_line);

        (void)memmove(dst, src, count);

        /* If the cursor was in the scrolled portion of the screen, adjust its position */
        if (v_cur_cy >= top_line) {
            /* Move the cursor one row up */
            move_cursor(v_cur_cx, v_cur_cy - 1);
        }
    }

    /* Blank bottom row of display area */
    blank_out(0, v_cel_my, v_cel_mx, v_cel_my);
}

void
scroll_down(const UWORD start_line)
{
    if (!have_video) {
        /* Dont do any video operations if we dont have a card */
        return;
    }

    /* If start_line is 0, the whole screen can be scrolled by adjusting the regen start address. For any other value,
     * a memmove() will be needed as only some portion of the screen is being scrolled. */
    if (start_line == 0) {
        /* Move 1 row back in regen memory */
        regen_start -= BYTES_LIN;

        /* Update CRTC regen start register */
        CRTC_WR(regen_start, COMET_VGA_REG_FILE_REGEN_START);

        /* The cursor needs to remain in its current position, so increment its Y position */
        move_cursor(v_cur_cx, v_cur_cy + 1);
    } else {
        UBYTE *src, *dst;
        ULONG count;
        UWORD work;

        /* Figure out the source address */
        work = regen_start + (start_line * BYTES_LIN);
        src = (UBYTE *)COMET_VGA_REGEN_ADDR + (work * 2);

        /* The destination address is one row below */
        work += BYTES_LIN;
        dst = (UBYTE *)COMET_VGA_REGEN_ADDR + (work * 2);

        /* The number of bytes to copy is doubled due to regen memory being comprised of pairs of character and
         * attribute */
        count = BYTES_LIN * 2 * (v_cel_my - start_line);

        (void)memmove(dst, src, count);

        /* If the cursor was in the scrolled portion of the screen, adjust its position */
        if (v_cur_cy >= start_line) {
            /* Move the cursor one row down */
            move_cursor(v_cur_cx, v_cur_cy + 1);
        }
    }
}

/* Some ideas borrowed from https://github.com/ddraig68k/emutos */

static void
vt82c42_write_wait(const UBYTE data, const UBYTE reg)
{
    UBYTE val;

    /* Wait until input buffer empty */
    do {
        val = I8042_RD(COMET_VGA_I8042_CMD);
    } while (val & VT82C42_STATUS_IBF);

    /* Send the command */
    I8042_WR(data, reg);
}

static UBYTE
vt82c42_cmd_data_polled(const UBYTE cmd)
{
    UBYTE val;

    /* Send the command */
    vt82c42_write_wait(cmd, COMET_VGA_I8042_CMD);

    /* Wait for the response by polling the OBF flag of the status register */
    do {
        val = I8042_RD(COMET_VGA_I8042_CMD);
    } while (!(val & VT82C42_STATUS_OBF));

    /* Return the value from the data register */
    val = I8042_RD(COMET_VGA_I8042_DATA);

    return val;
}

static UBYTE
vt82c42_data_data_polled(const UBYTE data)
{
    UBYTE val;

    /* Write the data */
    vt82c42_write_wait(data, COMET_VGA_I8042_DATA);

    /* Wait for the response by polling the OBF flag of the status register */
    do {
        val = I8042_RD(COMET_VGA_I8042_CMD);
    } while (!(val & VT82C42_STATUS_OBF));

    /* Return the value from the data register */
    val = I8042_RD(COMET_VGA_I8042_DATA);

    return val;
}

static UBYTE
vt82c42_data_polled(void)
{
    UBYTE val;

    /* Wait for the response by polling the OBF flag of the status register */
    do {
        val = I8042_RD(COMET_VGA_I8042_CMD);
    } while (!(val & VT82C42_STATUS_OBF));

    /* Return the value from the data register */
    val = I8042_RD(COMET_VGA_I8042_DATA);

    return val;
}

static BOOL
vt82c42_data_polled_timeout(UBYTE *data)
{
    UBYTE val;
    LONG timer;

    /* Set a timeout for how long we will wait for a response */
    timer = hz_200 + 20;

    /* Wait for the response by polling the OBF flag of the status register */
    do {
        val = I8042_RD(COMET_VGA_I8042_CMD);
    } while (!(val & VT82C42_STATUS_OBF) && hz_200 < timer);

    if (hz_200 == timer) {
        /* Timeout */
        return FALSE;
    }

    /* Return the value from the data register */
    *data = I8042_RD(COMET_VGA_I8042_DATA);

    return TRUE;
}

static UBYTE
vt82c42_send_device_cmd(const enum VT82C42_port port, const UBYTE cmd)
{
    UBYTE retries = 10;
    UBYTE val;
    LONG timer;

    while (retries--) {
        if (port == VT82C42_MS) {
            /* Will write to the mouse output port */
            vt82c42_write_wait(0xD4, COMET_VGA_I8042_CMD);
        }

        val = vt82c42_data_data_polled(cmd);

        if (retries == 1 || val != 0xFE) {
            break;
        }

        /* Delay until the next tick of the system timer before retrying */
        timer = hz_200;

        while (timer == hz_200) {}
    }

    return val;
}

static UBYTE
vt82c42_get_cmd_byte(void)
{
    return vt82c42_cmd_data_polled(0x20);
}

static void
vt82c42_set_cmd_byte(const UBYTE cmd)
{
    vt82c42_write_wait(0x60, COMET_VGA_I8042_CMD);
    vt82c42_write_wait(cmd, COMET_VGA_I8042_DATA);
}

void
comet_vga_vt82c42_init(void)
{
    KDEBUG(("comet_vga_vt82c42_init()\n"));

    /* Reset have flag */
    have_vt82c42 = FALSE;

    volatile UBYTE *reg = (UBYTE *)COMET_VGA_BASE + COMET_VGA_I8042 + COMET_VGA_I8042_CMD;

    if (!check_read_byte((LONG)reg)) {
        KDEBUG(("comet_vga_vt82c42_init(): VT82C42 not detected at %p\n", reg));

        return;
    }

    UBYTE data;
    UBYTE status;
    BOOL kb_stat = FALSE;
    BOOL ms_stat = FALSE;

    /* Disable keyboard and mouse interfaces, and inhibit interrupts */
    vt82c42_set_cmd_byte(VT82C42_CMD_PC_COMPAT | VT82C42_CMD_MS_DISABLE | VT82C42_CMD_KB_DISABLE);

    /* Flush the output buffer */
    for (;;) {
        status = I8042_RD(COMET_VGA_I8042_CMD);

        if (status & VT82C42_STATUS_OBF) {
            (void)I8042_RD(COMET_VGA_I8042_DATA);
            KDEBUG(("comet_vga_vt82c42_init(): flush\n"));
        } else {
            break;
        }
    }

    /* Controller self test */
    data = vt82c42_cmd_data_polled(0xAA);

    if (data != 0x55) {
        KDEBUG(("comet_vga_vt82c42_init(): Controller self test failed\n"));

        return;
    }

    /* Controller firmware/hardware versions */
    data = vt82c42_cmd_data_polled(0xA1);
    KDEBUG(("comet_vga_vt82c42_init(): Version (A1)=%02X\n", data));
    data = vt82c42_cmd_data_polled(0xAF);
    KDEBUG(("comet_vga_vt82c42_init(): Version (AF)=%02X\n", data));

    /* Operating mode */
    data = vt82c42_cmd_data_polled(0xCA);

    if (data == 0x01) {
        KDEBUG(("comet_vga_vt82c42_init(): PS/2 mode\n"));
    } else {
        KDEBUG(("comet_vga_vt82c42_init(): AT mode (unsupported)\n"));

        return;
    }

    /* Check fuse status */
    vt82c42_write_wait(0xC1, COMET_VGA_I8042_CMD);
    status = I8042_RD(COMET_VGA_I8042_CMD);

    if (!(status & 0x40)) {
        KDEBUG(("comet_vga_vt82c42_init(): Fuse NOT OK - tests failed\n"));

        return;
    }

    /* We have a viable controller */
    have_vt82c42 = TRUE;

    /* Initialise keyboard and mouse interfaces and devices */
    kb_stat = device_keyboard_init();
    ms_stat = device_mouse_init();

    if (kb_stat == FALSE) {
        /* Disable the keyboard interface because it is unused or errored */
        vt82c42_write_wait(0xAD, COMET_VGA_I8042_CMD);
    }

    if (ms_stat == FALSE) {
        /* Disable the mouse interface because it is unused or errored */
        vt82c42_write_wait(0xA7, COMET_VGA_I8042_CMD);
    }

    if (kb_stat == FALSE && ms_stat == FALSE) {
        KDEBUG(("comet_vga_vt82c42_init(): no peripherals, early exit\n"));

        return;
    }

    KDEBUG(("comet_vga_vt82c42_init(): viable peripherals: "));

    if (kb_stat == TRUE) {
        KDEBUG(("keyboard "));
    }

    if (ms_stat == TRUE) {
        KDEBUG(("mouse"));
    }

    KDEBUG(("\n"));

    /* Enable interrupt sources in the controller */
    data = vt82c42_get_cmd_byte();

    if (kb_stat == TRUE) {
        data |= VT82C42_CMD_KB_OBF_INT;
    }

    if (ms_stat == TRUE) {
        data |= VT82C42_CMD_MS_OBF_INT;
    }

    vt82c42_set_cmd_byte(data);

    /* Set up the interrupt handler and enable interruptor on the video card */
    PFVOID *v_blank = (PFVOID *)((COMET_VGA_VECTOR_BASE + COMET_VGA_KB_VECTOR) << 2);
    *v_blank = interrupt_vt82c42;

    struct csr1 int_src = { .u16 = CRTC_RD_CSR1() };
    int_src.K_EN = 1;
    CRTC_WR_CSR1(int_src.u16);
}

static BOOL
device_keyboard_init(void)
{
    /* Keyboard interface test */
    if (vt82c42_cmd_data_polled(0xAB) != 0) {
        KDEBUG(("device_keyboard_init(): Keyboard interface test failed\n"));

        return FALSE;
    }

    /* Enable the keyboard interface */
    vt82c42_write_wait(0xAE, COMET_VGA_I8042_CMD);

    /* Reset keyboard */
    if (device_keyboard_reset() != TRUE) {
        KDEBUG(("device_keyboard_init(): Keyboard reset failed - is a working keyboard connected?\n"));

        return FALSE;
    }

    /* Do a little LED animation :) */
    device_keyboard_led_animate();

    return TRUE;
}

static BOOL
device_keyboard_reset(void)
{
    /* Send the keyboard reset command */
    if (vt82c42_send_device_cmd(VT82C42_KB, 0xFF) != 0xFA) {
        /* Keyboard did not acknowledge reset command */
        return FALSE;
    }

    /* Keyboard acknowledged the reset command, it should also indicate whether selft tests were successful */
    if (vt82c42_data_polled() != 0xAA) {
        return FALSE;
    }

    /* Reset succeeded */
    return TRUE;
}

static void
device_keyboard_led_animate(void)
{
    /* Animation pattern: none -> num -> caps -> scroll -> none -> num - 0xFF terminates */
    const UBYTE anim[] = {0, KB_LED_NUM, KB_LED_CAPS, KB_LED_SCROLL, 0, KB_LED_NUM, 0xFF};

    UBYTE i;
    LONG timer;

    for (i = 0;; i++) {
        if (anim[i] == 0xFF) {
            /* Animation done */
            break;
        }

        /* Set LED */
        (void)vt82c42_data_data_polled(0xED);
        (void)vt82c42_data_data_polled(anim[i]);

        /* Update LED status for tracking purposes */
        kb_leds = anim[i];

        /* Delay between transitions */
        timer = hz_200 + 20;

        while (hz_200 < timer) {}
    }
}

static BOOL
device_mouse_init(void)
{
    /* Mouse interface test */
    if (vt82c42_cmd_data_polled(0xA9) != 0) {
        KDEBUG(("device_mouse_init(): Mouse interface test failed\n"));

        return FALSE;
    }

    /* Enable the mouse interface */
    vt82c42_write_wait(0xA8, COMET_VGA_I8042_CMD);

    /* Reset mouse */
    if (device_mouse_reset() != TRUE) {
        KDEBUG(("device_mouse_init(): Mouse reset failed - is a working mouse connected?\n"));

        return FALSE;
    }

    /* Configure mouse */
    if (device_mouse_configure() != TRUE) {
        KDEBUG(("device_mouse_init(): Mouse configuration failed\n"));

        return FALSE;
    }

    return TRUE;
}

static BOOL
device_mouse_reset(void)
{
    BOOL success = FALSE;
    UBYTE data;

    /* Send the mouse reset command */
    if (vt82c42_send_device_cmd(VT82C42_MS, 0xFF) != 0xFA) {
        /* Mouse did not acknowledge reset command */
        return FALSE;
    }

    /* Mouse acknowledged the reset command, it should also indicate whether selft tests were successful */
    if (vt82c42_data_polled() != 0xAA) {
        return FALSE;
    }

    /* Finally, the mouse should send an ID to indicate that the device is a mouse */
    success = vt82c42_data_polled_timeout(&data);

    if (success == FALSE || data != 0) {
        return FALSE;
    }

    /* Reset succeeded */
    return TRUE;
}

static BOOL
device_mouse_configure(void)
{
    /* Set the report rate */
    if (vt82c42_send_device_cmd(VT82C42_MS, 0xF3) != 0xFA) {
        return FALSE;
    }

    if (vt82c42_send_device_cmd(VT82C42_MS, 10) != 0xFA) {
        return FALSE;
    }

    /* Set the resolution */
    if (vt82c42_send_device_cmd(VT82C42_MS, 0xE8) != 0xFA) {
        return FALSE;
    }

    if (vt82c42_send_device_cmd(VT82C42_MS, 1) != 0xFA) {
        return FALSE;
    }

    /* Enable reporting to start getting updates */
    if (vt82c42_send_device_cmd(VT82C42_MS, 0xF4) != 0xFA) {
        return FALSE;
    }

    return TRUE;
}

static void __attribute__((interrupt))
interrupt_vt82c42(void)
{
    UBYTE status;
    UBYTE data;

    const struct csr1 int_src = { .u16 = CRTC_RD_CSR1() };

    if (int_src.K) {
        status = I8042_RD(COMET_VGA_I8042_CMD);
        data = I8042_RD(COMET_VGA_I8042_DATA);

        if (status & VT82C42_STATUS_MS_DATA) {
            /* Ignore mouse data for now */
            vt82c42_handle_mouse(data);
        } else {
            vt82c42_handle_key(data);
        }
    }
}

static void
vt82c42_handle_key(const UBYTE code)
{
    static enum key_state state = KEY_STATE_DEFAULT;
    const UBYTE make_code = code & 0x7F;
    static BOOL is_break_code = FALSE;
    UBYTE xlat_code = 0;
    BOOL queue_code = FALSE;
    BOOL update_leds = FALSE;
    static BOOL is_escape2 = FALSE;

    /* Ignore code 0 */
    if (code == 0) {
        return;
    }

    switch (state) {
        case KEY_STATE_ESCAPE:
            if (code == 0xF0) {
                /* Next code will be a break code */
                is_break_code = TRUE;

                return;
            }

            if (code > COMET_VGA_MAX_KEY_CODE) {
                /* Ignore/consume codes that are out of range */
                is_break_code = FALSE;
                /* Should we return to the default state here? */

                return;
            }

            if (code == 0x12) {
                /* Enable/disable double escaped code set */
                is_escape2 = is_break_code ? FALSE : TRUE;
                is_break_code = FALSE;
                state = KEY_STATE_DEFAULT;

                return;
            }

            /* Look up translated code ... */
            xlat_code = is_escape2 ? ps2_extended2_scancode_map[make_code] : ps2_extended_scancode_map[make_code];

            if (xlat_code == 0) {
                /* Ignore/consume this code */
                is_break_code = FALSE;
                state = KEY_STATE_DEFAULT;

                return;
            }

            if ((SBYTE)xlat_code != -1) {
                /* This code will be queued */
                queue_code = TRUE;
                state = KEY_STATE_DEFAULT;
            }

            break;

        case KEY_STATE_UNTIL_BREAK:
            if (code == 0xF0) {
                /* Next code will be a break code */
                is_break_code = TRUE;
                state = KEY_STATE_DEFAULT;
            }

            break;

        case KEY_STATE_PAUSE_BREAK:
            if (code == 0xF0) {
                /* Next code will be a break code */
                is_break_code = TRUE;

                return;
            }

            if (!is_escape2) {
                if (code == 0x14) {
                    /* Abuse the is_escape2 flag to keep track of where we are processing this key */
                    is_escape2 = TRUE;
                } else if (is_break_code && code == 0x77) {
                    /* Sequence complete */
                    is_break_code = FALSE;
                    state = KEY_STATE_DEFAULT;
                }
            } else {
                if (code == 0x77) {
                    /* Pause/Break key pressed */
                    /* TODO: something? */
                    KDEBUG(("vt82c42_handle_key(): Pause/Break\n"));
                } else if (is_break_code && code == 0x14) {
                    is_break_code = FALSE;
                    is_escape2 = FALSE;
                }
            }

            return;

        default:
            if (code == 0xF0) {
                /* Next code will be a break code */
                is_break_code = TRUE;

                return;
            }

            if (code == 0xE0) {
                /* Extended key code */
                state = KEY_STATE_ESCAPE;

                return;
            }

            if (code == 0xE1) {
                /* Probably Pause/Break */
                state = KEY_STATE_PAUSE_BREAK;

                return;
            }

            if (code > COMET_VGA_MAX_KEY_CODE) {
                /* Ignore/consume codes that are out of range */
                is_break_code = FALSE;

                return;
            }

            /* Look up translated code ... */
            xlat_code = kb_leds & KB_LED_NUM ? ps2_scancode_map_numlock[make_code] : ps2_scancode_map[make_code];

            if (xlat_code == 0) {
                /* Ignore/consume this code */
                is_break_code = FALSE;

                return;
            }

            if ((SBYTE)xlat_code != -1) {
                /* This code will be queued */
                queue_code = TRUE;
            } else {
                /* Special handling */
                if (make_code == 0x58) {
                    /* Caps lock */
                    if (!is_break_code) {
                        kb_leds ^= KB_LED_CAPS;
                        update_leds = TRUE;

                        /* Consume repeats to prevent toggling */
                        state = KEY_STATE_UNTIL_BREAK;
                    }

                    xlat_code = 0x3A;
                    queue_code = TRUE;

                    break;
                }

                if (make_code == 0x7E) {
                    /* Scroll lock */
                    if (!is_break_code) {
                        kb_leds ^= KB_LED_SCROLL;
                        update_leds = TRUE;

                        /* Consume repeats to prevent toggling */
                        state = KEY_STATE_UNTIL_BREAK;
                    }

                    xlat_code = 0x46;
                    queue_code = TRUE;

                    break;
                }

                if (make_code == 0x77) {
                    /* Num lock */
                    if (!is_break_code) {
                        kb_leds ^= KB_LED_NUM;
                        update_leds = TRUE;

                        /* Consume repeats to prevent toggling */
                        state = KEY_STATE_UNTIL_BREAK;
                    }

                    xlat_code = 0x45;
                    queue_code = TRUE;

                    break;
                }

                if (make_code == 0x7C) {
                    /* Numpad * */
                    if (!is_break_code) {
                        push_ascii_ikbdiorec('*');
                    }

                    is_break_code = FALSE;

                    return;
                }

                if (make_code == 0x71) {
                    /* Numpad . */
                    if (!is_break_code) {
                        push_ascii_ikbdiorec('.');
                    }

                    is_break_code = FALSE;

                    return;
                }
            }
    }

    if (queue_code) {
        if (is_break_code) {
            /* Set MSb for break code */
            xlat_code |= 0x80;

            is_break_code = FALSE;
        }

        call_ikbdraw(xlat_code);
    }

    if (update_leds) {
        /* Set LEDs */
        (void)vt82c42_data_data_polled(0xED);
        (void)vt82c42_data_data_polled(kb_leds);
    }
}

static void
vt82c42_handle_mouse(const UBYTE code)
{
    static UBYTE pktctr = 0;
    static UBYTE pkt[6] = {0};

    /* Collect up to 3 bytes of packet data */
    if (pktctr < 3) {
        pkt[pktctr++] = code;
    }

    /* Once 3 bytes have been collected, process the packet - form a new packet to be queued with EmuTOS */
    if (pktctr == 3) {
        pkt[3] = 0xF8;                      /* MOUSE_REL_POS_REPORT */
        pkt[3] |= (pkt[0] & 0x01) << 1;     /* LEFT_BUTTON_DOWN */
        pkt[3] |= (pkt[0] & 0x02) >> 1;     /* RIGHT_BUTTON_DOWN */
        pkt[4] = pkt[1];                    /* X rel */
        pkt[5] = -pkt[2];                   /* Y rel */

        /* Overflow handling */
        if (pkt[0] & 0x40) {
            /* X overflow */
            pkt[4] = pkt[0] & 0x10 ? -128 : 127;
        }

        if (pkt[0] & 0x80) {
            /* X overflow */
            pkt[5] = pkt[0] & 0x20 ? -128 : 127;
        }

        // KDEBUG(("Mouse: X=%i Y=%i B=%1X\n", (SBYTE)pkt[4], (SBYTE)pkt[5], pkt[0] & 0x03));

        call_mousevec((SBYTE *)&pkt[3]);

        /* Reset packet counter */
        pktctr = 0;
    }
}

#endif /* CONF_WITH_COMET_VGA */
