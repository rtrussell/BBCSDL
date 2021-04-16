/*****************************************************************\
*       32-bit or 64-bit BBC BASIC for SDL 2.0                    *
*       (C) 2017-2021  R.T.Russell  http://www.rtrussell.co.uk/   *
*                                                                 *
*       The name 'BBC BASIC' is the property of the British       *
*       Broadcasting Corporation and used with their permission   *
*                                                                 *
*       bbcvdu.c  VDU emulator and graphics drivers               *
*       This module runs in the context of the GUI thread         *
*       Version 1.21a, 05-Apr-2021                                *
\*****************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include "SDL2_gfxPrimitives.h"
#include "SDL_ttf.h"
#include "bbcsdl.h"

#define WM_SIZING 532
#define GL_COLOR_LOGIC_OP 0xBF2
#define NUMCOLOURS 16
#define NUMMODES 34

#define INSERT_KEY  134
#define DELETE_KEY  135
#define CARET_LEFT  136
#define CARET_RIGHT 137
#define CARET_UP    139
#define CARET_DOWN  138
#define COPY_KEY    9
#define CANCEL_COPY 155

// Functions in SDL_gfxPrimitives.c:
int thickEllipseColor(SDL_Renderer*, Sint16, Sint16, Sint16, Sint16, Uint32, Uint8) ;
int thickArcColor(SDL_Renderer*, Sint16, Sint16, Sint16, Sint16, Sint16, Uint32, Uint8) ;
int thickCircleColor(SDL_Renderer*, Sint16, Sint16, Sint16, Uint32, Uint8) ;
int thickLineColorStyle (SDL_Renderer*, Sint16, Sint16, Sint16, Sint16, Uint8, Uint32, int) ;
int RedefineChar (SDL_Renderer*, char, unsigned char*, Uint32, Uint32) ;

// Functions in flood.c:
void flood(unsigned int* pBitmap, int x, int y, int w, int h,
		unsigned int fill_color, unsigned int target_color, int type) ;

// Functions in bbcsdl.c:
#ifdef __WINDOWS__
extern void (__stdcall *glEnableBBC)  (int) ;
extern void (__stdcall *glLogicOpBBC) (int) ;
extern void (__stdcall *glDisableBBC) (int) ;
#else
extern void (*glEnableBBC)  (int) ;
extern void (*glLogicOpBBC) (int) ;
extern void (*glDisableBBC) (int) ;
#endif
extern int (*SDL_RenderFlushBBC) (SDL_Renderer*) ;

// Functions in bbcttx.c:
void page7 (void) ;
void send7 (char, short *, short *, int) ;
extern unsigned char frigo[], frign[] ;

static char xscrol[] = {0, 2, 1, 3} ;

static short logicop[] =
{
	0,	// GCOL 0 - just plot
        0x1507,	// GCOL 1 - OR  (GL_OR)
        0x1501,	// GCOL 2 - AND (GL_AND)
        0x1506,	// GCOL 3 - XOR (GL_XOR)
        0x150A,	// GCOL 4 - NOT (GL_INVERT)
        0,
        0,
        0
} ;

static SDL_BlendMode blendop[] = 
{
	SDL_BLENDMODE_NONE, // GCOL 0 - just plot
	SDL_BLENDMODE_ADD,  // GCOL 1 - add
	SDL_BLENDMODE_MOD,  // GCOL 2 - multiply
	0,		    // GCOL 3 - 'xor' (populated in vduinit)
	0,		    // GCOL 4 - 'not' (populated in vduinit)
        0,
        0,
        0
} ;

// It is important that solid colours aren't dithered, since
// 'flood fill while background' fails with dithered colours.
// Therefore make sure these colours can all be represented
// exactly even with the poorest colour resolution (15-bit RGB).

// With a non-paletted display, logical plotting modes (AND, OR,
// XOR, INVERT) act upon the RGB value, *not* the palette index;
// this may give unexpected results.  To maximise compatibility
// between paletted and non-paletted displays, the default coltab
// should have the property that logical maipulation of the index
// has the same effect as the equivalent manipulation of the RGB
// value.  This is achieved by a direct mapping between each bit
// of the index value and bits in the RGB values.  Unfortunately
// the mapping from 8-bit to 5-bit RGB values for 15-bit & 16-bit
// colour modes is slightly odd:
//    if (col5 == 0)       col8 = 0x00
//    else if (col5 == 1)  col8 = 0x0C
//    else if (col5 == 31) col8 = 0xFF
//    else                 col8 = (col5 + 1) * 8
// so the following mapping is chosen to satisfy both 15-bit and
// 24-bit modes:
//    00:  00000B -> 00000000B -> (0x00)
//    01:  00110B -> 00111000B -> (0x38)
//    10:  11000B -> 11001000B -> (0xC8)
//    11:  11110B -> 11111000B -> (0xF8)
//
// DO NOT MODIFY THESE TABLES WITHOUT REFERENCE TO THE ABOVE !!

static unsigned int coltab[NUMCOLOURS] =
{
0xFF000000,        // Black
0xFF0000C8,        // Red
0xFF00C800,        // Green
0xFF00C8C8,        // Yellow
0xFFC80000,        // Blue
0xFFC800C8,        // Magenta
0xFFC8C800,        // Cyan
0xFFC8C8C8,        // White
0xFF383838,        // Grey
0xFF0000FF,        // Intensified red
0xFF00FF00,        // Intensified green
0xFF00FFFF,        // Intensified yellow
0xFFFF0000,        // Intensified blue
0xFFFF00FF,        // Intensified magenta
0xFFFFFF00,        // Intensified cyan
0xFFFFFFFF         // Intensified white
} ;

// For logical plotting to work correctly in 4-colour modes
// (with both paletted and non-paletted displays) the colours
// other than black and white must be complementary.  It is
// impossible to maintain compatibility with the BBC Micro's
// default palette (black, red, yellow, white) in this case:

static unsigned int coltb4[] =
{
0xFF000000,        // Black
0xFFC800C8,        // Magenta
0xFF00C800,        // Green
0xFFC8C8C8         // White
} ;

static unsigned int coltb2[] =
{
0xFF000000,        // Black
0xFFC8C8C8         // White
} ;

// Table of client area width & height, character width & height
// and colour mask:

static short modetab[34][5] =
{
        {640,512,8,16,2},      // MODE 0
        {640,512,16,16,4},     // MODE 1
        {640,512,32,16,16},    // MODE 2
        {640,500,8,20,16},     // MODE 3 (3,11,15)
        {640,512,16,16,2},     // MODE 4
        {640,512,32,16,4},     // MODE 5
        {640,500,16,20,16},    // MODE 6 (6,14)
        {640,500,16,20,8},     // MODE 7 (7)

        {640,512,8,16,16},     // MODE 8
        {640,512,16,16,16},    // MODE 9
        {720,576,8,16,16},     // MODE 10
        {720,576,16,16,16},    // MODE 11
        {960,768,8,16,16},     // MODE 12
        {960,768,16,16,16},    // MODE 13
        {1280,1024,8,16,16},   // MODE 14
        {1280,1024,16,16,16},  // MODE 15

        {640,400,8,16,16},     // MODE 16 (8)
        {640,400,16,16,16},    // MODE 17 (9,10,12,13)
        {640,480,8,16,16},     // MODE 18 (18)
        {640,480,16,16,16},    // MODE 19
        {800,600,8,20,16},     // MODE 20
        {800,600,16,20,16},    // MODE 21
        {1024,768,8,16,16},    // MODE 22
        {1024,768,16,16,16},   // MODE 23

        {1152,864,8,16,16},    // MODE 24
        {1152,864,16,16,16},   // MODE 25
        {1280,960,8,16,16},    // MODE 26
        {1280,960,16,16,16},   // MODE 27
        {1440,1080,8,20,16},   // MODE 28
        {1440,1080,16,20,16},  // MODE 29
        {1600,1200,8,16,16},   // MODE 30
        {1600,1200,16,16,16},  // MODE 31

// The following modes are provided for compatibility with BBC BASIC (86):

        {640,400,8,16,2},      // MODE 32 (0)
        {640,400,16,16,4}      // MODE 33 (1,2,4,5)
} ;

// Conversion from BBC BASIC (86) modes to BBC BASIC for Windows modes:

static char xmodes[] = {32,33,33,3,33,33,6,7,16,17,17,3,17,17,6,3} ;

// Function template for universal API function:
void (*APIfunc) (int, int, int, int, int, int, int, int, int, int) ;

/*****************************************************************\
*       Bugfix replacements for SDL 2.0 functions                 *
\*****************************************************************/

// Bugfix version of SDL_RenderSetClipRect (SDL Bugzilla 2700)
// n.b. writes to the rect, which is supposedly a constant!
static int BBC_RenderSetClipRect (SDL_Renderer* renderer, SDL_Rect* rect)
{
	int w, h, result ;

	if ((rect == NULL) || (platform >= 0x02000400))
		return SDL_RenderSetClipRect(renderer, rect) ;

	SDL_GetRendererOutputSize (renderer, &w, &h) ;
	rect->y = h - rect->y - rect->h ;
	result = SDL_RenderSetClipRect (renderer, rect) ;
	rect->y = h - rect->y - rect->h ;
	return result ;
}

// Bugfix version of SDL_RenderReadPixels (SDL Bugzilla 2740)
// Also much faster on some platforms because reading pixels from the
// default render target is faster than reading from a target texture.
// n.b. This bugfix routine cannot read a bigger rect than the window!
int BBC_RenderReadPixels (SDL_Renderer* renderer, const SDL_Rect* rect,
                          Uint32 format, void* pixels, int pitch)
{
#ifdef __EMSCRIPTEN__
	return SDL_RenderReadPixels (renderer, rect, format, pixels, pitch) ;
#else
	int result ;
	SDL_Rect dst = {0, 0, rect->w, rect->h} ;
	SDL_Texture *tex = SDL_GetRenderTarget (renderer) ;
	SDL_SetRenderTarget (renderer, NULL) ;
	SDL_RenderCopy (renderer, tex, rect, &dst) ;
	result = SDL_RenderReadPixels (renderer, &dst, format, pixels, pitch) ;
	SDL_SetRenderTarget (renderer, tex) ;
	return result ;
#endif
}

