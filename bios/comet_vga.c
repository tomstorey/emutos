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
#include "comet_vga_keymap_us.h"

/* Macros for reading and writing hardware registers */
#define CRTC_WR_CSR0(v) (*(volatile UWORD *)(COMET_VGA_BASE + COMET_VGA_REG_FILE) = (v))
#define CRTC_RD_CSR0() (*(volatile UWORD *)(COMET_VGA_BASE + COMET_VGA_REG_FILE))
#define CRTC_WR_CSR1(v) (*(volatile UWORD *)(COMET_VGA_BASE + COMET_VGA_REG_FILE + COMET_VGA_REG_FILE_CSR1) = (v))
#define CRTC_RD_CSR1() (*(volatile UWORD *)(COMET_VGA_BASE + COMET_VGA_REG_FILE + COMET_VGA_REG_FILE_CSR1))
#define CRTC_WR(v, r) (*(volatile UWORD *)(COMET_VGA_BASE + COMET_VGA_REG_FILE + (r)) = (v))
#define RAMDAC_WR(v, r) (*(volatile UBYTE *)(COMET_VGA_BASE + COMET_VGA_RAMDAC + (r)) = (v))

#define I8042_RD(r) (*(volatile UBYTE *)(COMET_VGA_BASE + COMET_VGA_I8042 + (r)))
#define I8042_WR(v, r) (*(volatile UBYTE *)(COMET_VGA_BASE + COMET_VGA_I8042 + (r)) = (v))

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

/* Text mode - 80x25 screen with 9x16 characters, 16fg, 8bg, blinking text */
static const UWORD text_mode1_cfg[15] = {
    0x8187, (0x0200 | COMET_VGA_VECTOR_BASE), 0x0063, 0x0050, 0x0053, 0x0F06, 0x001B, 0x0002,
    0x0019, 0x001A, 0x000F, 0x600F, 0x0000, 0x0000, 0x0050
};

/* Default 16 colour palette, as loaded into RAMDAC */
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

/* Forward decls */
static void init_linea_vars(void);
static void set_text_mode(void);
static void load_palette(void);
static void load_font1(void);
static void interrupt_v_blank(void);

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

static void
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
ascii_out(int ch)
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
blank_out(int topx, int topy, int botx, int boty)
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
scroll_up(UWORD top_line)
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
scroll_down(UWORD start_line)
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




#define I8042_STATUS_OBF 0x01
#define I8042_STATUS_IBF 0x02
#define I8042_STATUS_MS_DATA 0x20

#define I8042_CMD_KB_OBF_INT 0x01
#define I8042_CMD_MS_OBF_INT 0x02
#define I8042_CMD_PC_COMPAT 0x40

#define KB_LED_SCROLL 0x01
#define KB_LED_NUM 0x02
#define KB_LED_CAPS 0x04

enum VT82C42_port {
    VT82C42_KB = 0,
    VT82C42_MS = 1
};

/* Holds the state of the LEDs */
static UBYTE kb_leds = 0;


static void vt82c42_write_wait(UBYTE data, UBYTE reg);
static UBYTE vt82c42_cmd_data_polled(UBYTE cmd);
static UBYTE vt82c42_data_data_polled(UBYTE data);
static UBYTE vt82c42_data_polled(void);
static UBYTE vt82c42_send_device_cmd(enum VT82C42_port port, UBYTE cmd);
static UBYTE vt82c42_get_cmd_byte(void);
static void vt82c42_set_cmd_byte(UBYTE cmd);

static BOOL device_keyboard_reset(void);
static void device_keyboard_led_animate(void);

static void interrupt_vt82c42(void);
static void vt82c42_handle_key(UBYTE data);

static void
vt82c42_write_wait(const UBYTE data, const UBYTE reg)
{
    UBYTE val;

    /* Wait until input buffer empty */
    do {
        val = I8042_RD(COMET_VGA_I8042_CMD);
    } while (val & I8042_STATUS_IBF);

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
    } while (!(val & I8042_STATUS_OBF));

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
    } while (!(val & I8042_STATUS_OBF));

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
    } while (!(val & I8042_STATUS_OBF));

    /* Return the value from the data register */
    val = I8042_RD(COMET_VGA_I8042_DATA);

    return val;
}

