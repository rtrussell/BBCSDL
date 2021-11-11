/* fbufvdu.c - Hardware interface for implementing VDU on a framebuffer */

#ifndef H_FBUFVDU
#define H_FBUFVDU

#include <stdint.h>

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
    uint32_t    yshf;       // Y scale (number of repeats of each line) = 2 ^ yshf
    uint32_t    thgt;       // Text height
    } MODE;

typedef struct
    {
    uint32_t        nclr;       // Number of colours
    const uint32_t *cpx;        // Colour patterns
    uint32_t        bitsh;      // Shift X coordinate to get bit position
    uint32_t        clrmsk;     // Colour mask
    uint32_t        csrmsk;     // Mask for cursor
    } CLRDEF;

bool setmode (int mode, uint8_t **pfbuf, MODE **ppmd, CLRDEF **ppcd);
void dispmode (void);
void hidecsr (void);
void showcsr (void);
void csrdef (int data2);
uint16_t defclr (int clr);
uint16_t rgbclr (int r, int g, int b);
int clrrgb (uint16_t clr);
void genrb (uint16_t *curpal);
uint8_t *swapbuf (void);
uint8_t *singlebuf (void);
uint8_t *doublebuf (void);
const char *checkbuf (void);

#endif
