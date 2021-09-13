/*  picovdu.c - VGA Display and USB keyboard for BBC Basic on Pico */

#define VID_CORE    1
#define USE_INTERP  1
#define HIRES       0   // CAUTION - Enabling HIRES requires extreme Pico overclock
#define DEBUG       1

#include "pico/platform.h"
#include "pico.h"
#include "pico/stdlib.h"
#include "pico/time.h"
#include "pico/scanvideo.h"
#include "pico/scanvideo/scanvideo_base.h"
#include "pico/scanvideo/composable_scanline.h"
#include "pico/multicore.h"
#include "hardware/clocks.h"
#if USE_INTERP
#include "hardware/interp.h"
#endif
#include "bsp/board.h"
#include "tusb.h"
#include "class/hid/hid.h"
#include "bbccon.h"
#include <stdio.h>

#if HIRES
#define SWIDTH  800
#define SHEIGHT 600
#else
#define SWIDTH  640
#define SHEIGHT 480
#endif
#define VGA_FLAG    0x1234  // Used to syncronise cores

static uint8_t  framebuf[SWIDTH * SHEIGHT / 8];
static uint16_t renderbuf[256 * 8];

static bool bBlank = true;      // Blank video screen
static int col = 0;     // Cursor column position
static int row = 0;     // Cursor row position
static int fg;          // Text foreground colour
static int bg;          // Text backgound colour
static uint8_t bgfill;  // Pixel fill for background colour
static int gmd;         // Graphics mode
static int gfg;         // Graphics foreground colour
static int gbg;         // Graphics background colour
static bool bPrint = false;     // Enable output to printer (UART)

// Temporary definitions for testing
#if 0
#define ESCFLG  1
#define KILL    2
#define VDUDIS  1
#define HRGFLG  2
static uint8_t  scroln;
static uint8_t  flags;
static uint8_t  cmcflg;
static uint8_t  vflags;
#endif

static uint16_t curpal[16];

static const uint16_t defpal[16] =
    {
    PICO_SCANVIDEO_PIXEL_FROM_RGB8(  0u,   0u,   0u),
    PICO_SCANVIDEO_PIXEL_FROM_RGB8(128u,   0u,   0u),
    PICO_SCANVIDEO_PIXEL_FROM_RGB8(  0u, 128u,   0u),
    PICO_SCANVIDEO_PIXEL_FROM_RGB8(128u, 128u,   0u),
    PICO_SCANVIDEO_PIXEL_FROM_RGB8(  0u,   0u, 128u),
    PICO_SCANVIDEO_PIXEL_FROM_RGB8(128u,   0u, 128u),
    PICO_SCANVIDEO_PIXEL_FROM_RGB8(  0u, 128u, 128u),
    PICO_SCANVIDEO_PIXEL_FROM_RGB8(128u, 128u, 128u),
    PICO_SCANVIDEO_PIXEL_FROM_RGB8( 64u,  64u,  64u),
    PICO_SCANVIDEO_PIXEL_FROM_RGB8(255u,   0u,   0u),
    PICO_SCANVIDEO_PIXEL_FROM_RGB8(  0u, 255u,   0u),
    PICO_SCANVIDEO_PIXEL_FROM_RGB8(255u, 255u,   0u),
    PICO_SCANVIDEO_PIXEL_FROM_RGB8(  0u,   0u, 255u),
    PICO_SCANVIDEO_PIXEL_FROM_RGB8(255u,   0u, 255u),
    PICO_SCANVIDEO_PIXEL_FROM_RGB8(  0u, 255u, 255u),
    PICO_SCANVIDEO_PIXEL_FROM_RGB8(255u, 255u, 255u)
    };

#include "font_10.h"
static const uint8_t cpx02[] = { 0x00, 0xFF };
static const uint16_t cpx04[] = { 0x0000, 0x5555, 0xAAAA, 0xFFFF };
static const uint32_t cpx16[] = { 0x00000000, 0x11111111, 0x22222222, 0x33333333,
    0x44444444, 0x5555555, 0x6666666, 0x7777777,
    0x88888888, 0x9999999, 0xAAAAAAA, 0xBBBBBBB,
    0xCCCCCCCC, 0xDDDDDDD, 0xEEEEEEE, 0xFFFFFFF };

