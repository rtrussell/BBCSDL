/*****************************************************************\
*       32-bit BBC BASIC Interpreter                              *
*       (c) 2018-2020  R.T.Russell  http://www.rtrussell.co.uk/   *
*                                                                 *
*       bbasmb.c: Simple ARM 4 assembler                          *
*       Version 1.15a, 09-Sep-2020                                *
\*****************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <emscripten.h>
#include "SDL2/SDL.h"
#include "BBC.h"
#include "SDL2_gfxPrimitives.h"

typedef size_t st ;
typedef double db ;
SDL_Surface* STBIMG_Load(const char*) ;

// External routines:
void error (int, const char *) ;

void assemble (void)
{
	error (255, "Assembler code is not supported in WebAssembly") ;
}

// Wrapper functions to match SYS signature:

// Anti-aliased graphics (used by aagfxlib.bbc):

long long WASM_RenderSetClipRect(st renderer, st rect, st i2, st i3, st i4, st i5, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ return SDL_RenderSetClipRect((SDL_Renderer*) renderer, (const SDL_Rect*) rect); }

long long BBC_bezierColor(st renderer, st vx, st vy, st n, st s, st color, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ return bezierColor((SDL_Renderer*) renderer, (const Sint16*) vx, (const Sint16*) vy, n, s, color); }

long long BBC_aaFilledEllipseColor(st renderer, st i1, st i2, st i3, st i4, st color, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
{
	float cx = *(float *)&i1 ; float cy = *(float *)&i2 ;
	float rx = *(float *)&i3 ; float ry = *(float *)&i4 ;
	return aaFilledEllipseColor((SDL_Renderer*) renderer, cx, cy, rx, ry, color);
}

long long BBC_aaFilledPolygonColor(st renderer, st vx, st vy, st n, st color, st i5, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ return aaFilledPolygonColor((SDL_Renderer*) renderer, (const double*) vx, (const double*) vy, n, color); }

long long BBC_aaFilledPieColor(st renderer, st i1, st i2, st i3, st i4, st i5, st i6, st chord,
	  st color, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
{
	float cx = *(float *)&i1 ; float cy = *(float *)&i2 ;
	float rx = *(float *)&i3 ; float ry = *(float *)&i4 ;
	float start = *(float *)&i5 ; float end = *(float *)&i6 ;
	return aaFilledPieColor((SDL_Renderer*) renderer, cx, cy, rx, ry, start, end, chord, color);
}

long long BBC_aaArcColor(st renderer, st i1, st i2, st i3, st i4, st i5, st i6, st i7,
	  st color, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
{
	float cx = *(float *)&i1 ; float cy = *(float *)&i2 ;
	float rx = *(float *)&i3 ; float ry = *(float *)&i4 ;
	float start = *(float *)&i5 ; float end = *(float *)&i6 ; float thick = *(float *)&i7 ;
	return aaArcColor((SDL_Renderer*) renderer, cx, cy, rx, ry, start, end, thick, color);
}

long long BBC_aaBezierColor(st renderer, st x, st y, st n, st s, st i5, st color, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
{
	float thick = *(float *)&i5 ;
	return aaBezierColor((SDL_Renderer*) renderer, (double *) x, (double *) y, n, s, thick, color);
}

long long BBC_aaFilledPolyBezierColor(st renderer, st x, st y, st n, st s, st color, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ return aaFilledPolyBezierColor((SDL_Renderer*) renderer, (double *) x, (double *) y, n, s, color); }

// Sprites (used by imglib.bbc):

long long BBC_CreateTextureFromSurface(st renderer, st surface, st i2, st i3, st i4, st i5, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ return (intptr_t) SDL_CreateTextureFromSurface((SDL_Renderer *)renderer, (SDL_Surface *) surface); }

long long BBC_DestroyTexture(st texture, st i1, st i2, st i3, st i4, st i5, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ SDL_DestroyTexture((SDL_Texture*) texture); return 0; }

long long BBC_FreeSurface(st surface, st i1, st i2, st i3, st i4, st i5, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ SDL_FreeSurface((SDL_Surface*) surface); return 0; }

long long BBC_QueryTexture(st texture, st format, st access, st w, st h, st i5, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ return SDL_QueryTexture((SDL_Texture*) texture, (Uint32*) format, (int*) access, (int*) w, (int*) h); }

long long BBC_RenderCopyEx(st renderer, st texture, st srcrect, st dstrect, st anglo, st anghi, st center, st flip,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
{
	long long anglei = ((long long)anghi << 32) | anglo;
	double angle = *(double *)&anglei;
	return SDL_RenderCopyEx((SDL_Renderer *)renderer, (SDL_Texture*) texture, (const SDL_Rect*) srcrect, (const SDL_Rect*) dstrect, (const double) angle, (const SDL_Point*) center, (const SDL_RendererFlip) flip);
}

long long BBC_RenderCopyExF(st renderer, st texture, st srcrect, st dstrect, st anglo, st anghi, st center, st flip,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
{
	long long anglei = ((long long)anghi << 32) | anglo;
	double angle = *(double *)&anglei;
	return SDL_RenderCopyExF((SDL_Renderer *)renderer, (SDL_Texture*) texture, (const SDL_Rect*) srcrect, (const SDL_FRect*) dstrect, (const double) angle, (const SDL_FPoint*) center, (const SDL_RendererFlip) flip);
}

long long BBC_SetTextureAlphaMod(st texture, st alpha, st i2, st i3, st i4, st i5, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ return SDL_SetTextureAlphaMod((SDL_Texture*) texture, alpha); }

long long BBC_SetTextureBlendMode(st texture, st blendmode, st i2, st i3, st i4, st i5, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ return SDL_SetTextureBlendMode((SDL_Texture*) texture, (SDL_BlendMode) blendmode); }

long long BBC_SetTextureColorMod(st texture, st r, st g, st b, st i4, st i5, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ return SDL_SetTextureColorMod((SDL_Texture*) texture, r, g, b); }

long long BBC_STBIMG_Load(st file, st i1, st i2, st i3, st i4, st i5, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ return (intptr_t) STBIMG_Load((const char*) file); }

// Miscellaneous:

long long BBC_SetHint(st name, st value, st i2, st i3, st i4, st i5, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ return SDL_SetHint((const char*) name, (const char*) value); }

long long BBC_AddTimer(st interval, st callback, st param, st i3, st i4, st i5, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ return SDL_AddTimer(interval, (SDL_TimerCallback) callback, (void *) param); }

long long BBC_RemoveTimer(st id, st i1, st i2, st i3, st i4, st i5, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ return SDL_RemoveTimer(id); }


// Emscripten:

void wget_report(const char *message)
{
	SDL_Log("emscripten_async_wget: %s\r\n", message);
}

long long BBC_emscripten_async_wget(st url, st file, st i2, st i3, st i4, st i5, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
{
	emscripten_async_wget((const char*) url, (const char*) file, wget_report, wget_report);
	return 0 ;
}

#define NSYS 22
#define POW2 32 // smallest power-of-2 >= NSYS

static char *sysname[NSYS] = {
	"GFX_aaArcColor",
	"GFX_aaBezierColor",
	"GFX_aaFilledEllipseColor",
	"GFX_aaFilledPieColor",
	"GFX_aaFilledPolyBezierColor",
	"GFX_aaFilledPolygonColor",
	"GFX_bezierColor",
	"SDL_AddTimer",
	"SDL_CreateTextureFromSurface",
	"SDL_DestroyTexture",
	"SDL_FreeSurface",
	"SDL_QueryTexture",
	"SDL_RemoveTimer",
	"SDL_RenderCopyEx",
	"SDL_RenderCopyExF",
	"SDL_RenderSetClipRect",
	"SDL_SetHint",
	"SDL_SetTextureAlphaMod",
	"SDL_SetTextureBlendMode",
	"SDL_SetTextureColorMod",
	"STBIMG_Load",
	"emscripten_async_wget"} ;

static void *sysfunc[NSYS] = {
	BBC_aaArcColor,
	BBC_aaBezierColor,
	BBC_aaFilledEllipseColor,
	BBC_aaFilledPieColor,
	BBC_aaFilledPolyBezierColor,
	BBC_aaFilledPolygonColor,
	BBC_bezierColor,
	BBC_AddTimer,
	BBC_CreateTextureFromSurface,
	BBC_DestroyTexture,
	BBC_FreeSurface,
	BBC_QueryTexture,
	BBC_RemoveTimer,
	BBC_RenderCopyEx,
	BBC_RenderCopyExF,
	WASM_RenderSetClipRect,
	BBC_SetHint,
	BBC_SetTextureAlphaMod,
	BBC_SetTextureBlendMode,
	BBC_SetTextureColorMod,
	BBC_STBIMG_Load,
	BBC_emscripten_async_wget} ;

void *dlsym (void *handle, const char *symbol)
{
	int b = 0, h = POW2, r = 0 ;
	do
	    {
		h /= 2 ;
		if (((b + h) < NSYS) && ((r = strcmp (symbol, sysname[b + h])) >= 0))
			b += h ;
	    }
	while (h) ;
	if (r == 0) return sysfunc[b] ;
	return NULL ;
}