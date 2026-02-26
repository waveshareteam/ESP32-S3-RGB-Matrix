#include "lvgl.h"

/*
 * LVGL 5x7 bitmap font
 * Range: 0x20-0x7E (ASCII)
 * Height: 7px
 * Line height: 7px
 * Base line: 0px (bottom)
 */

/* Bitmap data for 5x7 font */
/* Stored as bit-packed bytes: 1 bit per pixel */
/* For 1bpp, LVGL expects MSB-first packing (format 0) */
/* Each row is byte-aligned in this manual format, but LVGL's tiny font format expects continuous stream */
/* Let's try switching to a simpler, more standard definition */

static const uint8_t glyph_bitmap[] = {
    /* 0x20 ' ' (5x7) - 5 bytes (1 byte per row, 5 bits valid) */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

    /* 0x21 '!' */
    0x20, 0x20, 0x20, 0x20, 0x20, 0x00, 0x20,

    /* 0x22 '"' */
    0x50, 0x50, 0x50, 0x00, 0x00, 0x00, 0x00,

    /* 0x23 '#' */
    0x50, 0x50, 0xF8, 0x50, 0xF8, 0x50, 0x50,

    /* 0x24 '$' */
    0x20, 0x78, 0xA0, 0x70, 0x28, 0xF0, 0x20,

    /* 0x25 '%' */
    0xC0, 0xC8, 0x10, 0x20, 0x40, 0x98, 0x18,

    /* 0x26 '&' */
    0x40, 0xA0, 0xA0, 0x40, 0xA8, 0x90, 0x68,

    /* 0x27 ''' */
    0x60, 0x20, 0x40, 0x00, 0x00, 0x00, 0x00,

    /* 0x28 '(' */
    0x10, 0x20, 0x40, 0x40, 0x40, 0x20, 0x10,

    /* 0x29 ')' */
    0x40, 0x20, 0x10, 0x10, 0x10, 0x20, 0x40,

    /* 0x2A '*' */
    0x00, 0x20, 0xA8, 0x70, 0xA8, 0x20, 0x00,

    /* 0x2B '+' */
    0x00, 0x20, 0x20, 0xF8, 0x20, 0x20, 0x00,

    /* 0x2C ',' */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x60, 0x20,

    /* 0x2D '-' */
    0x00, 0x00, 0x00, 0xF8, 0x00, 0x00, 0x00,

    /* 0x2E '.' */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x60, 0x60,

    /* 0x2F '/' */
    0x00, 0x08, 0x10, 0x20, 0x40, 0x80, 0x00,

    /* 0x30 '0' */
    0x70, 0x88, 0x98, 0xA8, 0xC8, 0x88, 0x70,

    /* 0x31 '1' */
    0x20, 0x60, 0x20, 0x20, 0x20, 0x20, 0x70,

    /* 0x32 '2' */
    0x70, 0x88, 0x08, 0x10, 0x20, 0x40, 0xF8,

    /* 0x33 '3' */
    0x70, 0x88, 0x08, 0x30, 0x08, 0x88, 0x70,

    /* 0x34 '4' */
    0x10, 0x30, 0x50, 0x90, 0xF8, 0x10, 0x10,

    /* 0x35 '5' */
    0xF8, 0x80, 0xF0, 0x08, 0x08, 0x88, 0x70,

    /* 0x36 '6' */
    0x30, 0x40, 0x80, 0xF0, 0x88, 0x88, 0x70,

    /* 0x37 '7' */
    0xF8, 0x08, 0x10, 0x20, 0x40, 0x40, 0x40,

    /* 0x38 '8' */
    0x70, 0x88, 0x88, 0x70, 0x88, 0x88, 0x70,

    /* 0x39 '9' */
    0x70, 0x88, 0x88, 0x78, 0x08, 0x10, 0x60,

    /* 0x3A ':' */
    0x00, 0x60, 0x60, 0x00, 0x60, 0x60, 0x00,

    /* 0x3B ';' */
    0x00, 0x60, 0x60, 0x00, 0x60, 0x20, 0x40,

    /* 0x3C '<' */
    0x08, 0x10, 0x20, 0x40, 0x20, 0x10, 0x08,

    /* 0x3D '=' */
    0x00, 0x00, 0xF8, 0x00, 0xF8, 0x00, 0x00,

    /* 0x3E '>' */
    0x40, 0x20, 0x10, 0x08, 0x10, 0x20, 0x40,

    /* 0x3F '?' */
    0x70, 0x88, 0x08, 0x10, 0x20, 0x00, 0x20,

    /* 0x40 '@' */
    0x70, 0x88, 0x08, 0x68, 0xA8, 0xA8, 0x70,

    /* 0x41 'A' */
    0x70, 0x88, 0x88, 0xF8, 0x88, 0x88, 0x88,

    /* 0x42 'B' */
    0xF0, 0x88, 0x88, 0xF0, 0x88, 0x88, 0xF0,

    /* 0x43 'C' */
    0x70, 0x88, 0x80, 0x80, 0x80, 0x88, 0x70,

    /* 0x44 'D' */
    0xF0, 0x88, 0x88, 0x88, 0x88, 0x88, 0xF0,

    /* 0x45 'E' */
    0xF8, 0x80, 0x80, 0xF0, 0x80, 0x80, 0xF8,

    /* 0x46 'F' */
    0xF8, 0x80, 0x80, 0xF0, 0x80, 0x80, 0x80,

    /* 0x47 'G' */
    0x70, 0x88, 0x80, 0x80, 0x98, 0x88, 0x70,

    /* 0x48 'H' */
    0x88, 0x88, 0x88, 0xF8, 0x88, 0x88, 0x88,

    /* 0x49 'I' */
    0x70, 0x20, 0x20, 0x20, 0x20, 0x20, 0x70,

    /* 0x4A 'J' */
    0x08, 0x08, 0x08, 0x08, 0x08, 0x88, 0x70,

    /* 0x4B 'K' */
    0x88, 0x90, 0xA0, 0xC0, 0xA0, 0x90, 0x88,

    /* 0x4C 'L' */
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0xF8,

    /* 0x4D 'M' */
    0x88, 0xD8, 0xA8, 0xA8, 0x88, 0x88, 0x88,

    /* 0x4E 'N' */
    0x88, 0x88, 0xC8, 0xA8, 0x98, 0x88, 0x88,

    /* 0x4F 'O' */
    0x70, 0x88, 0x88, 0x88, 0x88, 0x88, 0x70,

    /* 0x50 'P' */
    0xF0, 0x88, 0x88, 0xF0, 0x80, 0x80, 0x80,

    /* 0x51 'Q' */
    0x70, 0x88, 0x88, 0x88, 0xA8, 0x90, 0x68,

    /* 0x52 'R' */
    0xF0, 0x88, 0x88, 0xF0, 0xA0, 0x90, 0x88,

    /* 0x53 'S' */
    0x70, 0x88, 0x80, 0x70, 0x08, 0x88, 0x70,

    /* 0x54 'T' */
    0xF8, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,

    /* 0x55 'U' */
    0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x70,

    /* 0x56 'V' */
    0x88, 0x88, 0x88, 0x88, 0x88, 0x50, 0x20,

    /* 0x57 'W' */
    0x88, 0x88, 0x88, 0xA8, 0xA8, 0xD8, 0x88,

    /* 0x58 'X' */
    0x88, 0x88, 0x50, 0x20, 0x50, 0x88, 0x88,

    /* 0x59 'Y' */
    0x88, 0x88, 0x88, 0x70, 0x20, 0x20, 0x20,

    /* 0x5A 'Z' */
    0xF8, 0x08, 0x10, 0x20, 0x40, 0x80, 0xF8,

    /* 0x5B '[' */
    0x70, 0x40, 0x40, 0x40, 0x40, 0x40, 0x70,

    /* 0x5C '\' */
    0x00, 0x80, 0x40, 0x20, 0x10, 0x08, 0x00,

    /* 0x5D ']' */
    0x70, 0x10, 0x10, 0x10, 0x10, 0x10, 0x70,

    /* 0x5E '^' */
    0x20, 0x50, 0x88, 0x00, 0x00, 0x00, 0x00,

    /* 0x5F '_' */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF8,

    /* 0x60 '`' */
    0x40, 0x20, 0x10, 0x00, 0x00, 0x00, 0x00,

    /* 0x61 'a' */
    0x00, 0x00, 0x70, 0x08, 0x78, 0x88, 0x78,

    /* 0x62 'b' */
    0x80, 0x80, 0xB0, 0xC8, 0x88, 0xC8, 0xB0,

    /* 0x63 'c' */
    0x00, 0x00, 0x70, 0x88, 0x80, 0x88, 0x70,

    /* 0x64 'd' */
    0x08, 0x08, 0x68, 0x98, 0x88, 0x98, 0x68,

    /* 0x65 'e' */
    0x00, 0x00, 0x70, 0x88, 0xF8, 0x80, 0x70,

    /* 0x66 'f' */
    0x30, 0x40, 0xE0, 0x40, 0x40, 0x40, 0x40,

    /* 0x67 'g' */
    0x00, 0x00, 0x78, 0x88, 0x88, 0x78, 0x08, 0x70, /* Decender */

    /* 0x68 'h' */
    0x80, 0x80, 0xB0, 0xC8, 0x88, 0x88, 0x88,

    /* 0x69 'i' */
    0x20, 0x00, 0x60, 0x20, 0x20, 0x20, 0x70,

    /* 0x6A 'j' */
    0x10, 0x00, 0x30, 0x10, 0x10, 0x10, 0x10, 0x60, /* Decender */

    /* 0x6B 'k' */
    0x80, 0x80, 0x90, 0xA0, 0xC0, 0xA0, 0x90,

    /* 0x6C 'l' */
    0x60, 0x20, 0x20, 0x20, 0x20, 0x20, 0x70,

    /* 0x6D 'm' */
    0x00, 0x00, 0xD0, 0xA8, 0xA8, 0xA8, 0x88,

    /* 0x6E 'n' */
    0x00, 0x00, 0xB0, 0xC8, 0x88, 0x88, 0x88,

    /* 0x6F 'o' */
    0x00, 0x00, 0x70, 0x88, 0x88, 0x88, 0x70,

    /* 0x70 'p' */
    0x00, 0x00, 0xF0, 0x88, 0x88, 0xF0, 0x80, 0x80, /* Decender */

    /* 0x71 'q' */
    0x00, 0x00, 0x68, 0x98, 0x88, 0x98, 0x68, 0x08, /* Decender */

    /* 0x72 'r' */
    0x00, 0x00, 0xB0, 0xC8, 0x80, 0x80, 0x80,

    /* 0x73 's' */
    0x00, 0x00, 0x70, 0x80, 0x70, 0x08, 0x70,

    /* 0x74 't' */
    0x40, 0x40, 0xE0, 0x40, 0x40, 0x48, 0x30,

    /* 0x75 'u' */
    0x00, 0x00, 0x88, 0x88, 0x88, 0x98, 0x68,

    /* 0x76 'v' */
    0x00, 0x00, 0x88, 0x88, 0x88, 0x50, 0x20,

    /* 0x77 'w' */
    0x00, 0x00, 0x88, 0x88, 0xA8, 0xA8, 0x50,

    /* 0x78 'x' */
    0x00, 0x00, 0x88, 0x50, 0x20, 0x50, 0x88,

    /* 0x79 'y' */
    0x00, 0x00, 0x88, 0x88, 0x78, 0x08, 0x70, 0x00, /* Decender */

    /* 0x7A 'z' */
    0x00, 0x00, 0xF8, 0x10, 0x20, 0x40, 0xF8,

    /* 0x7B '{' */
    0x10, 0x20, 0x20, 0x40, 0x20, 0x20, 0x10,

    /* 0x7C '|' */
    0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,

    /* 0x7D '}' */
    0x40, 0x20, 0x20, 0x10, 0x20, 0x20, 0x40,

    /* 0x7E '~' */
    0x00, 0x48, 0xB0, 0x00, 0x00, 0x00, 0x00,
};