static const uint16_t pmsk04[256] = {
    0x0000, 0x0003, 0x000C, 0x000F, 0x0030, 0x0033, 0x003C, 0x003F,
    0x00C0, 0x00C3, 0x00CC, 0x00CF, 0x00F0, 0x00F3, 0x00FC, 0x00FF,
    0x0300, 0x0303, 0x030C, 0x030F, 0x0330, 0x0333, 0x033C, 0x033F,
    0x03C0, 0x03C3, 0x03CC, 0x03CF, 0x03F0, 0x03F3, 0x03FC, 0x03FF,
    0x0C00, 0x0C03, 0x0C0C, 0x0C0F, 0x0C30, 0x0C33, 0x0C3C, 0x0C3F,
    0x0CC0, 0x0CC3, 0x0CCC, 0x0CCF, 0x0CF0, 0x0CF3, 0x0CFC, 0x0CFF,
    0x0F00, 0x0F03, 0x0F0C, 0x0F0F, 0x0F30, 0x0F33, 0x0F3C, 0x0F3F,
    0x0FC0, 0x0FC3, 0x0FCC, 0x0FCF, 0x0FF0, 0x0FF3, 0x0FFC, 0x0FFF,
    0x3000, 0x3003, 0x300C, 0x300F, 0x3030, 0x3033, 0x303C, 0x303F,
    0x30C0, 0x30C3, 0x30CC, 0x30CF, 0x30F0, 0x30F3, 0x30FC, 0x30FF,
    0x3300, 0x3303, 0x330C, 0x330F, 0x3330, 0x3333, 0x333C, 0x333F,
    0x33C0, 0x33C3, 0x33CC, 0x33CF, 0x33F0, 0x33F3, 0x33FC, 0x33FF,
    0x3C00, 0x3C03, 0x3C0C, 0x3C0F, 0x3C30, 0x3C33, 0x3C3C, 0x3C3F,
    0x3CC0, 0x3CC3, 0x3CCC, 0x3CCF, 0x3CF0, 0x3CF3, 0x3CFC, 0x3CFF,
    0x3F00, 0x3F03, 0x3F0C, 0x3F0F, 0x3F30, 0x3F33, 0x3F3C, 0x3F3F,
    0x3FC0, 0x3FC3, 0x3FCC, 0x3FCF, 0x3FF0, 0x3FF3, 0x3FFC, 0x3FFF,
    0xC000, 0xC003, 0xC00C, 0xC00F, 0xC030, 0xC033, 0xC03C, 0xC03F,
    0xC0C0, 0xC0C3, 0xC0CC, 0xC0CF, 0xC0F0, 0xC0F3, 0xC0FC, 0xC0FF,
    0xC300, 0xC303, 0xC30C, 0xC30F, 0xC330, 0xC333, 0xC33C, 0xC33F,
    0xC3C0, 0xC3C3, 0xC3CC, 0xC3CF, 0xC3F0, 0xC3F3, 0xC3FC, 0xC3FF,
    0xCC00, 0xCC03, 0xCC0C, 0xCC0F, 0xCC30, 0xCC33, 0xCC3C, 0xCC3F,
    0xCCC0, 0xCCC3, 0xCCCC, 0xCCCF, 0xCCF0, 0xCCF3, 0xCCFC, 0xCCFF,
    0xCF00, 0xCF03, 0xCF0C, 0xCF0F, 0xCF30, 0xCF33, 0xCF3C, 0xCF3F,
    0xCFC0, 0xCFC3, 0xCFCC, 0xCFCF, 0xCFF0, 0xCFF3, 0xCFFC, 0xCFFF,
    0xF000, 0xF003, 0xF00C, 0xF00F, 0xF030, 0xF033, 0xF03C, 0xF03F,
    0xF0C0, 0xF0C3, 0xF0CC, 0xF0CF, 0xF0F0, 0xF0F3, 0xF0FC, 0xF0FF,
    0xF300, 0xF303, 0xF30C, 0xF30F, 0xF330, 0xF333, 0xF33C, 0xF33F,
    0xF3C0, 0xF3C3, 0xF3CC, 0xF3CF, 0xF3F0, 0xF3F3, 0xF3FC, 0xF3FF,
    0xFC00, 0xFC03, 0xFC0C, 0xFC0F, 0xFC30, 0xFC33, 0xFC3C, 0xFC3F,
    0xFCC0, 0xFCC3, 0xFCCC, 0xFCCF, 0xFCF0, 0xFCF3, 0xFCFC, 0xFCFF,
    0xFF00, 0xFF03, 0xFF0C, 0xFF0F, 0xFF30, 0xFF33, 0xFF3C, 0xFF3F,
    0xFFC0, 0xFFC3, 0xFFCC, 0xFFCF, 0xFFF0, 0xFFF3, 0xFFFC, 0xFFFF };