/*****************************************************************\
*       Graphics support functions                                *
\*****************************************************************/

// Set render draw colour:
static void setcol (unsigned char ci)
{
	int col = palette[(int) ci] ;
	SDL_SetRenderDrawColor (memhdc, col & 0xFF, (col >> 8) & 0xFF,
				(col >> 16) & 0xFF, (col >> 24) & 0xFF) ;
}

// Get graphics viewport coordinates
static void grawin (int *l, int *r, int *t, int *b)
{
	if (hrect != NULL)
	{
		*l = hrect -> x ;
		*r = hrect -> x + hrect -> w ;
		*t = hrect -> y ;
		*b = hrect -> y + hrect -> h ;
	}
	else
	{
		*l = 0 ;
		*r = sizex ;
		*t = 0 ;
		*b = sizey ;
	}
}

// Scale from BBC BASIC graphics units to pixels:
static void ascale (int *x, int *y)
{
	*x = (*x + origx) >> 1 ;
	*y = sizey - 1 - ((*y + origy) >> 1) ;
}

// calculate radius:
static int radius (int cx, int cy, int rx, int ry)
{
	return (int)(sqrt ((rx - cx) * (rx - cx) + (ry - cy) * (ry - cy))) & -2 ;
}

// calculate arctangent (degrees 0-360):
static int arctan (int dx, int dy)
{
	int atn, octant = 0 ;
	int atntab[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 8, 9,10,11,12,13,14,
			14,15,16,17,18,19,19,20,21,22,22,23,24,25,25,26,
			27,28,28,29,30,30,31,32,32,33,34,34,35,35,36,37,
			37,38,38,39,39,40,40,41,41,42,42,43,43,44,44,45,45} ;

	if (dx < 0)
	{
		dx = -dx ;
		octant |= 1 ;
	}

	if (dy < 0)
	{
		dy = - dy ;
		octant |= 2 ;
	}

	if (dx < dy)
	{
		int tmp = dx ;
		dx = dy ;
		dy = tmp ;
		octant |= 4 ;
	}

	atn = atntab[(dy << 6) / dx] ;
	switch (octant)
	{
	case 4:
		atn = 90 - atn ;
		break ;
	case 1:
		atn = 180 - atn ;
		break ;
	case 7:
		atn = 270 - atn ;
		break ;
	case 2:
		atn = 360 - atn ;
		break ;
	case 5:
		atn += 90 ;
		break ;
	case 3:
		atn += 180 ;
		break ;
	case 6:
		atn += 270 ;
		break ;
	}
	return atn ;
}

// Blit a rectangular region (source and destination may overlap):
static void blit (int dstx, int dsty, int srcx, int srcy, int w, int h, int bg)
{
	SDL_Rect dst = {dstx, dsty, w, h} ;
	SDL_Rect src = {srcx, srcy, w, h} ;
	SDL_Texture *tex, *tex2, *target ;

	tex = SDL_CreateTexture (memhdc, SDL_PIXELFORMAT_ABGR8888,
                                 SDL_TEXTUREACCESS_TARGET, w, h) ;
	target = SDL_GetRenderTarget (memhdc) ;
	SDL_SetRenderTarget (memhdc, tex) ;
	SDL_RenderClear (memhdc) ; // important
	SDL_RenderCopy (memhdc, target, &src, NULL) ;
	if (bg == 1)
	{
		tex2 = SDL_CreateTexture (memhdc, SDL_PIXELFORMAT_ABGR8888,
					  SDL_TEXTUREACCESS_TARGET, w, h) ;
		SDL_SetRenderTarget (memhdc, tex2) ;
		SDL_RenderCopy (memhdc, target, &dst, NULL) ;
	}
	SDL_SetRenderTarget (memhdc, target) ;
	if (bg < 0)
	{
		setcol (bg & 0xFF) ;
		SDL_RenderFillRect (memhdc, &src) ;
	}
	SDL_RenderCopy (memhdc, tex, NULL, &dst) ;
	if (bg == 1)
	{
		SDL_RenderCopy (memhdc, tex2, NULL, &src) ;
		SDL_DestroyTexture (tex2) ;
	}
	SDL_DestroyTexture (tex) ;
}

// Flood fill WHILE colour = specified target
static void flooda (char col, char tar, int cx, int cy, int vl, int vr, int vt, int vb)
{
	unsigned int *p ;
	SDL_Texture *tex ;

	SDL_Rect rect = {vl, vt, vr - vl, vb - vt} ;

	if ((rect.w <= 0) || (rect.h <= 0) ||
	    (cx < vl) || (cx >= vr) || (cy < vt) || (cy >= vb))
		return ;

	p = (unsigned int*) malloc (rect.w * rect.h * 4) ;
	BBC_RenderReadPixels (memhdc, &rect, SDL_PIXELFORMAT_ABGR8888, p, rect.w * 4) ;
	flood(p, cx-rect.x, cy-rect.y, rect.w, rect.h, palette[(int)col], palette[(int)tar], 0) ;
	tex = SDL_CreateTexture (memhdc, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING, 
				rect.w, rect.h) ;
	SDL_UpdateTexture (tex, NULL, p, rect.w * 4) ;
	free (p) ;
	SDL_RenderCopy (memhdc, tex, NULL, &rect) ;
	SDL_DestroyTexture (tex) ;
}

// Flood fill UNTIL colour = specified target
static void floodb (char col, char tar, int cx, int cy, int vl, int vr, int vt, int vb)
{
	unsigned int *p ;
	SDL_Texture *tex ;

	SDL_Rect rect = {vl, vt, vr - vl, vb - vt} ;

	if ((rect.w <= 0) || (rect.h <= 0) ||
	    (cx < vl) || (cx >= vr) || (cy < vt) || (cy >= vb))
		return ;

	p = (unsigned int*) malloc (rect.w * rect.h * 4) ;
	BBC_RenderReadPixels (memhdc, &rect, SDL_PIXELFORMAT_ABGR8888, p, rect.w * 4) ;
	flood(p, cx-rect.x, cy-rect.y, rect.w, rect.h, palette[(int)col], palette[(int)tar], 1) ;
	tex = SDL_CreateTexture (memhdc, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING, 
				rect.w, rect.h) ;
	SDL_UpdateTexture (tex, NULL, p, rect.w * 4) ;
	free (p) ;
	SDL_RenderCopy (memhdc, tex, NULL, &rect) ;
	SDL_DestroyTexture (tex) ;
}

/*****************************************************************\
*       Text cursor (caret) support functions                     *
\*****************************************************************/

//Get cursor position:
//n.b. textx or texty may be negative in 'pending scroll' state!
static int gcsr (void)
{
	return (texty << 16) | (textx & 0xFFFF) ;
}

// Convert pixel coordinates to location in 'chrmap'
static void p2c (int x, int y, short **pc, short **sl, int *nc, int *oc)
{
	*nc = ((XSCREEN + 7) >> 3) ;	// no. of (wide) chars per line
	*oc = x / charx ;		// char offset from line start
	*sl = chrmap + (y / chary) * (*nc) ; // pointer to line start
	*pc = *sl + *oc ;		// pointer to chrmap
}

// Convert character coordinates to pixel coordinates
// adjusting for text viewport and checking range:
static int xy2p (int *x, int *y)
{
	int w, h ;

	if ((cmcflg & BIT3) != 0)
	{
		int tmp = *x ;
		*x = *y ;
		*y = tmp ;
	}

	w = (textwr - textwl) / charx ;
	if ((*x > w) || (*x < 0))
		return 0 ;  // Allow pending CRLF/scroll
	if ((cmcflg & BIT1) != 0)
		*x = ~(*x - w) ;

	h = (textwb - textwt) / chary ;
	if ((*y > h) || (*y < 0))
		return 0 ;  // Allow pending CRLF/scroll
	if ((cmcflg & BIT2) != 0)
		*y = ~(*y - h) ;

	*x = textwl + charx * (*x) ;
	*y = textwt + chary * (*y) ;
	return 1 ;
}

// Invert cursor/caret blob for 'copy key' editing:
static void blob (void)
{
	int pitch ;
	int *pixels, *p ;
	SDL_Texture *tex ;
	SDL_Rect rect = {textx, texty, charx, chary} ;

	if ((cursa & BIT6) != 0)
		return ;

	tex = SDL_CreateTexture (memhdc, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING,
				 rect.w, rect.h) ;
	SDL_LockTexture (tex, NULL, (void **) &pixels, &pitch) ;
	BBC_RenderReadPixels (memhdc, &rect, SDL_PIXELFORMAT_ABGR8888, pixels, pitch) ;
	for (p = pixels; p < (pixels + rect.h * rect.w); p++)
		*p ^= 0xFFFFFF ;
	SDL_UnlockTexture (tex) ;
	SDL_RenderCopy (memhdc, tex, NULL, &rect) ;
	SDL_DestroyTexture (tex) ;
	bChanged = 1 ;
}

//Swap cursors:
static void swap (int *xy)
{
	short x = *xy & 0xFFFF, y = (*xy >> 16) & 0xFFFF ;
	*xy = gcsr () ;
	textx = x ;
	texty = y ;
}

static void home (void) ;

// clear text viewport to background
static void cls (void)
{
	short *dst, *p ;
	short *pctl, *pcbr ; // pointers to character in character map
	short *sl ;          // pointer to start of line in character map
	int nc ;             // total number of (wide) characters per line
	int octl, ocbr ;     // (wide) character offset from start of line
	SDL_Rect rect = {textwl, textwt, textwr - textwl, textwb - textwt} ;

	// Find character coordinates from pixel coordinates:
	p2c (textwl, textwt, &pctl, &sl, &nc, &octl) ; // top-left (inside)
	p2c (textwr, textwb, &pcbr, &sl, &nc, &ocbr) ; // bottom-right (outside)
	ocbr -= octl ;		// no. of (wide) chars per line of viewport

	// Clear character map:
	dst = pctl ;
	while (dst < (pcbr - nc))
	{
		for (p = dst; p < dst + ocbr; p++)
			*p = L' ' ;
		dst += nc ;
	}

	// Clear text viewport:
	if (modeno == 7)
	{
		page7 () ;	// update screen from character map
		return ;
	}

	setcol (txtbak) ;
	SDL_RenderFillRect (memhdc, &rect) ; // n.b. Android needs SDL patch
	bChanged = 1 ;
}