/* Glyph descriptions */
static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {
    {.bitmap_index = 0,   .adv_w = 0,  .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0}, /* Dummy (Glyph 0) */
    {.bitmap_index = 0,   .adv_w = 80, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0}, /* ' ' */
    {.bitmap_index = 7,   .adv_w = 80, .box_w = 8, .box_h = 7, .ofs_x = 0, .ofs_y = 0}, /* '!' */
    {.bitmap_index = 14,  .adv_w = 80, .box_w = 8, .box_h = 7, .ofs_x = 0, .ofs_y = 0}, /* '"' */
    {.bitmap_index = 21,  .adv_w = 80, .box_w = 8, .box_h = 7, .ofs_x = 0, .ofs_y = 0}, /* '#' */
    {.bitmap_index = 28,  .adv_w = 80, .box_w = 8, .box_h = 7, .ofs_x = 0, .ofs_y = 0}, /* '$' */
    {.bitmap_index = 35,  .adv_w = 80, .box_w = 8, .box_h = 7, .ofs_x = 0, .ofs_y = 0}, /* '%' */
    {.bitmap_index = 42,  .adv_w = 80, .box_w = 8, .box_h = 7, .ofs_x = 0, .ofs_y = 0}, /* '&' */
    {.bitmap_index = 49,  .adv_w = 80, .box_w = 8, .box_h = 7, .ofs_x = 0, .ofs_y = 0}, /* ''' */
    {.bitmap_index = 56,  .adv_w = 80, .box_w = 8, .box_h = 7, .ofs_x = 0, .ofs_y = 0}, /* '(' */
    {.bitmap_index = 63,  .adv_w = 80, .box_w = 8, .box_h = 7, .ofs_x = 0, .ofs_y = 0}, /* ')' */
    {.bitmap_index = 70,  .adv_w = 80, .box_w = 8, .box_h = 7, .ofs_x = 0, .ofs_y = 0}, /* '*' */
    {.bitmap_index = 77,  .adv_w = 80, .box_w = 8, .box_h = 7, .ofs_x = 0, .ofs_y = 0}, /* '+' */
    {.bitmap_index = 84,  .adv_w = 80, .box_w = 8, .box_h = 7, .ofs_x = 0, .ofs_y = 0}, /* ',' */
    {.bitmap_index = 91,  .adv_w = 80, .box_w = 8, .box_h = 7, .ofs_x = 0, .ofs_y = 0}, /* '-' */
    {.bitmap_index = 98,  .adv_w = 80, .box_w = 8, .box_h = 7, .ofs_x = 0, .ofs_y = 0}, /* '.' */
    {.bitmap_index = 105, .adv_w = 80, .box_w = 8, .box_h = 7, .ofs_x = 0, .ofs_y = 0}, /* '/' */
    {.bitmap_index = 112, .adv_w = 80, .box_w = 8, .box_h = 7, .ofs_x = 0, .ofs_y = 0}, /* '0' */
    {.bitmap_index = 119, .adv_w = 80, .box_w = 8, .box_h = 7, .ofs_x = 0, .ofs_y = 0}, /* '1' */
    {.bitmap_index = 126, .adv_w = 80, .box_w = 8, .box_h = 7, .ofs_x = 0, .ofs_y = 0}, /* '2' */
    {.bitmap_index = 133, .adv_w = 80, .box_w = 8, .box_h = 7, .ofs_x = 0, .ofs_y = 0}, /* '3' */
    {.bitmap_index = 140, .adv_w = 80, .box_w = 8, .box_h = 7, .ofs_x = 0, .ofs_y = 0}, /* '4' */
    {.bitmap_index = 147, .adv_w = 80, .box_w = 8, .box_h = 7, .ofs_x = 0, .ofs_y = 0}, /* '5' */
    {.bitmap_index = 154, .adv_w = 80, .box_w = 8, .box_h = 7, .ofs_x = 0, .ofs_y = 0}, /* '6' */
    {.bitmap_index = 161, .adv_w = 80, .box_w = 8, .box_h = 7, .ofs_x = 0, .ofs_y = 0}, /* '7' */
    {.bitmap_index = 168, .adv_w = 80, .box_w = 8, .box_h = 7, .ofs_x = 0, .ofs_y = 0}, /* '8' */
    {.bitmap_index = 175, .adv_w = 80, .box_w = 8, .box_h = 7, .ofs_x = 0, .ofs_y = 0}, /* '9' */
    {.bitmap_index = 182, .adv_w = 80, .box_w = 8, .box_h = 7, .ofs_x = 0, .ofs_y = 0}, /* ':' */
    {.bitmap_index = 189, .adv_w = 80, .box_w = 8, .box_h = 7, .ofs_x = 0, .ofs_y = 0}, /* ';' */
    {.bitmap_index = 196, .adv_w = 80, .box_w = 8, .box_h = 7, .ofs_x = 0, .ofs_y = 0}, /* '<' */
    {.bitmap_index = 203, .adv_w = 80, .box_w = 8, .box_h = 7, .ofs_x = 0, .ofs_y = 0}, /* '=' */
    {.bitmap_index = 210, .adv_w = 80, .box_w = 8, .box_h = 7, .ofs_x = 0, .ofs_y = 0}, /* '>' */
    {.bitmap_index = 217, .adv_w = 80, .box_w = 8, .box_h = 7, .ofs_x = 0, .ofs_y = 0}, /* '?' */
    {.bitmap_index = 224, .adv_w = 80, .box_w = 8, .box_h = 7, .ofs_x = 0, .ofs_y = 0}, /* '@' */
    {.bitmap_index = 231, .adv_w = 80, .box_w = 8, .box_h = 7, .ofs_x = 0, .ofs_y = 0}, /* 'A' */
    {.bitmap_index = 238, .adv_w = 80, .box_w = 8, .box_h = 7, .ofs_x = 0, .ofs_y = 0}, /* 'B' */
    {.bitmap_index = 245, .adv_w = 80, .box_w = 8, .box_h = 7, .ofs_x = 0, .ofs_y = 0}, /* 'C' */
    {.bitmap_index = 252, .adv_w = 80, .box_w = 8, .box_h = 7, .ofs_x = 0, .ofs_y = 0}, /* 'D' */
    {.bitmap_index = 259, .adv_w = 80, .box_w = 8, .box_h = 7, .ofs_x = 0, .ofs_y = 0}, /* 'E' */
    {.bitmap_index = 266, .adv_w = 80, .box_w = 8, .box_h = 7, .ofs_x = 0, .ofs_y = 0}, /* 'F' */
    {.bitmap_index = 273, .adv_w = 80, .box_w = 8, .box_h = 7, .ofs_x = 0, .ofs_y = 0}, /* 'G' */
    {.bitmap_index = 280, .adv_w = 80, .box_w = 8, .box_h = 7, .ofs_x = 0, .ofs_y = 0}, /* 'H' */
    {.bitmap_index = 287, .adv_w = 80, .box_w = 8, .box_h = 7, .ofs_x = 0, .ofs_y = 0}, /* 'I' */
    {.bitmap_index = 294, .adv_w = 80, .box_w = 8, .box_h = 7, .ofs_x = 0, .ofs_y = 0}, /* 'J' */
    {.bitmap_index = 301, .adv_w = 80, .box_w = 8, .box_h = 7, .ofs_x = 0, .ofs_y = 0}, /* 'K' */
    {.bitmap_index = 308, .adv_w = 80, .box_w = 8, .box_h = 7, .ofs_x = 0, .ofs_y = 0}, /* 'L' */
    {.bitmap_index = 315, .adv_w = 80, .box_w = 8, .box_h = 7, .ofs_x = 0, .ofs_y = 0}, /* 'M' */
    {.bitmap_index = 322, .adv_w = 80, .box_w = 8, .box_h = 7, .ofs_x = 0, .ofs_y = 0}, /* 'N' */
    {.bitmap_index = 329, .adv_w = 80, .box_w = 8, .box_h = 7, .ofs_x = 0, .ofs_y = 0}, /* 'O' */
    {.bitmap_index = 336, .adv_w = 80, .box_w = 8, .box_h = 7, .ofs_x = 0, .ofs_y = 0}, /* 'P' */
    {.bitmap_index = 343, .adv_w = 80, .box_w = 8, .box_h = 7, .ofs_x = 0, .ofs_y = 0}, /* 'Q' */
    {.bitmap_index = 350, .adv_w = 80, .box_w = 8, .box_h = 7, .ofs_x = 0, .ofs_y = 0}, /* 'R' */
    {.bitmap_index = 357, .adv_w = 80, .box_w = 8, .box_h = 7, .ofs_x = 0, .ofs_y = 0}, /* 'S' */
    {.bitmap_index = 364, .adv_w = 80, .box_w = 8, .box_h = 7, .ofs_x = 0, .ofs_y = 0}, /* 'T' */
    {.bitmap_index = 371, .adv_w = 80, .box_w = 8, .box_h = 7, .ofs_x = 0, .ofs_y = 0}, /* 'U' */
    {.bitmap_index = 378, .adv_w = 80, .box_w = 8, .box_h = 7, .ofs_x = 0, .ofs_y = 0}, /* 'V' */
    {.bitmap_index = 385, .adv_w = 80, .box_w = 8, .box_h = 7, .ofs_x = 0, .ofs_y = 0}, /* 'W' */
    {.bitmap_index = 392, .adv_w = 80, .box_w = 8, .box_h = 7, .ofs_x = 0, .ofs_y = 0}, /* 'X' */
    {.bitmap_index = 399, .adv_w = 80, .box_w = 8, .box_h = 7, .ofs_x = 0, .ofs_y = 0}, /* 'Y' */
    {.bitmap_index = 406, .adv_w = 80, .box_w = 8, .box_h = 7, .ofs_x = 0, .ofs_y = 0}, /* 'Z' */
    {.bitmap_index = 413, .adv_w = 80, .box_w = 8, .box_h = 7, .ofs_x = 0, .ofs_y = 0}, /* '[' */
    {.bitmap_index = 420, .adv_w = 80, .box_w = 8, .box_h = 7, .ofs_x = 0, .ofs_y = 0}, /* '\' */
    {.bitmap_index = 427, .adv_w = 80, .box_w = 8, .box_h = 7, .ofs_x = 0, .ofs_y = 0}, /* ']' */
    {.bitmap_index = 434, .adv_w = 80, .box_w = 8, .box_h = 7, .ofs_x = 0, .ofs_y = 0}, /* '^' */
    {.bitmap_index = 441, .adv_w = 80, .box_w = 8, .box_h = 7, .ofs_x = 0, .ofs_y = 0}, /* '_' */
    {.bitmap_index = 448, .adv_w = 80, .box_w = 8, .box_h = 7, .ofs_x = 0, .ofs_y = 0}, /* '`' */
    {.bitmap_index = 455, .adv_w = 80, .box_w = 8, .box_h = 7, .ofs_x = 0, .ofs_y = 0}, /* 'a' */
    {.bitmap_index = 462, .adv_w = 80, .box_w = 8, .box_h = 7, .ofs_x = 0, .ofs_y = 0}, /* 'b' */
    {.bitmap_index = 469, .adv_w = 80, .box_w = 8, .box_h = 7, .ofs_x = 0, .ofs_y = 0}, /* 'c' */
    {.bitmap_index = 476, .adv_w = 80, .box_w = 8, .box_h = 7, .ofs_x = 0, .ofs_y = 0}, /* 'd' */
    {.bitmap_index = 483, .adv_w = 80, .box_w = 8, .box_h = 7, .ofs_x = 0, .ofs_y = 0}, /* 'e' */
    {.bitmap_index = 490, .adv_w = 80, .box_w = 8, .box_h = 7, .ofs_x = 0, .ofs_y = 0}, /* 'f' */
    {.bitmap_index = 497, .adv_w = 80, .box_w = 8, .box_h = 8, .ofs_x = 0, .ofs_y = -1}, /* 'g' */
    {.bitmap_index = 505, .adv_w = 80, .box_w = 8, .box_h = 7, .ofs_x = 0, .ofs_y = 0}, /* 'h' */
    {.bitmap_index = 512, .adv_w = 80, .box_w = 8, .box_h = 7, .ofs_x = 0, .ofs_y = 0}, /* 'i' */
    {.bitmap_index = 519, .adv_w = 80, .box_w = 8, .box_h = 8, .ofs_x = 0, .ofs_y = -1}, /* 'j' */
    {.bitmap_index = 527, .adv_w = 80, .box_w = 8, .box_h = 7, .ofs_x = 0, .ofs_y = 0}, /* 'k' */
    {.bitmap_index = 534, .adv_w = 80, .box_w = 8, .box_h = 7, .ofs_x = 0, .ofs_y = 0}, /* 'l' */
    {.bitmap_index = 541, .adv_w = 80, .box_w = 8, .box_h = 7, .ofs_x = 0, .ofs_y = 0}, /* 'm' */
    {.bitmap_index = 548, .adv_w = 80, .box_w = 8, .box_h = 7, .ofs_x = 0, .ofs_y = 0}, /* 'n' */
    {.bitmap_index = 555, .adv_w = 80, .box_w = 8, .box_h = 7, .ofs_x = 0, .ofs_y = 0}, /* 'o' */
    {.bitmap_index = 562, .adv_w = 80, .box_w = 8, .box_h = 8, .ofs_x = 0, .ofs_y = -1}, /* 'p' */
    {.bitmap_index = 570, .adv_w = 80, .box_w = 8, .box_h = 8, .ofs_x = 0, .ofs_y = -1}, /* 'q' */
    {.bitmap_index = 578, .adv_w = 80, .box_w = 8, .box_h = 7, .ofs_x = 0, .ofs_y = 0}, /* 'r' */
    {.bitmap_index = 585, .adv_w = 80, .box_w = 8, .box_h = 7, .ofs_x = 0, .ofs_y = 0}, /* 's' */
    {.bitmap_index = 592, .adv_w = 80, .box_w = 8, .box_h = 7, .ofs_x = 0, .ofs_y = 0}, /* 't' */
    {.bitmap_index = 599, .adv_w = 80, .box_w = 8, .box_h = 7, .ofs_x = 0, .ofs_y = 0}, /* 'u' */
    {.bitmap_index = 606, .adv_w = 80, .box_w = 8, .box_h = 7, .ofs_x = 0, .ofs_y = 0}, /* 'v' */
    {.bitmap_index = 613, .adv_w = 80, .box_w = 8, .box_h = 7, .ofs_x = 0, .ofs_y = 0}, /* 'w' */
    {.bitmap_index = 620, .adv_w = 80, .box_w = 8, .box_h = 7, .ofs_x = 0, .ofs_y = 0}, /* 'x' */
    {.bitmap_index = 627, .adv_w = 80, .box_w = 8, .box_h = 8, .ofs_x = 0, .ofs_y = -1}, /* 'y' */
    {.bitmap_index = 635, .adv_w = 80, .box_w = 8, .box_h = 7, .ofs_x = 0, .ofs_y = 0}, /* 'z' */
    {.bitmap_index = 642, .adv_w = 80, .box_w = 8, .box_h = 7, .ofs_x = 0, .ofs_y = 0}, /* '{' */
    {.bitmap_index = 649, .adv_w = 80, .box_w = 8, .box_h = 7, .ofs_x = 0, .ofs_y = 0}, /* '|' */
    {.bitmap_index = 656, .adv_w = 80, .box_w = 8, .box_h = 7, .ofs_x = 0, .ofs_y = 0}, /* '}' */
    {.bitmap_index = 663, .adv_w = 80, .box_w = 8, .box_h = 7, .ofs_x = 0, .ofs_y = 0}, /* '~' */
};

/* Character mapping: 0x20-0x7E */
/* LVGL 9 requires cmaps to be defined differently */
static const lv_font_fmt_txt_cmap_t cmaps[] = {
    {
        .range_start = 0x20,
        .range_length = 0x5F, /* 0x7E - 0x20 + 1 = 95 */
        .glyph_id_start = 1,
        .unicode_list = NULL,
        .glyph_id_ofs_list = NULL,
        .list_length = 0,
        .type = LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY
    }
};

const lv_font_t lv_font_5x7 = {
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,
    .line_height = 7,
    .base_line = 0,
    .dsc = &(lv_font_fmt_txt_dsc_t){
        .glyph_bitmap = glyph_bitmap,
        .glyph_dsc = glyph_dsc,
        .cmaps = cmaps,
        .kern_dsc = NULL,
        .kern_scale = 0,
        .cmap_num = 1,
        .bpp = 1,
        .bitmap_format = 0,
    }
};
