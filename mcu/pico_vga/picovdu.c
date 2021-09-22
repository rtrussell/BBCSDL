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

// Declared in bbmain.c:
void error (int, const char *);

#if HIRES
#define SWIDTH  800
#define SHEIGHT 600
#else
#define SWIDTH  640
#define SHEIGHT 480
#endif
#define VGA_FLAG    0x1234  // Used to syncronise cores
#define NPLT        3       // Length of plot history

static uint8_t  framebuf[SWIDTH * SHEIGHT / 8];
static uint16_t renderbuf[256 * 8];

static bool bBlank = true;              // Blank video screen
static int col = 0;                     // Cursor column position
static int row = 0;                     // Cursor row position
static int fg;                          // Text foreground colour
static int bg;                          // Text backgound colour
static uint8_t bgfill;                  // Pixel fill for background colour
static int gfg;                         // Graphics foreground colour & mode
static int gbg;                         // Graphics background colour & mode
static bool bPrint = false;             // Enable output to printer (UART)
static int csrtop;                      // Top of cursor
static int csrhgt;                      // Height of cursor
static bool bCsrVis = false;            // Cursor currently rendered
static uint8_t nCsrHide = 0;            // Cursor hide count (Bit 7 = Cursor off, Bit 6 = Outside screen)
static uint32_t nFlash = 0;             // Time counter for cursor flash
static critical_section_t cs_csr;       // Critical section controlling cursor flash
static int tvt;                         // Top of text viewport
static int tvb;                         // Bottom of text viewport
static int tvl;                         // Left edge of graphics viewport
static int tvr;                         // Right edge of graphics viewport
static int gxo;                         // X coordinate of graphics origin
static int gyo;                         // Y coordinate of graphics origin
static int gvt;                         // Top of graphics viewport
static int gvb;                         // Bottom of graphics viewport
static int gvl;                         // Left edge of graphics viewport
static int gvr;                         // Right edge of graphics viewport
typedef struct
    {
    int x;
    int y;
    } GPOINT;
static GPOINT pltpt[NPLT];              // History of plotted points (half pixels from top left)

#define CSR_OFF 0x80                    // Cursor turned off
#define CSR_INV 0x40                    // Cursor invalid (off-screen)
#define CSR_CNT 0x3F                    // Cursor hide count bits

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

static const uint32_t ttcsr = PICO_SCANVIDEO_PIXEL_FROM_RGB8(255u, 255u, 255u)
    | ( (PICO_SCANVIDEO_PIXEL_FROM_RGB8(255u, 255u, 255u)) << 16 );

#include "font_10.h"
#include "font_tt.h"
static const uint32_t cpx02[] = { 0x00000000, 0xFFFFFFFF };
static const uint32_t cpx04[] = { 0x00000000, 0x55555555, 0xAAAAAAAA, 0xFFFFFFFF };
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
static const uint32_t fwdmsk[] = {
    0xFFFFFFFF, 0xFFFFFFFE, 0xFFFFFFFC, 0xFFFFFFF8, 0xFFFFFFF0, 0xFFFFFFE0, 0xFFFFFFC0, 0xFFFFFF80,
    0xFFFFFF00, 0xFFFFFE00, 0xFFFFFC00, 0xFFFFF800, 0xFFFFF000, 0xFFFFE000, 0xFFFFC000, 0xFFFF8000,
    0xFFFF0000, 0xFFFE0000, 0xFFFC0000, 0xFFF80000, 0xFFF00000, 0xFFE00000, 0xFFC00000, 0xFF800000,
    0xFF000000, 0xFE000000, 0xFC000000, 0xF8000000, 0xF0000000, 0xE0000000, 0XC0000000, 0x80000000 };
static const uint32_t bkwmsk[] = {
    0x00000001, 0x00000003, 0x00000007, 0x0000000F, 0x0000001F, 0x0000003F, 0x0000007F, 0x000000FF,
    0x000001FF, 0x000003FF, 0x000007FF, 0x00000FFF, 0x00001FFF, 0x00003FFF, 0x00007FFF, 0x0000FFFF,
    0x0001FFFF, 0x0003FFFF, 0x0007FFFF, 0x000FFFFF, 0x001FFFFF, 0x003FFFFF, 0x007FFFFF, 0x00FFFFFF,
    0x01FFFFFF, 0x03FFFFFF, 0x07FFFFFF, 0x0FFFFFFF, 0x1FFFFFFF, 0x3FFFFFFF, 0x7FFFFFFF, 0xFFFFFFFF };

typedef struct
    {
    uint32_t        nclr;       // Number of colours
    const uint32_t *cpx;        // Colour patterns
    uint32_t        bitsh;      // Shift X coordinate to get bit position
    uint32_t        clrmsk;     // Colour mask
    uint32_t        csrmsk;     // Mask for cursor
    } CLRDEF;

static CLRDEF clrdef[] = {
//   nclr,   cpx, bsh, clrm,     csrmsk
    {   0,  NULL,   0, 0x00,       0x00},
    {   2, cpx02,   0, 0x01, 0x000000FF},
    {   4, cpx04,   1, 0x03, 0x0000FFFF},
    {   8,  NULL,   0, 0x07,       0x00},
    {  16, cpx16,   2, 0x0F, 0xFFFFFFFF}
    };
        

typedef struct
    {
    uint32_t    ncbt;       // Number of colour bits
    uint32_t    gcol;       // Number of displayed pixel columns
    uint32_t    grow;       // Number of displayed pixel rows
    uint32_t    tcol;       // Number of text columns
    uint32_t    trow;       // Number of text row
    uint32_t    vmgn;       // Number of black lines at top (and bottom)
    uint32_t    hmgn;       // Number of black pixels to left (and to right)
    uint32_t    nppb;       // Number of pixels per byte
    uint32_t    nbpl;       // Number of bytes per line
    uint32_t    yscl;       // Y scale (number of repeats of each line)
    uint32_t    thgt;       // Text height
    } MODE;

#if HIRES    // 800x600 VGA
static const MODE modes[] = {
// ncbt gcol grow tcol trw  vmg hmg pb nbpl ys thg
    { 1, 640, 256,  80, 32,  44, 80, 8,  80, 2,  8},  // Mode  0 - 20KB
    { 2, 320, 256,  40, 32,  44, 80, 8,  80, 2,  8},  // Mode  1 - 20KB
    { 4, 160, 256,  20, 32,  44, 80, 8,  80, 2,  8},  // Mode  2 - 20KB
    { 1, 640, 250,  80, 25,  47, 80, 8,  80, 2, 10},  // Mode  3 - 20KB
    { 1, 320, 256,  40, 32,  44, 80, 8,  80, 2,  8},  // Mode  4 - 10KB
    { 2, 160, 256,  20, 32,  44, 80, 8,  80, 2,  8},  // Mode  5 - 10KB
    { 1, 320, 250,  40, 25,  44, 80, 8,  80, 2, 10},  // Mode  6 - 10KB
    { 3, 320, 225,  40, 25,  75, 80, 8,  80, 2,TTH},  // Mode  7 - Teletext TODO
    { 1, 640, 512,  80, 32,  44, 80, 8,  80, 1, 16},  // Mode  8 - 40KB
    { 2, 320, 512,  40, 32,  44, 80, 8,  80, 1, 16},  // Mode  9 - 40KB
    { 4, 160, 512,  20, 32,  44, 80, 8,  80, 1, 16},  // Mode 10 - 40KB
    { 1, 640, 500,  80, 25,  47, 80, 8,  80, 1, 20},  // Mode 11 - 40KB
    { 1, 320, 512,  40, 32,  44, 80, 8,  80, 1, 16},  // Mode 12 - 20KB
    { 2, 160, 512,  20, 32,  44, 80, 8,  80, 1, 16},  // Mode 13 - 20KB
    { 1, 320, 500,  40, 25,  44, 80, 8,  80, 1, 20},  // Mode 14 - 20KB
    { 1, 640, 512,  40, 25,  44, 80, 8,  80, 1, 16},  // Mode 15 - Teletext ?
    { 1, 800, 600, 100, 30,   0,  0, 8, 100, 1, 20},  // Mode 16 - 59KB
    { 2, 400, 600,  50, 30,   0,  0, 8, 100, 1, 20},  // Mode 17 - 59KB
    { 4, 200, 600,  25, 30,   0,  0, 8, 100, 1, 20},  // Mode 18 - 59KB
    { 2, 800, 300, 100, 30,   0,  0, 4, 200, 2, 20},  // Mode 19 - 59KB
    { 4, 400, 300,  50, 30,   0,  0, 4, 200, 2, 10}   // Mode 20 - 59KB
    };