static const uint32_t pmsk16[256] = {
    0x00000000, 0x0000000F, 0x000000F0, 0x000000FF, 0x00000F00, 0x00000F0F, 0x00000FF0, 0x00000FFF,
    0x0000F000, 0x0000F00F, 0x0000F0F0, 0x0000F0FF, 0x0000FF00, 0x0000FF0F, 0x0000FFF0, 0x0000FFFF,
    0x000F0000, 0x000F000F, 0x000F00F0, 0x000F00FF, 0x000F0F00, 0x000F0F0F, 0x000F0FF0, 0x000F0FFF,
    0x000FF000, 0x000FF00F, 0x000FF0F0, 0x000FF0FF, 0x000FFF00, 0x000FFF0F, 0x000FFFF0, 0x000FFFFF,
    0x00F00000, 0x00F0000F, 0x00F000F0, 0x00F000FF, 0x00F00F00, 0x00F00F0F, 0x00F00FF0, 0x00F00FFF,
    0x00F0F000, 0x00F0F00F, 0x00F0F0F0, 0x00F0F0FF, 0x00F0FF00, 0x00F0FF0F, 0x00F0FFF0, 0x00F0FFFF,
    0x00FF0000, 0x00FF000F, 0x00FF00F0, 0x00FF00FF, 0x00FF0F00, 0x00FF0F0F, 0x00FF0FF0, 0x00FF0FFF,
    0x00FFF000, 0x00FFF00F, 0x00FFF0F0, 0x00FFF0FF, 0x00FFFF00, 0x00FFFF0F, 0x00FFFFF0, 0x00FFFFFF,
    0x0F000000, 0x0F00000F, 0x0F0000F0, 0x0F0000FF, 0x0F000F00, 0x0F000F0F, 0x0F000FF0, 0x0F000FFF,
    0x0F00F000, 0x0F00F00F, 0x0F00F0F0, 0x0F00F0FF, 0x0F00FF00, 0x0F00FF0F, 0x0F00FFF0, 0x0F00FFFF,
    0x0F0F0000, 0x0F0F000F, 0x0F0F00F0, 0x0F0F00FF, 0x0F0F0F00, 0x0F0F0F0F, 0x0F0F0FF0, 0x0F0F0FFF,
    0x0F0FF000, 0x0F0FF00F, 0x0F0FF0F0, 0x0F0FF0FF, 0x0F0FFF00, 0x0F0FFF0F, 0x0F0FFFF0, 0x0F0FFFFF,
    0x0FF00000, 0x0FF0000F, 0x0FF000F0, 0x0FF000FF, 0x0FF00F00, 0x0FF00F0F, 0x0FF00FF0, 0x0FF00FFF,
    0x0FF0F000, 0x0FF0F00F, 0x0FF0F0F0, 0x0FF0F0FF, 0x0FF0FF00, 0x0FF0FF0F, 0x0FF0FFF0, 0x0FF0FFFF,
    0x0FFF0000, 0x0FFF000F, 0x0FFF00F0, 0x0FFF00FF, 0x0FFF0F00, 0x0FFF0F0F, 0x0FFF0FF0, 0x0FFF0FFF,
    0x0FFFF000, 0x0FFFF00F, 0x0FFFF0F0, 0x0FFFF0FF, 0x0FFFFF00, 0x0FFFFF0F, 0x0FFFFFF0, 0x0FFFFFFF,
    0xF0000000, 0xF000000F, 0xF00000F0, 0xF00000FF, 0xF0000F00, 0xF0000F0F, 0xF0000FF0, 0xF0000FFF,
    0xF000F000, 0xF000F00F, 0xF000F0F0, 0xF000F0FF, 0xF000FF00, 0xF000FF0F, 0xF000FFF0, 0xF000FFFF,
    0xF00F0000, 0xF00F000F, 0xF00F00F0, 0xF00F00FF, 0xF00F0F00, 0xF00F0F0F, 0xF00F0FF0, 0xF00F0FFF,
    0xF00FF000, 0xF00FF00F, 0xF00FF0F0, 0xF00FF0FF, 0xF00FFF00, 0xF00FFF0F, 0xF00FFFF0, 0xF00FFFFF,
    0xF0F00000, 0xF0F0000F, 0xF0F000F0, 0xF0F000FF, 0xF0F00F00, 0xF0F00F0F, 0xF0F00FF0, 0xF0F00FFF,
    0xF0F0F000, 0xF0F0F00F, 0xF0F0F0F0, 0xF0F0F0FF, 0xF0F0FF00, 0xF0F0FF0F, 0xF0F0FFF0, 0xF0F0FFFF,
    0xF0FF0000, 0xF0FF000F, 0xF0FF00F0, 0xF0FF00FF, 0xF0FF0F00, 0xF0FF0F0F, 0xF0FF0FF0, 0xF0FF0FFF,
    0xF0FFF000, 0xF0FFF00F, 0xF0FFF0F0, 0xF0FFF0FF, 0xF0FFFF00, 0xF0FFFF0F, 0xF0FFFFF0, 0xF0FFFFFF,
    0xFF000000, 0xFF00000F, 0xFF0000F0, 0xFF0000FF, 0xFF000F00, 0xFF000F0F, 0xFF000FF0, 0xFF000FFF,
    0xFF00F000, 0xFF00F00F, 0xFF00F0F0, 0xFF00F0FF, 0xFF00FF00, 0xFF00FF0F, 0xFF00FFF0, 0xFF00FFFF,
    0xFF0F0000, 0xFF0F000F, 0xFF0F00F0, 0xFF0F00FF, 0xFF0F0F00, 0xFF0F0F0F, 0xFF0F0FF0, 0xFF0F0FFF,
    0xFF0FF000, 0xFF0FF00F, 0xFF0FF0F0, 0xFF0FF0FF, 0xFF0FFF00, 0xFF0FFF0F, 0xFF0FFFF0, 0xFF0FFFFF,
    0xFFF00000, 0xFFF0000F, 0xFFF000F0, 0xFFF000FF, 0xFFF00F00, 0xFFF00F0F, 0xFFF00FF0, 0xFFF00FFF,
    0xFFF0F000, 0xFFF0F00F, 0xFFF0F0F0, 0xFFF0F0FF, 0xFFF0FF00, 0xFFF0FF0F, 0xFFF0FFF0, 0xFFF0FFFF,
    0xFFFF0000, 0xFFFF000F, 0xFFFF00F0, 0xFFFF00FF, 0xFFFF0F00, 0xFFFF0F0F, 0xFFFF0FF0, 0xFFFF0FFF,
    0xFFFFF000, 0xFFFFF00F, 0xFFFFF0F0, 0xFFFFF0FF, 0xFFFFFF00, 0xFFFFFF0F, 0xFFFFFFF0, 0xFFFFFFFF };

typedef struct
    {
    uint32_t nclr;      // Number of colours (2, 4 or 16)
    uint32_t gcol;      // Number of displayed pixel columns
    uint32_t grow;      // Number of displayed pixel rows
    uint32_t tcol;      // Number of text columns
    uint32_t trow;      // Number of text row
    uint32_t vmgn;      // Number of black lines at top (and bottom)
    uint32_t hmgn;      // Number of black pixels to left (and to right)
    uint32_t nppb;      // Number of pixels per byte
    uint32_t nbpl;      // Number of bytes per line
    uint32_t yscl;      // Y scale (number of repeats of each line)
    uint32_t thgt;      // Text height
    } MODE;