//
// Scroll text up, down, left or right.
// al = 0 (right), 1 (down), 2 (left), 3 (up)
// ah = 0 for text viewport, ah != 0 for whole window
//
static void scroll (char al, char ah)
{
	int tl, tt, tr, tb ;
	short *src, *dst, *p ;
	short *pctl, *pcbr ; // pointers to character in character map
	short *sl ;          // pointer to start of line in character map
	int nc ;             // total number of (wide) characters per line
	int octl, ocbr ;     // (wide) character offset from start of line

	if (ah == 0)
	{
		tl = textwl ;
		tt = textwt ;
		tr = textwr ;
		tb = textwb ;
	}
	else
	{
		tl = 0 ;
		tt = 0 ;
		tr = sizex ;
		tb = sizey ;
		if (tr > XSCREEN)
			tr = XSCREEN ;
		if (tb > YSCREEN)
			tb = YSCREEN ;
	}

	// Find character coordinates from pixel coordinates:
	p2c (tl, tt, &pctl, &sl, &nc, &octl) ; // top-left (inside)
	p2c (tr, tb, &pcbr, &sl, &nc, &ocbr) ; // bottom-right (outside)
	ocbr -= octl ;		// no. of (wide) chars per line of viewport

	// Check for degenerate cases where we just do a CLS:
	if ((((al & BIT0) == 0) && (ocbr <= 1)) // horiz scroll with width <= 1 char
	|| (((al & BIT0) != 0) && ((pcbr - pctl) <= (nc + ocbr)))) // vert scroll with height <= 1 line
	{
		cls () ;
		return ;
	}

	// Scroll character map:
	switch (al & 3)
	{
	case 0:		// scroll right
		src = pctl ;		// source
		dst = src + 1 	;	// destination
		while (src <= (pcbr - ocbr - nc))
		{
			memmove (dst, src, 2 * (ocbr - 1)) ;
			*src = L' ' ;
			src += nc ;
			dst += nc ;
		}
		break ;

	case 2:		// scroll left
		dst = pctl ;		// destination
		src = dst + 1 ;		// source
		while (dst <= (pcbr - ocbr - nc))
		{
			memmove (dst, src, 2 * (ocbr - 1)) ;
			*(dst + ocbr - 1) = L' ' ;
			src += nc ;
			dst += nc ;
		}
		break ;

	case 1:		// scroll down
		src = pcbr - ocbr - 2*nc ;
		dst = pcbr - ocbr - nc ;
		while (src >= pctl)
		{
			memmove (dst, src, 2 * ocbr) ;
			src -= nc ;
			dst -= nc ;
		}
		for (p = dst; p < dst + ocbr; p++)
			*p = L' ' ;
		break ;

	case 3:		// scroll up
		dst = pctl ;
		src = pctl + nc ;
		while (src <= (pcbr - ocbr - nc))
		{
			memmove (dst, src, 2 * ocbr) ;
			src += nc ;
			dst += nc ;
		}
		for (p = dst; p < dst + ocbr; p++)
			*p = L' ' ;
	}

	// Scroll page:
	if (modeno == 7)
	{
		page7 () ;	// update screen from character map
		return ;
	}

	tr -= tl ;		// width (pixels)
	tb -= tt ;		// height (lines)
	switch (al & 3)
	{
	case 0:		// scroll right
		blit (tl + charx, tt, tl, tt, tr - charx, tb, txtbak + 0x80000000) ; 
		break ;

	case 2:		// scroll left
		blit (tl, tt, tl + charx, tt, tr - charx, tb, txtbak + 0x80000000) ; 
		break ;

	case 1:		// scroll down
		blit (tl, tt + chary, tl, tt, tr, tb - chary, txtbak + 0x80000000) ; 
		break ;

	case 3:		// scroll up
		blit (tl, tt, tl, tt + chary, tr, tb - chary, txtbak + 0x80000000) ; 
	}
	bChanged = 1 ;
}

// Scroll text viewport, testing for 'paged mode' first, or eject printer page:
static int scrolp (char al, char ah)
{
	if ((al & BIT4) == 0)
	    {
		if ((scroln > 0) && !(al & BIT0))
			return 0 ;
		sclflg = ah ;			// for COPY key
		scroll (ah, 0) ;
	    }
	/// else if ((al & BIT1) == 0)
	/// 	psheet () ;
	return 1 ;
}

static void psend (char al)
{
}

// Text caret movement, taking account of VDU 23,16... settings
//   movement code: return = &C0, home = &80, char = &40
//                  right = 0, left = 6, down = 8, up = 14
//                  unpend = 1 (CRLF) or 9 (scroll)
//                  set BIT5 to force 'wrap' rather than 'scroll'
//                  (inverting bit 3 gives correct secondary action)
//                  Secondary action corresponds to 'wrap' or 'home'
//        |  VDU 4  |  VDU 5  |  VDU 2
//  xpos  |  textx  |  lastx  |  prntx
//  ypos  |  texty  |  lasty  |  prnty
//  width |  charx* |  charx* |  prchx*
// height |  chary* |  chary* |  prchy*
//  left  | textwl  |    +    |  paperl
//   top  | textwt  |    +    |  papert
//  right | textwr  |    +    |  paperr
// bottom | textwb  |    +    |  paperb
//
// Notes: * or actual character metrics if proportional-spaced
//        + as returned from 'grawin'

static void cmove (unsigned char al, int dx, int dy, int *px, int *py, int ml, int mt, int mr, int mb)
{
	unsigned char tmp ;
	char ah = cmcflg ;
	mr -= dx ;	// adjust right edge
        mb -= dy ; 	// adjust bottom edge

	while (1)
	{
		if (((al & 0xC0) == 0x40) && ((ah & BIT5) != 0))
			break ;
		if (((al & BIT7) != 0) || ((al & 0x30) == 0))
			ah &= ~BIT6 ;	// if not VDU 5 or VDU 2 mode
		if (((al & 0xB0) != 0) || ((al & BIT6) == 0))
			ah &= ~BIT0 ;	// if not a character
		if ((ah & BIT4) != 0)
			al |= BIT5 ; 	// wrap rather than scroll
		tmp = al ^ (ah & 0x0E) ;
		if ((tmp & BIT3) == 0)
		{	// left or right
			if ((tmp & BIT1) == 0)
			{	// right
				if ((tmp & BIT7) == 0)
				{
					if ((tmp & BIT0) == 0)
						*px += dx ;	// move right
					if (*px <= mr)
						break ;		// all done
				}
				if (scroln && ((al & 9) == 8))
					scroln -= 1 ;
				if ((al & 0x07) == 6)
					mr = ml + ((mr - ml) / dx) * dx ;
				if ((ah & (BIT0 + BIT6)) != 0)
					break ;
				if (al & 8)
				{	// redgey
					if (al & 0xA0)
					{
						*px = ml ;
						break ;
					}
					if ((al & 0x50) == 0x40)
						break ;
					if (scrolp (al, 2)) *px -= dx ;	// undo move
					break ;
				}
				*px = ml ;
				if (al >= 0xC0)
					break ;
			}
			else
			{	// left
				if ((tmp & BIT7) == 0)
				{
					if ((tmp & BIT0) == 0)
						*px -= dx ;	// move left
					if (*px >= ml)
						break ;		// all done
				}
				if (scroln && ((al & 9) == 8))
					scroln -= 1 ;
				if ((al & 0x07) == 6)
					ml = mr + ((ml - mr) / dx) * dx ;
				if ((ah & (BIT0 + BIT6)) != 0)
					break ;
				if (al & 8)
				{	// ledgey
					if (al & 0xA0)
					{
						*px = mr ;
						break ;
					}
					if ((al & 0x50) == 0x40)
						break ;
					if (scrolp (al, 0)) *px += dx ;	// undo move
					break ;
				}
				*px = mr ;
				if (al >= 0xC0)
					break ;
			}
		}
		else
		{	// up or down
			if ((tmp & BIT2) == 0)
			{	// down
				if ((tmp & BIT7) == 0)
				{
					if ((tmp & BIT0) == 0)
						*py += dy ;	// move down
					if (*py <= mb)
						break ;		// all done
				}
				if (scroln && ((al & 9) == 8))
					scroln -= 1 ;
				if ((al & 0x07) == 6)
					mb = mt + ((mb - mt) / dy) * dy ;
				if ((ah & (BIT0 + BIT6)) != 0)
					break ;
				if (al & 8)
				{	// bedgey
					if (al & 0xA0)
					{
						*py = mt ;
						break ;
					}
					if ((al & 0x50) == 0x40)
						break ;
					if (scrolp (al, 3)) *py -= dy ;	// undo move
					break ;
				}
				*py = mt ;
				if (al >= 0xC0)
					break ;
			}
			else
			{	// up
				if ((tmp & BIT7) == 0)
				{
					if ((tmp & BIT0) == 0)
						*py -= dy ;	// move up
					if (*py >= mt)
						break ;		// all done
				}
				if (scroln && ((al & 9) == 8))
					scroln -= 1 ;
				if ((al & 0x07) == 6)
					mt = mb + ((mt - mb) / dy) * dy ;
				if ((ah & (BIT0 + BIT6)) != 0)
					break ;
				if (al & 8)
				{	// tedgey
					if (al & 0xA0)
					{
						*py = mb ;
						break ;
					}
			if ((al & 0x50) == 0x40)
						break ;
					if (scrolp (al, 1)) *py += dy ;	// undo move
					break ;
				}
				*py = mb ;
				if (al >= 0xC0)
					break ;
			}
		}
		al = ((al ^ BIT3) & ~BIT0) ;	// secondary action
	}
}

//Move cursor, with wrap (taking account of VDU 23,16 mode):
static void mcsr (char code)
{
	cmove (code, charx, chary, &textx, &texty, 0, 0, sizex, sizey) ;
}

static short getch (int x, int y)
{
	short *pc ; // pointer to character in character map
	short *sl ; // pointer to start of line in character map
	int nc ;    // total number of (wide) characters per line
	int oc ;    // (wide) character offset from start of line
	short ch ;

	p2c (x, y, &pc, &sl, &nc, &oc) ;
	ch = *pc ;
	if (modeno == 7)
	{
		switch (ch)
		{
		case 0x23:
			ch = 0x60 ;
			break ;
		case 0x60:
			ch = 0x5F ;
			break ;
		case 0x5F:
			ch = 0x23 ;
		}
	}
	return ch ;
}

