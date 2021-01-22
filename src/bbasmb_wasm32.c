/*****************************************************************\
*       32-bit BBC BASIC Interpreter (Emscripten / Web Assembly)  *
*       (c) 2018-2021  R.T.Russell  http://www.rtrussell.co.uk/   *
*                                                                 *
*       The name 'BBC BASIC' is the property of the British       *
*       Broadcasting Corporation and used with their permission   *
*                                                                 *
*       bbasmb.c: API Wrappers to satisfy function signatures     *
*       Version 1.19a, 10-Jan-2021                                *
\*****************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <emscripten.h>
#include <GLES2/gl2.h>
#include "SDL2/SDL.h"
#include "SDL_ttf.h"
#include "SDL_net.h"
#include "BBC.h"
#include "SDL2_gfxPrimitives.h"

typedef size_t st ;
typedef double db ;
SDL_Surface* STBIMG_Load(const char*) ;
SDL_Texture* STBIMG_LoadTexture(SDL_Renderer* renderer, const char* file) ;

typedef struct
{
    unsigned int outputChannels;
    unsigned int outputSampleRate;
} drmp3_config;

// External routines:
void error (int, const char *) ;
void stbi_set_flip_vertically_on_load(int) ;
float* drmp3_open_file_and_read_f32 (const char*, drmp3_config*, uint64_t *) ;
void drmp3dec_f32_to_s16 (const float*, int16_t *, int) ;
void drmp3_free (void*) ;
long long B2D_GetProcAddress (st, st, st, st, st, st, st, st, st, st, st, st, db, db, db, db, db, db, db, db) ;

// Global variables:
extern int bYield ;

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

long long BBC_filledPolyBezierColor(st renderer, st vx, st vy, st n, st s, st color, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ return filledPolyBezierColor((SDL_Renderer*) renderer, (const Sint16*) vx, (const Sint16*) vy, n, s, color); }

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

// 2D Surfaces and Textures (e.g. used by imglib.bbc):

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

long long BBC_SetColorKey(st surface, st flag, st key, st i3, st i4, st i5, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ return SDL_SetColorKey((SDL_Surface*) surface, flag, key); }

long long BBC_CreateTexture(st renderer, st format, st access, st w, st h, st i5, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ return (intptr_t) SDL_CreateTexture((SDL_Renderer*) renderer, format, access, w, h); }

long long BBC_GetRenderTarget(st renderer, st i1, st i2, st i3, st i4, st i5, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ return (intptr_t) SDL_GetRenderTarget((SDL_Renderer*) renderer); }

long long BBC_SetRenderTarget(st renderer, st texture, st i2, st i3, st i4, st i5, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ return SDL_SetRenderTarget((SDL_Renderer*) renderer, (SDL_Texture*) texture); }

long long BBC_RenderCopy(st renderer, st texture, st srcrect, st dstrect, st i4, st i5, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ return SDL_RenderCopy((SDL_Renderer*) renderer, (SDL_Texture*) texture, (const SDL_Rect*) srcrect, (const SDL_Rect*) dstrect); }

long long BBC_SetRenderDrawBlendMode(st renderer, st mode, st i2, st i3, st i4, st i5, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ return SDL_SetRenderDrawBlendMode((SDL_Renderer*) renderer, mode); }

long long BBC_SetRenderDrawColor(st renderer, st r, st g, st b, st a, st i5, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ return SDL_SetRenderDrawColor((SDL_Renderer*) renderer, r, g, b, a); }

long long BBC_RenderFillRect(st renderer, st rect, st i2, st i3, st i4, st i5, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ return SDL_RenderFillRect((SDL_Renderer*) renderer, (const SDL_Rect*) rect); }

long long BBC_RenderFillRects(st renderer, st rects, st count, st i3, st i4, st i5, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ return SDL_RenderFillRects((SDL_Renderer*) renderer, (const SDL_Rect*) rects, count); }

long long BBC_RenderDrawPoint(st renderer, st x, st y, st i3, st i4, st i5, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ return SDL_RenderDrawPoint((SDL_Renderer*) renderer, x, y); }

long long BBC_RenderDrawPoints(st renderer, st points, st count, st i3, st i4, st i5, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ return SDL_RenderDrawPoints((SDL_Renderer*) renderer, (const SDL_Point*) points, count); }

long long BBC_RenderClear(st renderer, st i1, st i2, st i3, st i4, st i5, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ return SDL_RenderClear((SDL_Renderer*) renderer); }

long long BBC_LockTexture(st texture, st rect, st pixels, st pitch, st i4, st i5, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ return SDL_LockTexture((SDL_Texture*) texture, (const SDL_Rect*) rect, (void**) pixels, (int*) pitch); }

long long BBC_UnlockTexture(st texture, st i1, st i2, st i3, st i4, st i5, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ SDL_UnlockTexture((SDL_Texture*) texture); return 0; }

long long BBC_ConvertSurfaceFormat(st surf, st pixel_format, st flgs, st i3, st i4, st i5, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ return (intptr_t) SDL_ConvertSurfaceFormat((SDL_Surface*) surf, pixel_format, flgs); }

long long BBC_ComposeCustomBlendMode(st srcColor, st dstColor, st colorOp, st srcAlpha, st dstAlpha, st alphaOp, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ return (intptr_t) SDL_ComposeCustomBlendMode(srcColor, dstColor, colorOp, srcAlpha, dstAlpha, alphaOp); }

long long BBC_GetDisplayUsableBounds(st displayIndex, st rect, st i2, st i3, st i4, st i5, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ return SDL_GetDisplayUsableBounds(displayIndex, (SDL_Rect*) rect); }

long long BBC_RenderDrawLine(st renderer, st x1, st y1, st x2, st y2, st i5, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ return SDL_RenderDrawLine((SDL_Renderer*) renderer, x1, y1, x2, y2); }

long long BBC_RenderDrawLines(st renderer, st points, st count, st i3, st i4, st i5, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ return SDL_RenderDrawLines((SDL_Renderer*) renderer, (const SDL_Point*) points, count); }

long long WASM_RenderReadPixels(st renderer, st rect, st format, st pixels, st pitch, st i5, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ return SDL_RenderReadPixels((SDL_Renderer*) renderer, (const SDL_Rect*) rect, format, (void*) pixels, pitch); }

long long BBC_CreateRGBSurface(st flgs, st width, st height, st depth, st Rmask, st Gmask, st Bmask, st Amask,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ return (intptr_t) SDL_CreateRGBSurface(flgs, width, height, depth, Rmask, Gmask, Bmask, Amask); }

long long BBC_SetSurfaceAlphaMod(st surface, st alpha, st i2, st i3, st i4, st i5, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ return SDL_SetSurfaceAlphaMod((SDL_Surface*) surface, alpha); }

long long BBC_SetSurfaceColorMod(st surface, st r, st g, st b, st i4, st i5, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ return SDL_SetSurfaceColorMod((SDL_Surface*) surface, r, g, b); }

long long BBC_UpperBlit(st src, st srcrect, st dst, st dstrect, st i4, st i5, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ return SDL_UpperBlit((SDL_Surface*) src, (const SDL_Rect*) srcrect, (SDL_Surface*) dst, (SDL_Rect*) dstrect); }

long long BBC_FillRect(st surface, st rect, st color, st i3, st i4, st i5, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ return SDL_FillRect((SDL_Surface*) surface, (const SDL_Rect*) rect, color); }

long long BBC_STBIMG_Load(st file, st i1, st i2, st i3, st i4, st i5, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ return (intptr_t) STBIMG_Load((const char*) file); }

long long BBC_STBIMG_LoadTexture(st renderer, st file, st i2, st i3, st i4, st i5, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ return (intptr_t) STBIMG_LoadTexture((SDL_Renderer*) renderer, (const char*) file); }

long long BBC_LoadBMP_RW(st src, st freesrc, st i2, st i3, st i4, st i5, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ return (intptr_t) SDL_LoadBMP_RW((SDL_RWops*) src, freesrc); }

long long BBC_SetPaletteColors(st palette, st colors, st first, st ncolors, st i4, st i5, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ return SDL_SetPaletteColors((SDL_Palette*) palette, (const SDL_Color*) colors, first, ncolors); }

long long BBC_stbi_set_flip_vertically_on_load(st flip, st i1, st i2, st i3, st i4, st i5, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ stbi_set_flip_vertically_on_load(flip); return 0; }

// 3D (OpenGL) graphics:

long long BBC_GL_CreateContext(st window, st i1, st i2, st i3, st i4, st i5, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ return (intptr_t) SDL_GL_CreateContext((SDL_Window*) window); }

long long BBC_GL_DeleteContext(st context, st i1, st i2, st i3, st i4, st i5, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ SDL_GL_DeleteContext((SDL_GLContext) context); return 0; }

long long BBC_GL_GetCurrentContext(st i0, st i1, st i2, st i3, st i4, st i5, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ return (intptr_t) SDL_GL_GetCurrentContext(); }

long long BBC_GL_GetDrawableSize(st window, st pw, st ph, st i3, st i4, st i5, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ SDL_GL_GetDrawableSize((SDL_Window*) window, (int*) pw, (int*) ph); return 0; }

long long BBC_GL_GetProcAddress(st, st, st, st, st, st, st, st, st, st, st, st, db, db, db, db, db, db, db, db) ;

long long BBC_GL_MakeCurrent(st window, st context, st i2, st i3, st i4, st i5, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ return SDL_GL_MakeCurrent((SDL_Window*) window, (SDL_GLContext) context); }

long long BBC_GL_SetAttribute(st attr, st value, st i2, st i3, st i4, st i5, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ return SDL_GL_SetAttribute(attr, value); }

long long BBC_GL_SetSwapInterval(st interval, st i1, st i2, st i3, st i4, st i5, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ return SDL_GL_SetSwapInterval(interval); }

long long BBC_GL_SwapWindow(st window, st i1, st i2, st i3, st i4, st i5, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ SDL_GL_SwapWindow((SDL_Window*) window); bYield = 1; return 0; }

// Audio:

long long BBC_OpenAudioDevice(st device, st iscapture, st desired, st obtained, st allow, st i5, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ return SDL_OpenAudioDevice((const char*) device, iscapture, (const SDL_AudioSpec*) desired, (SDL_AudioSpec*)obtained, allow); }

long long BBC_CloseAudioDevice(st device, st i1, st i2, st i3, st i4, st i5, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ SDL_CloseAudioDevice(device); return 0; }

long long BBC_QueueAudio(st device, st data, st len, st i3, st i4, st i5, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ return SDL_QueueAudio(device, (const void*) data, len); }

long long BBC_ClearQueuedAudio(st device, st i1, st i2, st i3, st i4, st i5, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ SDL_ClearQueuedAudio(device); return 0; }

long long BBC_GetQueuedAudioSize(st device, st i1, st i2, st i3, st i4, st i5, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ return SDL_GetQueuedAudioSize(device); }

long long BBC_MixAudioFormat(st dst, st src, st format, st len, st volume, st i5, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ SDL_MixAudioFormat((void*) dst, (const void*) src, format, len, volume); return 0; }

long long BBC_LockAudioDevice(st device, st i1, st i2, st i3, st i4, st i5, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ SDL_LockAudioDevice(device); return 0; }

long long BBC_UnlockAudioDevice(st device, st i1, st i2, st i3, st i4, st i5, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ SDL_UnlockAudioDevice(device); return 0; }

long long BBC_PauseAudioDevice(st device, st pause, st i2, st i3, st i4, st i5, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ SDL_PauseAudioDevice(device, pause); return 0; }

long long BBC_drmp3_open_file_and_read_f32(st path, st config, st total, st i3, st i4, st i5, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ return (intptr_t) drmp3_open_file_and_read_f32((const char*) path, (drmp3_config*) config, (uint64_t*) total); }

long long BBC_drmp3dec_f32_to_s16(st in, st out, st num, st i3, st i4, st i5, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ drmp3dec_f32_to_s16((const float*) in, (int16_t *) out, num); return 0; }

long long BBC_drmp3_free(st ptr, st i1, st i2, st i3, st i4, st i5, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ drmp3_free((void*) ptr); return 0; }

long long BBC_LoadWAV_RW(st src, st freesrc, st spec, st audio_buf, st audio_len, st i5, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ return (intptr_t) SDL_LoadWAV_RW((SDL_RWops*) src, freesrc, (SDL_AudioSpec*) spec, (Uint8**) audio_buf, (Uint32*) audio_len); }

long long BBC_FreeWAV(st audio_buf, st i1, st i2, st i3, st i4, st i5, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ SDL_FreeWAV((Uint8*) audio_buf); return 0; }

// Time-related functions:

long long BBC_asctime(st tm, st i1, st i2, st i3, st i4, st i5, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ return (intptr_t) asctime((const struct tm*) tm); }

long long BBC_gmtime(st timep, st i1, st i2, st i3, st i4, st i5, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ return (intptr_t) gmtime((const time_t*) timep); }

long long BBC_localtime(st timep, st i1, st i2, st i3, st i4, st i5, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ return (intptr_t) localtime((const time_t*) timep); }

long long BBC_mktime(st tm, st i1, st i2, st i3, st i4, st i5, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ return (intptr_t) mktime((struct tm*) tm); }

long long BBC_time(st tloc, st i1, st i2, st i3, st i4, st i5, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ return (intptr_t) time((time_t*) tloc); }

long long BBC_GetTicks(st i0, st i1, st i2, st i3, st i4, st i5, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ return SDL_GetTicks(); }

long long BBC_AddTimer(st interval, st callback, st param, st i3, st i4, st i5, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ return SDL_AddTimer(interval, (SDL_TimerCallback) callback, (void *) param); }

long long BBC_RemoveTimer(st id, st i1, st i2, st i3, st i4, st i5, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ return SDL_RemoveTimer(id); }

// Miscellaneous:

long long BBC_SetHint(st name, st value, st i2, st i3, st i4, st i5, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ return SDL_SetHint((const char*) name, (const char*) value); }

long long BBC_GetWindowFlags(st window, st i1, st i2, st i3, st i4, st i5, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ return SDL_GetWindowFlags((SDL_Window*) window); }

long long BBC_SetWindowResizable(st window, st resizable, st i2, st i3, st i4, st i5, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ SDL_SetWindowResizable((SDL_Window*) window, resizable); return 0; }

long long BBC_SetWindowTitle(st window, st title, st i2, st i3, st i4, st i5, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ SDL_SetWindowTitle((SDL_Window*) window, (const char*) title); return 0; }

long long BBC_RWFromMem(st mem, st size, st i2, st i3, st i4, st i5, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ return (intptr_t) SDL_RWFromMem((void*) mem, size); }

long long BBC_RWFromFile(st file, st mode, st i2, st i3, st i4, st i5, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ return (intptr_t) SDL_RWFromFile((const char*) file, (const char*) mode); }

long long BBC_ShowSimpleMessageBox(st flgs, st title, st message, st window, st i4, st i5, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ return SDL_ShowSimpleMessageBox(flgs, (const char*) title, (const char*) message, (SDL_Window*) window); }

long long BBC_malloc(st size, st i1, st i2, st i3, st i4, st i5, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ return (intptr_t) SDL_malloc(size); }

long long BBC_free(st ptr, st i1, st i2, st i3, st i4, st i5, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ SDL_free((void*) ptr); return 0; }

long long BBC_memcpy(st dest, st src, st n, st i3, st i4, st i5, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ return (intptr_t) SDL_memcpy((void*) dest, (void*) src, n); }

long long BBC_memset(st dest, st c, st n, st i3, st i4, st i5, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ return (intptr_t) SDL_memset((void*) dest, c, n); }

long long BBC_TTF_Linked_Version(st i0, st i1, st i2, st i3, st i4, st i5, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ return (intptr_t) TTF_Linked_Version(); }

// Networking (web sockets)

long long BBC_Net_AddSocket(st set, st sock, st i2, st i3, st i4, st i5, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ return SDLNet_AddSocket((SDLNet_SocketSet) set, (SDLNet_GenericSocket) sock); }

long long BBC_Net_AllocSocketSet(st maxsockets, st i1, st i2, st i3, st i4, st i5, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ return (intptr_t) SDLNet_AllocSocketSet(maxsockets); }

long long BBC_Net_CheckSockets(st set, st timeout, st i2, st i3, st i4, st i5, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ return SDLNet_CheckSockets((SDLNet_SocketSet) set, timeout); }

long long BBC_Net_DelSocket(st set, st sock, st i2, st i3, st i4, st i5, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ return SDLNet_DelSocket((SDLNet_SocketSet) set, (SDLNet_GenericSocket) sock); }

long long BBC_Net_FreeSocketSet(st set, st i1, st i2, st i3, st i4, st i5, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ SDLNet_FreeSocketSet((SDLNet_SocketSet) set); return 0; }

long long BBC_Net_GetError(st i0, st i1, st i2, st i3, st i4, st i5, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ return (intptr_t) SDLNet_GetError(); }

long long BBC_Net_Linked_Version(st i0, st i1, st i2, st i3, st i4, st i5, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ return (intptr_t) SDLNet_Linked_Version(); }

long long BBC_Net_ResolveHost(st addr, st host, st port, st i3, st i4, st i5, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ return SDLNet_ResolveHost((IPaddress*) addr, (const char*) host, port); }

long long BBC_Net_ResolveIP(st addr, st i1, st i2, st i3, st i4, st i5, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ return (intptr_t) SDLNet_ResolveIP((IPaddress*) addr); }

long long BBC_Net_TCP_Accept(st server, st i1, st i2, st i3, st i4, st i5, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ return (intptr_t) SDLNet_TCP_Accept((TCPsocket) server); }

long long BBC_Net_TCP_Close(st sock, st i1, st i2, st i3, st i4, st i5, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ SDLNet_TCP_Close((TCPsocket) sock); return 0; }

long long BBC_Net_TCP_GetPeerAddress(st sock, st i1, st i2, st i3, st i4, st i5, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ return (intptr_t) SDLNet_TCP_GetPeerAddress((TCPsocket) sock); }

long long BBC_Net_TCP_Open(st ip, st i1, st i2, st i3, st i4, st i5, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ return (intptr_t) SDLNet_TCP_Open((IPaddress*) ip); }

long long BBC_Net_TCP_Recv(st sock, st data, st maxlen, st i3, st i4, st i5, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ return SDLNet_TCP_Recv((TCPsocket) sock, (void*) data, maxlen); }

long long BBC_Net_TCP_Send(st sock, st data, st len, st i3, st i4, st i5, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ return SDLNet_TCP_Send((TCPsocket) sock, (const void*) data, len); }

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

#define NSYS 108
#define POW2 128 // smallest power-of-2 >= NSYS

static const char *sysname[NSYS] = {
	"B2D_GetProcAddress",
	"GFX_aaArcColor",
	"GFX_aaBezierColor",
	"GFX_aaFilledEllipseColor",
	"GFX_aaFilledPieColor",
	"GFX_aaFilledPolyBezierColor",
	"GFX_aaFilledPolygonColor",
	"GFX_bezierColor",
	"GFX_filledPolyBezierColor",
	"SDLNet_AddSocket",
	"SDLNet_AllocSocketSet",
	"SDLNet_CheckSockets",
	"SDLNet_DelSocket",
	"SDLNet_FreeSocketSet",
	"SDLNet_GetError",
	"SDLNet_Linked_Version",
	"SDLNet_ResolveHost",
	"SDLNet_ResolveIP",
	"SDLNet_TCP_Accept",
	"SDLNet_TCP_Close",
	"SDLNet_TCP_GetPeerAddress",
	"SDLNet_TCP_Open",
	"SDLNet_TCP_Recv",
	"SDLNet_TCP_Send",
	"SDL_AddTimer",
	"SDL_ClearQueuedAudio",
	"SDL_CloseAudioDevice",
	"SDL_ComposeCustomBlendMode",
	"SDL_ConvertSurfaceFormat",
	"SDL_CreateRGBSurface",
	"SDL_CreateTexture",
	"SDL_CreateTextureFromSurface",
	"SDL_DestroyTexture",
	"SDL_FillRect",
	"SDL_FreeSurface",
	"SDL_FreeWAV",
	"SDL_GL_CreateContext",
	"SDL_GL_DeleteContext",
	"SDL_GL_GetCurrentContext",
	"SDL_GL_GetDrawableSize",
	"SDL_GL_GetProcAddress",
	"SDL_GL_MakeCurrent",
	"SDL_GL_SetAttribute",
	"SDL_GL_SetSwapInterval",
	"SDL_GL_SwapWindow",
	"SDL_GetDisplayUsableBounds",
	"SDL_GetQueuedAudioSize",
	"SDL_GetRenderTarget",
	"SDL_GetTicks",
	"SDL_GetWindowFlags",
	"SDL_LoadBMP_RW",
	"SDL_LoadWAV_RW",
	"SDL_LockAudioDevice",
	"SDL_LockTexture",
	"SDL_MixAudioFormat",
	"SDL_OpenAudioDevice",
	"SDL_PauseAudioDevice",
	"SDL_QueryTexture",
	"SDL_QueueAudio",
	"SDL_RWFromFile",
	"SDL_RWFromMem",
	"SDL_RemoveTimer",
	"SDL_RenderClear",
	"SDL_RenderCopy",
	"SDL_RenderCopyEx",
	"SDL_RenderCopyExF",
	"SDL_RenderDrawLine",
	"SDL_RenderDrawLines",
	"SDL_RenderDrawPoint",
	"SDL_RenderDrawPoints",
	"SDL_RenderFillRect",
	"SDL_RenderFillRects",
	"SDL_RenderReadPixels",
	"SDL_RenderSetClipRect",
	"SDL_SetColorKey",
	"SDL_SetHint",
	"SDL_SetPaletteColors",
	"SDL_SetRenderDrawBlendMode",
	"SDL_SetRenderDrawColor",
	"SDL_SetRenderTarget",
	"SDL_SetSurfaceAlphaMod",
	"SDL_SetSurfaceColorMod",
	"SDL_SetTextureAlphaMod",
	"SDL_SetTextureBlendMode",
	"SDL_SetTextureColorMod",
	"SDL_SetWindowResizable",
	"SDL_SetWindowTitle",
	"SDL_ShowSimpleMessageBox",
	"SDL_UnlockAudioDevice",
	"SDL_UnlockTexture",
	"SDL_UpperBlit",
	"SDL_free",
	"SDL_malloc",
	"SDL_memcpy",
	"SDL_memset",
	"STBIMG_Load",
	"STBIMG_LoadTexture",
	"TTF_Linked_Version",
	"asctime",
	"drmp3_free",
	"drmp3_open_file_and_read_f32",
	"drmp3dec_f32_to_s16",
	"emscripten_async_wget",
	"gmtime",
	"localtime",
	"mktime",
	"stbi_set_flip_vertically_on_load",
	"time"} ;

static void *sysfunc[NSYS] = {
	B2D_GetProcAddress,
	BBC_aaArcColor,
	BBC_aaBezierColor,
	BBC_aaFilledEllipseColor,
	BBC_aaFilledPieColor,
	BBC_aaFilledPolyBezierColor,
	BBC_aaFilledPolygonColor,
	BBC_bezierColor,
	BBC_filledPolyBezierColor,
	BBC_Net_AddSocket,
	BBC_Net_AllocSocketSet,
	BBC_Net_CheckSockets,
	BBC_Net_DelSocket,
	BBC_Net_FreeSocketSet,
	BBC_Net_GetError,
	BBC_Net_Linked_Version,
	BBC_Net_ResolveHost,
	BBC_Net_ResolveIP,
	BBC_Net_TCP_Accept,
	BBC_Net_TCP_Close,
	BBC_Net_TCP_GetPeerAddress,
	BBC_Net_TCP_Open,
	BBC_Net_TCP_Recv,
	BBC_Net_TCP_Send,
	BBC_AddTimer,
	BBC_ClearQueuedAudio,
	BBC_CloseAudioDevice,
	BBC_ComposeCustomBlendMode,
	BBC_ConvertSurfaceFormat,
	BBC_CreateRGBSurface,
	BBC_CreateTexture,
	BBC_CreateTextureFromSurface,
	BBC_DestroyTexture,
	BBC_FillRect,
	BBC_FreeSurface,
	BBC_FreeWAV,
	BBC_GL_CreateContext,
	BBC_GL_DeleteContext,
	BBC_GL_GetCurrentContext,
	BBC_GL_GetDrawableSize,
	BBC_GL_GetProcAddress,
	BBC_GL_MakeCurrent,
	BBC_GL_SetAttribute,
	BBC_GL_SetSwapInterval,
	BBC_GL_SwapWindow,
	BBC_GetDisplayUsableBounds,
	BBC_GetQueuedAudioSize,
	BBC_GetRenderTarget,
	BBC_GetTicks,
	BBC_GetWindowFlags,
	BBC_LoadBMP_RW,
	BBC_LoadWAV_RW,
	BBC_LockAudioDevice,
	BBC_LockTexture,
	BBC_MixAudioFormat,
	BBC_OpenAudioDevice,
	BBC_PauseAudioDevice,
	BBC_QueryTexture,
	BBC_QueueAudio,
	BBC_RWFromFile,
	BBC_RWFromMem,
	BBC_RemoveTimer,
	BBC_RenderClear,
	BBC_RenderCopy,
	BBC_RenderCopyEx,
	BBC_RenderCopyExF,
	BBC_RenderDrawLine,
	BBC_RenderDrawLines,
	BBC_RenderDrawPoint,
	BBC_RenderDrawPoints,
	BBC_RenderFillRect,
	BBC_RenderFillRects,
	WASM_RenderReadPixels,
	WASM_RenderSetClipRect,
	BBC_SetColorKey,
	BBC_SetHint,
	BBC_SetPaletteColors,
	BBC_SetRenderDrawBlendMode,
	BBC_SetRenderDrawColor,
	BBC_SetRenderTarget,
	BBC_SetSurfaceAlphaMod,
	BBC_SetSurfaceColorMod,
	BBC_SetTextureAlphaMod,
	BBC_SetTextureBlendMode,
	BBC_SetTextureColorMod,
	BBC_SetWindowResizable,
	BBC_SetWindowTitle,
	BBC_ShowSimpleMessageBox,
	BBC_UnlockAudioDevice,
	BBC_UnlockTexture,
	BBC_UpperBlit,
	BBC_free,
	BBC_malloc,
	BBC_memcpy,
	BBC_memset,
	BBC_STBIMG_Load,
	BBC_STBIMG_LoadTexture,
	BBC_TTF_Linked_Version,
	BBC_asctime,
	BBC_drmp3_free,
	BBC_drmp3_open_file_and_read_f32,
	BBC_drmp3dec_f32_to_s16,
	BBC_emscripten_async_wget,
	BBC_gmtime,
	BBC_localtime,
	BBC_mktime,
	BBC_stbi_set_flip_vertically_on_load,
	BBC_time } ;

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

// OpenGLES functions:

long long BBC_glAttachShader(st program, st shader, st i2, st i3, st i4, st i5, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ glAttachShader(program, shader); return 0; }

long long BBC_glBindAttribLocation(st program, st index, st name, st i3, st i4, st i5, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ glBindAttribLocation(program, index, (const GLchar *) name); return 0; }

long long BBC_glBindBuffer(st target, st buffer, st i2, st i3, st i4, st i5, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ glBindBuffer((GLenum) target, buffer); return 0; }

long long BBC_glBlendFunc(st sfactor, st dfactor, st i2, st i3, st i4, st i5, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ glBlendFunc((GLenum) sfactor,(GLenum) dfactor); return 0; }

long long BBC_glBufferData(st target, st size, st data, st usage, st i4, st i5, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ glBufferData((GLenum) target, size, (const void*) data, (GLenum) usage); return 0; }

long long BBC_glClear(st mask, st i1, st i2, st i3, st i4, st i5, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ glClear((GLbitfield) mask); return 0; }

long long BBC_glClearColor(st i0, st i1, st i2, st i3, st i4, st i5, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{
		float r = *(float *)&i0 ; float g = *(float *)&i1 ;
		float b = *(float *)&i2 ; float a = *(float *)&i3 ;
		glClearColor((GLclampf) r, (GLclampf) g, (GLclampf) b, (GLclampf) a);
		return 0;
	}

long long BBC_glClearDepthf(st depth, st i1, st i2, st i3, st i4, st i5, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ glClearDepthf(*(float *)&depth); return 0; }

long long BBC_glCompileShader(st shader, st i1, st i2, st i3, st i4, st i5, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ glCompileShader(shader); return 0; }

long long BBC_glCreateProgram(st i0, st i1, st i2, st i3, st i4, st i5, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ return glCreateProgram(); }

long long BBC_glCreateShader(st type, st i1, st i2, st i3, st i4, st i5, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ return glCreateShader((GLenum) type); }

long long BBC_glCullFace(st mode, st i1, st i2, st i3, st i4, st i5, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ glCullFace((GLenum) mode); return 0; }

long long BBC_glDeleteBuffers(st num, st buffers, st i2, st i3, st i4, st i5, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ glDeleteBuffers(num, (const GLuint*) buffers); return 0; }

long long BBC_glDeleteProgram(st program, st i1, st i2, st i3, st i4, st i5, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ glDeleteProgram(program); return 0; }

long long BBC_glDeleteShader(st shader, st i1, st i2, st i3, st i4, st i5, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ glDeleteShader(shader); return 0; }

long long BBC_glDeleteTextures(st ntex, st textures, st i2, st i3, st i4, st i5, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ glDeleteTextures(ntex, (const GLuint*) textures); return 0; }

long long BBC_glDisable(st cap, st i1, st i2, st i3, st i4, st i5, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ glDisable(cap); return 0; }

long long BBC_glDisableVertexAttribArray(st index, st i1, st i2, st i3, st i4, st i5, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ glDisableVertexAttribArray(index); return 0; }

long long BBC_glDrawArrays(st mode, st first, st count, st i3, st i4, st i5, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ glDrawArrays((GLenum) mode, first, count); return 0; }

long long BBC_glEnable(st cap, st i1, st i2, st i3, st i4, st i5, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ glEnable(cap); return 0; }

long long BBC_glEnableVertexAttribArray(st index, st i1, st i2, st i3, st i4, st i5, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ glEnableVertexAttribArray(index); return 0; }

long long BBC_glGenBuffers(st num, st buffers, st i2, st i3, st i4, st i5, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ glGenBuffers(num, (GLuint*) buffers); return 0; }

long long BBC_glGetAttribLocation(st program, st name, st i2, st i3, st i4, st i5, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ return glGetAttribLocation(program, (const GLchar*) name); }

long long BBC_glGetIntegerv(st pname, st data, st i2, st i3, st i4, st i5, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ glGetIntegerv(pname, (GLint *) data); return 0; }

long long BBC_glGetProgramInfoLog(st program, st maxLength, st length, st infoLog, st i4, st i5, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ glGetProgramInfoLog(program, maxLength, (GLsizei*) length, (GLchar*) infoLog); return 0; }

long long BBC_glGetProgramiv(st program, st pname, st params, st i3, st i4, st i5, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ glGetProgramiv(program, (GLenum) pname, (GLint*) params); return 0; }

long long BBC_glGetShaderInfoLog(st shader, st maxLength, st length, st infoLog, st i4, st i5, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ glGetShaderInfoLog(shader, maxLength, (GLsizei*) length, (GLchar*) infoLog); return 0; }

long long BBC_glGetShaderiv(st shader, st pname, st params, st i3, st i4, st i5, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ glGetShaderiv(shader, (GLenum) pname, (GLint*) params); return 0; }

long long BBC_glGetUniformLocation(st program, st name, st i2, st i3, st i4, st i5, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ return glGetUniformLocation(program, (const GLchar*) name); }

long long BBC_glIsBuffer(st buffer, st i1, st i2, st i3, st i4, st i5, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ return glIsBuffer(buffer); }

long long BBC_glIsTexture(st texture, st i1, st i2, st i3, st i4, st i5, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ return glIsTexture(texture); }

long long BBC_glLinkProgram(st program, st i1, st i2, st i3, st i4, st i5, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ glLinkProgram(program); return 0; }

long long BBC_glShaderSource(st shader, st count, st string, st length, st i4, st i5, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ glShaderSource(shader, count,	(const GLchar**) string, (const GLint*) length); return 0; }

long long BBC_glUniform1fv(st location, st count, st value, st i3, st i4, st i5, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ glUniform1fv(location, count,	(const GLfloat*) value); return 0; }

long long BBC_glUniform2fv(st location, st count, st value, st i3, st i4, st i5, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ glUniform2fv(location, count,	(const GLfloat*) value); return 0; }

long long BBC_glUniform3fv(st location, st count, st value, st i3, st i4, st i5, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ glUniform3fv(location, count,	(const GLfloat*) value); return 0; }

long long BBC_glUniform4fv(st location, st count, st value, st i3, st i4, st i5, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ glUniform4fv(location, count,	(const GLfloat*) value); return 0; }

long long BBC_glUniformMatrix4fv(st location, st count, st transpose, st value, st i4, st i5, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ glUniformMatrix4fv(location, count, transpose, (const GLfloat*) value); return 0; }

long long BBC_glUseProgram(st program, st i1, st i2, st i3, st i4, st i5, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ glUseProgram(program); return 0; }

long long BBC_glVertexAttribPointer(st index, st size, st type, st normalized, st stride, st pointer, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ glVertexAttribPointer(index, size, type, normalized, stride, (const void*) pointer); return 0; }

long long BBC_glViewport(st x, st y, st w, st h, st i4, st i5, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ glViewport(x, y, w, h); return 0; }

long long BBC_glScissor(st x, st y, st w, st h, st i4, st i5, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ glScissor(x, y, w, h); return 0; }

long long BBC_glActiveTexture(st texture, st i1, st i2, st i3, st i4, st i5, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ glActiveTexture(texture); return 0; }

long long BBC_glBindTexture(st target, st texture, st i2, st i3, st i4, st i5, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ glBindTexture(target, texture); return 0; }

long long BBC_glGenTextures(st ntex, st textures, st i2, st i3, st i4, st i5, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ glGenTextures(ntex, (GLuint*) textures); return 0; }

long long BBC_glTexImage2D(st target, st level, st internalformat, st width, st height, st border, st format, st type,
	  st data, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ glTexImage2D(target, level, internalformat, width, height, border, format, type, (const void*) data); return 0; }

long long BBC_glTexParameteri(st target, st pname, st param, st i3, st i4, st i5, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ glTexParameteri(target, pname, param); return 0; }

long long BBC_glUniform1i(st location, st v0, st i2, st i3, st i4, st i5, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ glUniform1i(location, v0); return 0; }

long long BBC_glBindFramebuffer(st target, st fbo, st i2, st i3, st i4, st i5, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ glBindFramebuffer(target, fbo); return 0; }

long long BBC_glGenFramebuffers(st num, st framebuffers, st i2, st i3, st i4, st i5, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ glGenFramebuffers(num, (GLuint*) framebuffers); return 0; }

long long BBC_glFramebufferTexture2D(st target, st attach, st textarget, st texture, st level, st i5, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ glFramebufferTexture2D(target, attach, textarget, texture, level); return 0; }

long long BBC_glDeleteFramebuffers(st num, st framebuffers, st i2, st i3, st i4, st i5, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
	{ glDeleteFramebuffers(num, (const GLuint*) framebuffers); return 0; }

#define GLNSYS 52
#define GLPOW2 64 // smallest power-of-2 >= GLNSYS

static const char *GLname[GLNSYS] = {
	"glActiveTexture",
	"glAttachShader",
	"glBindAttribLocation",
	"glBindBuffer",
	"glBindFramebuffer",
	"glBindTexture",
	"glBlendFunc",
	"glBufferData",
	"glClear",
	"glClearColor",
	"glClearDepthf",
	"glCompileShader",
	"glCreateProgram",
	"glCreateShader",
	"glCullFace",
	"glDeleteBuffers",
	"glDeleteFramebuffers",
	"glDeleteProgram",
	"glDeleteShader",
	"glDeleteTextures",
	"glDisable",
	"glDisableVertexAttribArray",
	"glDrawArrays",
	"glEnable",
	"glEnableVertexAttribArray",
	"glFramebufferTexture2D",
	"glGenBuffers",
	"glGenFramebuffers",
	"glGenTextures",
	"glGetAttribLocation",
	"glGetIntegerv",
	"glGetProgramInfoLog",
	"glGetProgramiv",
	"glGetShaderInfoLog",
	"glGetShaderiv",
	"glGetUniformLocation",
	"glIsBuffer",
	"glIsTexture",
	"glLinkProgram",
	"glScissor",
	"glShaderSource",
	"glTexImage2D",
	"glTexParameteri",
	"glUniform1fv",
	"glUniform1i",
	"glUniform2fv",
	"glUniform3fv",
	"glUniform4fv",
	"glUniformMatrix4fv",
	"glUseProgram",
	"glVertexAttribPointer",
	"glViewport"} ;

static void *GLfunc[GLNSYS] = {
	BBC_glActiveTexture,
	BBC_glAttachShader,
	BBC_glBindAttribLocation,
	BBC_glBindBuffer,
	BBC_glBindFramebuffer,
	BBC_glBindTexture,
	BBC_glBlendFunc,
	BBC_glBufferData,
	BBC_glClear,
	BBC_glClearColor,
	BBC_glClearDepthf,
	BBC_glCompileShader,
	BBC_glCreateProgram,
	BBC_glCreateShader,
	BBC_glCullFace,
	BBC_glDeleteBuffers,
	BBC_glDeleteFramebuffers,
	BBC_glDeleteProgram,
	BBC_glDeleteShader,
	BBC_glDeleteTextures,
	BBC_glDisable,
	BBC_glDisableVertexAttribArray,
	BBC_glDrawArrays,
	BBC_glEnable,
	BBC_glEnableVertexAttribArray,
	BBC_glFramebufferTexture2D,
	BBC_glGenBuffers,
	BBC_glGenFramebuffers,
	BBC_glGenTextures,
	BBC_glGetAttribLocation,
	BBC_glGetIntegerv,
	BBC_glGetProgramInfoLog,
	BBC_glGetProgramiv,
	BBC_glGetShaderInfoLog,
	BBC_glGetShaderiv,
	BBC_glGetUniformLocation,
	BBC_glIsBuffer,
	BBC_glIsTexture,
	BBC_glLinkProgram,
	BBC_glScissor,
	BBC_glShaderSource,
	BBC_glTexImage2D,
	BBC_glTexParameteri,
	BBC_glUniform1fv,
	BBC_glUniform1i,
	BBC_glUniform2fv,
	BBC_glUniform3fv,
	BBC_glUniform4fv,
	BBC_glUniformMatrix4fv,
	BBC_glUseProgram,
	BBC_glVertexAttribPointer,
	BBC_glViewport} ;

long long BBC_GL_GetProcAddress(st symbol, st i1, st i2, st i3, st i4, st i5, st i6, st i7,
	  st i8, st i9, st i10, st i11, db f0, db f1, db f2, db f3, db f4, db f5, db f6, db f7)
{
	int b = 0, h = GLPOW2, r = 0 ;
	do
	    {
		h /= 2 ;
		if (((b + h) < GLNSYS) && ((r = strcmp ((const char*) symbol, GLname[b + h])) >= 0))
			b += h ;
	    }
	while (h) ;
	if (r == 0) return (intptr_t) GLfunc[b] ;
	return 0 ;
}