#if HIRES    // 800x600 VGA
static const MODE modes[] = {
// nclr gcol grow tcol trw  vmg hmg pb nbpl ys thg
    { 2, 640, 256,  80, 32,  44, 80, 8,  80, 2,  8},  // Mode  0 - 20KB
    { 4, 320, 256,  40, 32,  44, 80, 8,  80, 2,  8},  // Mode  1 - 20KB
    {16, 160, 256,  20, 32,  44, 80, 8,  80, 2,  8},  // Mode  2 - 20KB
    { 2, 640, 250,  80, 25,  47, 80, 8,  80, 2, 10},  // Mode  3 - 20KB
    { 2, 320, 256,  40, 32,  44, 80, 8,  80, 2,  8},  // Mode  4 - 10KB
    { 4, 160, 256,  20, 32,  44, 80, 8,  80, 2,  8},  // Mode  5 - 10KB
    { 2, 320, 250,  40, 25,  44, 80, 8,  80, 2, 10},  // Mode  6 - 10KB
    { 2, 640, 256,  40, 25,  44, 80, 8,  80, 2,  8},  // Mode  7 - Teletext TODO
    { 2, 640, 512,  80, 32,  44, 80, 8,  80, 2, 16},  // Mode  8 - 40KB
    { 4, 320, 512,  40, 32,  44, 80, 8,  80, 1, 16},  // Mode  9 - 40KB
    {16, 160, 512,  20, 32,  44, 80, 8,  80, 1, 16},  // Mode 10 - 40KB
    { 2, 640, 500,  80, 25,  47, 80, 8,  80, 1, 20},  // Mode 11 - 40KB
    { 2, 320, 512,  40, 32,  44, 80, 8,  80, 1, 16},  // Mode 12 - 20KB
    { 4, 160, 512,  20, 32,  44, 80, 8,  80, 1, 16},  // Mode 13 - 20KB
    { 2, 320, 500,  40, 25,  44, 80, 8,  80, 1, 20},  // Mode 14 - 20KB
    { 2, 640, 512,  40, 25,  44, 80, 8,  80, 1, 16},  // Mode 15 - Teletext ?
    { 2, 800, 600, 100, 30,   0,  0, 8, 100, 1, 20},  // Mode 16 - 59KB
    { 4, 400, 600,  50, 30,   0,  0, 8, 100, 1, 20},  // Mode 17 - 59KB
    {16, 200, 600,  25, 30,   0,  0, 8, 100, 1, 20},  // Mode 18 - 59KB
    { 4, 800, 300, 100, 30,   0,  0, 4, 200, 2, 20},  // Mode 19 - 59KB
    {16, 400, 300,  50, 30,   0,  0, 4, 200, 2, 10}   // Mode 20 - 59KB
    };
#else   // 640x480 VGA
static const MODE modes[] = {
// nclr gcol grow tcol trw  vmg hmg pb nbpl ys thg
    { 2, 640, 256,  80, 32, 112,  0, 8,  80, 1,  8},  // Mode  0 - 20KB
    { 4, 320, 256,  40, 32, 112,  0, 8,  80, 1,  8},  // Mode  1 - 20KB
    {16, 160, 256,  20, 32, 112,  0, 8,  80, 1,  8},  // Mode  2 - 20KB
    { 2, 640, 240,  80, 24,   0,  0, 8,  80, 2, 10},  // Mode  3 - 20KB
    { 2, 320, 256,  40, 32, 112,  0, 8,  80, 1,  8},  // Mode  4 - 10KB
    { 4, 160, 256,  20, 32, 112,  0, 8,  80, 1,  8},  // Mode  5 - 10KB
    { 2, 320, 240,  40, 25,   0,  0, 8,  80, 2, 10},  // Mode  6 - 10KB
    { 2, 640, 256,  40, 25, 112,  0, 8,  80, 1,  8},  // Mode  7 - Teletext TODO
    { 2, 640, 480,  80, 30,   0,  0, 8,  80, 1, 16},  // Mode  8 - 37.5KB
    { 4, 320, 480,  40, 30,   0,  0, 8,  80, 1, 16},  // Mode  9 - 37.5KB
    {16, 160, 480,  20, 30,   0,  0, 8,  80, 1, 16},  // Mode 10 - 37.5KB
    { 2, 640, 480,  80, 24,   0,  0, 8,  80, 1, 20},  // Mode 11 - 37.5KB
    { 2, 320, 480,  40, 30,   0,  0, 8,  80, 1, 16},  // Mode 12 - 18.75KB
    { 4, 160, 480,  20, 30,   0,  0, 8,  80, 1, 16},  // Mode 13 - 18.75KB
    { 2, 320, 480,  40, 24,   0,  0, 8,  80, 1, 20},  // Mode 14 - 18.75KB
    { 2, 640, 480,  40, 24,   0,  0, 8,  80, 1, 16},  // Mode 15 - Teletext ?
    };
#endif

static const MODE *pmode;

extern int getkey (unsigned char *pkey);

/****** Routines executed on Core 1 **********************************************************/