/*****************************************************************\
*       Text output support functions                             *
\*****************************************************************/

// Outout a proportional-spaced character to the screen.
void charttf(unsigned short ax, int col, SDL_Rect rect)
{
	SDL_Texture *tex = TTFcache[ax] ;
	if (tex == NULL)
	{
		SDL_Surface *surf ;
		SDL_Color white = {0xFF, 0xFF, 0xFF, 0xFF} ;
		Uint16 wchar[2] = {ax, 0} ;
		surf = TTF_RenderUNICODE_Blended (hfont, wchar, white) ;
		tex = SDL_CreateTextureFromSurface (memhdc, surf) ;
		SDL_FreeSurface (surf) ;
		TTFcache[ax] = tex ;
	}
	SDL_SetTextureColorMod (tex, col & 0xFF, (col >> 8) & 0xFF, (col >> 16) & 0xFF) ;
	SDL_QueryTexture (tex, NULL, NULL, &rect.w, &rect.h) ;
	SDL_RenderCopy (memhdc, tex, NULL, &rect) ;
}

// Output a character to the screen.
static void charout(unsigned short ax, unsigned char fg, unsigned char bg,
                    int cx, int cy, int dx)
{
	SDL_Rect rect = {cx, cy, dx, chary} ;
	int col = 0xFFFFFFFF ;

	if (fg != 0xFF)
		col = palette [(int) fg] ;

	if (bg != 0xFF)
	{
		setcol (bg) ;
		SDL_RenderFillRect (memhdc, &rect) ;
	}

	if ((vflags & UFONT) && ((ax >= 0x100) || (usrchr[ax] == 0)))
		charttf (ax, col, rect) ;
	else
		characterColor (memhdc, cx, cy, ax, col) ;
	return ;
}

// Output a character with a logical plotting mode
static void charout_logic (short ax, short fg, int cx, int cy, int dx)
{
	int pitch ;
	int *p ;
	unsigned char rop = fg & 7, col = fg >> 8 ;
	if (glLogicOpBBC)
	    {
		int c, x, y ;
		SDL_Rect rect = {0, 0, dx, chary} ;
		SDL_Texture *tex = SDL_GetRenderTarget (memhdc) ;
		SDL_Texture *src = SDL_CreateTexture (memhdc, SDL_PIXELFORMAT_ABGR8888,
				SDL_TEXTUREACCESS_STREAMING, dx, chary) ;
		SDL_SetRenderTarget (memhdc, NULL) ;
		SDL_SetRenderDrawColor (memhdc, 0, 0, 0, 0xFF) ;
		SDL_RenderFillRect (memhdc, &rect) ;
		charout (ax, 0xFF, 0xFF, 0, 0, dx) ;
		SDL_LockTexture (src, NULL, (void **) &p, &pitch) ;
		SDL_RenderReadPixels (memhdc, &rect, SDL_PIXELFORMAT_ABGR8888, p, pitch) ;
		SDL_SetRenderTarget (memhdc, tex) ;
		BBC_RenderSetClipRect (memhdc, hrect) ;
			if (SDL_RenderFlushBBC) SDL_RenderFlushBBC (memhdc) ;
		glEnableBBC (GL_COLOR_LOGIC_OP) ;
		glLogicOpBBC (logicop[rop]) ;
		setcol (col) ;
		for (y = cy; y < (cy + chary); y++)
			for (x = cx; x < (cx + dx); x++)
			    {
				c = *p++ ;
				if (c & 0x8000)
					SDL_RenderDrawPoint(memhdc, x, y) ;
			    }
		if (SDL_RenderFlushBBC) SDL_RenderFlushBBC (memhdc) ;
		glDisableBBC (GL_COLOR_LOGIC_OP) ;
		SDL_UnlockTexture (src) ;
		SDL_DestroyTexture (src) ;
	    }
	else
	    {
		SDL_Rect rect = {cx, cy, dx, chary} ;
		SDL_Texture *tex = SDL_CreateTexture (memhdc, SDL_PIXELFORMAT_ABGR8888,
				SDL_TEXTUREACCESS_STREAMING, dx, chary) ;
		SDL_LockTexture (tex, NULL, (void **) &p, &pitch) ;
		SDL_RenderReadPixels (memhdc, &rect, SDL_PIXELFORMAT_ABGR8888, p, pitch) ;
		SDL_UnlockTexture (tex) ;
		SDL_SetTextureBlendMode (tex, blendop[rop]) ;
		if (rop == 2)
			SDL_SetRenderDrawColor (memhdc, 0xFF, 0xFF, 0xFF, 0xFF) ;
		else
			SDL_SetRenderDrawColor (memhdc, 0, 0, 0, 0xFF) ;
		BBC_RenderSetClipRect (memhdc, hrect) ;
		SDL_RenderFillRect (memhdc, &rect) ;
		if (rop == 3) col |= 8 ;
		if (rop == 4) col = 255 ;
		charout (ax, col, 0xFF, cx, cy, dx) ;
		SDL_RenderCopy (memhdc, tex, NULL, &rect) ;
		SDL_DestroyTexture (tex) ;
	    }
}

// (variable pitch)
static void vmove (char code, int dx, int dy)
{
	if (vflags & HRGFLG)
	{
		int l, r, t, b ;
		grawin (&l, &r, &t, &b) ;
		cmove (code | BIT5, dx, dy, &lastx, &lasty, l, t, r, b) ;
	}
	else
		cmove (code, dx, dy, &textx, &texty, textwl, textwt, textwr, textwb) ;
}

// (fixed pitch)
static void fmove (char code)
{
	vmove (code, charx, chary) ;
}

// action pending scroll/CRLF
static void unpend (void)
{
	if (cmcflg & BIT0)
		fmove (1) ;
	fmove (9) ;
}

// Home text cursor:
static void home (void)
{
	scroln &= 0x80 ;
	cmove (0x80, charx, chary, &textx, &texty, textwl, textwt, textwr, textwb) ;
}

// Display character as text (VDU 4):
static void send (short ch)
{
	short *pc ; // pointer to character in character map
	short *sl ; // pointer to start of line in character map
	int nc ;    // total number of (wide) characters per line
	int oc ;    // (wide) character offset from start of line
	int dx ;    // character width

	unpend () ;
	p2c (textx, texty, &pc, &sl, &nc, &oc) ;
	if (modeno == 7)
	{
		send7 (ch, pc, sl, texty) ;
		fmove (0x40) ;
	}
	else
	{
		*pc = ch ;              // store character in chrmap
		dx = charx ;		// width (in case not ufont)
		if (vflags & UFONT)
		{
			int sp ;
			Uint16 tmp[3] = {ch, 0x20, 0} ;
			TTF_SizeUNICODE (hfont, tmp, &dx, NULL) ;
			TTF_SizeUNICODE (hfont, tmp + 1, &sp, NULL) ;
			dx = dx - sp + tweak ;
		}

		if (cmcflg & BIT1)	// direction
			charout(ch, txtfor, txtbak, textx+charx-dx, texty, dx) ;
		else
			charout(ch, txtfor, txtbak, textx, texty, dx) ;

		bChanged = 1 ;

		vmove (0x40, dx, chary) ;
	}
}

// Display character as graphics (VDU 5):
static void sendg (short ch)
{
	int dx = charx ;    // character width (default if not ufont)

	if (vflags & UFONT)
	{
		int sp ;
		Uint16 tmp[3] = {ch, 0x20, 0} ;
		TTF_SizeUNICODE (hfont, tmp, &dx, NULL) ;
		TTF_SizeUNICODE (hfont, tmp + 1, &sp, NULL) ;
		dx = dx - sp + tweak ;
	}

	if (ch == 0x20)
	    {
		vmove (0x40, dx, chary) ;
		return ;
	    }

	if ((lastx >= 0) && (lastx < XSCREEN) && (lasty >= 0) && (lasty < YSCREEN))
	{
		short *pc ; // pointer to character in character map
		short *sl ; // pointer to start of line in character map
		int nc ;    // total number of (wide) characters per line
		int oc ;    // (wide) character offset from start of line
		p2c (textx, texty, &pc, &sl, &nc, &oc) ;
		*pc = ch ;
	}

	if (forgnd & 0xFF)
	{
		if (cmcflg & BIT1)		// direction
			charout_logic(ch, forgnd, lastx+charx-dx, lasty, dx) ;
		else
			charout_logic(ch, forgnd, lastx, lasty, dx) ;
	}
	else
	{
		BBC_RenderSetClipRect (memhdc, hrect) ;
		if (cmcflg & BIT1)		// direction
			charout(ch, forgnd >> 8, 0xFF, lastx+charx-dx, lasty, dx) ;
		else
			charout(ch, forgnd >> 8, 0xFF, lastx, lasty, dx) ;
	}

	SDL_RenderSetClipRect (memhdc, NULL) ;
	bChanged = 1 ;

	vmove (0x40, dx, chary) ;
}

static void vducurs (void)
{
	cursa &= BIT5 + BIT6 ;		// Leave 'disabled' bits
	if ((vflags & IOFLAG) == 0)	// Insert or overtype ?
		cursa |= cursb - 2 ;
}

static void iniwin (void)
{
	textx = 0 ;			// text caret position
	texty = 0 ;
	textwl = 0 ;			// text viewport
	textwr = sizex ;
	textwt = 0 ;
	textwb = sizey ;
	origx = 0 ;			// graphics origin
	origy = 0 ;
	lastx = 0 ;			// graphics position bottom-left
	lasty = sizey - 1 ;
	prevx = 0 ;			// graphics position bottom-left
	prevy = sizey - 1 ;
	hrect = NULL ;
	home () ;
}

