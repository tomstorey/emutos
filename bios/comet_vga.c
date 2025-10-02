#define ENABLE_KDEBUG

#include "emutos.h"

#if CONF_WITH_COMET_VGA

#include "vectors.h"
#include "conout.h"
#include "lineavars.h"
#include "tosvars.h"
#include "string.h"
#include "comet_vga.h"
#include "comet_vga_font1.h"

/* Macros for reading and writing hardware registers */
#define CRTC_WR_CSR0(v) (*(volatile UWORD *)(COMET_VGA_BASE + COMET_VGA_REG_FILE) = (v))
#define CRTC_RD_CSR0(v) (*(volatile UWORD *)(COMET_VGA_BASE + COMET_VGA_REG_FILE))
#define CRTC_WR_CSR1(v) (*(volatile UWORD *)(COMET_VGA_BASE + COMET_VGA_REG_FILE + COMET_VGA_REG_FILE_CSR1) = (v))
#define CRTC_RD_CSR1(v) (*(volatile UWORD *)(COMET_VGA_BASE + COMET_VGA_REG_FILE + COMET_VGA_REG_FILE_CSR1))
#define CRTC_WR(v, r) (*(volatile UWORD *)(COMET_VGA_BASE + COMET_VGA_REG_FILE + (r)) = (v))
#define RAMDAC_WR(v, r) (*(volatile UBYTE *)(COMET_VGA_BASE + COMET_VGA_RAMDAC + (r)) = (v))

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
    0x8187, 0x0000, 0x0063, 0x0050, 0x0053, 0x0F06, 0x001B, 0x0002,
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

/* Indicates whether a card has been detected */
static BOOL have_card = FALSE;

/* Forward decls */
static void init_linea_vars(void);
static void set_text_mode(void);
static void load_palette(void);
static void load_font1(void);

void
comet_vga_screen_init(void)
{
    KDEBUG(("comet_vga_screen_init()\n"));

    /* Reset have flag */
    have_card = FALSE;

    if (!check_read_word(COMET_VGA_BASE + COMET_VGA_REG_FILE)) {
        KDEBUG(("comet_vga_screen_init(): COMET VGA card not detected at %p\n", (void *)(COMET_VGA_BASE + COMET_VGA_REG_FILE)));

        return;
    }

    /* We have a card */
    have_card = TRUE;

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
    for (i = 1; i < 15; i++) {
        *(reg + i) = text_mode1_cfg[i];
    }

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

void
ascii_out(int ch)
{
    if (!have_card) {
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
    if (!have_card) {
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
    if (!have_card) {
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
    if (!have_card) {
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
    if (!have_card) {
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
    if (!have_card) {
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

#endif /* CONF_WITH_COMET_VGA */