#if USE_INTERP
static bool bCfgInt = false;    // Configure interpolators
extern void convert_from_pal16_8 (uint32_t *dest, uint8_t *src, uint32_t count);
extern void convert_from_pal16_4 (uint32_t *dest, uint8_t *src, uint32_t count);
extern void convert_from_pal16 (uint32_t *dest, uint8_t *src, uint32_t count);
#endif

#if HIRES
extern const scanvideo_timing_t vga_timing_800x600_60_default;
#endif

// The interpolators are core specific, so must be
// configured on the same core as the render_loop
#if USE_INTERP
static void setup_interp (void)
    {
    if ( pmode != NULL )
        {
        interp_config c = interp_default_config();
        if ( pmode->nppb == 8 )
            {
            interp_config_set_shift (&c, 20);
            interp_config_set_mask (&c, 4, 11);
            interp_set_config (interp0, 0, &c);
            interp_config_set_shift (&c, 12);
            }
        else if ( pmode->nppb == 4 )
            {
            interp_config_set_shift (&c, 21);
            interp_config_set_mask (&c, 3, 10);
            interp_set_config (interp0, 0, &c);
            interp_config_set_shift (&c, 13);
            }
        else
            {
            interp_config_set_shift (&c, 22);
            interp_config_set_mask (&c, 2, 9);
            interp_set_config (interp0, 0, &c);
            interp_config_set_shift (&c, 14);
            }
        interp_config_set_cross_input (&c, true);
        interp_set_config (interp0, 1, &c);
        interp_set_base (interp0, 0, (uintptr_t)renderbuf);
        interp_set_base (interp0, 1, (uintptr_t)renderbuf);
#if DEBUG > 1
        printf ("Interpolator test:\n");
        for (uint32_t i = 0; i < 256; ++i )
            {
#if 1
            static uint16_t buf[32];
            static uint8_t dfb[4];
            dfb[0] = i;
            dfb[1] = i;
            dfb[2] = i;
            dfb[3] = i;
            convert_from_pal16_8 ((uint32_t *) buf, dfb, 4);
            printf ("interp (%3d) = I", i);
            for (int j = 0; j < 32; ++j)
                {
                if ( buf[j] ) printf ("*");
                else printf (" ");
                }
            printf ("I\n");
#else
            interp_set_accumulator (interp0, 0, i << 24);
            uint16_t *prb = (uint16_t *) interp_peek_lane_result (interp0, 0);
            printf ("interp (%3d) = 0x%08X:", i, prb);
            for (int j = 0; j < 8; ++j)
                {
                if ( *prb ) printf (" %04X", *prb);
                else printf (" ----");
                ++prb;
                }
            printf ("\n");
#endif
            }
#endif
        bCfgInt = false;
        bBlank = false;
        }
    }
#endif

void __time_critical_func(render_loop) (void)
    {
    while (true)
        {
#if USE_INTERP
        if ( bCfgInt ) setup_interp ();
#endif
        struct scanvideo_scanline_buffer *buffer = scanvideo_begin_scanline_generation (true);
        uint32_t *twopix = buffer->data;
        int iScan = scanvideo_scanline_number (buffer->scanline_id) - pmode->vmgn;
        if (( bBlank ) || (iScan < 0) || (iScan >= pmode->grow * pmode->yscl))
            {
            twopix[0] = COMPOSABLE_COLOR_RUN;
            twopix[2] = SWIDTH - 2 | ( COMPOSABLE_EOL_ALIGN << 16 );
            buffer->data_used = 2;
            }
        else
            {
            iScan /= pmode->yscl;
            if ( pmode->hmgn > 0 )
                {
                *twopix = COMPOSABLE_COLOR_RUN;             // High word is zero, black pixel
                ++twopix;
                *twopix = (pmode->hmgn - 5) | COMPOSABLE_RAW_2P;    // To use an even number of 16-bit words
                ++twopix;
                *twopix = 0;                                // 2 black pixels
                }
            uint32_t *pxline = twopix;
            uint8_t *pfb = framebuf + iScan * pmode->nbpl;
            if ( pmode->nppb == 8 )
                {
#if USE_INTERP
                convert_from_pal16_8 (twopix, pfb, pmode->nbpl);
                twopix += 4 * pmode->nbpl;
#else
                for (int i = 0; i < pmode->nbpl; ++i)
                    {
                    uint32_t *twoclr = (uint32_t *) &renderbuf[8 * (*pfb)];
                    *(++twopix) = *twoclr;
                    *(++twopix) = *(++twoclr);
                    *(++twopix) = *(++twoclr);
                    *(++twopix) = *(++twoclr);
                    ++pfb;
                    }
#endif
                }
            else if ( pmode->nppb == 4 )
                {
#if USE_INTERP
                convert_from_pal16_4 (twopix, pfb, pmode->nbpl);
                twopix += 2 * pmode->nbpl;
#else
                for (int i = 0; i < pmode->nbpl; ++i)
                    {
                    uint32_t *twoclr = (uint32_t *) &renderbuf[4 * (*pfb)];
                    *(++twopix) = *twoclr;
                    *(++twopix) = *(++twoclr);
                    ++pfb;
                    }
#endif
                }
            else if ( pmode->nppb == 2 )
                {
#if USE_INTERP
                convert_from_pal16 (twopix, pfb, pmode->nbpl);
                twopix += pmode->nbpl;
#else
                for (int i = 0; i < pmode->nbpl; ++i)
                    {
                    uint32_t *twoclr = (uint32_t *) &renderbuf[2*(*pfb)];
                    *(++twopix) = *twoclr;
                    ++pfb;
                    }
#endif
                }
            *twopix = COMPOSABLE_EOL_ALIGN << 16;   // Implicit zero (black) in low word
            ++twopix;
            pxline[0] = ( pxline[1] << 16 ) | COMPOSABLE_RAW_RUN;
            pxline[1] = ( pxline[1] & 0xFFFF0000 ) | ( pmode->nbpl * pmode->nppb - 2 );
            buffer->data_used = twopix - buffer->data;
            }
        scanvideo_end_scanline_generation (buffer);
        }
    }