static void minit (signed char bc)
{
	short *p ;

	if (bc < 0)
	    {
		forgnd = 0 ;
		txtfor = 0 ;
		bakgnd = colmsk << 8 ;
		txtbak = colmsk ;
		palette[(int) colmsk] = 0xFFFFFFFF ; // opaque peak white
	    }

	vflags &= ~UFONT ;
	vflags &= ~HRGFLG ;	// VDU 4
	cursa &= ~(BIT5 | BIT6) ;
	vducurs () ;		// Set cursor shape (after VDU 4)
        iniwin () ;		// Initialise viewports
	scroln = 0 ;		// Paging off
	tweak = 0 ;		// No character spacing adjustment
	setcol (txtbak) ;

	// Clear ALL character map:
	for (p = chrmap; p < chrmap + ((XSCREEN + 7) >> 3) * ((YSCREEN + 7) >> 3); p++)
		*p = L' ' ;

	//SDL_Delay (80) ;
	SDL_RenderClear (memhdc) ;
	bChanged = 1 ;
}

// SET graphics plotting mode & colour:
static void gcol (char al, signed char ah)
{
	al &= 7 ;
	if (al == 4)
		ah |= 0x7F ;		// GCOL 4 - NOT
	if (ah >= 0)
		forgnd = (((ah & colmsk) << 8) | al) ;
	else
		bakgnd = (((ah & colmsk) << 8) | al) ;
}

// Set default palette and colours:
static void rescol (void)
{
	int i, n = (colmsk & (NUMCOLOURS - 1)) + 1 ;
	unsigned int *p ;
	switch (n)
	{
	case 2:
		p = coltb2 ;
		break ;
	case 4:
		p = coltb4 ;
		break ;
	default:
		p = coltab ;
	}
	for (i = 0; i < n; i++)
		palette[i] = p[i] ;
	palette[255] = 0xFFFFFFFF ; // for 'invert' colour 
	txtfor = colmsk & 7 ;
	txtbak = 0 ;
	gcol (0, txtfor) ;
	gcol (0, 0x80) ;
}

// Change to a new screen mode:
static void newmode (short wx, short wy, short cx, short cy, short nc, signed char bc) 
{
	SDL_Texture *tex ;

	if (cx < 8) cx = 8 ;
        if (cy < 8) cy = 8 ;
	sizex = wx ;
	sizey = wy ;
	charx = cx ;
	pixelx = cx >> 3 ;
	chary = cy ;
	pixely = cy >> 3 ;
	cursb = cy ;
	colmsk = nc - 1 ;

	tex = SDL_GetRenderTarget (memhdc) ;
	SDL_SetRenderTarget (memhdc, NULL) ;
#if defined(__ANDROID__) || defined(__IPHONEOS__)
	{
		int w, h, wr, hr ;
		SDL_GL_GetDrawableSize (hwndProg, &w, &h) ;
		wr = (w << 15) / wx ;
		hr = (h << 15) / wy ;
		if (wr < hr)
			zoom = wr ;
		else
			zoom = hr ;
	}
#else
	{
		SDL_DisplayMode dm ;
		SDL_SetWindowSize (hwndProg, wx, wy) ;
		SDL_GetDesktopDisplayMode (0, &dm) ;
		SDL_SetWindowPosition (hwndProg, (dm.w - wx) >> 1, (dm.h - wy) >> 1) ;
	}
#endif
	SDL_SetRenderTarget (memhdc, tex) ;

	// Set font
	if ((modeno == 7) || ((modeno == -1) && (cx >= 16) && (cy >= 20) && ((bc & 1) == 0)))
	{
		gfxPrimitivesSetFont (ttxtfont, 16, 20) ;
		gfxPrimitivesSetFontZoom (cx / 16,  cy / 20) ;
		if (modeno == 7)
		    {
			int i ;
			for (i = 0; i < 12; i++)
				RedefineChar (memhdc, frigo[i],
					(unsigned char *) &ttxtfont[frign[i] * 20], 16, 20) ;
		    }
	}
	else
	{
		gfxPrimitivesSetFont (bbcfont, 8, 8) ;
		gfxPrimitivesSetFontZoom (cx / 8, cy / 8) ;
	}

	memset (usrchr, 0, 256) ;
	rescol () ;
	minit (bc) ;
}

// Initialise VDU system:
static void vduinit (void)
{
#ifdef __EMSCRIPTEN__
	blendop[3] = SDL_ComposeCustomBlendMode (SDL_BLENDFACTOR_ONE_MINUS_DST_COLOR, 
			SDL_BLENDFACTOR_ONE_MINUS_SRC_COLOR, SDL_BLENDOPERATION_ADD, 
			SDL_BLENDFACTOR_ZERO, SDL_BLENDFACTOR_ONE, SDL_BLENDOPERATION_ADD) ;
	blendop[4] = blendop[3] ;
#endif
	hfont = NULL ;
	modeno = -1 ;
	cursb = chary ;
	colmsk = 15 ;
	pixelx = 1 ;
	pixely = 1 ;
	lthick = 1 ;
	cursx = 0 ;
	tweak = 0 ;
	rescol () ;
#ifdef __IPHONEOS__
	minit (0) ;
#else
	minit (-1) ;
#endif
}

// VDU 4
static void qmove (char code)
{
	if (vflags & PTFLAG)
	{
		/// pstart () ;
		/// pmove (code, prchx, prchy) ;
	}
	fmove (code) ;
}

/*****************************************************************\
*       Graphics PLOT codes                                       *
\*****************************************************************/

