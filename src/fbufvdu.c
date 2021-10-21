/*  fbufvdu.c - Implementation of BBC BASIC VDU stream using a framebuffer

    The framebuffer uses 1, 2 or 4 bits per pixel, depending upon the number of colours.
    The framebuffer is strictly little-ended. Successive bytes are displayed left to right,
    and within each byte the least significant bits are displayed first.

    Mode 7 is the exception. In this case the buffer contains 7-bit character and control codes
    Bytes 0x00-0x1F are Teletext control codes and 0x20-0x7F are displayable characters.

    For the Pico in mode 7, the Teletext font is copied into the buffer following the displayed data
    in order to avoid video disruption when programming Flash memory. The font consists of
    three blocks of 96 glyphs for Alphanumeric, Continuous Graphics and Separated Graphics
    display.

*/

//  DEBUG = 0   No output
//          1   VDU characters
//          2   Principle primitives
//          3   Details
#define DEBUG 2

#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include "mcu/fbufvdu.h"
#include "bbccon.h"

#include "mcu/font_10.h"

#define NPLT        3           // Length of plot history

// VDU variables declared in bbcdata_*.s:
extern int origx;                       // Graphics x-origin (BASIC units)
extern int origy;                       // Graphics y-origin (BASIC units)
extern int lastx;                       // Graphics cursor x-position (pixels)
extern int lasty;                       // Graphics cursor y-position (pixels)
extern int prevx;                       // Previous Graphics cursor x-position (pixels)
extern int prevy;                       // Previous Graphics cursor y-position (pixels)
extern int textwl;                      // Text window left (pixels)
extern int textwr;                      // Text window right (pixels)
extern int textwt;                      // Text window top (pixels)
extern int textwb;                      // Text window bottom (pixels)
extern int pixelx;                      // Width of a graphics pixel
extern int pixely;                      // Height of a graphics pixel
extern int textx;                       // Text caret x-position (pixels)
extern int texty;                       // Text caret y-position (pixels)
extern short int forgnd;                // Graphics foreground colour/action
extern short int bakgnd;                // Graphics background colour/action
extern unsigned char txtfor;            // Text foreground colour
extern unsigned char txtbak;            // Text background colour
extern unsigned char modeno;            // Mode number
extern int bPaletted;                   // @ispal%
extern int sizex;                       // Total width of client area
extern int sizey;                       // Total height of client area
extern int charx;                       // Average character width
extern int chary;                       // Average character height

int xcsr;                               // Text cursor horizontal position
int ycsr;                               // Text cursor vertical position
int tvt;                                // Top of text viewport
int tvb;                                // Bottom of text viewport
int tvl;                                // Left edge of text viewport
int tvr;                                // Right edge of text viewport
int gvt;                                // Top of graphics viewport
int gvb;                                // Bottom of graphics viewport
int gvl;                                // Left edge of graphics viewport
int gvr;                                // Right edge of graphics viewport
static MODE *pmode = NULL;              // Current display mode
static uint8_t *framebuf;               // Pointer to framebuffer
CLRDEF *cdef;                           // Colour definitions
static int fg;                          // Text foreground colour
static int bg;                          // Text backgound colour
static uint8_t bgfill;                  // Pixel fill for background colour
static bool bPrint = false;             // Enable output to printer (UART)
static int gfg;                         // Graphics foreground colour & mode
static int gbg;                         // Graphics background colour & mode
static int xshift;                      // Shift to convert X graphics units to pixels
static int yshift;                      // Shift to convert Y graphics units to pixels
typedef struct
    {
    int x;
    int y;
    } GPOINT;
static GPOINT pltpt[NPLT];              // History of plotted points (half pixels from top left)

static uint16_t curpal[16];

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

// Declared in bbmain.c:
void error (int, const char *);

extern int getkey (unsigned char *pkey);

static void newpt (int xp, int yp)
    {
#if DEBUG > 1
    printf ("newpt (%d, %d)\n", xp, yp);
#endif
    memmove (&pltpt[1], &pltpt[0], (NPLT - 1) * sizeof (pltpt[0]));
    pltpt[0].x = xp;
    pltpt[0].y = yp;
    prevx = lastx;
    prevy = lasty;
    lastx = pltpt[0].x >> xshift;
    lasty = pltpt[0].y >> yshift;
    }

static void newpix (int xp, int yp)
    {
    memmove (&pltpt[1], &pltpt[0], (NPLT - 1) * sizeof (pltpt[0]));
    pltpt[0].x = xp << xshift;
    pltpt[0].y = yp << yshift;
    lastx = xp;
    lasty = yp;
    }

static void home (void)
    {
    hidecsr ();
    if ( vflags & HRGFLG )
        {
        newpix (gvl, gvt);
        if ( scroln & 0x80 ) scroln = 0x80 + ( gvb - gvt + 1 ) / pmode->thgt;
        }
    else
        {
        ycsr = tvt;
        xcsr = tvl;
        if ( scroln & 0x80 ) scroln = 0x80 + tvb - tvt + 1;
        }
    showcsr ();
    }