void setup_video (void)
    {
#if DEBUG > 0
#if USE_INTERP
    printf ("setup_video: Using interpolator\n");
#endif
    printf ("setup_video: System clock speed %d kHz\n", clock_get_hz (clk_sys) / 1000);
    printf ("setup_video: Starting video\n");
#endif
#if HIRES
    scanvideo_setup (&vga_mode_800x600_60);
#else
    scanvideo_setup (&vga_mode_640x480_60);
#endif
    multicore_fifo_push_blocking (VGA_FLAG);
    scanvideo_timing_enable (true);
#if DEBUG > 0
    printf ("setup_video: System clock speed %d kHz\n", clock_get_hz (clk_sys) / 1000);
#endif
#if DEBUG > 0
    printf ("setup_video: Starting render\n");
#endif
    render_loop ();
    }

/****** Routines executed on Core 0 **********************************************************/

static void genrb (void)
    {
#if DEBUG > 0
    printf ("genrb\n");
#endif
    uint16_t *prb = (uint16_t *)renderbuf;
    if ( pmode->nclr == 2 )
        {
        for (int i = 0; i < 256; ++i)
            {
            uint8_t pix = i;
            for (int j = 0; j < 8; ++j)
                {
                *prb = curpal[pix & 0x01];
                ++prb;
                pix >>= 1;
                }
            }
        }
    else if ( pmode->nclr == 4 )
        {
        for (int i = 0; i < 256; ++i)
            {
            uint8_t pix = i;
            for (int j = 0; j < 4; ++j)
                {
                *prb = curpal[pix & 0x03];
                ++prb;
                if ( pmode->nppb == 8 )
                    {
                    *prb = curpal[pix & 0x03];
                    ++prb;
                    }
                pix >>= 2;
                }
            }
        }
    else if ( pmode->nclr == 16 )
        {
        for (int i = 0; i < 256; ++i)
            {
            uint16_t clr = curpal[i & 0x0F];
            for (int j = 0; j < pmode->nppb / 2; ++j)
                {
                *prb = clr;
                ++prb;
                }
            clr = curpal[i >> 4];
            for (int j = 0; j < pmode->nppb / 2; ++j)
                {
                *prb = clr;
                ++prb;
                }
            }
        }
#if DEBUG > 0
    printf ("End genrb: prb - renderbuf = %p - %p = %d\n", prb, renderbuf, prb - renderbuf);
#endif
    }

static void scrldn (void)
    {
    memmove (framebuf + pmode->thgt * pmode->nbpl, framebuf,
        (pmode->grow - pmode->thgt) * pmode->nbpl);
    memset (framebuf, bgfill, pmode->thgt * pmode->nbpl);
    }

static void scrlup (void)
    {
    memmove (framebuf, framebuf + pmode->thgt * pmode->nbpl,
        (pmode->grow - pmode->thgt) * pmode->nbpl);
    memset (framebuf + (pmode->grow - pmode->thgt) * pmode->nbpl,
        bgfill, pmode->thgt * pmode->nbpl);
    }

static void cls (uint8_t fill)
    {
    memset (framebuf, fill, pmode->grow * pmode->nbpl);
    }

static void dispchr (int chr)
    {
#if DEBUG > 1
    if ((chr >= 0x20) && (chr <= 0x7F)) printf ("dispchr ('%c')\n", chr);
    else printf ("dispchr (0x%02X)\n", chr);
#endif
    uint32_t fhgt = pmode->thgt;
    bool bDbl = false;
    if ( fhgt > 10 )
        {
        fhgt /= 2;
        bDbl = true;
        }
    const uint8_t *pch = font_10[chr];
    uint8_t *pfb = framebuf + row * pmode->thgt * pmode->nbpl;
    if ( fhgt == 8 ) ++pch;
    if ( pmode->nclr == 2 )
        {
        pfb += col;
        for (int i = 0; i < fhgt; ++i)
            {
#if DEBUG > 1
            printf ("0x%02X:", *pch);
            for (int j = 0; j < 8; ++j)
                {
                uint16_t clr = renderbuf[8*(*pch)+j];
                if ( clr > 0 ) printf (" 0x%04X", clr);
                else printf ("       ");
                }
            printf ("\n");
#endif
            *pfb = *pch;
            pfb += pmode->nbpl;
            if ( bDbl )
                {
                *pfb = *pch;
                pfb += pmode->nbpl;
                }
            ++pch;
            }
        }
    else if ( pmode->nclr == 4 )
        {
        pfb += 2 * col;
        uint16_t fpx = cpx04[fg];
        uint16_t bpx = cpx04[bg];
        for (int i = 0; i < fhgt; ++i)
            {
            uint16_t mask = pmsk04[*pch];
            uint16_t pix = ( mask & fpx ) | ( (~ mask) & bpx );
            *((uint16_t *)pfb) = pix;
            pfb += pmode->nbpl;
            if ( bDbl )
                {
                *((uint16_t *)pfb) = pix;
                pfb += pmode->nbpl;
                }
            ++pch;
            }
        }
    else if ( pmode->nclr == 16 )
        {
        pfb += 2 * col;
        uint32_t fpx = cpx16[fg];
        uint32_t bpx = cpx16[bg];
        for (int i = 0; i < fhgt; ++i)
            {
            uint32_t mask = pmsk16[*pch];
            uint32_t pix = ( mask & fpx ) | ( (~ mask) & bpx );
            *((uint32_t *)pfb) = pix;
            pfb += pmode->nbpl;
            if ( bDbl )
                {
                *((uint32_t *)pfb) = pix;
                pfb += pmode->nbpl;
                }
            ++pch;
            }
        }
    }

