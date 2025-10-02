#ifndef COMET_VGA_H
#define COMET_VGA_H

/* Base address of the regen buffer on a COMET VGA card */
#define COMET_VGA_REGEN_ADDR 0x00D00000

/* Offsets to various register sets from the base address of a COMET VGA card */
#define COMET_VGA_REG_FILE 0
#define COMET_VGA_RAMDAC 0x800
#define COMET_VGA_I2C 0x1000
#define COMET_VGA_PS2 0x1800
#define COMET_VGA_FONTRAM 0x2000

/* Offsets for CRTC register file */
#define COMET_VGA_REG_FILE_CSR0 0x00
#define COMET_VGA_REG_FILE_CSR1 0x02
#define COMET_VGA_REG_FILE_H_TOTAL 0x04
#define COMET_VGA_REG_FILE_H_DISP 0x06
#define COMET_VGA_REG_FILE_H_SYNC_POS 0x08
#define COMET_VGA_REG_FILE_HV_SYNC_WIDTH 0x0A
#define COMET_VGA_REG_FILE_V_TOTAL 0x0C
#define COMET_VGA_REG_FILE_V_ADJ 0x0E
#define COMET_VGA_REG_FILE_V_DISP 0x10
#define COMET_VGA_REG_FILE_V_SYNC_POS 0x12
#define COMET_VGA_REG_FILE_ROW_SIZE 0x14
#define COMET_VGA_REG_FILE_CURSOR 0x16
#define COMET_VGA_REG_FILE_REGEN_START 0x18
#define COMET_VGA_REG_FILE_CURSOR_ADDR 0x1A
#define COMET_VGA_REG_FILE_REGEN_INC 0x1C

/* Offsets for RAMDAC registers */
#define COMET_VGA_RAMDAC_ADDR_WR 0
#define COMET_VGA_RAMDAC_PALRAM 2
#define COMET_VGA_RAMDAC_MASK 4
#define COMET_VGA_RAMDAC_ADDR_RD 6

void comet_vga_screen_init(void);

#endif /* COMET_VGA_H */