//plot - multi-function plotting routine
//   Inputs: code = plot code (0-95)
//           xpos = x-coordinate (BASIC units, absolute or relative)
//           ypos = y-coordinate (BASIC units, absolute or relative)
//     0 : move relative
//     1 : draw line relative (foreground)
//     2 : draw line relative (inverse)
//     3 : draw line relative (background)
//   4-7 : as 0-3 but absolute
//  8-15 : as 0-7 but omit last point
// 16-31 : as 0-15 but dotted (....)
// 32-47 : as 0-15 but dashed (----)
// 48-63 : as 0-15 but broken (.-.-)
// 64-71 : as 0-7 but plot single 'dot'
// 72-79 : left & right fill while background
// 80-87 : plot & fill triangle
// 88-95 : right only fill until background
// 96-103: plot & fill axis-aligned rectangle
//104-111: left & right fill until foreground
//112-119: plot & fill parallelogram
//120-127: right only fill while foreground
//128-135: flood fill while background
//136-143: flood fill until foreground
//144-151: draw circle (outline)
//152-159: draw disc (filled circle)
//160-167: draw a circular arc
//168-175: plot & fill a segment
//176-183: plot & fill a sector
//185/189: move a rectangular block
//187/191: copy a rectangular block
//192-199: draw an outline axis-aligned ellipse
//200-207: plot & fill a solid axis-aligned ellipse
//249/253: swap a rectangular block
//
// Plot (absolute pixel coordinates, no scaling)
static void plotns (unsigned char al, int cx, int cy)
{
	int style = 0 ;
	unsigned char rop = 0, col = 0 ;
	int lx, ly, px, py ;
	short vx[4], vy[4] ;
	SDL_Rect rect ;

	switch (al & 3)
	{
	case 1:		// plot in foreground colour
		rop = forgnd & 7 ;
		col = forgnd >> 8 ;
		break ;

	case 2:		// plot in inverse colour
		rop = 4 ;
		col = colmsk ;
		break ;

	case 3:		// plot in background colour
		rop = bakgnd & 7 ;
		col = bakgnd >> 8 ;
	}

	lx = lastx ;
	ly = lasty ;
	px = prevx ;
	py = prevy ;

	prevx = lx ;
	prevy = ly ;
	lastx = cx ;
	lasty = cy ;

	if ((al & 0xC3) == 0)
	    {
		bChanged = 1 ;		// so moves can't flood event queue
		return ;		// just move, don't plot
	    }

	if (rop != 0)
	{
		if (glLogicOpBBC)
		    {
			if (SDL_RenderFlushBBC) SDL_RenderFlushBBC (memhdc) ;
			glEnableBBC (GL_COLOR_LOGIC_OP) ;
			glLogicOpBBC (logicop[rop]) ;
		    }
		else
		    {
			SDL_SetRenderDrawBlendMode (memhdc, blendop[rop]) ;
			if (rop == 3) col |= 8 ;
			if (rop == 4) col = 255 ;
		    }
	}

	BBC_RenderSetClipRect (memhdc, hrect) ;

	switch (al >> 3)
	{
	case 0:
	case 1:
	case 2:
	case 3:
	case 4:
	case 5:
	case 6:
	case 7:	// PLOT 0-63, draw line
		switch (al & 0x30)
		{
		case 0x00:	// solid
			style = 0xFFFFFFFF ;
			break ;

		case 0x10:	// dotted
			style = 0xCCCCCCCC ;
			break ;

		case 0x20:	// dashed
			style = 0xF8F8F8F8 ;
			break ;

		case 0x30:	// broken
			style = 0xFE38FE38 ;
		}

		if ((lthick > 1) || (style != 0xFFFFFFFF))
			thickLineColorStyle (memhdc, lx, ly, cx, cy, 
					     lthick, palette[(int) col], style) ;
		else
		{
			setcol (col) ;
			if ((lx != cx) || (ly != cy))
				SDL_RenderDrawLine (memhdc, lx, ly, cx, cy) ;
			else
				al |= BIT3 ;
			if ((al & BIT3) != 0)
				SDL_RenderDrawPoint (memhdc, cx, cy) ;
		}
		break ;

	case 8:		// PLOT 64-71, Plot a single 'dot' (size depends on mode)
		rect.x = cx ;
		rect.y = cy ;
		rect.w = pixelx ;
		rect.h = pixely ;
		setcol (col) ;
		SDL_RenderFillRect(memhdc, &rect) ;
		break ;

	case 9:		// PLOT 72-79, Fill left & right while background
		grawin (&lx, &px, &ly, &py) ;
		flooda (col, bakgnd >> 8, cx, cy, lx, px, cy, cy + pixely) ;
		break ;

	case 10:	// PLOT 80-87, Plot and fill triangle
		vx[0] = cx ;
		vy[0] = cy ;
		vx[1] = lx ;
		vy[1] = ly ;
		vx[2] = px ;
		vy[2] = py ;
		filledPolygonColor (memhdc, vx, vy, 3, palette[(int) col]) ;
		break ;

	case 11:	// PLOT 88-95, Fill right until background
		grawin (&lx, &px, &ly, &py) ;
		floodb (col, bakgnd >> 8, cx, cy, cx, px, cy, cy + pixely) ;
		break ;

	case 12:	// PLOT 96-103, Plot & fill rectangle
		vx[0] = lx ;
		vy[0] = cy ;
		vx[1] = cx ;
		vy[1] = cy ;
		vx[2] = cx ;
		vy[2] = ly ;
		vx[3] = lx ;
		vy[3] = ly ;
		filledPolygonColor (memhdc, vx, vy, 4, palette[(int) col]) ;
		break ;

	case 13:	// PLOT 104-111, Fill left & right until foreground
		grawin (&lx, &px, &ly, &py) ;
		floodb (col, forgnd >> 8, cx, cy, lx, px, cy, cy + pixely) ;
		break ;

	case 14:	// PLOT 112-119, Plot & fill parallelogram
		vx[0] = px ;
		vy[0] = py ;
		vx[1] = lx ;
		vy[1] = ly ;
		vx[2] = cx ;
		vy[2] = cy ;
		vx[3] = cx - lx + px ;
		vy[3] = cy - ly + py ;
		filledPolygonColor (memhdc, vx, vy, 4, palette[(int) col]) ;
		break ;

	case 15:	// PLOT 120-127, Fill right while foreground
		grawin (&lx, &px, &ly, &py) ;
		flooda (col, forgnd >> 8, cx, cy, cx, px, cy, cy + pixely) ;
		break ;

	case 16:	// PLOT 128-135, Flood fill while background
		grawin (&lx, &px, &ly, &py) ;
		flooda (col, bakgnd >> 8, cx, cy, lx, px, ly, py) ;
		break ;

	case 17:	// PLOT 136-143, Flood fill until foreground
		grawin (&lx, &px, &ly, &py) ;
		floodb (col, forgnd >> 8, cx, cy, lx, px, ly, py) ;
		break ;

	case 18:	// PLOT 144-151, Draw outline circle
		thickCircleColor (memhdc, lx, ly, radius (lx, ly, cx, cy), palette[(int) col], lthick) ;
		break ;

	case 19:	// PLOT 152-159, Draw filled disc
		filledCircleColor (memhdc, lx, ly, radius (lx, ly, cx, cy), palette[(int) col]) ;
		break ;

	case 20:	// PLOT 160-167, Draw circular arc
		thickArcColor (memhdc, px, py, radius (px, py, lx, ly),
			  arctan (cx-px, cy-py), arctan (lx-px, ly-py), palette[(int) col], lthick) ;
		break ;

	case 21:	// PLOT 168-175, Plot filled segment (NOT CURRENTLY IMPLEMENTED)
		break ;

	case 22:	// PLOT 176-183, Plot filled sector
		filledPieColor (memhdc, px, py, radius (px, py, lx, ly),
				arctan (cx-px, cy-py), arctan (lx-px, ly-py), palette[(int) col]) ;
		break ;

	case 23:	// PLOT 184-191, Block copy / move
		if (lx < px)
		{
			int tmp = lx ;
			lx = px ;
			px = tmp ;
		}
		if (ly < py)
		{
			int tmp = ly ;
			ly = py ;
			py = tmp ;
		}
		if (rop != 0)
		    {
			if (glLogicOpBBC)
			    {
				if (SDL_RenderFlushBBC) SDL_RenderFlushBBC (memhdc) ;
				glDisableBBC (GL_COLOR_LOGIC_OP) ;
			    }
			else
				SDL_SetRenderDrawBlendMode (memhdc, SDL_BLENDMODE_NONE) ;
			rop = 0 ;
		    }
		if ((al & BIT1) != 0)
			blit (cx, cy-ly+py, px, py, lx-px+1, ly-py+1, 0) ; // Copy
		else
			blit (cx, cy-ly+py, px, py, lx-px+1, ly-py+1, (bakgnd >> 8) | 0x80000000) ;
		break ;

	case 24:	// PLOT 192-199, Draw outline ellipse
		thickEllipseColor (memhdc, px, py, abs(lx - px), abs(cy - py), palette[(int) col], lthick) ;
		break ;

	case 25:	// PLOT 200-207, Plot filled ellipse
		filledEllipseColor (memhdc, px, py, abs(lx - px), abs(cy - py),
				    palette[(int) col]) ;
		break ;

	case 31:	// PLOT 248-255, Block swap
		if (lx < px)
		{
			int tmp = lx ;
			lx = px ;
			px = tmp ;
		}
		if (ly < py)
		{
			int tmp = ly ;
			ly = py ;
			py = tmp ;
		}
		if (rop != 0)
		    {
			if (glLogicOpBBC)
			    {
				if (SDL_RenderFlushBBC) SDL_RenderFlushBBC (memhdc) ;
				glDisableBBC (GL_COLOR_LOGIC_OP) ;
			    }
			else
				SDL_SetRenderDrawBlendMode (memhdc, SDL_BLENDMODE_NONE) ;
			rop = 0 ;
		    }
		blit (cx, cy-ly+py, px, py, lx-px+1, ly-py+1, 1) ; // Swap
		break ;
	}

	SDL_RenderSetClipRect (memhdc, NULL) ;
	if (rop != 0)
	    {
		if (glLogicOpBBC)
		    {
			if (SDL_RenderFlushBBC) SDL_RenderFlushBBC (memhdc) ;
			glDisableBBC (GL_COLOR_LOGIC_OP) ;
		    }
		else
			SDL_SetRenderDrawBlendMode (memhdc, SDL_BLENDMODE_NONE) ;
	    }
	bChanged = 1 ;
}

static void plot (char code, short xs, short ys)
{
	int xpos = xs, ypos = ys ;

	if ((code & BIT2) != 0)
		ascale (&xpos, &ypos) ;
	else
	{
		xpos = lastx + (xpos >> 1) ;
		ypos = lasty - (ypos >> 1) ;
	}
	plotns (code, xpos, ypos) ;
}

//VDU 16 - CLG
static void clg (void)
{
	if (modeno != 7)
	{
		int l, r, t, b ;
		unsigned char rop = bakgnd & 7, col = bakgnd >> 8 ;
		grawin (&l, &r, &t, &b) ;
		if (rop != 0)
		    {
			short vx[4], vy[4] ;
			vx[0] = l ;
			vy[0] = t ;
			vx[1] = r - 1 ;
			vy[1] = t ;
			vx[2] = r - 1 ;
			vy[2] = b - 1 ;
			vx[3] = l ;
			vy[3] = b - 1 ;

			if (glLogicOpBBC)
			    {
				if (SDL_RenderFlushBBC) SDL_RenderFlushBBC (memhdc) ;
				glEnableBBC (GL_COLOR_LOGIC_OP) ;
				glLogicOpBBC (logicop[rop]) ;
			    }
			else
			    {
				SDL_SetRenderDrawBlendMode (memhdc, blendop[rop]) ;
				if (rop == 3) col |= 8 ;
				if (rop == 4) col = 255 ;
			    }
			filledPolygonColor (memhdc, vx, vy, 4, palette[(int) col]) ;
			if (glLogicOpBBC)
			    {
				if (SDL_RenderFlushBBC) SDL_RenderFlushBBC (memhdc) ;
				glDisableBBC (GL_COLOR_LOGIC_OP) ;
			    }
			else
				SDL_SetRenderDrawBlendMode (memhdc, SDL_BLENDMODE_NONE) ;
		    }
		else
		    {
			SDL_Rect rect = {l, t, r - l, b - t} ;
			setcol (bakgnd >> 8) ;
			SDL_RenderFillRect (memhdc, &rect) ; // n.b. Android needs SDL patch
		    }
		bChanged = 1 ;
	}
}

//VDU 17 - COLOUR - Set text fgnd + backgnd colour
static void colour (signed char al)
{
	if (al >= 0)
		txtfor = al & colmsk ;
	else
		txtbak = al & colmsk ;
}

//VDU 19, l, p, 0, 0, 0 - SET PHYSICAL COLOUR
//VDU 19, l,-1, r, g, b (rgb: 6-bits)
//VDU 19, l,16, R, G, B (RGB: 8-bits)
static void setpal (char n, signed char m, unsigned char r, unsigned char g, unsigned char b)
{
	switch (m)
	{
	case 16:
		palette[(int) n] = 0xFF000000 | (b << 16) | (g << 8) | r ;
		break ;

	case -1:
		palette[(int) n] = 0xFF000000 | (b << 18) | (g << 10) | (r << 2) ;
		break ;

	default:
		palette[(int) n] = (coltab[m & 15] & 0xFFE0F0F0) |
			     ((n & 1) << 3) | ((n & 2) << 10) | ((n & 12) << 17) ;
	}
}

//VDU 22,n - MODE n
static void modechg (char al) 
{
	short wx, wy, cx, cy, nc ;

	al &= 0x7F ;
	if (al >= NUMMODES)
		return ;

	if ((al < 16) && ((vflags & CGAFLG) != 0))
	{
		if ((vflags & EGAFLG) != 0)
			al |= BIT3 ;
		al = xmodes[(int) al] ;	// translate mode for CGA/EGA
	}

	modeno = al ;		// MODE number
	wx = modetab[(int) al][0] ;	// width
	wy = modetab[(int) al][1] ;	// height
	cx = modetab[(int) al][2] ;	// charx
	cy = modetab[(int) al][3] ;	// chary
	nc = modetab[(int) al][4] ;	// no. of colours
	newmode (wx, wy, cx, cy, nc, vflags & (CGAFLG + EGAFLG)) ;
}