#else   // 640x480 VGA
static const MODE modes[] = {
// ncbt gcol grow tcol trw  vmg hmg pb nbpl ys thg
    { 1, 640, 256,  80, 32, 112,  0, 8,  80, 1,  8},  // Mode  0 - 20KB
    { 2, 320, 256,  40, 32, 112,  0, 8,  80, 1,  8},  // Mode  1 - 20KB
    { 4, 160, 256,  20, 32, 112,  0, 8,  80, 1,  8},  // Mode  2 - 20KB
    { 1, 640, 240,  80, 24,   0,  0, 8,  80, 2, 10},  // Mode  3 - 20KB
    { 1, 320, 256,  40, 32, 112,  0, 8,  80, 1,  8},  // Mode  4 - 10KB
    { 2, 160, 256,  20, 32, 112,  0, 8,  80, 1,  8},  // Mode  5 - 10KB
    { 1, 320, 240,  40, 24,   0,  0, 8,  80, 2, 10},  // Mode  6 - 10KB
    { 3, 320, 225,  40, 25,  15,  0, 4, 160, 2,TTH},  // Mode  7 - ~1KB - Teletext
    { 1, 640, 480,  80, 30,   0,  0, 8,  80, 1, 16},  // Mode  8 - 37.5KB
    { 2, 320, 480,  40, 30,   0,  0, 8,  80, 1, 16},  // Mode  9 - 37.5KB
    { 4, 160, 480,  20, 30,   0,  0, 8,  80, 1, 16},  // Mode 10 - 37.5KB
    { 1, 640, 480,  80, 24,   0,  0, 8,  80, 1, 20},  // Mode 11 - 37.5KB
    { 1, 320, 480,  40, 30,   0,  0, 8,  80, 1, 16},  // Mode 12 - 18.75KB
    { 2, 160, 480,  20, 30,   0,  0, 8,  80, 1, 16},  // Mode 13 - 18.75KB
    { 1, 320, 480,  40, 24,   0,  0, 8,  80, 1, 20},  // Mode 14 - 18.75KB
    { 4, 320, 240,  40, 24,   0,  0, 4, 160, 2, 10},  // Mode 15 - 37.5KB
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
#define FLASH_BIT   0x20    // Bit in frame count used to control flash

void __time_critical_func(render_mode7) (void)
    {
    uint32_t nFrame = 0;
    bool bDouble = false;
    bool bLower = false;
#if DEBUG > 0
    printf ("Entered mode 7 rendering\n");
#endif
    while (pmode == &modes[7])
        {
        if ( bCfgInt )
            {
            bCfgInt = false;
            bBlank = false;
#if DEBUG > 0
            printf ("Mode 7 display enabled\n");
#endif
            }
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
            if ( iScan == 0 )
                {
                ++nFrame;
                bDouble = false;
                }
            else if ( iScan % ( pmode->thgt * pmode->yscl ) == 0 )
                {
                bLower = bDouble;
                bDouble = false;
                }
            iScan /= pmode->yscl;
            int iRow = iScan / pmode->thgt;
            iScan -= iRow * pmode->thgt;
            uint8_t *pch = &framebuf[iRow * pmode->tcol];
            uint32_t *pxline = twopix;
            const uint8_t *pfont = font_tt[0x00] - 0x20 * TTH;
            int nfg = 7;
            uint32_t bgnd = ((uint32_t *)curpal)[0];
            uint32_t fgnd = ((uint32_t *)curpal)[nfg];
            bool bFlash = false;
            bool bGraph = false;
            bool bCont = true;
            bool bHold = false;
            int iScan2 = iScan;
            for (int iCol = 0; iCol < pmode->tcol; ++iCol)
                {
                uint8_t ch = *pch;
                if ( ch >= 0x20 )
                    {
                    uint8_t px = pfont[TTH * ch + iScan2];
                    ++twopix;
                    if ( px & 0x01 ) *twopix = fgnd;
                    else             *twopix = bgnd;
                    ++twopix;
                    if ( px & 0x02 ) *twopix = fgnd;
                    else             *twopix = bgnd;
                    ++twopix;
                    if ( px & 0x04 ) *twopix = fgnd;
                    else             *twopix = bgnd;
                    ++twopix;
                    if ( px & 0x08 ) *twopix = fgnd;
                    else             *twopix = bgnd;
                    ++twopix;
                    if ( px & 0x10 ) *twopix = fgnd;
                    else             *twopix = bgnd;
                    ++twopix;
                    if ( px & 0x20 ) *twopix = fgnd;
                    else             *twopix = bgnd;
                    ++twopix;
                    if ( px & 0x40 ) *twopix = fgnd;
                    else             *twopix = bgnd;
                    ++twopix;
                    if ( px & 0x80 ) *twopix = fgnd;
                    else             *twopix = bgnd;
                    }
                else
                    {
                    if ( bHold )
                        {
                        twopix[0] = twopix[-8];
                        twopix[1] = twopix[-7];
                        twopix[2] = twopix[-6];
                        twopix[3] = twopix[-5];
                        twopix[4] = twopix[-4];
                        twopix[5] = twopix[-3];
                        twopix[6] = twopix[-2];
                        twopix[7] = twopix[-1];
                        }
                    else
                        {
                        twopix[0] = bgnd;
                        twopix[1] = bgnd;
                        twopix[2] = bgnd;
                        twopix[3] = bgnd;
                        twopix[4] = bgnd;
                        twopix[5] = bgnd;
                        twopix[6] = bgnd;
                        twopix[7] = bgnd;
                        }
                    twopix += 8;
                    if ((ch >= 0x00) && (ch <= 0x07))
                        {
                        pfont = font_tt[0x00] - 0x20 * TTH;
                        bGraph = false;
                        nfg = ch & 0x07;
                        if (( bFlash ) && ( nFrame & FLASH_BIT ))
                            fgnd = ((uint32_t *)curpal)[nfg ^ 0x07];
                        else
                            fgnd = ((uint32_t *)curpal)[nfg];
                        }
                    else if (ch == 0x08)
                        {
                        bFlash = true;
                        if ( nFrame & FLASH_BIT )
                            fgnd = ((uint32_t *)curpal)[nfg ^ 0x07];
                        else
                            fgnd = ((uint32_t *)curpal)[nfg];
                        }
                    else if (ch == 0x09)
                        {
                        bFlash = false;
                        fgnd = ((uint32_t *)curpal)[nfg];
                        }
                    else if (ch == 0x0C)
                        {
                        iScan2 = iScan;
                        }
                    else if (ch == 0x0D)
                        {
                        if ( bLower )   iScan2 = ( iScan + 9 ) / 2;
                        else            iScan2 = iScan / 2;
                        bDouble = true;
                        }
                    else if ((ch >= 0x10) && (ch <= 0x17))
                        {
                        if ( bCont )    pfont = font_tt[0x60] - 0x20 * TTH;
                        else            pfont = font_tt[0xC0] - 0x20 * TTH;
                        bGraph = true;
                        nfg = ch & 0x07;
                        if (( bFlash ) && ( nFrame & FLASH_BIT ))
                            fgnd = ((uint32_t *)curpal)[nfg ^ 0x07];
                        else
                            fgnd = ((uint32_t *)curpal)[nfg];
                        }
                    else if (ch == 0x19)
                        {
                        bCont = true;
                        if ( bGraph ) pfont = font_tt[0x60] - 0x20 * TTH;
                        }
                    else if (ch == 0x1A)
                        {
                        bCont = false;
                        if ( bGraph ) pfont = font_tt[0xC0] - 0x20 * TTH;
                        }
                    else if (ch == 0x1C)
                        {
                        bgnd = ((uint32_t *)curpal)[0];
                        }
                    else if (ch == 0x1D)
                        {
                        bgnd = ((uint32_t *)curpal)[nfg];
                        }
                    else if (ch == 0x1E)
                        {
                        bHold = true;
                        }
                    else if (ch == 0x1F)
                        {
                        bHold = false;
                        }
                    }
                ++pch;
                }
            ++twopix;
            *twopix = COMPOSABLE_EOL_ALIGN << 16;   // Implicit zero (black) in low word
            ++twopix;
            buffer->data_used = twopix - buffer->data;
            if (( iRow == row ) && ( nCsrHide == 0 ) && ( nFrame & FLASH_BIT )
                && ( iScan >= csrtop ) && ( iScan < csrtop + csrhgt ))
                {
                twopix = pxline + 8 * col + 1;
                twopix[0] ^= ttcsr;
                twopix[1] ^= ttcsr;
                twopix[2] ^= ttcsr;
                twopix[3] ^= ttcsr;
                twopix[4] ^= ttcsr;
                twopix[5] ^= ttcsr;
                twopix[6] ^= ttcsr;
                twopix[7] ^= ttcsr;
                }
            pxline[0] = ( pxline[1] << 16 ) | COMPOSABLE_RAW_RUN;
            pxline[1] = ( pxline[1] & 0xFFFF0000 ) | ( pmode->nbpl * pmode->nppb - 2 );
            }
        scanvideo_end_scanline_generation (buffer);
        }
#if DEBUG > 0
    printf ("Quit mode 7 rendering\n");
#endif
    }

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
        if ( pmode == &modes[7] ) render_mode7 ();
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
            ++twopix;
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

static void flipcsr (void)
    {
    int xp;
    int yp;
    if ( vflags & HRGFLG )
        {
        xp = pltpt[0].x / 2;
        yp = pltpt[0].y / 2;
        if (( xp < gvl ) || ( xp + 7 > gvr ) || ( yp < gvt ) || ( yp + pmode->thgt - 1 > gvb ))
            {
            nCsrHide |= CSR_INV;
            bCsrVis = false;
            return;
            }
        }
    else
        {
        if (( row < 0 ) || ( row >= pmode->trow ) || ( col < 0 ) || ( col >= pmode->tcol ))
            {
            nCsrHide |= CSR_INV;
            bCsrVis = false;
            return;
            }
        xp = 8 * col;
        yp = row * pmode->thgt;
        }
    yp += csrtop;
    CLRDEF *cdef = &clrdef[pmode->ncbt];
    uint32_t *fb = (uint32_t *)(framebuf + yp * pmode->nbpl);
    xp <<= cdef->bitsh;
    fb += xp >> 5;
    xp &= 0x1F;
    uint32_t msk1 = cdef->csrmsk << xp;
    uint32_t msk2 = cdef->csrmsk >> ( 32 - xp );
    for (int i = 0; i < csrhgt; ++i)
        {
        *fb ^= msk1;
        *(fb + 1) ^= msk2;
        fb += pmode->nbpl / 4;
        ++yp;
        }
#if 0
        }
    else
        {
        if (( row < 0 ) || ( row >= pmode->trow ) || ( col < 0 ) || ( col >= pmode->tcol ))
            {
            nCsrHide |= CSR_INV;
            bCsrVis = false;
            return;
            }
        uint8_t *pfb = framebuf + (row * pmode->thgt + csrtop) * pmode->nbpl;
        if ( pmode->ncbt == 1 )
            {
            pfb += col;
            for (int i = 0; i < csrhgt; ++i)
                {
                *pfb ^= 0xFF;
                pfb += pmode->nbpl;
                }
            }
        else if ( pmode->ncbt == 2 )
            {
            pfb += 2 * col;
            for (int i = 0; i < csrhgt; ++i)
                {
                *((uint16_t *)pfb) ^= 0xFFFF;
                pfb += pmode->nbpl;
                }
            }
        else if ( pmode->ncbt == 4 )
            {
            pfb += 4 * col;
            for (int i = 0; i < csrhgt; ++i)
                {
                *((uint32_t *)pfb) ^= 0xFFFFFFFF;
                pfb += pmode->nbpl;
                }
            }
        }