static void scrldn (void)
    {
    hidecsr ();
    if ( pmode->ncbt == 3 )
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
            + ( tvl << cdef->bitsh );
        uint8_t *fb2 = fb1 + pmode->thgt * pmode->nbpl;
        int nr = ( tvb - tvt ) * pmode->thgt;
        int nb = ( tvr - tvl + 1 ) << cdef->bitsh;
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
    if ( pmode->ncbt == 3 )
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
            + ( tvl << cdef->bitsh );
        uint8_t *fb2 = fb1 + pmode->thgt * pmode->nbpl;
        int nr = ( tvb - tvt ) * pmode->thgt;
        int nb = ( tvr - tvl + 1 ) << cdef->bitsh;
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
#if DEBUG > 1
    printf ("cls: bgfill = 0x%02\n", bgfill);
    printf ("tvt = %d, tvb = %d, tvl = %d, tvr = %d\n", tvt, tvb, tvl, tvr);
    printf ("thgt = %d, nbpl = %d\n", pmode->thgt, pmode->nbpl);
#endif
    hidecsr ();
    if ( pmode->ncbt == 3 )
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
#if DEBUG > 1
        printf ("cls: start = %d, length = %d\n", tvt * pmode->thgt * pmode->nbpl,
            ( tvb - tvt + 1 ) * pmode->thgt * pmode->nbpl);
#endif
        memset (framebuf + tvt * pmode->thgt * pmode->nbpl, bgfill,
            ( tvb - tvt + 1 ) * pmode->thgt * pmode->nbpl);
        }
    else
        {
        uint8_t *fb1 = framebuf + tvt * pmode->thgt * pmode->nbpl
            + ( tvl << cdef->bitsh );
        int nr = ( tvb - tvt + 1 ) * pmode->thgt;
        int nb = ( tvr - tvl + 1 ) << cdef->bitsh;
#if DEBUG > 1
        printf ("cls: start = %d, nr = %d, nb = \n", tvt * pmode->thgt * pmode->nbpl, nr, nb);
#endif
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
    if (( ycsr < 0 ) || ( ycsr >= pmode->trow ) || ( xcsr < 0 ) || ( xcsr >= pmode->tcol ))
        {
#if DEBUG > 1
        printf ("Cursor out of range: ycsr = %d, xcsr = %d, screen = %dx%d\n",
            ycsr, xcsr, pmode->trow, pmode->tcol);
#endif
        return;
        }
#if DEBUG > 2
    if ((chr >= 0x20) && (chr <= 0x7F)) printf ("dispchr ('%c')\n", chr);
    else printf ("dispchr (0x%02X)\n", chr);
#endif
    if ( pmode->ncbt == 3 )
        {
        framebuf[ycsr * pmode->tcol + xcsr] = chr & 0x7F;
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
        uint8_t *pfb = framebuf + ycsr * pmode->thgt * pmode->nbpl;
        if ( fhgt == 8 ) ++pch;
        if ( pmode->ncbt == 1 )
            {
            pfb += xcsr;
            uint8_t fpx = (uint8_t) cdef->cpx[fg];
            uint8_t bpx = (uint8_t) cdef->cpx[bg];
            for (int i = 0; i < fhgt; ++i)
                {
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
            pfb += 2 * xcsr;
            uint16_t fpx = cdef->cpx[fg];
            uint16_t bpx = cdef->cpx[bg];
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
            pfb += 4 * xcsr;
            uint32_t fpx = cdef->cpx[fg];
            uint32_t bpx = cdef->cpx[bg];
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
                usleep (5000);
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
    xcsr = tvl;
    newline (&xcsr, &ycsr);
    showcsr ();
    }

static void tabxy (int x, int y)
    {
#if DEBUG > 1
    printf ("tab: ycsr = %d, xcsr = %d\n", y, x);
#endif
    x += tvl;
    y += tvt;
    if (( x >= tvl ) && ( x <= tvr ) && ( y >= tvt ) && ( y <= tvb ))
        {
        hidecsr ();
        ycsr = y;
        xcsr = x;
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
    textwl = 8 * tvl;
    textwr = 8 * tvr + 7;
    textwt = pmode->thgt * tvt;
    textwb = pmode->thgt * (tvb + 1) - 1;
    if (( xcsr < tvl ) || ( xcsr > tvr ) || ( ycsr < tvt ) || ( ycsr > tvb ))
        home ();
    }

static void clrreset (void)
    {
    bg = 0;
    bgfill = 0;
    gbg = 0;
    if ( pmode->ncbt == 1 )
        {
#if DEBUG > 1
        printf ("clrreset: nclr = 2\n");
#endif
        curpal[0] = defclr (0);
        curpal[1] = defclr (15);
        fg = 1;
        gfg = 1;
        }
    else if ( pmode->ncbt == 2 )
        {
#if DEBUG > 1
        printf ("clrreset: nclr = 4\n");
#endif
        curpal[0] = defclr (0);
        curpal[1] = defclr (9);
        curpal[2] = defclr (11);
        curpal[3] = defclr (15);
        fg = 3;
        gfg = 3;
        }
    else if ( pmode->ncbt == 3 )
        {
#if DEBUG > 1
        printf ("clrreset: nclr = 4\n");
#endif
        curpal[0] = defclr (0);
        for (int i = 1; i < 8; ++i)
            {
            curpal[i] = defclr (i+8);
            }
        }
    else
        {
#if DEBUG > 1
        printf ("clrreset: nclr = 16\n");
#endif
        for (int i = 0; i < 16; ++i)
            {
            curpal[i] = defclr (i);
            }
        fg = 15;
        gfg = 15;
        }
    txtfor = fg;
    txtbak = bg;
    forgnd = gfg;
    bakgnd = gbg;
    genrb (curpal);
    }

static void rstview (void)
    {
    hidecsr ();
    twind (0, pmode->trow - 1, pmode->tcol - 1, 0);
    xcsr = 0;
    ycsr = 0;
    origx = 0;
    origy = 0;
    gvt = 0;
    gvb = pmode->grow - 1;
    gvl = 0;
    gvr = pmode->gcol - 1;
    newpt (0, (pmode->grow << yshift) - 1);
    showcsr ();
    }

static inline int clrmsk (int clr)
    {
    return ( clr & cdef->clrmsk );
    }

void modechg (int mode)
    {
#if DEBUG > 1
    printf ("modechg (%d)\n", mode);
#endif
    hidecsr ();
    if ( setmode (mode, &framebuf, &pmode, &cdef) )
        {
#if DEBUG > 1
        printf ("grow = %d, gcol = %d\n", pmode->grow, pmode->gcol);
#endif
        modeno = mode;
        colmsk = cdef->clrmsk;
        int gunit = pmode->gcol;
        pixelx = 1;
        xshift = 0;
        while ( gunit < 1280 )
            {
            ++xshift;
            pixelx <<= 1;
            gunit <<= 1;
            }
        gunit = pmode->grow;
        pixely = 1;
        yshift = 0;
        while ( gunit < 900 )
            {
            ++yshift;
            pixely <<= 1;
            gunit <<= 1;
            }
#if DEBUG > 1
        printf ("pixelx = %d, xshift = %d, pixely = %d, yshift = %d\n", pixelx, xshift, pixely, yshift);
#endif
        clrreset ();
        rstview ();
        cls ();
        bPaletted = 1;
        sizex = pmode->gcol;
        sizey = pmode->grow;
        charx = 8;
        chary = pmode->thgt;
#if DEBUG > 1
        printf ("modechg: Completed cls\n");
#endif
        dispmode ();
#if DEBUG > 1
        printf ("modechg: New mode set\n");
#endif
        showcsr ();
        }
    else
        {
        showcsr ();
        error (25, NULL);
        }
    }

static inline void pixop (int op, uint32_t *fb, uint32_t msk, uint32_t cpx)
    {
#if DEBUG > 2
    printf ("pixop (%d, %p, 0x%08X, 0x%08X)\n", op, fb, msk, cpx);
#endif
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
#if DEBUG > 2
    printf ("hline (0x%04X, %d, %d, %d)\n", clrop, xp1, xp2, yp);
#endif
    xp1 <<= cdef->bitsh;
    xp2 <<= cdef->bitsh;
    uint32_t *fb1 = (uint32_t *)(framebuf + yp * pmode->nbpl);
    uint32_t *fb2 = fb1 + ( xp2 >> 5 );
    fb1 += ( xp1 >> 5 );
    uint32_t msk1 = fwdmsk[xp1 & 0x1F];
    uint32_t msk2 = bkwmsk[(xp2 & 0x1F) + pmode->ncbt - 1];
    uint32_t cpx = cdef->cpx[clrop & cdef->clrmsk];
#if DEBUG > 2
    printf ("xp1 = %d, xp2 = %d, clrmsk = %d, fb1 = %p, fb2 = %p\n",
        xp1 & 0x1F, xp2 & 0x1F, cdef->clrmsk, fb1, fb2);
    printf ("msk1 = 0x%08X, msk2 = 0x%08X, cpx = 0x%08X\n", msk1, msk2, cpx);
#endif
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
#if DEBUG > 2
    printf ("point (0x%04X, %d, %d)\n", clrop, xp, yp);
#endif
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
#if DEBUG > 1
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
#if DEBUG > 1
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
#if DEBUG > 1
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
            if ( ya >= xd )
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
            if ( ya >= xd )
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
#if DEBUG > 1
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
#if DEBUG > 1
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
#if DEBUG > 1
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
#if DEBUG > 1
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
    return xp >> xshift;
    }

static inline int gyscale (int yp)
    {
    return pmode->grow - 1 - (yp >> yshift);
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

// Get text cursor (caret) coordinates:
void getcsr(int *px, int *py)
    {
    if ( px ) *px = xcsr - tvl;
    if ( py ) *py = ycsr - tvt;
    }

static int8_t getpix (int xp, int yp)
    {
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
    return clrrgb (curpal[clr]);
    }

int vgetc (int x, int y)
    {
    x += tvl;
    y += tvt;
    if (( x < tvl ) || ( x > tvr ) || ( y < tvt ) || ( y > tvb )) return -1;
    if ( pmode->ncbt == 3 )
        {
        int chr = framebuf[y * pmode->trow + xcsr];
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

// Get string width in graphics units:
int widths (unsigned char *s, int l)
    {
	return ( 8 * l ) << xshift;
    }

#define FT_LEFT     0x01
#define FT_RIGHT    0x02
#define FT_EQUAL    0x04

static void linefill (int clrop, int xp, int yp, int ft, uint8_t clr)
    {
#if DEBUG > 1
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
#if DEBUG > 1
    printf ("flood (%d, 0x%02X, 0x%02X, %d, %d)\n", bEq, tclr, clrop, xp, yp);
#endif
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
#if DEBUG > 1
    printf ("iroot (%d)\n", s);
#endif
    int r1 = 0;
    int r2 = s;
    // 46340 = sqrt (MAX_INT)
    if ( r2 > 46340 ) r2 = 46340;
    while ( r2 > r1 + 1 )
        {
        int r3 = ( r1 + r2 ) / 2;
        int st = r3 * r3;
#if DEBUG > 2
        printf ("r1 = %d, r2 = %d, r3 = %d, st = %d\n", r1, r2, r3, st);
#endif
        if ( st == s ) { r1 = r3; break; }
        else if ( st < s ) r1 = r3;
        else r2 = r3;
        }
#if DEBUG > 1
    printf ("iroot (%d) = %d\n", s, r1);
#endif
    return r1;
    }

static void ellipse (bool bFill, int clrop, int xc, int yc, uint32_t aa, uint32_t bb, uint64_t dd)
    {
#if DEBUG > 1
    printf ("ellipse (%d, 0x%02X, (%d, %d), %d, %d, %lld)\n", bFill, clrop, xc, yc, aa, bb, dd);
#endif
    uint32_t xp = pmode->gcol;
    uint32_t yp = 0;
    uint32_t xl = 0;
    int dx = 0;
    while ( true )
        {
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
        if ( yp == 0 )
            {
            if ( bFill )
                {
                hline (clrop, xc - xp, xc + xp, yc + yp);
                }
            dx = 1;
            }
        else if ( bFill )
            {
#if DEBUG > 2
            printf ("yp = %d: fill %d\n", yp, xp);
#endif
            hline (clrop, xc - xp, xc + xp, yc + yp);
            hline (clrop, xc - xp, xc + xp, yc - yp);
            }
        else if ( xp == xl )
            {
#if DEBUG > 2
            printf ("yp = %d: dot %d\n", yp, xp);
#endif
            clippoint (clrop, xc + xl, yc + yp - 1);
            clippoint (clrop, xc - xl, yc + yp - 1);
            if ( yp > 1 )
                {
                clippoint (clrop, xc + xl, yc - yp + 1);
                clippoint (clrop, xc - xl, yc - yp + 1);
                }
            }
        else
            {
#if DEBUG > 2
            printf ("yp = %d: dash %d - %d\n", yp, xp, xl);
#endif
            hline (clrop, xc + xp + 1, xc + xl, yc + yp - 1);
            hline (clrop, xc - xl, xc - xp - 1, yc + yp - 1);
            if ( yp > 1 )
                {
                hline (clrop, xc + xp + 1, xc + xl, yc - yp + 1);
                hline (clrop, xc - xl, xc - xp - 1, yc - yp + 1);
                }
            }
        dx = xl - xp;
        if ( dx <= 0 ) dx = 1;
        xl = xp;
        ++yp;
        }
    if ( ! bFill )
        {
#if DEBUG > 2
        printf ("yp = %d: final dash 0 - %d\n", yp, xl);
#endif
        hline (clrop, xc - xl, xc + xl, yc + yp - 1);
        if ( yp > 1 )
            hline (clrop, xc - xl, xc + xl, yc - yp + 1);
        }
    }

static void plotcir (bool bFill, int clrop)
    {
    int xd = pltpt[0].x - pltpt[1].x;
    int yd = pltpt[0].y - pltpt[1].y;
    int r2 = xd * xd + yd * yd;
#if DEBUG > 1
    printf ("plotcir: (%d, %d) (%d, %d), r2 = %d\n", pltpt[1].x, pltpt[1].y, pltpt[0].x,
        pltpt[0].y, r2);
#endif
    if ( r2 < 4 ) clippoint (clrop, pltpt[1].x >> xshift, pltpt[1].y >> yshift);
    else ellipse (bFill, clrop, pltpt[1].x >> xshift, pltpt[1].y >> yshift,
        pixelx << xshift, pixely << yshift, r2);
    }

static void plotellipse (bool bFill, int clrop)
    {
    int xc = pltpt[2].x >> xshift;
    int yc = pltpt[2].y >> yshift;
    int xa = pltpt[1].x - pltpt[2].x;
    int yb = pltpt[0].y - pltpt[2].y;
    if ( xa < 0 ) xa = - xa;
    if ( yb < 0 ) yb = - yb;
    if (( xa < pixelx ) && ( yb < pixely ))
        {
        clippoint (clrop, xc, xc);
        }
    else if ( yb < pixely )
        {
        xa >>= xshift;
        hline (clrop, xc - xa, xc + xa, yc);
        }
    else if ( xa < 2 )
        {
        yb >>= yshift;
        clipline (clrop, xc, yc - yb, xc, yc + yb, 0xFFFFFFFF, SKIP_NONE);
        }
    else
        {
        xa *= xa;
        yb *= yb;
        uint64_t dd = ((uint64_t) xa) * ((uint64_t) yb);
        xa <<= 2 * yshift;
        yb <<= 2 * xshift;
        ellipse (bFill, clrop, xc, yc, yb, xa, dd);
        }
    }

static int octant (int xp, int yp)
    {
#if DEBUG > 1
    printf ("octant (%d, %d)\n", xp, yp);
#endif
    xp <<= xshift;
    yp <<= yshift;
#if DEBUG > 1
    printf ("scaled: xp = %d, yp = %d\n", xp, yp);
#endif
    int iOct;
    if ( yp >= 0 )
        {
        if ( xp >= 0 )
            {
            if ( yp < xp ) iOct = 0;
            else iOct = 1;
            }
        else
            {
            if ( yp > -xp ) iOct = 2;
            else iOct = 3;
            }
        }
    else
        {
        if ( xp < 0 )
            {
            if ( xp < yp ) iOct = 4;
            else iOct = 5;
            }
        else
            {
            if ( xp < -yp ) iOct = 6;
            else iOct = 7;
            }
        }
#if DEBUG > 1
    printf ("octant = %d\n", iOct);
#endif
    return iOct;
    }

static void arc (int clrop)
    {
#if DEBUG > 1
    printf ("arc ((%d, %d), (%d, %d), (%d, %d))\n", pltpt[0].x, pltpt[0].y, pltpt[1].x, pltpt[1].y,
        pltpt[2].x, pltpt[2].y);
#endif
    int xc = pltpt[2].x >> xshift;
    int yc = pltpt[2].y >> yshift;
    int xp = pltpt[1].x - pltpt[2].x;
    int yp = pltpt[2].y - pltpt[1].y;
    int xe = pltpt[0].x - pltpt[2].x;
    int ye = pltpt[2].y - pltpt[0].y;
    int r2 = xp * xp + yp * yp;
    int s1 = iroot (r2 << 8);
    int s2 = iroot ((xe * xe + ye * ye) << 8);
    xe = xe * s1 / s2;
    ye = ye * s1 / s2;
    pltpt[0].x = pltpt[2].x + xe;
    pltpt[0].y = pltpt[2].y - ye;
    int iOct = octant (xp, yp);
    int iEnd = octant (xe, ye);
    if ( iEnd < iOct )
        {
        iEnd += 8;
        }
    else if ( iEnd == iOct )
        {
        switch (iOct)
            {
            case 0:
                if ( ye < yp ) iEnd += 8;
                break;
            case 1:
            case 2:
                if ( xe > xp ) iEnd += 8;
                break;
            case 3:
            case 4:
                if ( ye > yp ) iEnd += 8;
                break;
            case 5:
            case 6:
                if ( xe < xp ) iEnd += 8;
                break;
            case 7:
                if ( ye < yp ) iEnd += 8;
                break;
            }
        }
#if DEBUG > 1
    printf ("Arc from: %d: (%d, %d), to: %d: (%d, %d)\n", iOct, xp, yp, iEnd, xe, ye);
#endif
    bool bDone = false;
    xp >>= xshift;
    yp >>= yshift;
    xe >>= xshift;
    ye >>= yshift;
    int xs = 2 * xshift;
    int ys = 2 * yshift;
#if DEBUG > 1
    printf ("Octant %d\n", iOct);
#endif
    while (! bDone)
        {
        clippoint (clrop, xc + xp, yc - yp);
        switch (iOct & 0x07)
            {
            case 0:
                ++yp;
                if ( ((xp * xp) << xs) + ((yp * yp) << ys) > r2 ) --xp;
                if ( (yp << ys) >= (xp << xs) )
                    {
                    ++iOct;
#if DEBUG > 1
                    printf ("Octant %d\n", iOct);
#endif
                    }
                if (( iOct >= iEnd ) && ( yp > ye )) bDone = true;
                break;
            case 1:
                --xp;
                ++yp;
                if ( ((xp * xp) << xs) + ((yp * yp) << ys) > r2 ) --yp;
                if ( xp <= 0 )
                    {
                    ++iOct;
#if DEBUG > 1
                    printf ("Octant %d\n", iOct);
#endif
                    }
                if (( iOct >= iEnd ) && ( xp < xe )) bDone = true;
                break;
            case 2:
                --xp;
                if ( ((xp * xp) << xs) + ((yp * yp) << ys) > r2 ) --yp;
                if ( (yp << ys) <= ((-xp) << xs) )
                    {
                    ++iOct;
#if DEBUG > 1
                    printf ("Octant %d\n", iOct);
#endif
                    }
                if (( iOct >= iEnd ) && ( xp < xe )) bDone = true;
                break;
            case 3:
                --yp;
                --xp;
                if ( ((xp * xp) << xs) + ((yp * yp) << ys) > r2 ) ++xp;
                if ( yp <= 0 )
                    {
                    ++iOct;
#if DEBUG > 1
                    printf ("Octant %d\n", iOct);
#endif
                    }
                if (( iOct >= iEnd ) && ( yp < ye )) bDone = true;
                break;
            case 4:
                --yp;
                if ( ((xp * xp) << xs) + ((yp * yp) << ys) > r2 ) ++xp;
                if ( (yp << ys) <= (xp << xs) )
                    {
                    ++iOct;
#if DEBUG > 1
                    printf ("Octant %d\n", iOct);
#endif
                    }
                if (( iOct >= iEnd ) && ( yp < ye )) bDone = true;
                break;
            case 5:
                ++xp;
                --yp;
                if ( ((xp * xp) << xs) + ((yp * yp) << ys) > r2 ) ++yp;
                if ( xp >= 0 )
                    {
                    ++iOct;
#if DEBUG > 1
                    printf ("Octant %d\n", iOct);
#endif
                    }
                if (( iOct >= iEnd ) && ( xp > xe )) bDone = true;
                break;
            case 6:
                ++xp;
                if ( ((xp * xp) << xs) + ((yp * yp) << ys) > r2 ) ++yp;
                if ( (xp << xs) >= ((-yp) << ys) )
                    {
                    ++iOct;
#if DEBUG > 1
                    printf ("Octant %d\n", iOct);
#endif
                    }
                if (( iOct >= iEnd ) && ( xp > xe )) bDone = true;
                break;
            case 7:
                ++yp;
                ++xp;
                if ( ((xp * xp) << xs) + ((yp * yp) << ys) > r2 ) --xp;
                if ( yp >= 0 )
                    {
                    ++iOct;
#if DEBUG > 1
                    printf ("Octant %d\n", iOct);
#endif
                    }
                if (( iOct >= iEnd ) && ( yp > ye )) bDone = true;
                break;
            }
        }
    }

static void plot (uint8_t code, int16_t xp, int16_t yp)
    {
#if DEBUG > 1
    printf ("plot (0x%02X, %d, %d)\n", code, xp, yp);
#endif
    if ( ( code & 0x04 ) == 0 )
        {
#if DEBUG > 1
        printf ("Relative position\n");
#endif
        newpt (pltpt[0].x + xp, pltpt[0].y - yp);
        }
    else
        {
#if DEBUG > 1
        printf ("Absolute position\n");
#endif
        newpt (xp + origx, (pmode->grow << yshift) - 1 - ( yp + origy ));
        }
#if DEBUG > 1
    printf ("origin: (%d, %d)\n", origx, origy);
    printf ("pltpt: (%d, %d) (%d, %d) (%d, %d)\n", pltpt[0].x, pltpt[0].y,
        pltpt[1].x, pltpt[1].y, pltpt[2].x, pltpt[2].y);
    printf ("pixelx = %d, xshift = %d, pixely = %d, yshift = %d\n", pixelx, xshift, pixely, yshift);
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
        clipline (clrop, pltpt[1].x >> xshift, pltpt[1].y >> yshift,
            pltpt[0].x >> xshift, pltpt[0].y >> yshift,
            style[(code >> 4) & 0x03], ( code & 0x08 ) ? SKIP_LAST : SKIP_NONE);
        return;
        }
    switch ( code & 0xF8 )
        {
        case 0x40:
            clippoint (clrop, pltpt[0].x >> xshift, pltpt[0].y >> yshift);
            break;
        case 0x48:
            linefill (clrop, pltpt[0].x >> xshift, pltpt[0].y >> yshift,
                FT_LEFT | FT_RIGHT, clrmsk (gbg));
            break;
        case 0x50:
            triangle (clrop, pltpt[0].x >> xshift, pltpt[0].y >> yshift,
                pltpt[1].x >> xshift, pltpt[1].y >> yshift,
                pltpt[2].x >> xshift, pltpt[2].y >> yshift);
            break;
        case 0x55:
            linefill (clrop, pltpt[0].x >> xshift, pltpt[0].y >> yshift,
                FT_RIGHT | FT_EQUAL, clrmsk (gbg));
            break;
        case 0x60:
            rectangle (clrop, pltpt[0].x >> xshift, pltpt[0].y >> yshift,
                pltpt[1].x >> xshift, pltpt[1].y >> yshift);
            break;
        case 0x68:
            linefill (clrop, pltpt[0].x >> xshift, pltpt[0].y >> yshift,
                FT_LEFT | FT_RIGHT | FT_EQUAL, clrmsk (gfg));
            break;
        case 0x70:
            trapizoid (clrop, pltpt[0].x >> xshift, pltpt[0].y >> yshift,
                pltpt[1].x >> xshift, pltpt[1].y >> yshift,
                pltpt[2].x >> xshift, pltpt[2].y >> yshift);
            break;
        case 0x78:
            linefill (clrop, pltpt[0].x >> xshift, pltpt[0].y >> yshift,
                FT_RIGHT, clrmsk (gfg));
            break;
        case 0x80:
            flood (true, clrmsk (gbg), clrop, pltpt[0].x >> xshift, pltpt[0].y >> yshift);
            break;
        case 0x88:
            flood (false, clrmsk (gfg), clrop, pltpt[0].x >> xshift, pltpt[0].y >> yshift);
            break;
        case 0x90:
        case 0x98:
            plotcir (code >= 0x98, clrop);
            break;
        case 0xA0:
            arc (clrop);
            break;
        case 0xC0:
        case 0xC8:
            plotellipse (code >= 0xC8, clrop);
            break;
        default:
            error (255, "Sorry, PLOT function not implemented");
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
    int xp = pltpt[0].x >> xshift;
    int yp = pltpt[0].y >> yshift;
    if ( xp + 7 > gvr )
        {
        xp = gvl;
        yp += pmode->thgt;
        }
    if ( yp + pmode->thgt - 1 > gvb )
        {
        yp = gvt;
        }
    newpix (xp, yp);
    if ( pxp != NULL ) *pxp = xp;
    if ( pyp != NULL ) *pyp = yp;
    }

void hrback (void)
    {
    int xp = pltpt[0].x >> xshift;
    int yp = pltpt[0].y >> yshift;
    if ( xp < gvl )
        {
        int wth = ( gvr - gvl ) / 8;
        xp = gvl + 8 * ( wth - 1 );
        yp -= pmode->thgt;
        }
    if ( yp < gvt )
        {
        int hgt = ( gvb - gvt ) / pmode->thgt;
        yp = gvt + ( hgt - 1 ) * pmode->thgt;
        }
    newpix (xp, yp);
    }

void showchr (int chr)
    {
    hidecsr ();
    if ( vflags & HRGFLG )
        {
        int xp;
        int yp;
        hrwrap (&xp, &yp);
        plotchr (gfg, xp, yp, chr);
        newpix (xp + 8, yp);
        if ( (cmcflg & 1) == 0 ) hrwrap (NULL, NULL);
        }
    else
        {
        if (xcsr > tvr) wrap ();
        dispchr (chr);
        if ((++xcsr > tvr) && ((cmcflg & 1) == 0)) wrap ();
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
    int vdu = code >> 8;

#if DEBUG > 0
    if ((vdu > 32) && (vdu < 127)) printf ("%c", vdu);
    else if ((vdu == 10) || (vdu == 13)) printf ("%c", vdu);
    else if (vdu == 32) printf ("_");
    else printf (" 0x%02X ", vdu);
    fflush (stdout);
#endif

    if ((vflags & VDUDIS) && (vdu != 6))
        return;

    hidecsr ();
    switch (vdu)
        {
        case 0: // 0x00 - NULL
            break;
            
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
                pltpt[0].x -= 16;
                hrback ();
                if ( bPrint ) putchar (vdu);
                }
            else
                {
                if (xcsr == tvl)
                    {
                    xcsr = tvr;
                    if (ycsr == tvt)
                        {
                        scrldn ();
                        if ( bPrint ) printf ("\033M");
                        }
                    else
                        {
                        ycsr--;
                        if ( bPrint ) printf ("\033[%i;%iH", ycsr + 1, xcsr + 1);
                        }
                    }
                else
                    {
                    xcsr--;
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
                if (xcsr > tvr)
                    {
                    wrap ();
                    }
                else
                    {
                    xcsr++;
                    }
                }
            if ( bPrint ) printf ("\033[C");
            break;

        case 10: // 0x0A - LINE FEED
            if ( vflags & HRGFLG )
                {
                pltpt[0].y += 2 * pmode->thgt;
                hrwrap (NULL, NULL);
                }
            else
                {
                newline (&xcsr, &ycsr);
                }
            break;

        case 11: // 0x0B - UP
            if ( vflags & HRGFLG )
                {
                pltpt[0].y -= 2 * pmode->thgt;
                hrback ();
                }
            else
                {
                if ( ycsr == tvt ) scrldn ();
                else --ycsr;
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
                newpix (gvl, pltpt[0].y >> yshift);
                }
            else
                {
                xcsr = tvl;
                }
            if ( bPrint ) putchar (vdu);
            break;

        case 14: // 0x0E - PAGING ON
            scroln = 0x80 + tvb - ycsr + 1;
            break;

        case 15: // 0x0F - PAGING OFF
            scroln = 0;
            break;

        case 16: // 0x10 - CLEAR GRAPHICS SCREEN
            if ( pmode->ncbt != 3 )
                {
                clrgraph ();
                if ( bPrint ) printf ("\033[H\033[J");
                break;
                }

        case 17: // 0x11 - COLOUR n
            if ( code & 0x80 )
                {
                bg = clrmsk (code);
                bgfill = (uint8_t) cdef->cpx[bg];
                txtbak = bg;
#if DEBUG > 1
                printf ("Background colour %d, bgfill = 0x%02X\n", bg, bgfill);
#endif
                }
            else
                {
                fg = clrmsk (code);
                txtfor = fg;
#if DEBUG > 1
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
            if ( code & 0x80 )
                {
                gbg = clrmsk (code) | (( data1 >> 16 ) & 0x0700);
                bakgnd = gbg;
                }
            else
                {
                gfg = clrmsk (code) | (( data1 >> 16 ) & 0x0700);
                forgnd = gfg;
                }
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
            int g = ( data1 >> 24 ) & 0xFF;;
            int b = code & 0xFF;
#if DEBUG > 0
            printf ("pal = %d, phy = %d, r = %d, g = %d, b = %d\n", pal, phy, r, g, b);
#endif
            if ( pmode->ncbt == 3 ) pal *= 2;
            if ( phy < 16 ) curpal[pal] = defclr (phy);
            else if ( phy == 16 ) curpal[pal] = rgbclr (r, g, b);
            else if ( phy == 255 ) curpal[pal] = rgbclr (8*r, 8*g, 8*b);
#if DEBUG > 0
            printf ("curpal[%d] = 0x%04X\n", pal, curpal[pal]);
#endif
            if ( pmode->ncbt == 3 ) curpal[pal+1] = curpal[pal];
            else genrb (curpal);
            break;
        }

        case 20: // 0x14 - RESET COLOURS
            clrreset ();
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
#if DEBUG > 1
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
                cmcflg = (cmcflg & b) ^ a;
                }
            break;

        case 24: // 0x18 - DEFINE GRAPHICS VIEWPORT
            gwind ((data2 >> 8) & 0xFFFF,
                ((data2 >> 24) & 0xFF) | ((data1 & 0xFF) << 8),
                (data1 >> 8) & 0xFFFF,
                ((data1 >> 24) & 0xFF) | ((code & 0xFF) << 8));
            break;

        case 25: // 0x19 - PLOT
            if ( pmode->ncbt != 3 )
                plot (data1 & 0xFF, (data1 >> 8) & 0xFFFF,
                  	((data1 >> 24) & 0xFF) | ((code & 0xFF) << 8));
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
            break;

        case 29: // 0x1D - SET GRAPHICS ORIGIN
            origx = (data1 >> 8) & 0xFFFF;
            origy = ((data1 >> 24) & 0xFF) | ((code & 0xFF) << 8);
            break;

        case 30: // 0x1E - CURSOR HOME
            if ( bPrint ) printf ("\033[H");
            home ();
            break;

        case 31: // 0x1F - TAB(X,Y)
            tabxy (data1 >> 24, code & 0xFF);
            if ( bPrint ) printf ("\033[%i;%iH", ycsr + 1, xcsr + 1);
            break;

        case 127: // DEL
            xeqvdu (0x0800, 0, 0);
            if ( vflags & HRGFLG )
                {
                rectangle (clrmsk (gbg), pltpt[0].x >> xshift, pltpt[0].y >> yshift,
                    pltpt[0].x >> xshift + 7, pltpt[0].y >> yshift + pmode->thgt - 1);
                }
            else
                {
                xeqvdu (0x2000, 0, 0);
                xeqvdu (0x0800, 0, 0);
                }
            break;

        default:
            showchr (vdu);
            break;
        }
    showcsr ();
    textx = 8 * xcsr;
    texty = pmode->thgt * ycsr;
    if ( bPrint ) fflush (stdout);
    }