static void newline (int *px, int *py)
    {
    if (++(*py) == pmode->trow)
        {
        if ((scroln & 0x80) && (--scroln == 0x7F))
            {
            unsigned char ch;
            scroln = 0x7F + pmode->trow;;
            do
                {
                sleep_us (5000);
                } 
            while ((getkey (&ch) == 0) && ((flags & (ESCFLG | KILL)) == 0));
            }
        scrlup ();
        *py = pmode->trow - 1;
        }
	if ( bPrint )
        {
        printf ("\n");
        if (*px) printf ("\033[%i;%iH", *py + 1, *px + 1);
        }
    }

static void wrap (void)
    {
    if ( bPrint ) printf ("\r");
    col = 0;
    newline (&col, &row);
    }

void showchr (int chr)
    {
    if (col > pmode->tcol) wrap ();
    dispchr (chr);
    if ( bPrint ) putchar (chr);
    if ((col == pmode->tcol) && ((cmcflg & 1) == 0)) wrap ();
    else if ((signed char)(chr & 0xFF) >= -64) col++;
    }

static void clrreset (void)
    {
    if ( pmode->nclr == 2 )
        {
#if DEBUG > 0
        printf ("clrreset: nclr = 2\n");
#endif
        curpal[0] = defpal[0];
        curpal[1] = defpal[15];
        }
    else if ( pmode->nclr == 4 )
        {
#if DEBUG > 0
        printf ("clrreset: nclr = 4\n");
#endif
        curpal[0] = defpal[0];
        curpal[1] = defpal[9];
        curpal[2] = defpal[11];
        curpal[3] = defpal[15];
        }
    else
        {
#if DEBUG > 0
        printf ("clrreset: nclr = 16\n");
#endif
        memcpy (curpal, defpal, sizeof (defpal));
        }
    genrb ();
    }

static void modechg (int mode)
    {
#if DEBUG > 0
    printf ("modechg (%d)\n", mode);
#endif
    if ( mode < sizeof (modes) / sizeof (modes[0]) )
        {
        bBlank = true;
        pmode = &modes[mode];
        clrreset ();
#if USE_INTERP
        bCfgInt = true;
#else
        bBlank = false;
#endif
#if DEBUG > 0
        printf ("modechg: New mode set\n");
#endif
        }
    }

static inline int clrmsk (int clr)
    {
    if ( pmode->nclr == 16 ) return ( clr & 0x0F );
    else if ( pmode->nclr == 4 ) return ( clr & 0x03 );
    return ( clr & 0x01 );
    }