#endif
    bCsrVis = ! bCsrVis;
    }

static void hidecsr (void)
    {
    ++nCsrHide;
    if ( pmode != &modes[7] )
        {
        critical_section_enter_blocking (&cs_csr);
        if ( bCsrVis ) flipcsr ();
        critical_section_exit (&cs_csr);
        }
    }

static void showcsr (void)
    {
    if ( ( nCsrHide & CSR_CNT ) > 0 ) --nCsrHide;
    if ( vflags & HRGFLG )
        {
        int xp = pltpt[0].x / 2;
        int yp = pltpt[0].y / 2;
        if (( xp >= gvl ) && ( xp + 7 <= gvr ) && ( yp >= gvt ) && ( yp + pmode->thgt - 1 <= gvb ))
            nCsrHide &= ~CSR_INV;
        else
            nCsrHide |= CSR_INV;
        }
    else
        {
        if ( ( row >= tvt ) && ( row <= tvb ) && ( col >= tvl ) && ( col <= tvr ))
            nCsrHide &= ~CSR_INV;
        else
            nCsrHide |= CSR_INV;
        }
    if (( pmode != &modes[7] ) && ( nCsrHide == 0 ))
        {
        critical_section_enter_blocking (&cs_csr);
        if ( ! bCsrVis ) flipcsr ();
        critical_section_exit (&cs_csr);
        }
    }

static void flashcsr (void)
    {
    if (( pmode != &modes[7] ) && ( nCsrHide == 0 ))
        {
        critical_section_enter_blocking (&cs_csr);
        flipcsr ();
        critical_section_exit (&cs_csr);
        }
    }

static void csrdef (int data2)
    {
    uint32_t p1 = data2 & 0xFF;
    uint32_t p2 = ( data2 >> 8 ) & 0xFF;
    uint32_t p3 = ( data2 >> 16 ) & 0xFF;
    if ( p1 == 1 )
        {
        if ( p2 == 0 ) nCsrHide |= CSR_OFF;
        else nCsrHide &= ~CSR_OFF;
        }
    else if ( p1 == 0 )
        {
        if ( p2 == 10 )
            {
            if ( p3 < pmode->thgt ) csrtop = p3;
            }
        else if ( p2 == 11 )
            {
            if ( p3 < pmode->thgt ) csrhgt = p3 - csrtop + 1;
            }
        }
    }

static void pushpts (void)
    {
    memmove (&pltpt[1], &pltpt[0], (NPLT - 1) * sizeof (pltpt[0]));
    }

static void home (void)
    {
    hidecsr ();
    if ( vflags & HRGFLG )
        {
        pushpts ();
        pltpt[0].x = 2 * gvl;
        pltpt[0].y = 2 * gvt;
        if ( scroln & 0x80 ) scroln = 0x80 + ( gvb - gvt + 1 ) / pmode->thgt;
        }
    else
        {
        row = tvt;
        col = tvl;
        if ( scroln & 0x80 ) scroln = 0x80 + tvb - tvt + 1;
        }
    showcsr ();
    }

static void scrldn (void)
    {
    hidecsr ();
    if ( pmode == &modes[7] )
        {
        if (( tvl == 0 ) && ( tvr == pmode->tcol - 1 ))
            {
            memmove (framebuf + ( tvt + 1 ) * pmode->tcol,
                framebuf + tvt * pmode->tcol,
                ( tvb - tvt ) * pmode->tcol);
            memset (framebuf + tvt * pmode->tcol, ' ', pmode->tcol);
            }
        else
            {
            uint8_t *fb1 = framebuf + tvb * pmode->tcol + tvl;
            uint8_t *fb2 = fb1 + pmode->tcol;
            int nr = tvb - tvt;
            int nb = tvr - tvl + 1;
            for (int ir = 0; ir < nr; ++ir)
                {
                fb1 -= pmode->tcol;
                fb2 -= pmode->tcol;
                memcpy (fb2, fb1, nb);
                }
            memset (fb1, ' ', nb);
            }
        }
    else if (( tvl == 0 ) && ( tvr == pmode->tcol - 1 ))
        {
        memmove (framebuf + ( tvt + 1 ) * pmode->thgt * pmode->nbpl,
            framebuf + tvt * pmode->thgt * pmode->nbpl,
            ( tvb - tvt ) * pmode->thgt * pmode->nbpl);
        memset (framebuf + tvt * pmode->thgt * pmode->nbpl, bgfill, pmode->thgt * pmode->nbpl);
        }
    else
        {
        uint8_t *fb1 = framebuf + tvb * pmode->thgt * pmode->nbpl
            + ( tvl << clrdef[pmode->ncbt].bitsh );
        uint8_t *fb2 = fb1 + pmode->thgt * pmode->nbpl;
        int nr = ( tvb - tvt ) * pmode->thgt;
        int nb = ( tvr - tvl + 1 ) << clrdef[pmode->ncbt].bitsh;
        for (int ir = 0; ir < nr; ++ir)
            {
            fb1 -= pmode->nbpl;
            fb2 -= pmode->nbpl;
            memcpy (fb2, fb1, nb);
            }
        for (int ir = 0; ir < pmode->thgt; ++ir)
            {
            fb2 -= pmode->nbpl;
            memset (fb2, bgfill, nb);
            }
        }
    showcsr ();
    }

static void scrlup (void)
    {
    hidecsr ();
    if ( pmode == &modes[7] )
        {
        if (( tvl == 0 ) && ( tvr == pmode->tcol - 1 ))
            {
            memmove (framebuf, framebuf + pmode->tcol, ( pmode->trow - 1 ) * pmode->tcol);
            memset (framebuf + ( pmode->trow - 1 ) * pmode->tcol, ' ', pmode->tcol);
            }
        else
            {
            uint8_t *fb1 = framebuf + tvt * pmode->tcol + tvl;
            uint8_t *fb2 = fb1 + pmode->tcol;
            int nr = tvb - tvt;
            int nb = tvr - tvl + 1;
            for (int ir = 0; ir < nr; ++ir)
                {
                memcpy (fb1, fb2, nb);
                fb1 += pmode->tcol;
                fb2 += pmode->tcol;
                }
            memset (fb2, ' ', nb);
            }
        }
    else if (( tvl == 0 ) && ( tvr == pmode->tcol - 1 ))
        {
        memmove (framebuf + tvt * pmode->thgt * pmode->nbpl,
            framebuf + ( tvt + 1 ) * pmode->thgt * pmode->nbpl,
            ( tvb - tvt ) * pmode->thgt * pmode->nbpl);
        memset (framebuf + tvb * pmode->thgt * pmode->nbpl, bgfill, pmode->thgt * pmode->nbpl);
        }
    else
        {
        uint8_t *fb1 = framebuf + tvt * pmode->thgt * pmode->nbpl
            + ( tvl << clrdef[pmode->ncbt].bitsh );
        uint8_t *fb2 = fb1 + pmode->thgt * pmode->nbpl;
        int nr = ( tvb - tvt ) * pmode->thgt;
        int nb = ( tvr - tvl + 1 ) << clrdef[pmode->ncbt].bitsh;
        for (int ir = 0; ir < nr; ++ir)
            {
            memcpy (fb1, fb2, nb);
            fb1 += pmode->nbpl;
            fb2 += pmode->nbpl;
            }
        for (int ir = 0; ir < pmode->thgt; ++ir)
            {
            memset (fb1, bgfill, nb);
            fb1 += pmode->nbpl;
            }
        }
    showcsr ();
    }

static void cls ()
    {
#if DEBUG > 0
    printf ("cls: bgfill = 0x%02\n", bgfill);
#endif
    hidecsr ();
    if ( pmode == &modes[7] )
        {
        if (( tvl == 0 ) && ( tvr == pmode->tcol - 1 ))
            {
            memset (framebuf + tvt * pmode->tcol, ' ', ( tvb - tvt + 1 ) * pmode->tcol);
            }
        else
            {
            uint8_t *fb1 = framebuf + tvt * pmode->tcol + tvl;
            int nr = tvb - tvt + 1;
            int nb = tvr - tvl + 1;
            for (int ir = 0; ir < nr; ++ir)
                {
                memset (fb1, ' ', nb);
                fb1 += pmode->tcol;
                }
            }
        }
    else if (( tvl == 0 ) && ( tvr == pmode->tcol - 1 ))
        {
        memset (framebuf + tvt * pmode->thgt * pmode->nbpl, bgfill,
            ( tvb - tvt + 1 ) * pmode->thgt * pmode->nbpl);
        }
    else
        {
        uint8_t *fb1 = framebuf + tvt * pmode->thgt * pmode->nbpl
            + ( tvl << clrdef[pmode->ncbt].bitsh );
        int nr = ( tvb - tvt + 1 ) * pmode->thgt;
        int nb = ( tvr - tvl + 1 ) << clrdef[pmode->ncbt].bitsh;
        for (int ir = 0; ir < nr; ++ir)
            {
            memset (fb1, bgfill, nb);
            fb1 += pmode->nbpl;
            }
        }
    home ();
    showcsr ();
    }