//VDU 23, n, a, b, c, d, e, f, g, h - DEFINE CHARACTER (etc.)
static void defchr (unsigned char n, unsigned char a, unsigned char b,
		    unsigned char c, unsigned char d, unsigned char e,
		    unsigned char f, unsigned char g, unsigned char h)
{
	switch (n)
	{
	case 0:		// cursor shape
		switch (a)
		{
		case 10:
			cursa = b ;
			break ;
		case 11:
			cursb = b;
			break ;
		case 18:
			cursx = b;
		}
		break ;

	case 1:		// cursor on/off
		if (a)
			cursa &= ~BIT5 ;
		else
			cursa |= BIT5 ;
		break ;

	case 7:		// text scroll
		scroll (xscrol[b & 3], a) ;
		break ;

	case 16:	// caret movement control
		cmcflg = (cmcflg & b) ^ a ;
		break ;

	case 18:	// teletext extensions
		if (a == 3)
		    {
			if (b)
				vflags |= EGAFLG ;
			else 
				vflags &= ~EGAFLG ;
			if (modeno == 7)
				page7 () ;
		    }
		break ;

	case 22:	// user-defined mode
		modeno = -1 ;
		newmode (a + 256*b, c + 256*d, e, f, g, h) ;
		if (h & 8)
			vflags |= UTF8 ;
		else
			vflags &= ~UTF8 ;
		break ;

	case 23:	// line thickness
		lthick = a ;
		break ;

	case 24:	// character spacing adjustment
		tweak = (signed char) a ;
		break ;

	case 31:	// initialise VDU system
		vduinit () ;
		break ;

	default:	// redefine character
		if (n >= 32)
		{
			unsigned char pattern[8] ;
			pattern[0] = a ;
			pattern[1] = b ;
			pattern[2] = c ;
			pattern[3] = d ;
			pattern[4] = e ;
			pattern[5] = f ;
			pattern[6] = g ;
			pattern[7] = h ;
			RedefineChar (memhdc, n, pattern, 8, 8) ;
			usrchr[n] = 1 ;
		}
	}
}

//VDU 24,lx;by;rx;ty; - SET GRAPHICS VIEWPORT
static void gwind (short sl, short sb, short sr, short st)
{
	int left = sl, top = st, right = sr, bottom = sb ;
	ascale (&right, &bottom) ;
	ascale (&left, &top) ;
	right += 1 ;			// right outside
	bottom += 1 ;			// bottom outside
	if ((left < 0) | (right <= left) | (top < 0) | (bottom <= top))
		return ;
	ClipRect.x = left ;
	ClipRect.y = top ;
	ClipRect.w = right - left ;
	ClipRect.h = bottom - top ;
	hrect = &ClipRect ;
}

//VDU 26 - reset viewports and home cursor
static void reswin (void)
{
	int w = 0, h = 0 ;
	SDL_GL_GetDrawableSize (hwndProg, &w, &h) ;
	if ((w != 0) && (h != 0))
	    {
		SDL_Texture *tex = SDL_GetRenderTarget (memhdc) ;
		SDL_SetRenderTarget (memhdc, NULL) ;
		SDL_SetWindowSize (hwndProg, w, h) ;
		SDL_SetRenderTarget (memhdc, tex) ;
	    }
	sizex = w ;
	sizey = h ;
	zoom = 0x8000 ;
	iniwin () ;
}

//VDU 28,lx,by,rx,ty - SET TEXT VIEWPORT
static void twind (unsigned char lx, unsigned char by, unsigned char rx, unsigned char ty)
{
	int l, r, t, b ;
	l = lx * charx ;
	r = (rx + 1) * charx ;
	t = ty * chary ;
	b = (by + 1) * chary ;

	if ((l < 0) | (r <= l) | (r > XSCREEN) | (t < 0) | (b <= t) | (b > YSCREEN))
		return ;

	textwl = l ;
	textwt = t ;
	textwr = r ;
	textwb = b ;

	if ((textx < l) | (textx >= r) | (texty < t) | (texty >= b))
		home () ;
}

//VDU 29,x;y; - SET GRAPHICS ORIGIN
static void origin (short x, short y)
{
	origx = x ;
	origy = y ;
}

//VDU 31,x,y - POSITION CURSOR
//Co-ords are relative to text viewport
static void tabxy(char x, char y)
{
	int px = x ;
	int py = y ;
	if (xy2p (&px, &py))
	{
		textx = px ;
		texty = py ;
	}
}

/*****************************************************************\
*       VDU codes                                                 *
\*****************************************************************/

// Execute a VDU command:
//          0  ^
// queue->  0  | ev.user.code
//          V  | (eax)
//          h  v
//          g  ^
//          f  | ev.user.data1
//          e  | (ecx)
//          d  v
//          c  ^
//          b  | ev.user.data2
//          a  | (edx)
//          n  v

void xeqvdu_ (int data2, int data1, int code)
{
	int vdu = code >> 8 ;

	if ((vflags & VDUDIS) && (vdu != 1) && (vdu != 6))
	  psend (vdu) ;
	else
	  switch (vdu)
	{
		case 1: // PRINT NEXT BYTE
		  // prtnxt (code & 0xFF) ;
		  break ;

		case 2: // PRINTER ON
		  vflags |= PTFLAG ;
		  break ;

		case 3: // PRINTER OFF
		  vflags &= ~PTFLAG ;
		  break ;

		case 4: // LO-RES TEXT
		  vflags &= ~HRGFLG ;
		  cursa &= ~BIT6 ;
		  break ;

		case 5: // HI-RES TEXT
		  if (modeno == 7)
		    cursa &= ~BIT6 ;
		  else
		    {
		      vflags |= HRGFLG ;
		      cursa |= BIT6 ;
		    }
		  break ;

		case 6: // ENABLE VDU DRIVERS
		  vflags &= ~VDUDIS ;
		  break ;

		case 7: // BELL
		  printf ("\7") ;
		  break ;

		case 8: // BACKSPACE
		  qmove (6) ;
		  break ;

		case 9: // RIGHT
		  unpend () ;
		  qmove (0) ;
		  break ;

		case 10: // LINE FEED
		  qmove (8) ;
		  break ;

		case 11: // UP
		  qmove (14) ;
		  break ;

		case 12: // CLEAR SCREEN
		  home () ;
		  cls () ;
		  break ;

		case 13: // RETURN
		  qmove (0xC0) ;
		  break ;

		case 14: // PAGING ON
		  scroln = 0x80 ;
		  break ;

		case 15: // PAGING OFF
		  scroln = 0 ;
		  break ;

		case 16: // CLG
		  clg () ;
		  break ;

		case 17: // COLOUR n
		  colour (code & 0xFF) ;
		  break ;

		case 18: // GCOL m,n
		  gcol ((data1 >> 24) & 0xFF, code & 0xFF) ;
		  break ;

		case 19: // CHANGE PALETTE
		  setpal (data1 & 0xFF, (data1 >> 8) & 0xFF,
			  (data1 >> 16) & 0xFF, (data1 >> 24) & 0xFF, code & 0xFF) ;
		  break ;

		case 20: // RESET COLOURS
		  rescol() ;
		  break ;

		case 21: // DISABLE VDU DRIVERS
		  vflags |= VDUDIS ;
		  break ;

		case 22: // MODE CHANGE
		  modechg (code & 0xFF) ;
		  break ;

		case 23: // DEFINE CHARACTER ETC.
		  defchr (data2 & 0xFF, (data2 >> 8) & 0xFF, (data2 >> 16) & 0xFF,
			 (data2 >> 24) & 0xFF, data1 & 0xFF, (data1 >> 8) & 0xFF,
			 (data1 >> 16) & 0xFF, (data1 >> 24) & 0xFF, code & 0xFF) ;
		  break ;

		case 24: // DEFINE GRAPHICS VIEWPORT
		  gwind ((data2 >> 8) & 0xFFFF,
			((data2 >> 24) & 0xFF) + ((data1 & 0xFF) << 8),
 			 (data1 >> 8) & 0xFFFF,
			((data1 >> 24) & 0xFF) + ((code & 0xFF) << 8)) ;
		  break ;

		case 25: // PLOT
		  if (modeno != 7)
		  	plot (data1 & 0xFF, (data1 >> 8) & 0xFFFF,
                  	((data1 >> 24) & 0xFF) + ((code & 0xFF) << 8)) ;
		  break ;

		case 26: // RESET VIEWPORTS ETC.
		  reswin () ;
		  break ;

		case 27: // SEND NEXT TO OUTC
		  if (vflags & HRGFLG)
			sendg (code & 0xFF) ;
		  else
			send (code & 0xFF) ;
		  break ;

		case 28: // SET TEXT VIEWPORT
		  twind ((data1 >> 8) & 0xFF, (data1 >> 16) & 0xFF,
			(data1 >> 24) & 0xFF, code & 0xFF) ;
		  break ;

		case 29: // SET GRAPHICS ORIGIN
		  origin ((data1 >> 8) & 0xFFFF,
			 ((data1 >> 24) & 0xFF) + ((code & 0xFF) << 8)) ;
		  break ;

		case 30: // CURSOR HOME
		  scroln &= 0x80 ;
		  qmove (0x80) ;
		  break ;

		case 31: // TAB(X,Y)
		  tabxy ((data1 >> 24) & 0xFF, code & 0xFF) ;
		  break ;
	}
}

// Get character at specified text coordinates
// x coordinate = 0x80000000 signifies return character at cursor
int getchar_ (int x, int y)
{
	if (x == 0x80000000)
		return getch (textx, texty) & 0xFFFF ;
	if (xy2p (&x, &y))
		return getch (x, y) & 0xFFFF ;
	return -1 ;
}

// Get RGB colour at specified graphics coordinates
int vtint_ (int x, int y)
{
	int l, r, t, b, p ;
	SDL_Rect rect ;

	ascale (&x, &y) ;
	grawin (&l, &r, &t, &b) ;
	if ((x < l) || (x >= r) || (y < t) || (y >= b))
		return -1 ;

	rect.x = x ;
	rect.y = y ;
	rect.w = 1 ;
	rect.h = 1 ;
	BBC_RenderReadPixels (memhdc, &rect, SDL_PIXELFORMAT_ABGR8888, &p, 4) ;
	return (p & 0xFFFFFF) ;
}

// Get text caret coordinates:
// x returned in LS 16 bits, y in MS 16 bits
int getcsr_ (void) 
{
	unsigned short x, y ;
	if ((cmcflg & BIT1) != 0)
		x = (textwr - textx) / charx - 1 ;
	else
		x = (textx - textwl) / charx ;
	if ((cmcflg & BIT2) != 0)
		y = (textwb - texty) / chary - 1 ;
	else
		y = (texty - textwt) / chary ;
	if ((cmcflg & BIT3) != 0)
		return ((x << 16) | y) ;
	return ((y << 16) | x) ;
}

// Get string width in graphics units:
int getwid_ (int l, char *s)
{
	if ((vflags & UFONT) == 0)
		return (l * charx * 2) ;
	if ((vflags & UTF8) != 0)
		TTF_SizeUTF8 (hfont, s, &l, NULL) ;
	else
		TTF_SizeText (hfont, s, &l, NULL) ;
	return (l * 2) ;
}

