/*****************************************************************\
*       32-bit or 64-bit BBC BASIC for SDL 2.0                    *
*       Copyright (c) R. T. Russell, 2016-2019                    *
*                                                                 *
*       BBCVTX.C  MODE 7 (teletext / videotex) emulator           *
*       This module runs in the context of the GUI thread         *
*       Version 1.02a, 03-Apr-2019                                *
\*****************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include "SDL2_gfxPrimitives.h"
#include "SDL_ttf.h"
#include "bbcsdl.h"

#define	CHARX	16      // Character width (pixels)
#define	CHARY	20      // Character height (pixels)
#define	THIRD	7       // Height of a 'sixel'
#define	SEPSIZ	4       // Width/height of separated

void charttf(unsigned short ax, int col, SDL_Rect rect) ;

//Code conversion for special symbols:
static unsigned char frigo[] = {0x23,0x5B,0x5C,0x5D,0x5E,0x5F,0x60,0x7B,0x7C,0x7D,0x7E,0x7F} ;
static unsigned char frign[] = { 163, 143, 189, 144, 141,  35, 151, 188, 157, 190, 247, 129} ; // not 035
static unsigned short frigw[] = {0xA3,0x2190,0xBD,0x2192,0x2191,0x23,0x2014,0xBC,0x2016,0xBE,0xF7,0x25A0} ;

static int rgbtab[] = {
		0xFF000000,	// black
        	0xFFFF0000,	// blue
        	0xFF00FF00,	// green
        	0xFFFFFF00,	// cyan
        	0xFF0000FF,	// red
        	0xFFFF00FF,	// magenta
        	0xFF00FFFF,	// yellow
        	0xFFFFFFFF} ; 	// white

//In this table, bit significance is as follows:
//
//	 bit 7  - flashing      ) If ONE of bits 7-3
//       bit 6  - graphics      ) is set, then set the
//       bit 5  - separated     ) appropriate attribute,
//       bit 4  - double-height ) if FOUR bits are set,
//       bit 3  - release       ) then clear attribute.
//
//       bit 2  - red           ) If ANY of bits 0-2
//       bit 1  - green         ) are set, then use ALL
//       bit 0  - blue          ) three bits as colour.
//
static char ctab7[] = {
		0x00,	// 0b00000000: NUL (no effect)
		0xBC,	// 0b10111100: Alpha red
		0xBA,	// 0b10111010: Alpha green
		0xBE,	// 0b10111110: Alpha yellow
		0xB9,	// 0b10111001: Alpha blue
		0xBD,	// 0b10111101: Alpha magenta
		0xBB,	// 0b10111011: Alpha cyan
		0xBF,	// 0b10111111: Alpha white
		0x80, 	// 0b10000000: Flash
		0x78,	// 0b01111000: Steady
		0x00,	// 0b00000000: End box (no effect)
		0x00,	// 0b00000000: Start box (no effect)
		0xE8,	// 0b11101000: Normal height
		0x10,	// 0b00010000: Double height
		0x00,	// 0b00000000: SO (no effect)
		0x00,	// 0b00000000: SI (no effect)
		0x00,	// 0b00000000: DLE (no effect)
		0x44,	// 0b01000100: Graphics red
		0x42,	// 0b01000010: Graphics green
		0x46,	// 0b01000110: Graphics yellow
		0x41,	// 0b01000001: Graphics blue
		0x45,	// 0b01000101: Graphics magenta
		0x43,	// 0b01000011: Graphics cyan
		0x47,	// 0b01000111: Graphics white
		0x00,	// 0b00000000: Conceal
		0xD8,	// 0b11011000: Contiguous graphics
		0x20,	// 0b00100000: Separated graphics
		0x00,	// 0b00000000: ESC (no effect)
		0x00,	// 0b00000000: Black background
		0x00,	// 0b00000000: New background
		0xF0,	// 0b11110000: Hold graphics
		0x08} ;	// 0b00001000: Release graphics

// Check if row contains any double-height characters:
static int anydh (short *sl)
{
	int col ;
	for (col = 0; col < 40; col++)
		if ((*sl++ & 0x7F) == 13)
			return 1 ;
	return 0 ; 
}

static int find7 (short *sl)
{
	int row = 0 ;
	while (sl > chrmap)
	{
		sl -= (XSCREEN + 7) >> 3 ;
		if (!anydh (sl))
			break ;
		row-- ;
	}
	return (row & BIT0) ;
}

// Set render draw colour:
static void setrgb (char al)
{
	SDL_SetRenderDrawColor (memhdc, al & BIT2 ? 0xFF : 0,
					al & BIT1 ? 0xFF : 0,
					al & BIT0 ? 0xFF : 0, 0xFF ) ;
}

//Output a character to the screen (outputs both normal
//ASCII alphanumerics and 'sixel' graphics characters).
//  al = character:
//         bit 7 - 0=alpha, 1=graphics
//         if alpha:    bit 6..bit 0 is ASCII code
//         if graphics: bit 6 and bit 4..bit 0 are sixels
//                      bit 5 - 0=contiguous, 1=separated
//  ah = colour:
//         bit 6 - background R
//         bit 5 - background G
//         bit 4 - background B
//         bit 2 - foreground R
//         bit 1 - foreground G
//         bit 0 - foreground B
//  xpos = horizontal coordinate
//  ypos = vertical coordinate

static void chout7 (unsigned char al, unsigned char ah, int xpos, int ypos)
{
	SDL_Rect rect = {xpos, ypos, CHARX, CHARY} ;

	setrgb (ah >> 4) ;	// background colour
	SDL_RenderFillRect (memhdc, &rect) ;

	if ((al & 0x5F) == 0)
		al = ' ' ;

	if ((al & BIT7) == 0)
	{
		if (vflags & UFONT)
		    {
			int i ;
			Uint16 tmp[2] = {al, 0} ;
			for (i = 0; i < 12; i++)
				if (al == frigo[i])
					tmp[0] = frigw[i] ;
			TTF_SizeUNICODE (hfont, tmp, &rect.w, &rect.h) ;
			charttf (tmp[0], rgbtab[ah & 7], rect) ;
		    }
		else
		    {
			int i ;
			for (i = 0; i < 12; i++)
				if (al == frigo[i])
					al = frign[i] ;
			characterColor (memhdc, xpos, ypos, al, rgbtab[ah & 7]) ;
		    }
		return ;
	}

	setrgb (ah) ;
	signed char mask = 1 ;
	if (al & BIT5)
	{
		rect.x = xpos + SEPSIZ/2 ;
		rect.y = ypos + SEPSIZ/2 ;
		rect.w = SEPSIZ ;
		rect.h = SEPSIZ ;
	}
	else
	{
		rect.x = xpos ;
		rect.y = ypos ;
		rect.w = CHARX / 2 ;
		rect.h = THIRD ;
	}
	for ( ; mask >= 0; mask = mask << 1)
	{
		if (al & mask)
			SDL_RenderFillRect (memhdc, &rect) ;
		if (mask == 0x10)
			mask = mask << 1 ;
		rect.x += CHARX / 2 ;
		if (mask & 0x0A)
		{
			rect.x -= CHARX ;
			rect.y += THIRD ;
			if (mask & 8)
				rect.h &= ~BIT0 ;
		}
	}
}

//Subroutine outch7
static void outch7 (unsigned char al, unsigned char ah, unsigned char mode, int xpos, int ypos)
{
	if ((ah & BIT7) && (flags & PHASE))
		al = ' ' ;
	chout7 (al, ah, xpos, ypos) ;
	if ((mode & BIT5) && (al & 0x5F))
	{
		SDL_Rect src = {xpos, ypos, CHARX, CHARY/2} ;
		SDL_Rect dst = {xpos, ypos, CHARX, CHARY} ;
		SDL_Texture *tex, *target ;

		if (mode & BIT3)
			src.y += CHARY/2 ;	// bottom half
		tex = SDL_CreateTexture (memhdc, SDL_PIXELFORMAT_ABGR8888,
			SDL_TEXTUREACCESS_TARGET, CHARX, CHARY/2) ;
		target = SDL_GetRenderTarget (memhdc) ;
		SDL_SetRenderTarget (memhdc, tex) ;
		SDL_RenderClear (memhdc) ;
		SDL_RenderCopy (memhdc, target, &src, NULL) ;
		SDL_SetRenderTarget (memhdc, target) ;
		SDL_RenderCopy (memhdc, tex, NULL, &dst) ;
		SDL_DestroyTexture (tex) ;
	}
	bChanged = 1 ;
}

//Process a character from the character map
//and update the display attributes etc.
//
//   Inputs:  pc = address of character in character map
//          attr = current display attributes (&07 at row start):
//                 (n.b. 0s are SET AT, 1s are SET AFTER)
//                 bit 7 - 0=steady, 1=flashing
//                 bit 6 - background R
//                 bit 5 - background G
//                 bit 4 - background B
//                 bit 3 - 0=flash-update, 1=normal (unaffected)
//                 bit 2 - foreground R
//                 bit 1 - foreground G
//                 bit 0 - foreground B
//          mode = current mode (&10 at row start):
//                 bit 7 - 0=alphanumerics, 1=graphics
//                 bit 6 - 0=contiguous, 1=separated graphics
//                 bit 5 - 0=normal-height, 1=double-height
//                 bit 4 - 0=hold graphics, 1=release graphics
//                 bit 3 - 0=top half, 1=bottom half of pair
//                 bit 2 - 1=double-height changed (unaffected)
//                 bit 1 - 1=held-graphics changed (unaffected)
//                 bit 0 - 1=control-char changed (unaffected)
//          held = held graphics character
//
static short char7 (short *pc, unsigned char *pattr, unsigned char *pmode, char *pheld)
{
	unsigned char ah, al = *pc++ & ~BIT7 ;
	unsigned char attr = *pattr ;
	unsigned char mode = *pmode ;
	unsigned char oldmode = *pmode ;
	char held = *pheld ;

	if (al & BIT5)
	{				// Lowercase or graphics
		if (mode & BIT7)
		{			// Graphics
			al |= BIT7 ;
			if ((mode & BIT6) == 0)
				al &= ~BIT5 ;	// Contiguous
			held = al ;
		}
	}
	else if ((al & BIT6) == 0)	// Capitals or control
	{
		if ((mode & BIT7) == 0)	
			held = 0 ;	// Clear held graphics character

		switch (al)
		{
		case 24:		// Conceal
			attr = (attr & 0xF8) | ((attr & 0x70) >> 4) ;
			*pattr = attr ;	// Set at
			break ;

		case 28:		// Black background
			attr &= 0x8F ;
			*pattr = attr ;	// Set at
			break ;

		case 29:		// New background
			attr = (attr & 0x8F) | ((attr & 7) << 4) ;
			*pattr = attr ;	// Set at
			break ;

		default:
			al = ctab7[(int) al] ;		// Action code

			if ((al & 0xF8 & ((al & 0xF8) - 1)))	// Reset bit
			{
				mode &= ((al << 1) | 0x0F) ;
				attr &= (al | 0x7F) ;	// flashing
				*pmode = mode ;
				*pattr = attr ;		// Set AT
			}
			else
			{				// Set bit (or nothing)
				mode |= ((al << 1) & 0xF0) ; 
				attr |= (al & 0x80) ;	// flashing
			}

			if (al & 7)			// Foreground colour
				attr = (attr & 0xF8) | (al & 7) ;
		}

		if ((mode ^ oldmode) & BIT5)
			held = 0 ;		// Clear held graphic on D/H change

		if (held)
			al = held | 0x80 ;	// Display held graphic
		else
			al = ' ' ;		// Display space

		if (*pmode & BIT4)
		{
			held = 0 ;		// Clear held graphic on release
			al = ' ' ;
		}
	}

	if ((mode & BIT3) && !(mode & BIT5))
		al = ' ' ;			// Second row of D/H pair

	ah = *pattr ;
	*pattr = attr ;
	*pmode = mode ;
	*pheld = held ;
	return (ah << 8) | al ;
}

// Process next row:
static void next7 (short *sl, int ypos, unsigned char *pattr,  unsigned char *pmode)
{
	char held = 0 ;
	int col, xpos = 0 ;
	for (col = 0; col < 40; col++)
	{
		short atch = char7 (sl, pattr, pmode, &held) ;
		if ((*pmode & (BIT0 | BIT2)) || ((*pmode & BIT1) && !(*pmode & BIT4)))
			outch7 (atch & 0xFF, atch >> 8, *pmode, xpos, ypos) ;
		sl++ ;
		xpos += CHARX ;
	}
}

//Process one (40-character) row of the Viewdata screen.
static void row7 (short *pc, short *sl, int ypos, unsigned char *pattr,  unsigned char *pmode)
{
	char held = 0 ;
	int col, xpos = 0 ;
	for (col = 0; col < 40; col++)
	{
		short atch = char7 (sl, pattr, pmode, &held) ;
		if (sl == pc)
		{
			outch7 (atch & 0xFF, atch >> 8, *pmode, xpos, ypos) ;
			if ((*pmode & (BIT0 | BIT1 | BIT2)) == 0)
			return ;
		}
		if ((sl > pc) && ((*pmode & (BIT0 | BIT2))
				 || ((*pmode & BIT1) && !(*pmode & BIT4))))
			outch7 (atch & 0xFF, atch >> 8, *pmode, xpos, ypos) ;
		sl++ ;
		xpos += CHARX ;
	}
}

// Write a character to the teletext screen and update the
// screen as appropriate.
//
// The rules are as follows:
// 1. If the character written is the same as the previous
//    character at that location, then do nothing.
// 2. If the character written *or the character it replaces*
//    is a control character, re-display the rest of the row.
// 3. If the character written *or the character it replaces*
//    could be a held-graphics character, re-display any held
//    graphics characters in the rest of the row.
// 4. If the character written *or the character it replaces*
//    is a double-height control character, re-display double
//    height characters in the next and subsequent rows until
//    one is found which doesn't contain a D/H control (n.b.
//    display single-height characters too - as spaces - if
//    the row is the second row of a double-height pair).
void send7 (unsigned char al, short *pc, short *sl, int ypos)
{
	unsigned char ah, attr, mode ;
	switch (al)
	{
	case 0x23:	al = 0x5F ;
			break ;

	case 0x5F:	al = 0x60 ;
			break ;

	case 0x60:	al = 0x23 ;
	}

	ah = *pc ;
	if (al == ah)
		return ;
	*pc = al ;

	attr = 7 ;
	mode = 0x10 ;

// Test new and old characters:
	al &= ~BIT7 ;			// Strip high bits
	ah &= ~BIT7 ;
	if ((al < 0x20) || (ah < 0x20))
		mode |= BIT0 ;		// Flag control character
	if ((al & BIT5) || (ah & BIT5))
		mode |= BIT1 ;		// Could be held-graphics
	if ((al == 13) || (ah == 13))
		mode |= BIT2 ;		// Flag double-height
	if (find7 (sl))
		mode |= BIT3 ;		// Flag second of pair
	row7 (pc, sl, ypos, &attr, &mode) ;

	if ((mode & BIT2) == 0)		// Double-height changed?
		return ;
	if (!anydh (sl))
		mode |= BIT3 ;

	while (sl <= (chrmap + 24 * ((XSCREEN + 7) >> 3)))
	{
		sl += (XSCREEN + 7) >> 3 ;
		ypos += CHARY ;
		attr = 7 ;
		mode = (mode & 8) ^ 0x1C ;
		next7 (sl, ypos, &attr, &mode) ;
		if (!anydh (sl))
			return ;
	}
}

// Update the entire Viewdata screen
static void update7 (unsigned char attr)
{
	int row, ypos = 0 ;
	short *pc = chrmap ;
	unsigned char mode = 0x10 ;		// Initial mode

	for (row = 0; row < 25; row++)
	{
		int col, xpos = 0 ;
		char held = 0 ;		// Held graphics character
		unsigned char sticky = 0 ;	// To detect any D/H
		attr = (attr & 8) | 7 ; // Initial attributes
		for (col = 0; col < 40; col++)
		{
			short atch = char7 (pc, &attr, &mode, &held) ;
			sticky |= mode ;
			if (atch & 0x8800)
				outch7 (atch & 0xFF, atch >> 8, mode, xpos, ypos) ; 
			pc++ ;
			xpos += CHARX ;
		}
		pc += ((XSCREEN + 7) >> 3) - 40 ;
		ypos += CHARY ;
		mode = (mode & 8) ^ 0x18 ;
		if ((sticky & BIT5) == 0)
			mode = 0x10 ;
	}
}

// Flip the state of flashing characters:
void flip7 (void)
{
	if (modeno == 7)
	{
		flags ^= PHASE ;
		update7 (0) ;		// Flash update only
	}
}

// Update the entire MODE7 screen:
void page7 (void)
{
	update7 (8) ;			// Full update
}