static UBYTE
vt82c42_send_device_cmd(const enum VT82C42_port port, UBYTE cmd)
{
    UBYTE retries = 10;
    UBYTE val;
    LONG timer;

    while (retries--) {
        if (port == VT82C42_MS) {
            /* Will write to the mouse output port */
            vt82c42_write_wait(0xD3, COMET_VGA_I8042_CMD);
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

    /* Disable keyboard and mouse interfaces, and inhibit interrupts */
    vt82c42_set_cmd_byte(0x70);

    /* Flush the output buffer */
    for (;;) {
        status = I8042_RD(COMET_VGA_I8042_CMD);

        if (status & I8042_STATUS_OBF) {
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

    /* Keyboard interface test */
    data = vt82c42_cmd_data_polled(0xAB);

    if (data != 0) {
        KDEBUG(("comet_vga_vt82c42_init(): Keyboard interface test failed\n"));

        return;
    }

    /* Check fuse status */
    vt82c42_write_wait(0xC1, COMET_VGA_I8042_CMD);
    status = I8042_RD(COMET_VGA_I8042_CMD);

    if (!(status & 0x40)) {
        KDEBUG(("comet_vga_vt82c42_init(): Fuse NOT OK - tests failed\n"));

        return;
    }

    /* We have a viable controller and keyboard interface */
    have_vt82c42 = TRUE;

    /* Enable the keyboard interface */
    vt82c42_write_wait(0xAE, COMET_VGA_I8042_CMD);

    /* Reset keyboard */
    if (device_keyboard_reset() != TRUE) {
        KDEBUG(("comet_vga_vt82c42_init(): Keyboard reset failed\n"));

        return;
    }

    /* Do a little LED animation :) */
    device_keyboard_led_animate();

    /* Set up interrupt handler */
    PFVOID *v_blank = (PFVOID *)((COMET_VGA_VECTOR_BASE + COMET_VGA_KB_VECTOR) << 2);
    *v_blank = interrupt_vt82c42;

    /* Enable interrupts */
    data = vt82c42_get_cmd_byte();
    vt82c42_set_cmd_byte(data | I8042_CMD_KB_OBF_INT);

    struct csr1 int_src = { .u16 = CRTC_RD_CSR1() };
    int_src.K_EN = 1;
    CRTC_WR_CSR1(int_src.u16);
}

static BOOL
device_keyboard_reset(void)
{
    UBYTE val;

    /* Send the keyboard reset command */
    val = vt82c42_send_device_cmd(VT82C42_KB, 0xFF);

    if (val != 0xFA) {
        /* Keyboard did not acknowledge reset command */
        return FALSE;
    }

    /* Keyboard acknowledged the reset command, it should also indicate whether the reset was successful */
    val = vt82c42_data_polled();

    if (val != 0xAA) {
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

static void __attribute__((interrupt))
interrupt_vt82c42(void)
{
    UBYTE status;
    UBYTE data;

    const struct csr1 int_src = { .u16 = CRTC_RD_CSR1() };

    if (int_src.K) {
        status = I8042_RD(COMET_VGA_I8042_CMD);
        data = I8042_RD(COMET_VGA_I8042_DATA);

        if (status & I8042_STATUS_MS_DATA) {
            /* Ignore mouse data for now */
        } else {
            vt82c42_handle_key(data);
        }

        for (;;) {
            status = I8042_RD(COMET_VGA_I8042_CMD);

            if (status & I8042_STATUS_OBF) {
                data = I8042_RD(COMET_VGA_I8042_DATA);
            } else {
                break;
            }
        }
    }
}








enum key_state {
    KEY_STATE_DEFAULT = 0,
    KEY_STATE_ESCAPE0,
    KEY_STATE_ESCAPE1,
    KEY_STATE_ESCAPE_DOUBLE,
    KEY_STATE_PAUSEBRK
};


static void
vt82c42_handle_key(const UBYTE data)
{
    // KDEBUG(("vt82c42_handle_key(): data=%02X\n", data));

    static enum key_state state = KEY_STATE_DEFAULT;

    const UBYTE code = data & 0x7F;
    const UBYTE is_break = data & 0x80;
    UBYTE chr = 0;

    BOOL update_leds = FALSE;
    BOOL queue_chr = FALSE;

    switch (state) {
        case KEY_STATE_ESCAPE0:
            if (data == 0x2A) {
                /* Key is double escaped */
                state = KEY_STATE_ESCAPE_DOUBLE;
            } else {
                chr = ps2_extended_scancode_map[code];

                if ((SBYTE)chr != -1) {
                    if (chr == 0) {
                        /* No translation was needed, take raw code */
                        chr = data;
                    }

                    queue_chr = TRUE;
                }

                state = KEY_STATE_DEFAULT;
            }

            break;

        case KEY_STATE_ESCAPE_DOUBLE:
            if (data == 0xE0) {
                /* Consume escape code during escape mode */
            } else if (data == 0xAA) {
                /* Sequence complete  */
                state = KEY_STATE_DEFAULT;
            } else {
                chr = ps2_extended2_scancode_map[code];

                if ((SBYTE)chr != -1) {
                    if (chr == 0) {
                        /* No translation was needed, take raw code */
                        chr = data;
                    }

                    queue_chr = TRUE;
                }
            }

            break;

        case KEY_STATE_ESCAPE1:
            /* Wait for 0x1D code (break masked out) */
            if (code == 0x1D) {
                state = KEY_STATE_PAUSEBRK;
            }

            break;

        case KEY_STATE_PAUSEBRK:
            if (code == 0x45) {
                /* Break key */

                if (is_break > 0) {
                    /* Break event - sequence complete */
                    state = KEY_STATE_DEFAULT;
                } else {
                    /* Make event */
                    /* TODO: what to do with break key? Just log it for now. */
                    KDEBUG(("vt82c42_handle_key(): Pause/Break\n"));
                }
            } else {
                /* Other codes consumed */
            }

            break;

        case KEY_STATE_DEFAULT:
        default:
            if (data == 0xE0) {
                state = KEY_STATE_ESCAPE0;
            } else if (data == 0xE1) {
                state = KEY_STATE_ESCAPE1;
            } else {
                /* Any other key */
                if (kb_leds & KB_LED_NUM) {
                    chr = ps2_scancode_map_numlock[code];
                } else {
                    chr = ps2_scancode_map[code];
                }

                if ((SBYTE)chr != -1) {
                    if (chr == 0) {
                        /* No translation was needed, take raw code */
                        chr = data;
                    }

                    queue_chr = TRUE;

                    /* LED control keys - ignore break codes */
                    if (data == 0x3A) {
                        kb_leds ^= KB_LED_CAPS;
                        update_leds = TRUE;
                    } else if (data == 0x46) {
                        kb_leds ^= KB_LED_SCROLL;
                        update_leds = TRUE;
                    } else if (data == 0x45) {
                        kb_leds ^= KB_LED_NUM;
                        update_leds = TRUE;
                    }
                } else {
                    if (data == 0x37) {
                        /* Numpad * - handle specially and push in an ASCII character instead of sending a key code */
                        push_ascii_ikbdiorec('*');
                    }
                }
            }
    }

    if (update_leds) {
        /* Set LEDs */
        (void)vt82c42_data_data_polled(0xED);
        (void)vt82c42_data_data_polled(kb_leds);
    }

    if (queue_chr) {
        // KDEBUG(("vt82c42_handle_key(): ikbdraw(%02X)\n", chr | is_break));
        call_ikbdraw(chr | is_break);
    }
}

#endif /* CONF_WITH_COMET_VGA */