// OPENFONT
TTF_Font *openfont_ (char *filename, int sizestyle)
{
	if (hfont)
		{
			SDL_Texture **p ;
			TTF_CloseFont (hfont) ;
			for (p = TTFcache; p < TTFcache + 65536; p++)
				if (*p != NULL)
				{
					SDL_DestroyTexture (*p) ;
					*p = NULL ;
				}
		}

	hfont = NULL ;
	vflags &= ~UFONT ;
	if ((sizestyle & 0xFFFF) == 0)	// default font?
	{
		if (modeno == -1)
		{
			charx = 8 ;
			chary = 20 ;
		}
		else
		{
			charx = modetab[(int) modeno][2] ;
			chary = modetab[(int) modeno][3] ;
		}
	}
	else
	{
		hfont = TTF_OpenFont (filename, sizestyle & 0xFFFF) ;
		if (hfont)
		{
			int dx, dy ;
			TTF_SetFontStyle (hfont, sizestyle >> 16) ;
			TTF_SizeText (hfont, "x", &dx, &dy) ;
			charx = dx ;
			chary = dy ;
			cursb = dy ;
			vducurs () ;
			vflags |= UFONT ;
		}
	}
	return hfont ;
}

// Copy Key support:
int copkey_ (int *xy, int al)
{
	switch (al)
	{
	case 0:		// swap cursors
		if (*xy != -1)
		{
			blob () ;
			swap (xy) ;
		}
		return 0 ;

	case COPY_KEY:
		if (*xy != -1)
		{
			al = getch (textx, texty) ;
			mcsr (0x20) ;
		}
		else
		{
			*xy = gcsr () ;
			al = 0 ;
		}
		break ;

	case CANCEL_COPY:
		if (*xy != -1)
		{
			swap (xy) ;
			blob () ;
			*xy = -1 ;
		}
		return 0 ;

	case CARET_RIGHT:
		if (*xy == -1)
			return al ;
		mcsr ((cmcflg & 0x0E) | 0x20) ;
		return 0 ;

	case CARET_LEFT:
		if (*xy == -1)
			return al ;
		mcsr (((cmcflg ^ 6) & 0x0E) | 0x20) ;
		return 0 ;

	case CARET_DOWN:
		if (*xy == -1)
			return al ;
		mcsr (((cmcflg ^ 8) & 0x0E) | 0x20) ;
		return 0 ;

	case CARET_UP:
		if (*xy == -1)
			return al ;
		mcsr (((cmcflg ^ 14) & 0x0E) | 0x20) ;
		return 0 ;

	case INSERT_KEY:
		vflags ^= IOFLAG ;
		vducurs () ;
		return 0 ;
	}

	if (*xy != -1)
	{
		swap (xy) ;
		blob () ;
	}
	return al ;
}

// Output 'printing' character (may be DEL):
void vduchr_ (short ucs2)
{
	if (vflags & VDUDIS)
	  return ;

	if (vflags & HRGFLG)
	{
	  if (ucs2 == 127)
	  {
	    int tmp = lasty ;
	    lasty += chary ;
	    plotns (99, lastx - charx, tmp) ;
	  }
	  else
	    sendg (ucs2) ;
	}
	else
	{
	  if (ucs2 == 127)
	  {
	    qmove (6) ;
	    send (' ') ;
	    qmove (6) ;
	  }
	  else
	    send (ucs2) ;
	}
}

// APICALL
#ifdef __llvm__
long long apicall_ (long long (*APIfunc) (size_t, size_t, size_t, size_t, size_t, size_t,
			size_t, size_t, size_t, size_t, size_t, size_t,
			double, double, double, double, double, double, double, double), PARM *p)
{
		return APIfunc (p->i[0], p->i[1], p->i[2], p->i[3], p->i[4], p->i[5],
				p->i[6], p->i[7], p->i[8], p->i[9], p->i[10], p->i[11],
			p->f[0], p->f[1], p->f[2], p->f[3], p->f[4], p->f[5], p->f[6], p->f[7]) ;
}
double fltcall_ (double (*APIfunc) (size_t, size_t, size_t, size_t, size_t, size_t,
			size_t, size_t, size_t, size_t, size_t, size_t,
			double, double, double, double, double, double, double, double), PARM *p)
{
		return APIfunc (p->i[0], p->i[1], p->i[2], p->i[3], p->i[4], p->i[5],
				p->i[6], p->i[7], p->i[8], p->i[9], p->i[10], p->i[11],
			p->f[0], p->f[1], p->f[2], p->f[3], p->f[4], p->f[5], p->f[6], p->f[7]) ;
}
#else
#pragma GCC optimize ("O0")
long long apicall_ (long long (*APIfunc) (size_t, size_t, size_t, size_t, size_t, size_t,
			      size_t, size_t, size_t, size_t, size_t, size_t), PARM *p)
{
#ifdef ARMHF
	if (p->f[0] == 0.0)
		memcpy (&p->f[0], &p->i[0], 48) ;
	if ((void*) APIfunc == (void*) SDL_RenderCopyEx) 
	    {
		memcpy (&p->f[0], &p->i[4], 8) ;
		memcpy (&p->i[4], &p->i[6], 24) ;
	    }
#endif
	long long wrapper (volatile double a, volatile double b, volatile double c, volatile double d,
			volatile double e, volatile double f, volatile double g, volatile double h)
	{
		long long result ;
#ifdef __WIN32__
		static void* savesp ;
		asm ("mov %%esp,%0" : "=m" (savesp)) ;
#endif
		result = APIfunc (p->i[0], p->i[1], p->i[2], p->i[3], p->i[4], p->i[5],
				p->i[6], p->i[7], p->i[8], p->i[9], p->i[10], p->i[11]) ;
#ifdef __WIN32__
		asm ("mov %0,%%esp" : : "m" (savesp)) ;
#endif
		return result ;
	}

	return wrapper (p->f[0], p->f[1], p->f[2], p->f[3], p->f[4], p->f[5], p->f[6], p->f[7]) ;
}
double fltcall_ (double (*APIfunc) (size_t, size_t, size_t, size_t, size_t, size_t,
			      size_t, size_t, size_t, size_t, size_t, size_t), PARM *p)
{
#ifdef ARMHF
	if (p->f[0] == 0.0)
		memcpy (&p->f[0], &p->i[0], 48) ;
	if ((void*) APIfunc == (void*) SDL_RenderCopyEx) 
	    {
		memcpy (&p->f[0], &p->i[4], 8) ;
		memcpy (&p->i[4], &p->i[6], 24) ;
	    }
#endif
	double wrapper (volatile double a, volatile double b, volatile double c, volatile double d,
			volatile double e, volatile double f, volatile double g, volatile double h)
	{
		double result ;
#ifdef __WIN32__
		static void* savesp ;
		asm ("mov %%esp,%0" : "=m" (savesp)) ;
#endif
		result = APIfunc (p->i[0], p->i[1], p->i[2], p->i[3], p->i[4], p->i[5],
				p->i[6], p->i[7], p->i[8], p->i[9], p->i[10], p->i[11]) ;
#ifdef __WIN32__
		asm ("mov %0,%%esp" : : "m" (savesp)) ;
#endif
		return result ;
	}

	return wrapper (p->f[0], p->f[1], p->f[2], p->f[3], p->f[4], p->f[5], p->f[6], p->f[7]) ;
}
#pragma GCC reset_options
#endif

// Display surface (for *DISPLAY):
int disply_ (SDL_Rect *dst, SDL_Surface *surf)
{
	int flip = 0 ;
	SDL_Texture *tex ;
	ascale (&dst->x, &dst->y) ;
	tex = SDL_CreateTextureFromSurface (memhdc, surf) ;
	SDL_FreeSurface (surf) ;
	if ((dst->w == 0) || (dst->h == 0))
		SDL_QueryTexture (tex, NULL, NULL, &dst->w, &dst->h) ;
	if (dst->w < 0)
	    {
		flip |= SDL_FLIP_HORIZONTAL ;
		dst->w = - dst->w ;
		dst->x -= dst->w ;
	    }
	if (dst->h < 0)
	    {
		flip |= SDL_FLIP_VERTICAL ;
		dst->h = - dst->h ;
		dst->y += dst->h + 1 ;
	    }
	dst->y -= (dst->h - 1) ;	// adjust ypos to top
	if (flip)
		SDL_RenderCopyEx (memhdc, tex, NULL, dst, 0, NULL, flip) ;
	else
		SDL_RenderCopy (memhdc, tex, NULL, dst) ;
	SDL_DestroyTexture (tex) ;
	bChanged = 1 ;
	return 1 ;
}

// Read pixels (for *GSAVE/*SCREENSAVE):
void getpix_ (SDL_Rect *src, void *buffer)
{
#ifdef __EMSCRIPTEN__
	int i, j, k = src->h - 1 ;
	int pitch = (src->w * 3 + 3) & -4 ;
	ascale (&src->x, &src->y) ;
	src->y -= (src->h - 1) ;	// adjust ypos to top
	SDL_RenderReadPixels (memhdc, src, SDL_PIXELFORMAT_BGR24, buffer, pitch) ;
	for (j = 0; j < src->h / 2; j++, k--)
		for (i = 0; i < pitch; i += 4)
		    {
			int t = *(int *)(buffer + j*pitch + i) ;
			*(int *)(buffer + j*pitch + i) = *(int *)(buffer + k*pitch + i) ;
			*(int *)(buffer + k*pitch + i) = t ;
		    }
#else
	SDL_Texture *tex = SDL_GetRenderTarget (memhdc) ;
	SDL_SetRenderTarget (memhdc, NULL) ;
	SDL_Rect dst = {0, 0, src->w, src->h} ;
	int pitch = (src->w * 3 + 3) & -4 ;
	ascale (&src->x, &src->y) ;
	src->y -= (src->h - 1) ;	// adjust ypos to top
	SDL_RenderCopyEx (memhdc, tex, src, &dst, 0, NULL, SDL_FLIP_VERTICAL) ;
	SDL_RenderReadPixels (memhdc, &dst, SDL_PIXELFORMAT_BGR24, buffer, pitch) ;
	SDL_SetRenderTarget (memhdc, tex) ;
#endif
}