static void dispchr (int chr)
    {
    if (( row < 0 ) || ( row >= pmode->trow ) || ( col < 0 ) || ( col >= pmode->tcol ))
        {
#if DEBUG > 0
        printf ("Cursor out of range: row = %d, col = %d, screen = %dx%d\n",
            row, col, pmode->trow, pmode->tcol);
#endif
        return;
        }
#if DEBUG > 1
    if ((chr >= 0x20) && (chr <= 0x7F)) printf ("dispchr ('%c')\n", chr);
    else printf ("dispchr (0x%02X)\n", chr);
#endif
    if ( pmode == &modes[7] )
        {
        framebuf[row * pmode->tcol + col] = chr & 0x7F;
        }
    else
        {
        hidecsr ();
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
        if ( pmode->ncbt == 1 )
            {
            pfb += col;
            uint8_t fpx = (uint8_t) cpx02[fg];
            uint8_t bpx = (uint8_t) cpx02[bg];
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
                uint8_t pix = ( (*pch) & fpx ) | ( (~ (*pch)) & bpx );
                *pfb = pix;
                pfb += pmode->nbpl;
                if ( bDbl )
                    {
                    *pfb = pix;
                    pfb += pmode->nbpl;
                    }
                ++pch;
                }
            }
        else if ( pmode->ncbt == 2 )
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
        else if ( pmode->ncbt == 4 )
            {
            pfb += 4 * col;
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
        showcsr ();
        }
    }

static void newline (int *px, int *py)
    {
    hidecsr ();
    if (++(*py) > tvb)
        {
        if ((scroln & 0x80) && (--scroln == 0x7F))
            {
            unsigned char ch;
            scroln = 0x80 + tvb - tvt + 1;
            do
                {
                sleep_us (5000);
                } 
            while ((getkey (&ch) == 0) && ((flags & (ESCFLG | KILL)) == 0));
            }
        scrlup ();
        *py = tvb;
        }
	if ( bPrint )
        {
        printf ("\n");
        if (*px) printf ("\033[%i;%iH", *py + 1, *px + 1);
        }
    showcsr ();
    }

static void wrap (void)
    {
    if ( bPrint ) printf ("\r");
    hidecsr ();
    col = tvl;
    newline (&col, &row);
    showcsr ();
    }

static void tabxy (int x, int y)
    {
#if DEBUG > 0
    printf ("tab: row = %d, col = %d\n", y, x);
#endif
    x += tvl;
    y += tvt;
    if (( x >= tvl ) && ( x <= tvr ) && ( y >= tvt ) && ( y <= tvb ))
        {
        hidecsr ();
        row = y;
        col = x;
        showcsr ();
        }
    }

static void twind (int vl, int vb, int vr, int vt)
    {
    if (( vl < 0 ) || ( vl > vr ) || ( vr >= pmode->tcol )
        || ( vt < 0 ) || ( vt > vb ) || ( vb >= pmode->trow )) return;
    tvl = vl;
    tvr = vr;
    tvt = vt;
    tvb = vb;
    if (( col < tvl ) || ( col > tvr ) || ( row < tvt ) || ( row > tvb ))
        home ();
    }