void xeqvdu (int code, int data1, int data2)
    {
    if ( pmode == NULL ) return;
    int vdu = code >> 8;

    if ((vflags & VDUDIS) && (vdu != 6))
        return;

    switch (vdu)
        {
        case 1: // Next character to printer only
            putchar (code & 0xFF);
            break;
            
        case 2: // PRINTER ON
            bPrint = true;
            break;

        case 3: // PRINTER OFF
            bPrint = false;
            break;

        case 4: // LO-RES TEXT
            vflags &= ~HRGFLG;
            break;

        case 5: // HI-RES TEXT
            vflags |= HRGFLG;
            break;

        case 6: // ENABLE VDU DRIVERS
            vflags &= ~VDUDIS;
            break;

        case 7: // BELL
            if ( bPrint ) putchar (vdu);
            break;

        case 8: // LEFT
            if (col == 0)
                {
                col = pmode->tcol;
                if (row == 0)
                    {
                    scrldn ();
                    if ( bPrint ) printf ("\033M");
                    }
                else
                    {
                    row --;
                    if ( bPrint ) printf ("\033[%i;%iH", row + 1, col + 1);
                    }
                }
            else
                {
                col--;
                if ( bPrint ) putchar (vdu);
                }
            break;

        case 9: // RIGHT
            if (col == pmode->tcol)
                {
                wrap ();
                }
            else
                {
                col++;
                if ( bPrint ) printf ("\033[C");
                }
            break;

        case 10: // LINE FEED
            newline (&col, &row);
            break;

        case 11: // UP
            if ( row == 0 ) scrldn ();
            else --row;
            if ( bPrint ) printf ("\033M");
            break;

        case 12: // CLEAR SCREEN
            cls (bgfill);
            if ( bPrint ) printf ("\033[H\033[J");
            col = 0;
            row = 0;
            break;

        case 13: // RETURN
            if ( bPrint ) putchar (vdu);
            col = 0;
            break;

        case 14: // PAGING ON
            scroln = 0x80 + row;
            break;

        case 15: // PAGING OFF
            scroln = 0;
            break;

        case 16: // CLEAR GRAPHICS SCREEN
        {
            uint8_t fill;
            if ( pmode->nclr == 16 )        fill = (uint8_t) cpx16[bg];
            else if ( pmode->nclr == 4 )    fill = (uint8_t) cpx04[bg];
            else                            fill = cpx02[bg];
            cls (fill);
            if ( bPrint ) printf ("\033[H\033[J");
            col = 0;
            row = 0;
            break;
        }

        case 17: // COLOUR n
            fg = clrmsk (code);
            bg = clrmsk (code >> 4);
            if ( pmode->nclr == 16 )        bgfill = (uint8_t) cpx16[bg];
            else if ( pmode->nclr == 4 )    bgfill = (uint8_t) cpx04[bg];
            else                            bgfill = cpx02[bg];
            if ( bPrint )
                {
                vdu = 30 + (code & 7);
                if (code & 8)
                    vdu += 60;
                if (code & 0x80)
                    vdu += 10;
                printf ("\033[%im", vdu);
                }
            break;

        case 18: // GCOL m, n
            gmd = code & 0x0F;
            gfg = clrmsk (data1);
            gbg = clrmsk (data1 >> 4);
            if ( bPrint )
                {
                vdu = 30 + (data1 & 7);
                if (code & 8)
                    vdu += 60;
                if (code & 0x80)
                    vdu += 10;
                printf ("\033[%im", vdu);
                }
            break;

        case 19: // SET CURPAL
        {
            int pal = code & 0x0F;
            int phy = data1 & 0xFF;
            int r = ( data1 >> 8 ) & 0xFF;
            int g = ( data1 >> 16 ) & 0xFF;
            int b = data1 >> 24;
            if ( phy < 16 ) curpal[pal] = defpal[phy];
            else if ( phy == 16 ) curpal[pal] = PICO_SCANVIDEO_PIXEL_FROM_RGB8(r, g, b);
            else if ( phy == 255 ) curpal[pal] = PICO_SCANVIDEO_PIXEL_FROM_RGB5(r, g, b);
            genrb ();
        }

        case 20: // RESET COLOURS
            clrreset ();
            fg = clrmsk (7);
            bg = 0;
            bgfill = 0;
            gfg = clrmsk (7);
            gbg = 0;
            if ( bPrint ) printf ("\033[37m\033[40m");
            break;

        case 21: // DISABLE VDU DRIVERS
            vflags |= VDUDIS;
            break;

        case 22: // MODE CHANGE
            modechg (code & 0x7F);
            col = 0;
            row = 0;
            break;

        case 23: // DEFINE CHARACTER ETC.
            /*
            defchr (data2 & 0xFF, (data2 >> 8) & 0xFF, (data2 >> 16) & 0xFF,
                (data2 >> 24) & 0xFF, data1 & 0xFF, (data1 >> 8) & 0xFF,
                (data1 >> 16) & 0xFF, (data1 >> 24) & 0xFF, code & 0xFF);
            */
            if ((data2 & 0xFF) == 22)
                {
                col = 0;
                row = 0;
                }
            break;

        case 24: // DEFINE GRAPHICS VIEWPORT - TODO
            break;

        case 25: // PLOT - TODO
            break;

        case 26: // RESET VIEWPORTS
            col = 0;
            row = 0;
            break;

        case 27: // SEND NEXT TO OUTC
            showchr (code & 0xFF);
            break;

        case 28: // SET TEXT VIEWPORT - TODO
            break;

        case 29: // SET GRAPHICS ORIGIN - TODO
            break;

        case 30: // CURSOR HOME
            if ( bPrint ) printf ("\033[H");
            col = 0;
            row = 0;
            scroln &= 0x80;
            break;

        case 31: // TAB(X,Y)
            col = data1 >> 24 & 0xFF;
            row = code & 0xFF;
            if ( bPrint ) printf ("\033[%i;%iH", row + 1, col + 1);
            break;

        case 127: // DEL
            xeqvdu (0x0800, 0, 0);
            xeqvdu (0x2000, 0, 0);
            xeqvdu (0x0800, 0, 0);
            break;

        default:
            if (vdu >= 0x20)
                {
                showchr (vdu);
                }
        }
    if ( bPrint ) fflush (stdout);
    }

int setup_vdu (void)
    {
#if DEBUG > 0
#if HIRES
    printf ("setup_vdu: 800x600 " __DATE__ " " __TIME__ "\n");
#else
    printf ("setup_vdu: 640x480 " __DATE__ " " __TIME__ "\n");
#endif
#if USE_INTERP
    printf ("setup_vdu: Using interpolator\n");
#else
    printf ("setup_vdu: Without interpolator\n");
#endif
    sleep_ms(500);
#endif
#if HIRES   // 800x600 VGA
    set_sys_clock_khz (7 * vga_timing_800x600_60_default.clock_freq / 1000, true);
#else       // 640x480 VGA
    set_sys_clock_khz (6 * vga_timing_640x480_60_default.clock_freq / 1000, true);
#endif
    stdio_init_all();
#if DEBUG > 0
    sleep_ms(500);
    printf ("setup_vdu: Set clock frequency %d kHz.\n", clock_get_hz (clk_sys) / 1000);
#endif
    memset (framebuf, 0, sizeof (framebuf));
    modechg (11);
    multicore_launch_core1 (setup_video);

    // Do not return until Core 1 has claimed DMA channels
    uint32_t test =- multicore_fifo_pop_blocking ();
    if ( test != VGA_FLAG )
        {
        printf ("Unexpected value 0x%04X from multicore FIFO\n", test);
        }
    }