static void genrb (void)
    {
#if DEBUG > 0
    printf ("genrb\n");
#endif
    uint16_t *prb = (uint16_t *)renderbuf;
    if ( pmode->ncbt == 1 )
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
    else if ( pmode->ncbt == 2 )
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
    else if ( pmode->ncbt == 4 )
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

static void clrreset (void)
    {
    bg = 0;
    bgfill = 0;
    gbg = 0;
    if ( pmode->ncbt == 1 )
        {
#if DEBUG > 0
        printf ("clrreset: nclr = 2\n");
#endif
        curpal[0] = defpal[0];
        curpal[1] = defpal[15];
        fg = 1;
        gfg = 1;
        }
    else if ( pmode->ncbt == 2 )
        {
#if DEBUG > 0
        printf ("clrreset: nclr = 4\n");
#endif
        curpal[0] = defpal[0];
        curpal[1] = defpal[9];
        curpal[2] = defpal[11];
        curpal[3] = defpal[15];
        fg = 3;
        gfg = 3;
        }
    else if ( pmode->ncbt == 3 )
        {
#if DEBUG > 0
        printf ("clrreset: nclr = 4\n");
#endif
        curpal[0] = defpal[0];
        curpal[1] = defpal[0];
        for (int i = 1; i < 8; ++i)
            {
            curpal[2*i] = defpal[i+8];
            curpal[2*i+1] = defpal[i+8];
            }
        }
    else
        {
#if DEBUG > 0
        printf ("clrreset: nclr = 16\n");
#endif
        memcpy (curpal, defpal, sizeof (defpal));
        fg = 15;
        gfg = 15;
        }
    genrb ();
    }

static void rstview (void)
    {
    hidecsr ();
    tvt = 0;
    tvb = pmode->trow - 1;
    tvl = 0;
    tvr = pmode->tcol - 1;
    col = 0;
    row = 0;
    gxo = 0;
    gyo = 0;
    gvt = 0;
    gvb = pmode->grow - 1;
    gvl = 0;
    gvr = pmode->gcol - 1;
    pushpts ();
    pltpt[0].x = 0;
    pltpt[0].y = 2 * pmode->grow - 1;
    showcsr ();
    }

static void modechg (int mode)
    {
#if DEBUG > 0
    printf ("modechg (%d)\n", mode);
#endif
    if (( mode >= 0 ) && ( mode < sizeof (modes) / sizeof (modes[0]) ))
        {
        bBlank = true;
        pmode = &modes[mode];
        nCsrHide |= CSR_OFF;
        clrreset ();
        rstview ();
        cls ();
        csrtop = pmode->thgt - 1;
        csrhgt = 1;
        nCsrHide = 0;
        showcsr ();
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
    return ( clr & clrdef[pmode->ncbt].clrmsk );
    }

static inline void pixop (int op, uint32_t *fb, uint32_t msk, uint32_t cpx)
    {
    cpx &= msk;
    switch (op)
        {
        case 1:
            *fb |= cpx;
            break;
        case 2:
            *fb &= cpx | ~msk;
            break;
        case 3:
            *fb ^= cpx;
            break;
        case 4:
            *fb ^= msk;
            break;
        default:
            *fb &= ~msk;
            *fb |= cpx;
            break;
        }
    }

static void hline (int clrop, int xp1, int xp2, int yp)
    {
    int op = clrop >> 8;
    CLRDEF *cdef = &clrdef[pmode->ncbt];
    if (( yp < gvt ) || ( yp > gvb )) return;
    if ( xp1 > xp2 )
        {
        int tmp = xp1;
        xp1 = xp2;
        xp2 = tmp;
        }
    if (( xp2 < gvl ) || ( xp1 > gvr )) return;
    if ( xp1 < gvl ) xp1 = gvl;
    if ( xp2 > gvr ) xp2 = gvr;
    xp1 <<= cdef->bitsh;
    xp2 <<= cdef->bitsh;
    uint32_t *fb1 = (uint32_t *)(framebuf + yp * pmode->nbpl);
    uint32_t *fb2 = fb1 + ( xp2 >> 5 );
    fb1 += ( xp1 >> 5 );
    uint32_t msk1 = fwdmsk[xp1 & 0x1F];
    uint32_t msk2 = bkwmsk[xp2 & 0x1F];
    uint32_t cpx = cdef->cpx[clrop & cdef->clrmsk];
    if ( fb2 == fb1 )
        {
        pixop (op, fb1, msk1 & msk2, cpx);
        }
    else
        {
        pixop (op, fb1, msk1, cpx);
        ++fb1;
        while ( fb1 < fb2 )
            {
            pixop (op, fb1, 0xFFFFFFFF, cpx);
            ++fb1;
            }
        pixop (op, fb1, msk2, cpx);
        }
    }

static void point (int clrop, uint32_t xp, uint32_t yp)
    {
    CLRDEF *cdef = &clrdef[pmode->ncbt];
    uint32_t *fb = (uint32_t *)(framebuf + yp * pmode->nbpl);
    xp <<= cdef->bitsh;
    fb += xp >> 5;
    xp &= 0x1F;
    uint32_t msk = cdef->clrmsk << xp;
    uint32_t cpx = ( clrop & cdef->clrmsk ) << xp;
    pixop (clrop >> 8, fb, msk, cpx);
    }

static void clippoint  (int clrop, int xp, int yp)
    {
    if (( xp >= gvl ) && ( xp <= gvr ) && ( yp >= gvt ) && ( yp <= gvb )) point (clrop, xp, yp);
    }

#define SKIP_NONE   0
#define SKIP_FIRST  -1
#define SKIP_LAST   1

static void line (int clrop, uint32_t xp1, uint32_t yp1, uint32_t xp2, uint32_t yp2, uint32_t dots, int skip)
    {
#if DEBUG > 0
    printf ("line (0x%02X, %d, %d, %d, %d, 0x%08X, %d)\n", clrop, xp1, yp1, xp2, yp2, dots, skip);
#endif
    int xd = xp2 - xp1;
    int yd = yp2 - yp1;
    bool bVert = false;
    bool bSwap = false;
    if ( xd >= 0 )
        {
        if ( yd > xd )
            {
            bVert = true;
            }
        else if ( yd < -xd )
            {
            bVert = true;
            bSwap = true;
            }
        }
    else
        {
        bSwap = true;
        if ( yd > -xd )
            {
            bVert = true;
            bSwap = false;
            }
        else if ( yd < xd )
            {
            bVert = true;
            }
        }
    if ( bSwap )
        {
        uint32_t tmp = xp1;
        xp1 = xp2;
        xp2 = tmp;
        tmp = yp1;
        yp1 = yp2;
        yp2 = tmp;
        xd  = -xd;
        yd  = -yd;
        skip = -skip;
        }
    if ( bVert )
        {
#if DEBUG > 0
    printf ("vertical: %d, %d, %d, %d\n", xp1, yp1, xp2, yp2);
#endif
        int xs = 1;
        if ( xd < 0 )
            {
            xs = -1;
            xd = -xd;
            }
        int xa = 0;
        if ( skip == SKIP_FIRST )
            {
            if ( dots & 1 )
                {
                dots >>= 1;
                dots |= 0x80000000;
                }
            else
                {
                dots >>= 1;
                }
            xa += xd;
            if ( xa >= yd )
                {
                xp1 +=  xs;
                xa -= yd;
                }
            ++yp1;
            }
        else if ( skip == SKIP_LAST )
            {
            --yp2;
            }
        while ( yp1 <= yp2 )
            {
            if ( dots & 1 )
                {
                point (clrop, xp1, yp1);
                dots >>= 1;
                dots |= 0x80000000;
                }
            else
                {
                dots >>= 1;
                }
            xa += xd;
            if ( xa >= yd )
                {
                xp1 +=  xs;
                xa -= yd;
                }
            ++yp1;
            }
        }
    else
        {
#if DEBUG > 0
    printf ("horizontal: %d, %d, %d, %d\n", xp1, yp1, xp2, yp2);
#endif
        int ys = 1;
        if ( yd < 0 )
            {
            ys = -1;
            yd = -yd;
            }
        int ya = 0;
        if ( skip == SKIP_FIRST )
            {
            if ( dots & 1 )
                {
                dots >>= 1;
                dots |= 0x80000000;
                }
            else
                {
                dots >>= 1;
                }
            ya += yd;
            if ( ya >= yd )
                {
                yp1 += ys;
                ya -= xd;
                }
            ++xp1;
            }
        else if ( skip == SKIP_LAST )
            {
            --xp2;
            }
        while ( xp1 <= xp2 )
            {
            if ( dots & 1 )
                {
                point (clrop, xp1, yp1);
                dots >>= 1;
                dots |= 0x80000000;
                }
            else
                {
                dots >>= 1;
                }
            ya += yd;
            if ( ya >= yd )
                {
                yp1 += ys;
                ya -= xd;
                }
            ++xp1;
            }
        }
    }

static void clipline (int clrop, int xp1, int yp1, int xp2, int yp2, uint32_t dots, int skip)
    {
#if DEBUG > 0
    printf ("clipline (0x%02X, %d, %d, %d, %d, 0x%08X, %d)\n", clrop, xp1, yp1, xp2, yp2, dots, skip);
#endif
    if ( xp2 < xp1 )
        {
        int tmp = xp1;
        xp1 = xp2;
        xp2 = tmp;
        tmp = yp1;
        yp1 = yp2;
        yp2 = tmp;
        skip = -skip;
        }
    if (( xp2 < gvl ) || ( xp1 > gvr )) return;
    if ( xp1 < gvl )
        {
        yp1 += ( yp2 - yp1 ) * ( gvl - xp1 ) / ( xp2 - xp1 );
        xp1 = gvl;
        if ( skip == SKIP_FIRST ) skip = SKIP_NONE;
        }
    if ( xp2 > gvr )
        {
        yp2 -= ( yp2 - yp1 ) * ( xp2 - gvr ) / ( xp2 - xp1 );
        xp2 = gvr;
        if ( skip == SKIP_LAST ) skip = SKIP_NONE;
        }
    if ( yp2 < yp1 )
        {
        int tmp = xp1;
        xp1 = xp2;
        xp2 = tmp;
        tmp = yp1;
        yp1 = yp2;
        yp2 = tmp;
        skip = -skip;
        }
    if (( yp2 < gvt ) || ( yp1 > gvb )) return;
    if ( yp1 < gvt )
        {
        xp1 += ( xp2 - xp1 ) * ( gvt - yp1 ) / ( yp2 - yp1 );
        yp1 = gvt;
        if ( skip == SKIP_FIRST ) skip = SKIP_NONE;
        }
    if ( yp2 > gvb )
        {
        xp2 -= ( xp2 - xp1 ) * ( yp2 - gvb ) / ( yp2 - yp1 );
        yp2 = gvb;
        if ( skip == SKIP_LAST ) skip = SKIP_NONE;
        }
    line (clrop, xp1, yp1, xp2, yp2, dots, skip);
    }

static void clrgraph (void)
    {
    int clr = clrmsk (gbg);
    for (int yp = gvt; yp <= gvb; ++yp)
        {
        hline (clr, gvl, gvr, yp);
        }
    }

static void triangle (int clrop, int xp1, int yp1, int xp2, int yp2, int xp3, int yp3)
    {
#if DEBUG > 0
    printf ("triangle (0x%02X, (%d, %d), (%d, %d), (%d, %d))\n", clrop, xp1, yp1, xp2, yp2, xp3, yp3);
#endif
    if (( xp1 < gvl ) && ( xp2 < gvl ) && ( xp3 < gvl )) return;
    if (( xp1 > gvr ) && ( xp2 > gvr ) && ( xp3 > gvr )) return;
    if (( yp1 < gvt ) && ( yp2 < gvt ) && ( yp3 < gvt )) return;
    if (( yp1 > gvb ) && ( yp2 > gvb ) && ( yp3 > gvb )) return;
    if ( yp2 < yp1 )
        {
        int tmp = xp1;
        xp1 = xp2;
        xp2 = tmp;
        tmp = yp1;
        yp1 = yp2;
        yp2 = tmp;
        }
    if ( yp3 < yp2 )
        {
        int tmp = xp2;
        xp2 = xp3;
        xp3 = tmp;
        tmp = yp2;
        yp2 = yp3;
        yp3 = tmp;
        }
    if ( yp2 < yp1 )
        {
        int tmp = xp1;
        xp1 = xp2;
        xp2 = tmp;
        tmp = yp1;
        yp1 = yp2;
        yp2 = tmp;
        }
    int aa = 0;
    int xa = xp1;
    int na = xp2 - xp1;
    int da = yp2 - yp1;
    int ab = 0;
    int xb = xp1;
    int nb = xp3 - xp1;
    int db = yp3 - yp1;
    while ( yp1 < yp2 )
        {
        hline (clrop, xa, xb, yp1);
        aa += na;
        xa += aa / da;
        aa %= da;
        ab += nb;
        xb += ab / db;
        ab %= db;
        ++yp1;
        }
    aa = 0;
    xa = xp2;
    na = xp3 - xp2;
    da = yp3 - yp2;
    while ( yp1 < yp3 )
        {
        hline (clrop, xa, xb, yp1);
        aa += na;
        xa += aa / da;
        aa %= da;
        ab += nb;
        xb += ab / db;
        ab %= db;
        ++yp1;
        }
    hline (clrop, xa, xb, yp1);
    }

static void rectangle (int clrop, int xp1, int yp1, int xp2, int yp2)
    {
#if DEBUG > 0
    printf ("rectangle (0x%02X, (%d, %d), (%d, %d))\n", clrop, xp1, yp1, xp2, yp2);
#endif
    if ( xp2 < xp1 )
        {
        int tmp = xp1;
        xp1 = xp2;
        xp2 = tmp;
        }
    if ( yp2 < yp1 )
        {
        int tmp = yp1;
        yp1 = yp2;
        yp2 = tmp;
        }
    if (( xp2 < gvl ) || ( xp1 > gvr ) || ( yp2 < gvt ) || ( yp1 > gvb )) return;
    while ( yp1 <= yp2 )
        {
        hline (clrop, xp1, xp2, yp1);
        ++yp1;
        }
    }

static void trapizoid (int clrop, int xp1, int yp1, int xp2, int yp2, int xp3, int yp3)
    {
#if DEBUG > 0
    printf ("trapizoid (0x%02X, (%d, %d), (%d, %d), (%d, %d))\n", clrop, xp1, yp1, xp2, yp2, xp3, yp3);
#endif
    GPOINT pts[4] = {{xp1, yp1}, {xp2, yp2}, {xp3, yp3}, {xp1 - xp2 + xp3, yp1 - yp2 + yp3}};
    int xmin = xp1;
    int xmax = xp1;
    int ymin = yp1;
    int ymax = yp1;
    int itop = 0;
    for (int i = 1; i < 4; ++i )
        {
        if ( pts[i].x < xmin ) xmin = pts[i].x;
        if ( pts[i].x > xmax ) xmax = pts[i].x;
        if ( pts[i].y < ymin )
            {
            ymin = pts[i].y;
            itop = i;
            }
        if ( pts[i].y > ymax ) ymax = pts[i].y;
        }
    if (( xmax < gvl ) || ( xmin > gvr ) || ( ymax < gvt ) || ( ymin > gvb )) return;
    GPOINT *pp[4];
    for (int i = 0; i < 4; ++i)
        {
        pp[i] = &pts[itop];
        if ( ++itop > 3 ) itop = 0;
        }
    if ( pp[1]->y > pp[3]->y )
        {
        GPOINT *pt = pp[1];
        pp[1] = pp[3];
        pp[3] = pt;
        }
    int yy = pp[0]->y;
    int aa = 0;
    int xa = pp[0]->x;
    int na = pp[1]->x - xa;
    int da = pp[1]->y - yy;
    int ab = 0;
    int xb = xa;
    int nb = pp[3]->x - xb;
    int db = pp[3]->y - yy;
    while ( yy < pp[1]->y )
        {
        hline (clrop, xa, xb, yy);
        aa += na;
        xa += aa / da;
        aa %= da;
        ab += nb;
        xb += ab / db;
        ab %= db;
        ++yy;
        }
    aa = 0;
    xa = pp[1]->x;
    na = pp[2]->x - xa;
    da = pp[2]->y - yy;
    while ( yy < pp[3]->y )
        {
        hline (clrop, xa, xb, yy);
        aa += na;
        xa += aa / da;
        aa %= da;
        ab += nb;
        xb += ab / db;
        ab %= db;
        ++yy;
        }
    ab = 0;
    xb = pp[3]->x;
    nb = pp[2]->x - xb;
    db = pp[2]->y - yy;
    while ( yy < pp[2]->y )
        {
        hline (clrop, xa, xb, yy);
        aa += na;
        xa += aa / da;
        aa %= da;
        ab += nb;
        xb += ab / db;
        ab %= db;
        ++yy;
        }
    hline (clrop, xa, xb, yy);
    }

static inline int gxscale (int xp)
    {
    return xp / 2;
    }

static inline int gyscale (int yp)
    {
    return pmode->grow - 1 - yp / 2;
    }

static void gwind (int vl, int vb, int vr, int vt)
    {
    vl = gxscale (vl);
    vr = gxscale (vr);
    vt = gyscale (vt);
    vb = gyscale (vb);
    if (( vl < 0 ) || ( vl > vr ) || ( vr >= pmode->gcol )
        || ( vt < 0 ) || ( vt > vb ) || ( vb >= pmode->grow )) return;
    gvl = vl;
    gvr = vr;
    gvt = vt;
    gvb = vb;
    }

static int8_t getpix (int xp, int yp)
    {
    CLRDEF *cdef = &clrdef[pmode->ncbt];
    uint32_t *fb = (uint32_t *)(framebuf + yp * pmode->nbpl);
    xp <<= cdef->bitsh;
    fb += xp >> 5;
    xp &= 0x1F;
    uint8_t pix = (*fb) >> xp;
    return ( pix & cdef->clrmsk );
    }

int vpoint (int xp, int yp)
    {
    xp = gxscale (xp);
    yp = gyscale (yp);
    if (( xp < gvl ) || ( xp > gvr ) || ( yp < gvt ) || ( yp > gvb )) return -1;
    return getpix (xp, yp);
    }

int vtint (int xp, int yp)
    {
    int clr = vpoint (xp, yp);
    if ( clr < 0 ) return -1;
    clr = curpal[clr];
    return ( PICO_SCANVIDEO_R5_FROM_PIXEL(clr) << 3 )
        | ( PICO_SCANVIDEO_G5_FROM_PIXEL(clr) << 11 )
        | ( PICO_SCANVIDEO_B5_FROM_PIXEL(clr) << 19 );
    }

int vgetc (int x, int y)
    {
    x += tvl;
    y += tvt;
    if (( x < tvl ) || ( x > tvr ) || ( y < tvt ) || ( y > tvb )) return -1;
    if ( pmode == &modes[7] )
        {
        int chr = framebuf[y * pmode->trow + col];
        if ( chr < 0x20 ) chr += 0x80;
        return chr;
        }
    else
        {
        uint8_t chrow[10];
        uint8_t *prow = chrow;
        memset (chrow, 0, sizeof (chrow));
        int bgclr = ( vflags & HRGFLG ) ? clrmsk (gbg) : bg;
        int fhgt = pmode->thgt;
        bool bDbl = false;
        if ( fhgt > 10 )
            {
            fhgt /= 2;
            bDbl = true;
            }
        if ( fhgt == 8 ) ++prow;
        x *= 8;
        y *= pmode->thgt;
        for (int j = 0; j < fhgt; ++j)
            {
            for (int i = 0; i < 8; ++i)
                {
                *prow >>= 1;
                if ( getpix (x+i, y) != bgclr ) *prow |= 0x80;
                }
            ++y;
            if ( bDbl ) ++y;
            ++prow;
            }
        for (int i = 0; i < 256; ++i)
            {
            bool bMatch = true;
            for (int j = 0; j < 10; ++j)
                {
                if ( chrow[j] != font_10[i][j] )
                    {
                    bMatch = false;
                    break;
                    }
                }
            if ( bMatch ) return i;
            }
        }
    return -1;
    }

#define FT_LEFT     0x01
#define FT_RIGHT    0x02
#define FT_EQUAL    0x04

static void linefill (int clrop, int xp, int yp, int ft, uint8_t clr)
    {
#if DEBUG > 0
    printf ("linefill (0x%02X, %d, %d, %d, 0x%02X)\n", clrop, xp,yp, ft, clr);
#endif
    if (( xp < gvl ) || ( xp > gvr ) || ( yp < gvt ) || ( yp > gvb )) return;
    int xp1 = xp;
    if ( ft & FT_LEFT )
        {
        while ( xp1 > gvl )
            {
            uint8_t pix = getpix (xp1 - 1, yp);
            if ( pix == clr )
                {
                if ( ft & FT_EQUAL ) break;
                }
            else
                if ( !( ft & FT_EQUAL ) ) break;
            --xp1;
            }
        }
    int xp2 = xp;
    if ( ft & FT_RIGHT )
        {
        while ( xp2 < gvr )
            {
            uint8_t pix = getpix (xp2 + 1, yp);
            if ( pix == clr )
                {
                if ( ft & FT_EQUAL ) break;
                }
            else
                if ( !( ft & FT_EQUAL ) ) break;
            ++xp2;
            }
        }
    hline (clrop, xp1, xp2, yp);
    }

static inline bool doflood (bool bEq, uint8_t tclr, int xp, int yp)
    {
    if (( xp < gvl ) || ( xp > gvr ) || ( yp < gvt ) || ( yp > gvb )) return false;
    uint8_t pclr = getpix (xp, yp);
    if ( bEq ) return ( pclr == tclr );
    else return ( pclr != tclr );
    }

static void flood (bool bEq, uint8_t tclr, int clrop, int xp, int yp)
    {
    if ( ! doflood (bEq, tclr, xp, yp) ) return;
    int xl = xp;
    int xr = xp;
    while ( doflood (bEq, tclr, xl-1, yp) ) --xl;
    while ( doflood (bEq, tclr, xr+1, yp) ) ++xr;
    hline (clrop, xl, xr, yp);
    int xc = ( xl + xr ) / 2;
    int yt = yp;
    while ( doflood (bEq, tclr, xc, yt-1) ) --yt;
    if ( yt < yp - 1 ) flood (bEq, tclr, clrop, xc, (yp + yt) / 2);
    int yb = yp;
    while ( doflood (bEq, tclr, xc, yb+1) ) ++yb;
    if ( yb > yp + 1 ) flood (bEq, tclr, clrop, xc, (yp + yb) / 2);
    for (int xx = xl; xx <= xr; ++xx)
        {
        if ( doflood (bEq, tclr, xx, yp-1) ) flood (bEq, tclr, clrop, xx, yp-1);
        if ( doflood (bEq, tclr, xx, yp+1) ) flood (bEq, tclr, clrop, xx, yp+1);
        }
    }

static int iroot (int s)
    {
#if DEBUG > 0
    printf ("iroot (%d)\n", s);
#endif
    int r1 = 0;
    int r2 = s;
    while ( r2 > r1 + 1 )
        {
        int r3 = ( r1 + r2 ) / 2;
        int st = r3 * r3;
#if DEBUG > 0
        printf ("r1 = %d, r2 = %d, r3 = %d, st = %d\n", r1, r2, r3, st);
#endif
        if ( st == s ) return r3;
        else if ( st < s ) r1 = r3;
        else r2 = r3;
        }
    return r1;
    }

static void ellipse (bool bFill, int clrop, bool bCircle, int xc, int yc, uint32_t xa, uint32_t yb)
    {
#if DEBUG > 0
    printf ("ellipse (%d, 0x%02X, %d, (%d, %d), %d, %d)\n", bFill, clrop, bCircle, xc, yc, xa, yb);
#endif
    uint32_t bb;
    uint32_t aa;
    uint64_t dd;
    uint32_t xp = xa;
    uint32_t yp = 0;
    if ( bCircle )
        {
        aa = 1;
        bb = 1;
        dd = yb;
        }
    else
        {
        aa = yb * yb;
        bb = xa * xa;
        dd = ((uint64_t) aa) * ((uint64_t) bb);
        }
    if ( yc & 1 )
        {
        --xp;
        yp = 1;
        }
    uint32_t xl = xp;
    int dx = 1;
    if ( bFill )
        {
        hline (clrop, (xc - xp) / 2, (xc + xp) / 2, ( yc + yp ) / 2);
        if ( yp > 0 ) hline (clrop, (xc - xp) / 2, (xc + xp) / 2, ( yc - yp ) / 2);
        }
    while ( true )
        {
        yp += 2;
        uint32_t sq = yp * yp;
        uint64_t pp = ((uint64_t) bb) * ((uint64_t) sq);
        if ( pp > dd ) break;
        uint64_t qq = dd - pp;
        sq = xp * xp;
        pp = ((uint64_t) aa) * ((uint64_t) sq);
        if ( pp > qq )
            {
            int x1 = 0;
            int x2 = xp;
            while ( x2 - x1 > 1 )
                {
                if ( dx > 0 )
                    {
                    xp = x2 - dx;
                    dx *= 2;
                    if ( xp < x1 )
                        {
                        xp = ( x1 + x2 ) / 2;
                        dx = 0;
                        }
                    }
                else
                    {
                    xp = ( x1 + x2 ) / 2;
                    }
                sq = xp * xp;
                pp = ((uint64_t) aa) * ((uint64_t) sq);
                if ( pp > qq )
                    {
                    x2 = xp;
                    }
                else if ( pp == qq )
                    {
                    x1 = xp;
                    break;
                    }
                else
                    {
                    x1 = xp;
                    dx = 0;
                    }
                }
            xp = x1;
            }
        if ( bFill )
            {
#if DEBUG > 0
            printf ("yp = %d: fill %d\n", yp, xp);
#endif
            hline (clrop, (xc - xp) / 2, (xc + xp) / 2, (yc + yp) / 2);
            hline (clrop, (xc - xp) / 2, (xc + xp) / 2, (yc - yp) / 2);
            }
        else if ( xp == xl )
            {
#if DEBUG > 0
            printf ("yp = %d: dot %d\n", yp, xp);
#endif
            clippoint (clrop, (xc + xl) / 2, (yc + yp - 2) / 2);
            clippoint (clrop, (xc - xl) / 2, (yc + yp - 2) / 2);
            if ( yp > 2 )
                {
                clippoint (clrop, (xc + xl) / 2, (yc - yp + 2) / 2);
                clippoint (clrop, (xc - xl) / 2, (yc - yp + 2) / 2);
                }
            }
        else
            {
#if DEBUG > 0
            printf ("yp = %d: dash %d - %d\n", yp, xp, xl);
#endif
            hline (clrop, (xc + xp + 1) / 2, (xc + xl) / 2, (yc + yp - 2) / 2);
            hline (clrop, (xc - xl) / 2, (xc - xp - 1) / 2, (yc + yp - 2) / 2);
            if ( yp > 2 )
                {
                hline (clrop, (xc + xp + 1) / 2, (xc + xl) / 2, (yc - yp + 2) / 2);
                hline (clrop, (xc - xl) / 2, (xc - xp - 1) / 2, (yc - yp + 2) / 2);
                }
            }
        dx = xl - xp;
        if ( dx == 0 ) dx = 1;
        xl = xp;
        }
    if ( ! bFill )
        {
#if DEBUG > 0
        printf ("yp = %d: final dash 0 - %d\n", yp, xl);
#endif
        hline (clrop, (xc - xl) / 2, (xc + xl) / 2, (yc + yp - 2) / 2);
        if ( yp > 2 )
            hline (clrop, (xc - xl) / 2, (xc + xl) / 2, (yc - yp + 2) / 2);
        }
    }

static void plotcir (bool bFill, int clrop)
    {
    int xd = pltpt[0].x - pltpt[1].x;
    int yd = pltpt[0].y - pltpt[1].y;
    int r;
    int r2;
    if ( yd == 0 )
        {
        r = ( xd >= 0 ) ? xd : -xd;
        r2 = xd * xd;
        }
    else if ( xd == 0 )
        {
        r = ( yd >= 0 ) ? yd : -yd;
        r2 = yd * yd;
        }
    else
        {
        r2 = xd * xd + yd * yd;
        r = iroot (r2);
        }
#if DEBUG > 0
    printf ("plotcir: (%d, %d) (%d, %d), r = %d, r2 = %d\n", pltpt[1].x, pltpt[1].y, pltpt[0].x,
        pltpt[0].y, r, r2);
#endif
    if ( r < 2 ) clippoint (clrop, pltpt[1].x / 2, pltpt[1].y / 2);
    else ellipse (bFill, clrop, true, pltpt[1].x, pltpt[1].y, r, r2);
    }

static void plotellipse (bool bFill, int clrop)
    {
    int xc = pltpt[2].x;
    int yc = pltpt[2].y;
    int xa = ( pltpt[1].x - pltpt[2].x );
    int yb = ( pltpt[0].y - pltpt[2].y );
    if ( xa < 0 ) xa = - xa;
    if ( yb < 0 ) yb = - yb;
    if (( xa < 2 ) && ( yb < 2 )) clippoint (clrop, xc, xc);
    else if ( yb < 2 ) hline (clrop, xc - xa, xc + xa, yc);
    else if ( xa < 2 ) clipline (clrop, xc, yc - yb, xc, yc + yb, 0xFFFFFFFF, SKIP_NONE);
    else ellipse (bFill, clrop, false, xc, yc, xa, yb);
    }

static void plot (uint8_t code, int xp, int yp)
    {
#if DEBUG > 0
    printf ("plot (0x%02X, %d, %d)\n", code, xp, yp);
#endif
    pushpts ();
    if ( ( code & 0x04 ) == 0 )
        {
#if DEBUG > 0
        printf ("Relative position\n");
#endif
        pltpt[0].x = pltpt[1].x + xp;
        pltpt[0].y = pltpt[1].y - yp;
        }
    else
        {
#if DEBUG > 0
        printf ("Absolute position\n");
#endif
        pltpt[0].x = xp + gxo;
        pltpt[0].y = 2 * pmode->grow - 1 - ( yp + gyo );
        }
#if DEBUG > 0
    printf ("origin: (%d, %d)\n", gxo, gyo);
    printf ("pltpt: (%d, %d) (%d, %d) (%d, %d)\n", pltpt[0].x, pltpt[0].y,
        pltpt[1].x, pltpt[1].y, pltpt[2].x, pltpt[2].y);
#endif
    int clrop;
    switch (code & 0x03)
        {
        case 0:
            return;
        case 1:
            clrop = gfg;
            break;
        case 2:
            clrop = 0x400;
            break;
        case 3:
            clrop = gbg;
            break;
        }
    if ( code < 0x40 )
        {
        static const uint32_t style[] = { 0xFFFFFFFF, 0x33333333, 0x1F1F1F1F, 0x1C7F1C7F };
        clipline (clrop, pltpt[1].x / 2, pltpt[1].y / 2, pltpt[0].x / 2, pltpt[0].y / 2,
            style[(code >> 4) & 0x03], ( code & 0x08 ) ? SKIP_LAST : SKIP_NONE);
        return;
        }
    switch ( code & 0xF8 )
        {
        case 0x40:
            clippoint (clrop, pltpt[0].x / 2, pltpt[0].y / 2);
            break;
        case 0x48:
            linefill (clrop, pltpt[0].x / 2, pltpt[0].y / 2,
                FT_LEFT | FT_RIGHT, clrmsk (gbg));
            break;
        case 0x50:
            triangle (clrop, pltpt[0].x / 2, pltpt[0].y / 2,
                pltpt[1].x / 2, pltpt[1].y / 2,
                pltpt[2].x / 2, pltpt[2].y / 2);
            break;
        case 0x55:
            linefill (clrop, pltpt[0].x / 2, pltpt[0].y / 2,
                FT_RIGHT | FT_EQUAL, clrmsk (gbg));
            break;
        case 0x60:
            rectangle (clrop, pltpt[0].x / 2, pltpt[0].y / 2,
                pltpt[1].x / 2, pltpt[1].y / 2);
            break;
        case 0x68:
            linefill (clrop, pltpt[0].x / 2, pltpt[0].y / 2,
                FT_LEFT | FT_RIGHT | FT_EQUAL, clrmsk (gfg));
            break;
        case 0x70:
            trapizoid (clrop, pltpt[0].x / 2, pltpt[0].y / 2,
                pltpt[1].x / 2, pltpt[1].y / 2,
                pltpt[2].x / 2, pltpt[2].y / 2);
            break;
        case 0x78:
            linefill (clrop, pltpt[0].x / 2, pltpt[0].y / 2,
                FT_RIGHT, clrmsk (gfg));
            break;
        case 0x80:
            flood (true, clrmsk (gbg), clrop, pltpt[0].x / 2, pltpt[0].y / 2);
            break;
        case 0x88:
            flood (false, clrmsk (gfg), clrop, pltpt[0].x / 2, pltpt[0].y / 2);
            break;
        case 0x90:
        case 0x98:
            plotcir (code >= 0x98, clrop);
            break;
        case 0xC0:
        case 0xC8:
            plotellipse (code >= 0xC8, clrop);
            break;
        default:
            error (255, "Sorry, not implemented") ;
            break;
        }
    }

static void plotchr (int clrop, int xp, int yp, int chr)
    {
    hidecsr ();
    uint32_t fhgt = pmode->thgt;
    bool bDbl = false;
    if ( fhgt > 10 )
        {
        fhgt /= 2;
        bDbl = true;
        }
    const uint8_t *pch = font_10[chr];
    if ( fhgt == 8 ) ++pch;
    for (int i = 0; i < fhgt; ++i)
        {
        int xx = xp;
        int pix = *pch;
        while ( pix )
            {
            if ( pix & 1 ) clippoint (clrop, xx, yp);
            pix >>= 1;
            ++xx;
            }
        ++yp;
        if ( bDbl )
            {
            xx = xp;
            pix = *pch;
            while ( pix )
                {
                if ( pix & 1 ) clippoint (clrop, xx, yp);
                pix >>= 1;
                ++xx;
                }
            ++yp;
            }
        ++pch;
        }
    showcsr ();
    }

void hrwrap (int *pxp, int *pyp)
    {
    int xp;
    int yp;
    xp = pltpt[0].x / 2;
    if ( xp + 7 > gvr )
        {
        xp = gvl;
        pltpt[0].x = 2 * gvl;
        pltpt[0].y += 2 * pmode->thgt;
        }
    yp = pltpt[0].y / 2;
    if ( yp + pmode->thgt - 1 > gvb )
        {
        yp = gvt;
        pltpt[0].y = 2 * gvt;
        }
    if ( pxp != NULL ) *pxp = xp;
    if ( pyp != NULL ) *pyp = yp;
    }

void hrback (void)
    {
    if ( pltpt[0].x / 2 < gvl )
        {
        int wth = ( gvr - gvl ) / 8;
        pltpt[0].x = 2 * ( gvl + 8 * ( wth - 1 ));
        pltpt[0].y -= 2 * pmode->thgt;
        }
    if ( pltpt[0].y / 2 < gvt )
        {
        int hgt = ( gvb - gvt ) / pmode->thgt;
        pltpt[0].y = 2 * ( gvt + ( hgt - 1 ) * pmode->thgt );
        }
    }

void showchr (int chr)
    {
    hidecsr ();
    if ( vflags & HRGFLG )
        {
        int xp;
        int yp;
        pushpts ();
        hrwrap (&xp, &yp);
        plotchr (gfg, xp, yp, chr);
        pltpt[0].x += 16;
        if ( (cmcflg & 1) == 0 ) hrwrap (NULL, NULL);
        }
    else
        {
        if (col > tvr) wrap ();
        dispchr (chr);
        if ((++col > tvr) && ((cmcflg & 1) == 0)) wrap ();
        }
    if ( bPrint ) putchar (chr);
    showcsr ();
    }

/* Process character sequences:
   code (bits 8-15)     - Control byte
   code (bits 0-7)      - Last parameter byte
   data1 (bits 24-31)   - 2nd from last parameter byte
   data1 (bits 16-23)   - 3rd from last parameter byte
   data1 (bits 8-15)    - 4th from last parameter byte
   data1 (bits 0-7)     - 5th from last parameter byte
   data2 (bits 24-31)   - 6th from last parameter byte
   data2 (bits 16-23)   - 7th from last parameter byte
   data2 (bits 8-15)    - 8th from last parameter byte
   data2 (bits 0-7)     - 9th from last parameter byte

   The control byte is always in the higher byte of code.
   Successive parameter bytes then effectively push earlier
   parameters further down the above list.
*/
void xeqvdu (int code, int data1, int data2)
    {
    if ( pmode == NULL ) return;
    int vdu = code >> 8;

    // WJB
    if ((vdu > 32) && (vdu < 127)) printf ("%c", vdu);
    else if ((vdu == 10) || (vdu == 13)) printf ("%c", vdu);
    else if (vdu == 32) printf ("_");
    else printf (" 0x%02X ", vdu);
    fflush (stdout);

    if ((vflags & VDUDIS) && (vdu != 6))
        return;

    hidecsr ();
    switch (vdu)
        {
        case 1: // 0x01 - Next character to printer only
            putchar (code & 0xFF);
            break;
            
        case 2: // 0x02 - PRINTER ON
            bPrint = true;
            break;

        case 3: // 0x03 - PRINTER OFF
            bPrint = false;
            break;

        case 4: // 0x04 - LO-RES TEXT
            vflags &= ~HRGFLG;
            break;

        case 5: // 0x05 - HI-RES TEXT
            vflags |= HRGFLG;
            break;

        case 6: // 0x06 - ENABLE VDU DRIVERS
            vflags &= ~VDUDIS;
            break;

        case 7: // 0x07 - BELL
            if ( bPrint ) putchar (vdu);
            break;

        case 8: // 0x08 - LEFT
            if ( vflags & HRGFLG )
                {
                pushpts ();
                pltpt[0].x -= 16;
                hrback ();
                if ( bPrint ) putchar (vdu);
                }
            else
                {
                if (col == tvl)
                    {
                    col = tvr;
                    if (row == tvt)
                        {
                        scrldn ();
                        if ( bPrint ) printf ("\033M");
                        }
                    else
                        {
                        row--;
                        if ( bPrint ) printf ("\033[%i;%iH", row + 1, col + 1);
                        }
                    }
                else
                    {
                    col--;
                    if ( bPrint ) putchar (vdu);
                    }
                }
            break;

        case 9: // 0x09 - RIGHT
            if ( vflags & HRGFLG )
                {
                pltpt[0].x += 16;
                hrwrap (NULL, NULL);
                }
            else
                {
                if (col > tvr)
                    {
                    wrap ();
                    }
                else
                    {
                    col++;
                    }
                }
            if ( bPrint ) printf ("\033[C");
            break;

        case 10: // 0x0A - LINE FEED
            if ( vflags & HRGFLG )
                {
                pushpts ();
                pltpt[0].y += 2 * pmode->thgt;
                hrwrap (NULL, NULL);
                }
            else
                {
                newline (&col, &row);
                }
            break;

        case 11: // 0x0B - UP
            if ( vflags & HRGFLG )
                {
                pushpts ();
                pltpt[0].y -= 2 * pmode->thgt;
                hrback ();
                }
            else
                {
                if ( row == tvt ) scrldn ();
                else --row;
                }
            if ( bPrint ) printf ("\033M");
            break;

        case 12: // 0x0C - CLEAR SCREEN
            cls ();
            if ( bPrint ) printf ("\033[H\033[J");
            break;

        case 13: // 0x0D - RETURN
            if ( vflags & HRGFLG )
                {
                pushpts ();
                pltpt[0].x = 2 * gvl;
                }
            else
                {
                if ( bPrint ) putchar (vdu);
                col = tvl;
                }
            break;

        case 14: // 0x0E - PAGING ON
            scroln = 0x80 + tvb - row + 1;
            break;

        case 15: // 0x0F - PAGING OFF
            scroln = 0;
            break;

        case 16: // 0x10 - CLEAR GRAPHICS SCREEN
            if ( pmode != &modes[7] )
                {
                clrgraph ();
                if ( bPrint ) printf ("\033[H\033[J");
                break;
                }

        case 17: // 0x11 - COLOUR n
            if ( code & 0x80 )
                {
                bg = clrmsk (code);
                bgfill = (uint8_t) clrdef[pmode->ncbt].cpx[bg];
#if DEBUG > 0
                printf ("Background colour %d, bgfill = 0x%02X\n", bg, bgfill);
#endif
                }
            else
                {
                fg = clrmsk (code);
#if DEBUG > 0
                printf ("Foreground colour %d\n", fg);
#endif
                }
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

        case 18: // 0x12 - GCOL m, n
            if ( code & 0x80 ) gbg = clrmsk (code) | (( data1 >> 16 ) & 0x0700);
            else    gfg = clrmsk (code) | (( data1 >> 16 ) & 0x0700);
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

        case 19: // 0x13 - SET CURPAL
        {
            int pal = data1 & 0x0F;
            int phy = ( data1 >> 8 ) & 0xFF;
            int r = ( data1 >> 16 ) & 0xFF;
            int g = data1 >> 24;
            int b = code & 0xFF;
            if ( pmode == &modes[7] ) pal *= 2;
            if ( phy < 16 ) curpal[pal] = defpal[phy];
            else if ( phy == 16 ) curpal[pal] = PICO_SCANVIDEO_PIXEL_FROM_RGB8(r, g, b);
            else if ( phy == 255 ) curpal[pal] = PICO_SCANVIDEO_PIXEL_FROM_RGB5(r, g, b);
            if ( pmode == &modes[7] ) curpal[pal+1] = curpal[pal];
            else genrb ();
        }

        case 20: // 0x14 - RESET COLOURS
            clrreset ();
            fg = clrmsk (15);
            bg = 0;
            bgfill = 0;
            gfg = clrmsk (15);
            gbg = 0;
            if ( bPrint ) printf ("\033[37m\033[40m");
            break;

        case 21: // 0x15 - DISABLE VDU DRIVERS
            vflags |= VDUDIS;
            break;

        case 22: // 0x16 - MODE CHANGE
            modechg (code & 0x7F);
            break;

        case 23: // 0x17 - DEFINE CHARACTER ETC.
            /*
            defchr (data2 & 0xFF, (data2 >> 8) & 0xFF, (data2 >> 16) & 0xFF,
                (data2 >> 24) & 0xFF, data1 & 0xFF, (data1 >> 8) & 0xFF,
                (data1 >> 16) & 0xFF, (data1 >> 24) & 0xFF, code & 0xFF);
            */
#if DEBUG > 0
            printf ("VDU 0x17: code = 0x%04X, data1 = 0x%08X, data2 = 0x%08X\n",
                code, data1, data2);
#endif
            vdu = data2 & 0xFF;
            if ( vdu <= 1 )
                {
                csrdef (data2);
                }
            else if ( vdu == 18 )
                {
                uint8_t a = (data2 >> 8) & 0xFF;
                uint8_t b = (data2 >> 16) & 0xFF;
                cmcflg = (cmcflg & b) ^ a ;
                }
            break;

        case 24: // 0x18 - DEFINE GRAPHICS VIEWPORT
            gwind ((data2 >> 8) & 0xFFFF,
                ((data2 >> 24) & 0xFF) | ((data1 & 0xFF) << 8),
                (data1 >> 8) & 0xFFFF,
                ((data1 >> 24) & 0xFF) | ((code & 0xFF) << 8)) ;
            break;

        case 25: // 0x19 - PLOT
            if ( pmode != &modes[7] )
                plot (data1 & 0xFF, (data1 >> 8) & 0xFFFF,
                  	((data1 >> 24) & 0xFF) | ((code & 0xFF) << 8)) ;
            break;

        case 26: // 0x1A - RESET VIEWPORTS
            rstview ();
            break;

        case 27: // 0x1B - SEND NEXT TO OUTC
            showchr (code & 0xFF);
            break;

        case 28: // 0x1C - SET TEXT VIEWPORT
            twind ((data1 >> 8) & 0xFF, (data1 >> 16) & 0xFF,
                (data1 >> 24) & 0xFF, code & 0xFF);
            break ;

        case 29: // 0x1D - SET GRAPHICS ORIGIN
            gxo = (data1 >> 8) & 0xFFFF;
            gyo = ((data1 >> 24) & 0xFF) | ((code & 0xFF) << 8);
            break;

        case 30: // 0x1E - CURSOR HOME
            if ( bPrint ) printf ("\033[H");
            home ();
            break;

        case 31: // 0x1F - TAB(X,Y)
            tabxy (data1 >> 24, code & 0xFF);
            if ( bPrint ) printf ("\033[%i;%iH", row + 1, col + 1);
            break;

        case 127: // DEL
            xeqvdu (0x0800, 0, 0);
            if ( vflags & HRGFLG )
                {
                rectangle (clrmsk (gbg), pltpt[0].x / 2, pltpt[0].y / 2,
                    pltpt[0].x / 2 + 7, pltpt[0].y / 2 + pmode->thgt - 1);
                }
            else
                {
                xeqvdu (0x2000, 0, 0);
                xeqvdu (0x0800, 0, 0);
                }
            break;

        default:
            if (vdu >= 0x20)
                {
                showchr (vdu);
                }
        }
    showcsr ();
    if ( bPrint ) fflush (stdout);
    }

void video_periodic (void)
    {
    if ( ++nFlash >= 5 )
        {
        flashcsr ();
        nFlash = 0;
        }
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
    set_sys_clock_khz (8 * vga_timing_800x600_60_default.clock_freq / 1000, true);
#else       // 640x480 VGA
    set_sys_clock_khz (6 * vga_timing_640x480_60_default.clock_freq / 1000, true);
#endif
    stdio_init_all();
#if DEBUG > 0
    sleep_ms(500);
    printf ("setup_vdu: Set clock frequency %d kHz.\n", clock_get_hz (clk_sys) / 1000);
#endif
    critical_section_init (&cs_csr);
    memset (framebuf, 0, sizeof (framebuf));
    modechg (8);
    multicore_launch_core1 (setup_video);

    // Do not return until Core 1 has claimed DMA channels
    uint32_t test =- multicore_fifo_pop_blocking ();
    if ( test != VGA_FLAG )
        {
        printf ("Unexpected value 0x%04X from multicore FIFO\n", test);
        }
    }
