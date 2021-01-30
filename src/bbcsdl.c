/*****************************************************************\
*       32-bit or 64-bit BBC BASIC for SDL 2.0                    *
*       (C) 2017-2021  R.T.Russell  http://www.rtrussell.co.uk/   *
*                                                                 *
*       The name 'BBC BASIC' is the property of the British       *
*       Broadcasting Corporation and used with their permission   *
*                                                                 *
*       bbcsdl.c Main program: Initialisation, Polling Loop       *
*       Version 1.20a, 30-Jan-2021                                *
\*****************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include "SDL2_gfxPrimitives.h"
#include "SDL_ttf.h"
#include "SDL_net.h"

#ifdef __WINDOWS__
#include <windows.h>
#include <wchar.h>
#if defined __x86_64__
#define PLATFORM "Win64"
#else
#include <psapi.h>
#define PLATFORM "Win32"
#define K32EnumProcessModules EnumProcessModules
#endif
#define realpath(N,R) _fullpath((R),(N),_MAX_PATH)
BOOL WINAPI K32EnumProcessModules (HANDLE, HMODULE*, DWORD, LPDWORD) ;
#endif
#ifdef __LINUX__
#include <sys/mman.h>
#include <sys/stat.h>
#define PLATFORM "Linux"
#endif
#ifdef __MACOSX__
#include <sys/mman.h>
#define PLATFORM "MacOS"
int GetTempPath(size_t, char *) ;
#endif
#ifdef __ANDROID__
#include <sys/mman.h>
unsigned int DIRoff = 19 ; // Used by Android x86-32 build
#define PLATFORM "Android"
#endif
#ifdef __IPHONEOS__
#include <sys/mman.h>
#define PLATFORM "iOS"
#endif
#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#include <sys/mman.h>
#define PLATFORM "Wasm"
#endif

#undef MAX_PATH
#define MAX(X,Y) (((X) > (Y)) ? (X) : (Y))
#define GL_TEXTURE_2D		0x0DE1
#define GL_TEXTURE_MAG_FILTER	0x2800
#define GL_TEXTURE_MIN_FILTER	0x2801
#define GL_NEAREST		0x2600
#define GL_LINEAR		0x2601

// Program constants:
#define SCREEN_WIDTH  640
#define SCREEN_HEIGHT 500
#define WM_MOVE 0x0003
#define WM_SIZE 0x0005
#define WM_CLOSE 0x0010
#define WM_TIMER 0x0113
#define WM_LBUTTONDOWN 0x0201
#define WM_MBUTTONDOWN 0x0207
#define WM_RBUTTONDOWN 0x0204
#define MAX_PATH 260
#define AUDIOLEN 441 * 4

// Performance tuning parameters:
#define POLLT 2  // Poll for approaching vSync every 2 milliseconds 
#define BUSYT 40 // Busy-wait for 40 ms after last user output event
#define PACER 12 // 12 ms 'processing time' per frame (max. ~75 fps)
#define MAXEV 10 // Don't refresh display if > 10 waiting events ...
#define MAXFP 50 // ... unless >50 ms has elapsed since last refresh
#define PUMPT 16 // Call SDL_PollEvent at least every 16 milliseconds

#include "bbcsdl.h"
#include "version.h"

// Routines in BBCCMOS:

int entry (void*) ;
void stick (unsigned char*) ;
int putkey (char) ;
int putevt (int, int, int, int) ; // Force first parameter 32-bits

// Routines in BBCCVDU:

void xeqvdu_ (void *, void *, int) ;
void vduchr_ (void *) ;
int getcsr_ (void) ;
int getchar_ (void *, void *) ;
int copkey_ (void *, void *) ;
int vtint_ (void *, void *) ;
int disply_ (void *, void *) ;
void getpix_ (void *, void *) ;
int openfont_ (void *, void *) ;
int getwid_ (void *, void *) ;
long long apicall_ (void *, void *) ;

// Routines in BBCTTXT:

void flip7 (void) ;

// Routines in SDL2_gfxPrimitives:

int RedefineChar (SDL_Renderer*, char, unsigned char*, Uint32, Uint32);

// Global variables (external linkage):

void *userRAM = NULL ;
void *progRAM = NULL ;
void *userTOP = NULL ;
const int bLowercase = 0 ;    // Dummy
const char szVersion[] = "BBC BASIC for "PLATFORM" version "VERSION ;
const char szNotice[] = "(C) Copyright R. T. Russell, "YEAR ;
char szAutoRun[MAX_PATH + 1] = "autorun.bbc" ;
char *szLoadDir ;
char *szLibrary ;
char *szUserDir ;
char *szTempDir ;
char *szCmdLine ;
SDL_Texture *TTFcache[65536] = {NULL} ; // cache for character textures
unsigned int palette[256] ;
size_t iResult = 0 ;
int bChanged = 0 ;
int nUserEv = 0 ;
int OSKtime = 6 ;
SDL_Rect ClipRect ;
SDL_Rect DestRect ;
SDL_TimerID PollTimerID ;
SDL_TimerID UserTimerID ;
SDL_TimerID BlinkTimerID ;
SDL_Event UserEvent ; // Used by Android x86-32 build
SDL_Joystick * Joystick = NULL ;
SDL_sem * Sema4 ;
SDL_mutex * Mutex ;
SDL_sem * Idler ;
int bBackground = 0 ;
int bYield = 0 ;

int useGPA = 0 ;

#ifdef __WINDOWS__
void (__stdcall *glEnableBBC)  (int) ;
void (__stdcall *glLogicOpBBC) (int) ;
void (__stdcall *glDisableBBC) (int) ;
void (__stdcall *glTexParameteriBBC) (int, int, int) ;
#else
void (*glEnableBBC)  (int) ;
void (*glLogicOpBBC) (int) ;
void (*glDisableBBC) (int) ;
void (*glTexParameteriBBC) (int, int, int) ;
#endif
void (*SDL_RenderFlushBBC) (SDL_Renderer*) ;

static SDL_Window * window ;
static SDL_Renderer *renderer ;
static SDL_Thread *Thread ;
static int blink = 0 ;
static signed char oldscroln = 0 ;
static const Uint8* keystate ;
static int exitcode = 0, running = 1 ;
static int MaximumRAM = MAXIMUM_RAM ;
static SDL_AudioSpec want, have ;
static SDL_Rect backbutton = {0} ;
static SDL_Texture *buttexture ;

#if defined __EMSCRIPTEN__
// dlsym implemented locally
#elif defined __WINDOWS__
void *dlsym (void *handle, const char *symbol)
{
	void *procaddr ;
	HMODULE modules[100] ;
	long unsigned int i, needed ;
	K32EnumProcessModules ((HANDLE)-1, modules, sizeof (modules), &needed) ;
	for (i = 0; i < needed / sizeof (HMODULE); i++)
	    {
		procaddr = GetProcAddress (modules[i], symbol) ;
		if (procaddr != NULL) break ;
	    }
	return procaddr ;
}
#else
#include "dlfcn.h"
#endif

#if defined __LINUX__ || defined __ANDROID__
static void *mymap (unsigned int size)
{
	FILE *fp ;
	char line[256] ;
	void *start, *finish, *base = (void *) 0x400000 ;

	fp = fopen ("/proc/self/maps", "r") ;
	if (fp == NULL)
		return NULL ;

	while (NULL != fgets (line, 256, fp))
	    {
		sscanf (line, "%p-%p", &start, &finish) ;
		start = (void *)((size_t)start & -0x1000) ; // page align (GCC extension)
		if (start >= (base + size)) 
			return base ;
		if (finish > (void *)0xFFFFF000)
			return NULL ;
		base = (void *)(((size_t)finish + 0xFFF) & -0x1000) ; // page align
		if (base > ((void *)0xFFFFFFFF - size))
			return NULL ;
	    }
	return base ;
}
#endif

static Uint32 BlinkTimerCallback(Uint32 interval, void *param)
{
#if defined __ANDROID__ || defined __IPHONEOS__
	if ((--OSKtime == 0) && SDL_IsTextInputActive())
	    {
		SDL_Event event = {0} ;
		event.type = SDL_USEREVENT ;
		event.user.code = EVT_OSK ;
		SDL_PushEvent(&event) ;
	    }
#endif
	if (flags & KILL)
	    {
		if (!SDL_SemValue(Sema4))
			SDL_SemPost (Sema4) ;
		nUserEv = 0 ;
		reflag = 0 ;
		bBackground = 0 ;
	    }
	blink = !blink ;
	bChanged = 1 ;
	return(interval) ;
}

Uint32 UserTimerCallback(Uint32 interval, void *param)
{
	SDL_Event event ;
	if (bBackground)
		return(interval) ;

	event.type = SDL_USEREVENT ;
	event.user.code = WMU_TIMER ;

	SDL_AtomicAdd ((SDL_atomic_t*)&nUserEv, 1) ; // Before PushEvent
	SDL_LockMutex (Mutex) ;
	SDL_PushEvent(&event) ;
	SDL_UnlockMutex (Mutex) ;
	return(interval) ;
}

static Uint32 PollTimerCallback(Uint32 interval, void *param)
{
	if (!SDL_SemValue(Idler))
		SDL_SemPost (Idler) ;
	return interval ;
}

static void UserAudioCallback(void* userdata, Uint8* stream, int len)
{
static Uint8 surplus[AUDIOLEN * MAX_TEMPO] ;
static int leftover ;
int blocksize = AUDIOLEN * (tempo & 0x3F) ;

	if (len < leftover)
	    {
		memcpy (stream, surplus, len) ;
		leftover -= len ;
		memmove (surplus, surplus + len, leftover) ;
		return ;
	    }

	if (leftover)
	    {
		memcpy (stream, surplus, leftover) ;
		stream += leftover ;
		len -= leftover ;
		leftover = 0 ;
	    }

	while (len >= blocksize)
	    {
		stick (stream) ;
		stream += blocksize ;
		len -= blocksize ;
	    }

	if (len)
	    {
		stick (surplus) ;
		memcpy (stream, surplus, len) ;
		leftover = blocksize - len ;
		memmove (surplus, surplus + len, leftover) ;
	    }

	return;
}

static Uint32 FlipCaret(SDL_Renderer *renderer, SDL_Rect *rect)
{
	SDL_Texture *texture ;
	int *p ;
	int w = rect -> w ;
	int h = rect -> h ;
	int *buffer = (int *) malloc (w * h * 4) ;
	SDL_RenderReadPixels (renderer, rect, SDL_PIXELFORMAT_ABGR8888, buffer, w * 4) ;
	texture = SDL_CreateTexture (renderer, SDL_PIXELFORMAT_ABGR8888, 0, w, h) ;
	for (p = buffer; p < buffer + w * h; p += 1)
		*p = *p ^ 0xFFFFFF ;
	SDL_UpdateTexture (texture, 0, buffer, w * 4) ;
	SDL_RenderCopy (renderer, texture, 0, rect) ;
	SDL_DestroyTexture (texture) ;
	free (buffer) ;
	return (0) ;
}

static void CaptureScreen (void)
{
	int i ;
	short *eol ;
	short *ptr = chrmap ;
	short *end = chrmap + ((XSCREEN+7)>>3)*((YSCREEN+7)>>3) ;
	char *tmp ;
	char clip[((XSCREEN+7)>>3)*((YSCREEN+7)>>3)] ;

	while (((--end) >= chrmap) && (end[0] == ' ')) ;

	tmp = clip ;

	while (ptr <= end)
	    {
		eol = ptr + ((XSCREEN + 7) >> 3) ;
		while (((--eol) >= ptr) && (eol[0] == ' ')) ;
		for (i = 0; i < (eol - ptr + 1); i++)
			tmp[i] = (char) ptr[i] ; /// Needs UCS-2 to UTF-8 conversion!! 
		tmp += (eol - ptr + 1) ;
		(tmp++)[0] = 0x0D ;
		(tmp++)[0] = 0x0A ;
		ptr += (XSCREEN + 7) >> 3 ;
	    }
	tmp[0] = 0 ; // Null terminator
	SDL_SetClipboardText (clip) ;
	return ;
}

static void SetDir (char *newpath)
{
	char temp[MAX_PATH] ;
	char *p ;
	strcpy (temp, newpath) ;
	p = strrchr (temp, '/') ;
	if (p == NULL) p = strrchr (temp, '\\') ;
	if (p)
	    {
		*p = '\0' ;
		realpath (temp, szLoadDir) ;
	    }
	else
	    {
		getcwd (szLoadDir, MAX_PATH) ;
	    }
#ifdef __WINDOWS__
	wchar_t widepath[MAX_PATH] ;
	strcat (szLoadDir, "\\") ;
	MultiByteToWideChar (CP_UTF8, 0, szLoadDir, -1, widepath, MAX_PATH) ;
	_wchdir (widepath) ;
#else
	strcat (szLoadDir, "/") ;
	chdir (szLoadDir) ;
#endif
}

static void ShutDown ()
{
	int i ;

	flags |= KILL ;       // Tell worker thread to commit suicide
	if (!SDL_SemValue(Sema4))
		SDL_SemPost (Sema4) ; // Worker thread may be waiting on semaphore
	nUserEv = 0 ;         // Worker thread may be waiting on nUserEv
	reflag = 0 ;          // Worker thread may be waiting on reflag
	bBackground = 0 ;     // Worker thread may be waiting on bBackground
	SDL_WaitThread (Thread, &i) ;
	SDL_DestroySemaphore (Sema4) ;
	SDL_DestroyMutex (Mutex) ; 

	SDL_RemoveTimer(PollTimerID) ;
	SDL_RemoveTimer(UserTimerID) ;
	SDL_RemoveTimer(BlinkTimerID) ;

	SDL_DestroyTexture(SDL_GetRenderTarget (renderer)) ;
	SDL_DestroyRenderer(renderer) ;
	SDL_DestroyWindow(window) ;

	free (chrmap) ;
	// free (userRAM) ; Don't free memory as it contains the worker thread's stack!

	SDLNet_Quit() ;
	TTF_Quit() ;
	SDL_Quit() ;
	return ;
}

static int BBC_PeepEvents(SDL_Event* ev, int nev, SDL_eventaction action, Uint32 min, Uint32 max)
{
	int ret ;
	SDL_LockMutex (Mutex) ;
	ret = SDL_PeepEvents (ev, nev, action, min, max) ;
	SDL_UnlockMutex (Mutex) ;
	return ret ;
}

static int myEventFilter(void* userdata, SDL_Event* pev)
{
	if (!SDL_SemValue(Idler))
		SDL_SemPost (Idler) ;

	switch (pev->type)
	    {
		case SDL_APP_WILLENTERBACKGROUND:
		case SDL_APP_DIDENTERBACKGROUND:
		bBackground = 1 ;
		break ;

		case SDL_APP_WILLENTERFOREGROUND:
		case SDL_APP_DIDENTERFOREGROUND:
		bBackground = 0 ;
		break ;
	    }

#ifdef __EMSCRIPTEN__
	if ((pev->type == SDL_USEREVENT) || (pev->type == SDL_WINDOWEVENT))
		return 1;
	BBC_PeepEvents (pev, 1, SDL_ADDEVENT, SDL_FIRSTEVENT, SDL_LASTEVENT) ;
#endif
	return 0 ;
}

static int maintick (void) ;

void mainloop (void)
{
	while (maintick()) ;
}

int main(int argc, char* argv[])
{
int i ;
size_t immediate ;
int fullscreen = 0, fixedsize = 0, hidden = 0, borderless = 0 ;
SDL_RWops *ProgFile ;
const SDL_version *TTFversion ;
SDL_version SDLversion ;
SDL_Event ev ;

unsigned int *pixels ;
int pitch ;

#ifdef __WINDOWS__
	SDL_setenv ("SDL_AUDIODRIVER", "directsound", 1) ;
#endif
if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER |
		SDL_INIT_EVENTS | SDL_INIT_JOYSTICK) != 0)
{
	SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,
				szVersion, SDL_GetError(), NULL) ;
	return 1;
}

SDL_GetVersion (&SDLversion) ;
platform = SDLversion.major * 0x1000000 +
	   SDLversion.minor * 0x10000 +
	   SDLversion.patch * 0x100 ;
if (platform < 0x2000200)
{
	SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,
				szVersion, "Requires SDL version 2.0.2 or later", NULL) ;
	SDL_Quit() ;
	return 2;
}

#ifdef __LINUX__
	platform |= 1 ;
#endif
#ifdef __MACOSX__
	platform |= 2 ;
#endif
#ifdef __ANDROID__
	platform |= 3 ;
	fullscreen = 1 ;
#endif
#ifdef __IPHONEOS__
	platform |= 4 ;
	fullscreen = 1 ;
#endif
#ifdef __EMSCRIPTEN__
	platform |= 5 ;
#endif
#if defined __x86_64__ || defined __aarch64__ 
	platform |= 0x40 ;
#endif

if (TTF_Init() == -1)
{
	SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,
				szVersion, TTF_GetError(), NULL) ;
	SDL_Quit() ;
	return 3;
}

TTFversion = TTF_Linked_Version () ;
if ((TTFversion->major * 10000 + TTFversion->minor * 100 + TTFversion->patch) < 20012)
{
	SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,
				szVersion, "Requires SDL2_ttf version 2.0.12 or later", NULL) ;
	TTF_Quit() ;
	SDL_Quit() ;
	return 4;
}

if (SDLNet_Init() == -1)
{
	SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,
				szVersion, SDLNet_GetError(), NULL) ;
	TTF_Quit() ;
	SDL_Quit() ;
	return 5;
}

// Setting quality to "linear" can break flood-fill, affecting 'jigsaw.bbc' etc.
#if defined __ANDROID__ || defined __IPHONEOS__
	SDL_SetHint ("SDL_RENDER_BATCHING", "1") ;
	SDL_SetHint (SDL_HINT_RENDER_DRIVER, "opengles") ;
	SDL_SetHint (SDL_HINT_RENDER_SCALE_QUALITY, "nearest") ;
	SDL_SetHint ("SDL_TV_REMOTE_AS_JOYSTICK", "0") ;
	SDL_GL_SetAttribute (SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES) ;
	SDL_GL_SetAttribute (SDL_GL_CONTEXT_MAJOR_VERSION, 1) ;
	SDL_GL_SetAttribute (SDL_GL_RED_SIZE, 5) ;
	SDL_GL_SetAttribute (SDL_GL_GREEN_SIZE, 6) ;
	SDL_GL_SetAttribute (SDL_GL_BLUE_SIZE, 5) ;
#else
	SDL_SetHint ("SDL_RENDER_BATCHING", "1") ;
	SDL_SetHint (SDL_HINT_RENDER_DRIVER, "opengl") ;
	SDL_SetHint (SDL_HINT_RENDER_SCALE_QUALITY, "nearest") ;
	SDL_SetHint ("SDL_MOUSE_FOCUS_CLICKTHROUGH", "1") ; 
#endif

for (i = 1; i < argc; i++)
{
	fixedsize |= (NULL != strstr (argv[i], "-fixedsize")) ;
	fullscreen |= (NULL != strstr (argv[i], "-fullscreen")) ;
	borderless |= (NULL != strstr (argv[i], "-borderless")) ;
	hidden |= (NULL != strstr (argv[i], "-hidden")) ;
}

window = SDL_CreateWindow("BBCSDL",  SDL_WINDOWPOS_CENTERED,  SDL_WINDOWPOS_CENTERED, 
				SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_OPENGL | 
#ifdef __IPHONEOS__
				SDL_WINDOW_ALLOW_HIGHDPI |
#endif
#ifdef __ANDROID__
				SDL_WINDOW_BORDERLESS |
#endif
				(fixedsize ? 0 : SDL_WINDOW_RESIZABLE) | 
				(fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0) | 
				(borderless ? SDL_WINDOW_BORDERLESS : 0) | 
				(hidden ? SDL_WINDOW_HIDDEN : 0)) ;
if (window == NULL)
{
	SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,
				szVersion, SDL_GetError(), NULL) ;
	SDLNet_Quit() ;
	TTF_Quit() ;
	SDL_Quit() ;
	return 6;
}

renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC) ;
if (renderer == NULL)
{
	SDL_DestroyWindow(window) ;
	SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,
				szVersion, SDL_GetError(), NULL) ;
	SDLNet_Quit() ;
	TTF_Quit() ;
	SDL_Quit() ;
	return 7;
}

if (!SDL_RenderTargetSupported(renderer))
{
	SDL_DestroyWindow(window) ;
	SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,
				szVersion, "Render targets not supported", NULL) ;
	SDLNet_Quit() ;
	TTF_Quit() ;
	SDL_Quit() ;
	return 8;
}

SDL_GL_GetDrawableSize (window, &sizex, &sizey) ; // Window may not be the requested size

#if defined __ANDROID__ || defined __IPHONEOS__
{
	int size = MAX (sizex, sizey) ;
	SDL_SetRenderTarget(renderer, SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ABGR8888, 
				SDL_TEXTUREACCESS_TARGET, MAX(size,XSCREEN), MAX(size,YSCREEN))) ;
}
#else
{
	SDL_DisplayMode dm ;
	SDL_GetDesktopDisplayMode (0, &dm) ;
	SDL_SetRenderTarget(renderer, SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, 
				SDL_TEXTUREACCESS_TARGET, MAX(dm.w,XSCREEN), MAX(dm.h,YSCREEN))) ;
}
#endif

#ifdef __IPHONEOS__
SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255) ;
#else
SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255) ;
#endif
SDL_RenderClear(renderer) ;

if (sizex < sizey)
	backbutton.w = sizex / 10 ;
else
	backbutton.w = sizey / 10 ;
backbutton.h = backbutton.w ;
buttexture = SDL_CreateTexture (renderer, SDL_PIXELFORMAT_ABGR8888, 
				SDL_TEXTUREACCESS_STREAMING, 32, 32) ;
SDL_LockTexture (buttexture, NULL, (void **)&pixels, &pitch) ;
for (i = 0; i < 32 * 32; i++)
	*(pixels + i) = 0xC0FFFFFF ; // background
for (i = 5; i < 16; i++)
    {
	*(pixels + i*32 - i -  7) = 0xE0E0A090 ; // arrow
	*(pixels + i*32 - i -  8) = 0xFFFF4020 ; // arrow
	*(pixels + i*32 - i -  9) = 0xFFFF4020 ; // arrow
	*(pixels + i*32 - i - 10) = 0xFFFF4020 ; // arrow
	*(pixels + i*32 - i - 11) = 0xE0E0A090 ; // arrow
    }
for (i--; i < 32 - 5; i++)
    {
	*(pixels + i*32 + i -  6) = 0xE0E0A090 ; // arrow
	*(pixels + i*32 + i -  7) = 0xFFFF4020 ; // arrow
	*(pixels + i*32 + i -  8) = 0xFFFF4020 ; // arrow
	*(pixels + i*32 + i -  9) = 0xFFFF4020 ; // arrow
	*(pixels + i*32 + i - 10) = 0xE0E0A090 ; // arrow
    }
SDL_UnlockTexture (buttexture) ;
SDL_SetTextureBlendMode(buttexture, SDL_BLENDMODE_BLEND) ;

#if defined __WINDOWS__
	// Allocate the program buffer
	// First reserve the maximum amount:

	char *pstrVirtual = NULL ;

	while ((MaximumRAM > DEFAULT_RAM) &&
		(NULL == (pstrVirtual = (char*) VirtualAlloc (NULL, MaximumRAM,
						MEM_RESERVE, PAGE_EXECUTE_READWRITE))))
		MaximumRAM /= 2 ;

	// Now commit the initial amount to physical RAM:

	if (pstrVirtual != NULL)
		userRAM = (char*) VirtualAlloc (pstrVirtual, DEFAULT_RAM,
						MEM_COMMIT, PAGE_EXECUTE_READWRITE) ;

#elif defined __APPLE__ || defined __EMSCRIPTEN__

	while ((MaximumRAM > DEFAULT_RAM) &&
			((void*)-1 == (userRAM = mmap ((void *)0x10000000, MaximumRAM, 
						PROT_EXEC | PROT_READ | PROT_WRITE, 
						MAP_PRIVATE | MAP_ANON, -1, 0))) &&
			((void*)-1 == (userRAM = mmap ((void *)0x10000000, MaximumRAM, 
						PROT_READ | PROT_WRITE, 
						MAP_PRIVATE | MAP_ANON, -1, 0))))
		MaximumRAM /= 2 ;

#else // __LINUX__ and __ANDROID__

	void *base = NULL ;

	while ((MaximumRAM > DEFAULT_RAM) && (NULL == (base = mymap (MaximumRAM))))
		MaximumRAM /= 2 ;

	// Now commit the initial amount to physical RAM:

	if (base != NULL)
		userRAM = mmap (base, MaximumRAM, PROT_EXEC | PROT_READ | PROT_WRITE, 
					MAP_FIXED | MAP_PRIVATE | MAP_ANON, -1, 0) ;

#endif

if ((userRAM == NULL) || (userRAM == (void *)-1))
{
	SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,
				szVersion, "Couldn't allocate memory", NULL) ;
	SDLNet_Quit() ;
	TTF_Quit() ;
	SDL_Quit() ;
	return 9 ;
}

userTOP = userRAM + DEFAULT_RAM ;
progRAM = userRAM + PAGE_OFFSET ; // Will be raised if @cmd$ exceeds 255 bytes
szCmdLine = progRAM - 0x100 ;     // Must be immediately below default progRAM
szTempDir = szCmdLine - 0x100 ;   // Strings must be allocated on BASIC's heap
szUserDir = szTempDir - 0x100 ;
szLibrary = szUserDir - 0x100 ;
szLoadDir = szLibrary - 0x100 ;

#if defined __IPHONEOS__ || defined __EMSCRIPTEN__
SDL_RenderFlushBBC = SDL_GL_GetProcAddress ("SDL_RenderFlush") ;
#else
SDL_RenderFlushBBC = dlsym ((void *) -1, "SDL_RenderFlush") ;
#endif

glTexParameteriBBC = SDL_GL_GetProcAddress ("glTexParameteri") ;
glLogicOpBBC = SDL_GL_GetProcAddress("glLogicOp") ;
glEnableBBC  = SDL_GL_GetProcAddress("glEnable") ;
glDisableBBC = SDL_GL_GetProcAddress("glDisable") ;
#ifndef __EMSCRIPTEN__
if ((glTexParameteriBBC == NULL) || (glLogicOpBBC == NULL) || (glEnableBBC == NULL) || (glDisableBBC == NULL))
{
	SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,
				szVersion, "SDL_GL_GetProcAddress failed", NULL) ;
	SDLNet_Quit() ;
	TTF_Quit() ;
	SDL_Quit() ;
	return 10 ;
}
#endif

#ifdef __WINDOWS__
	wchar_t widepath[MAX_PATH] ;
	GetTempPathW (MAX_PATH, widepath) ;
	_wmkdir (widepath) ;
	WideCharToMultiByte (CP_UTF8, 0, widepath, -1, szTempDir, 256, NULL, NULL) ;
#endif

#ifdef __LINUX__
	strcpy (szTempDir, SDL_GetPrefPath("tmp", "")) ;
	*(szTempDir + strlen(szTempDir) - 1) = 0 ;
	mkdir (szTempDir, 0777) ;
#endif

#ifdef __MACOSX__
	GetTempPath (MAX_PATH, szTempDir) ;
	mkdir (szTempDir, 0777) ;
#endif

#ifdef __ANDROID__
	strcpy (szTempDir, (char *) SDL_AndroidGetInternalStoragePath ()) ;
	strcat (szTempDir, "/tmp/") ;
	mkdir (szTempDir, 0777) ;
#endif

#ifdef __IPHONEOS__
	strcpy (szTempDir, SDL_GetPrefPath("BBCBasic", "tmp")) ;
	char *p = strstr (szTempDir, "/Library/") ;
	if (p)
	    {
		strcpy (p, "/tmp/") ;
		mkdir (szTempDir, 0777) ;
	    }
#endif

#ifdef __ANDROID__
	useGPA = 1 ;
	strcpy (szUserDir, (char *) SDL_AndroidGetExternalStoragePath ()) ;
	strcat (szUserDir, "/") ;
	strcpy (szLibrary, (char *) SDL_AndroidGetInternalStoragePath ()) ;
	strcat (szLibrary, "/lib/") ;
	mkdir (szLibrary, 0777) ;
#else
#ifdef __IPHONEOS__
	useGPA = 1 ;
	strcpy (szUserDir, SDL_GetPrefPath("BBCBasic", "usr")) ;
	strcpy (szLibrary, SDL_GetBasePath()) ;
	strcat (szLibrary, "lib/") ;

	p = strstr (szUserDir, "/Library/") ;
	if (p)
	    {
		strcpy (p, "/Documents/") ;
		mkdir (szUserDir, 0777) ;
	    }
#else
	strcpy (szUserDir, SDL_GetPrefPath("BBCBasic", "")) ;
	strcpy (szLibrary, SDL_GetBasePath()) ;

	char *p ;
	*(szUserDir + strlen(szUserDir) - 1) = 0 ;

	p = strrchr (szLibrary, '/') ;
	if (p == NULL) p = strrchr (szLibrary, '\\') ;
#ifdef __WINDOWS__
	if (p) strcpy (p + 1, "lib\\") ;
#else
	if (p) strcpy (p + 1, "lib/") ;
#endif

#ifdef __EMSCRIPTEN__
	strcpy (szTempDir, "/tmp/") ;
	strcpy (szUserDir, "/usr/") ;
	EM_ASM(
	    FS.mkdir('/usr');
            FS.mount(IDBFS, {}, '/usr');
            FS.syncfs(true, function (err) {});
	);
#endif

	if (argc >= 2)
	{
		strncpy (szAutoRun, argv[1], 256) ;
		szAutoRun[255] = '\0';
	}
	if ((argc == 1) || (*argv[1] == '-'))
	{
		char *q ;
		strcpy (szAutoRun, SDL_GetBasePath()) ;
		q = szAutoRun + strlen (szAutoRun) ;
		p = strrchr (argv[0], '/') ;
		if (p == NULL) p = strrchr (argv[0], '\\') ;
		if (p)
			strcat (szAutoRun, p + 1) ;
		else
			strcat (szAutoRun, argv[0]) ;
		p = strrchr (szAutoRun, '.') ;
		if (p > q) *p = '\0' ;
		strcat (szAutoRun, ".bbc") ;
		SDL_SetWindowTitle (window, szVersion) ;
	}
#endif
#endif

strcpy (szCmdLine, szAutoRun) ;

if (argc >= 2)
	{
		*szCmdLine = 0 ;
		for (i = 1; i < argc; i++)
		{
			if (i > 1) strcat (szCmdLine, " ") ;
			strcat (szCmdLine, argv[i]) ;
		}
		SDL_SetWindowTitle (window, szCmdLine) ;
		progRAM = (void *)(((intptr_t) szCmdLine + strlen(szCmdLine) + 256) & -256) ;
	}

#ifdef __IPHONEOS__
	strcpy (szAutoRun, SDL_GetBasePath()) ;
	strcat (szAutoRun, "autorun.bbc") ;
#endif

#ifdef __EMSCRIPTEN__
	strcpy (szAutoRun, "lib/autorun.bbc") ;
#endif

SDL_EventState(SDL_DROPFILE, SDL_ENABLE);
SDL_PumpEvents () ;
if (SDL_PeepEvents (&ev, 1, SDL_GETEVENT, SDL_DROPFILE, SDL_DROPFILE))
{
	char *p = strstr (ev.drop.file, "://") ;
	if (p)
	    {
		strcpy (szAutoRun, szUserDir) ;
		strcat (szAutoRun, p + 3) ;
	    }
	else
		strcpy (szAutoRun, ev.drop.file) ;
	SDL_free (ev.drop.file) ;
}

if (*szAutoRun && (NULL != (ProgFile = SDL_RWFromFile (szAutoRun, "rb"))))
{
	SDL_RWread(ProgFile, progRAM, 1, userTOP - progRAM) ;
	SDL_RWclose(ProgFile) ;
	immediate = 0 ;
}
else
	immediate = 1 ;

SetDir (szAutoRun) ;

if (argc < 2)
	*szCmdLine = 0 ;

chrmap = (short*) malloc (2 * ((XSCREEN + 7) >> 3) * ((YSCREEN + 7) >> 3)) ;

// Audio buffer should be <= 40 ms (total queued SOUNDs at min. duration)
// but must be >= 20ms (minimum likely frame rate) because in Emscripten /
// Web Assembly the browser can only service audio interrupts at frame rate.  
SDL_memset(&want, 0, sizeof(want)) ;
want.freq = 44100 ;
want.format = AUDIO_S16LSB ;
want.channels = 2 ;
want.samples = 1024 ; // Largest power-of-two <= AUDIOLEN (~ 23 ms)
want.callback = UserAudioCallback ;

if (sizex > 800)
{
	charx = 16 ;
	chary = 38 ;
}
else
{
	charx = 8 ;
	chary = 20 ;
}

gfxPrimitivesSetFont(bbcfont, 8, 8) ;
gfxPrimitivesSetFontZoom(charx >> 3, chary >> 3) ;

reflag = 0 ;
sysflg = 0 ;
panx = 0 ;
pany = 0 ;
offsetx = 0 ;
offsety = 0 ;
cursx = 0 ;
zoom = 32768 ; // normalised 1.0

DestRect.w = sizex ; // Prevent crash if MOUSE used too soon
DestRect.h = sizey ;

// Poll timer:
PollTimerID = SDL_AddTimer(POLLT, PollTimerCallback, 0) ;

// 4 Hz timer:
UserTimerID = SDL_AddTimer(250, UserTimerCallback, 0) ;

// Caret timer:
BlinkTimerID = SDL_AddTimer(400, BlinkTimerCallback, 0) ;

// Semaphores and mutexes:
Sema4 = SDL_CreateSemaphore (0) ;
Idler = SDL_CreateSemaphore (0) ;
Mutex = SDL_CreateMutex () ;

// Copies of window and renderer:
hwndProg = window ;
memhdc = renderer ;

// Start interpreter thread:

Thread = SDL_CreateThread(entry, "Interpreter", (void *) immediate) ;

// Get global keyboard state:
keystate = SDL_GetKeyboardState(NULL) ;

// Set up to monitor enter/exit background events:
#ifdef __EMSCRIPTEN__
SDL_SetEventFilter(myEventFilter, 0) ;
#else
SDL_AddEventWatch(myEventFilter, 0) ;
#endif

// Main polling loop:
#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(mainloop, -1, 1);
#else
while (running)
    {
	mainloop () ;
	SDL_SemWait (Idler) ;
    }
#endif

ShutDown () ;
exit(exitcode) ;
}

static int maintick (void)
{
	int ptsx, ptsy, winx, winy, i, c ;
	static float oldx, oldy ;
	static int oldtextx, oldtexty ;
	static unsigned int lastpaint, lastusrev, lastpump ;
	unsigned int now = SDL_GetTicks () ;
	volatile unsigned int temp = zoom ;
	float scale = (float)(temp) / 32768.0 ; // must be float
	SDL_GetWindowSize (window, &ptsx, &ptsy) ;
	SDL_GL_GetDrawableSize (window, &winx, &winy) ;
	SDL_Rect caret ;
	SDL_Event ev ;

	DestRect.x = -panx * scale ;
	DestRect.y = -pany * scale ;
	DestRect.w = sizex * scale ;
	DestRect.h = sizey * scale ;
#if defined __ANDROID__ || defined __IPHONEOS__
	DestRect.x += (winx - DestRect.w) >> 1 ;
#endif

#define PAINT1 (((unsigned int)(now - lastpaint) >= PACER) && (nUserEv < MAXEV)) // Time window
#define PAINT2 (reflag & 1) // Interpreter thread is waiting for refresh (vSync)
#define PAINT3 ((unsigned int)(now - lastpaint) >= MAXFP) // Fallback minimum frame rate

	if ((reflag != 2) && (PAINT1 || PAINT2 || PAINT3) && 
	    (bChanged || (reflag & 1) || (textx != oldtextx) || (texty != oldtexty)))
	    {
		SDL_Rect SrcRect ;
		SDL_Texture *bitmap = SDL_GetRenderTarget (renderer) ;

		SrcRect.x = offsetx ;
		SrcRect.y = offsety ;
		SrcRect.w = sizex ;
		SrcRect.h = sizey ;

		oldtextx = textx ;
		oldtexty = texty ;
		int careton = blink && (cursa < 32) ;
		caret.x = (textx - offsetx) * scale + DestRect.x ;
		caret.y = (texty - offsety + cursa) * scale + DestRect.y ;
		if (cursx) caret.w = cursx * scale ; else caret.w = charx * scale ;
		caret.h = (cursb - cursa) * scale ;

		SDL_SetRenderTarget(renderer, NULL) ;
		SDL_SetRenderDrawColor (renderer, 0, 0, 0, 255) ;
		SDL_RenderClear (renderer) ;
		if (glTexParameteriBBC)
		    {
			SDL_GL_BindTexture (bitmap, NULL, NULL) ;
			glTexParameteriBBC (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR) ;
			glTexParameteriBBC (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR) ;
			SDL_GL_UnbindTexture (bitmap) ;
		    }
		SDL_RenderCopy(renderer, bitmap, &SrcRect, &DestRect) ;
#ifdef __IPHONEOS__
		if ((flags & ESCDIS) == 0)
			SDL_RenderCopy (renderer, buttexture, NULL, &backbutton) ;
#endif
		if (glTexParameteriBBC)
		    {
			SDL_GL_BindTexture (bitmap, NULL, NULL) ;
			glTexParameteriBBC (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST) ;
			glTexParameteriBBC (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST) ;
			SDL_GL_UnbindTexture (bitmap) ;
		    }
		if (careton) FlipCaret (renderer, &caret) ;
		SDL_RenderPresent(renderer) ;
		SDL_SetRenderTarget(renderer, bitmap);

		lastpaint = SDL_GetTicks() ; // wraps around after 50 days
		now = lastpaint ;
		bChanged = 0 ;
		reflag &= 2 ;
		return 0 ; // Must yield back to Browser after RenderPresent
	    }

	// SDL_PumpEvents is very slow on iOS (more than 2 microseconds!) so run it only
	// every PUMPT milliseconds.  Note that, contrary to what the SDL docs imply, we
	// do not need to call SDL_PumpEvents to put SDL_USEREVENTs in the queue.

	if ((unsigned int)(now - lastpump) >= PUMPT)
	    {
		SDL_LockMutex (Mutex) ;
		SDL_PumpEvents() ;
		SDL_UnlockMutex (Mutex) ;
		lastpump = SDL_GetTicks() ;
	    }

	if (scroln != oldscroln)
	    {
		oldscroln = scroln ;
		if (*(keystate + SDL_SCANCODE_LSHIFT) || *(keystate + SDL_SCANCODE_RSHIFT) ||
				SDL_GetMouseState (NULL,NULL))
			SDL_Delay (10) ;
	    }

	if (scroln > 0)
	    {
		if (*(keystate + SDL_SCANCODE_LSHIFT) || *(keystate + SDL_SCANCODE_RSHIFT) ||
				SDL_GetMouseState (NULL,NULL))
			scroln = 18 - 128 ; // 18 lines before next pause
		if (*(keystate + SDL_SCANCODE_ESCAPE) || (flags & ESCFLG))
			scroln = 0 ;        // exit paged mode
	    }

	if (((scroln <= 0) && BBC_PeepEvents(&ev, 1, SDL_GETEVENT, 0, SDL_LASTEVENT)) ||
	    ((scroln > 0)  && BBC_PeepEvents(&ev, 1, SDL_GETEVENT, 0, SDL_USEREVENT-1)))
	{
		switch (ev.type)
		{
		case SDL_QUIT:
			if (clotrp)
			{
				putevt (clotrp, WM_CLOSE, 0, 0) ;
				flags |= ALERT ;
				break ;
			}
			running = 0 ;
			break ;

		case SDL_TEXTINPUT:
			i = 0;
			while (ev.text.text[i] != '\0')
			  putkey(ev.text.text[i++]) ;
			break ;

		case SDL_USEREVENT:
			SDL_AtomicDecRef ((SDL_atomic_t*) &nUserEv) ;

			if ((ev.user.code >= 0x0100) && (ev.user.code <= 0x1FFF))
			{
				xeqvdu_ (ev.user.data2, ev.user.data1, ev.user.code) ;
				if ((ev.user.code >> 8) == 25)
					lastusrev = SDL_GetTicks() ;
				break ;
			}
			else switch (ev.user.code)
			{
				case EVT_VDU :
				vduchr_ (ev.user.data1) ;
				lastusrev = SDL_GetTicks() ;
				break ;

				case EVT_COPYKEY :
				iResult = copkey_ (ev.user.data1, ev.user.data2) ;
				SDL_SemPost (Sema4) ;
				break ;

				case EVT_TINT :
				iResult = vtint_ (ev.user.data1, ev.user.data2) ;
				SDL_SemPost (Sema4) ;
				lastusrev = SDL_GetTicks() ;
				break ;

				case EVT_DISPLAY :
				iResult = disply_ (ev.user.data1, ev.user.data2) ;
				SDL_SemPost (Sema4) ;
				lastusrev = SDL_GetTicks() ;
				break ;

				case EVT_PIXELS :
				getpix_ (ev.user.data1, ev.user.data2) ;
				SDL_SemPost (Sema4) ;
				lastusrev = SDL_GetTicks() ;
				break ;

				case EVT_CARET :
				iResult = getcsr_ () ;
				SDL_SemPost (Sema4) ;
				break ;

				case EVT_FONT :
				iResult = openfont_ (ev.user.data1, ev.user.data2) ;
				SDL_SemPost (Sema4) ;
				lastusrev = SDL_GetTicks() ;
				break ;

				case EVT_CHAR :
				iResult = getchar_ (ev.user.data1, ev.user.data2) ;
				SDL_SemPost (Sema4) ;
				lastusrev = SDL_GetTicks() ;
				break ;

				case EVT_WIDTH :
				iResult = getwid_ (ev.user.data1, ev.user.data2) ;
				SDL_SemPost (Sema4) ;
				lastusrev = SDL_GetTicks() ;
				break ;

				case EVT_REFLAG :
				reflag = ((reflag & ((size_t)ev.user.data1 & 0xFF00) >> 8)
					^  ((size_t)ev.user.data1 & 0x00FF)) ;
				SDL_SemPost (Sema4) ;
				break ;

				case EVT_OSWORD :
				RedefineChar (renderer, *(char *)(ev.user.data2 + 2),
							ev.user.data2 + 4, 16, 20) ;
				SDL_SemPost (Sema4) ;
				break ;

				case EVT_SYSCALL :
				iResult = apicall_ (ev.user.data1, ev.user.data2) ;
				SDL_SemPost (Sema4) ;
				lastusrev = SDL_GetTicks() ;
				if (bYield)
				    {
					bYield = 0 ;
					return 0 ; // Force yield when SDL_GL_SwapWindow
				    }
				break ;

				case EVT_MOUSE :
				{
				int x, y, b ;
				b = SDL_GetMouseState (&x, &y) ;
				x = (x * winx) / ptsx ;
				y = (y * winy) / ptsy ;
				*(int *)ev.user.data1 = ((((x - DestRect.x) * sizex / DestRect.w)
							+ offsetx) << 1) - origx ;
				*(int *)ev.user.data2 = ((sizey + ~(((y - DestRect.y) * sizey / DestRect.h)
							+ offsety)) << 1) - origy ;
				iResult = (b & 0x1A) | (((b & BIT0) != 0) << 2) | ((b & BIT2) != 0) ;
				}
				SDL_SemPost (Sema4) ;
				break ;

				case EVT_MOUSETO :
				{
				int x, y ;
				x = (((((size_t)ev.user.data1 + origx) >> 1) - offsetx) 
						* DestRect.w / sizex) + DestRect.x ;
				y = ((~((((size_t)ev.user.data2 + origy) >> 1) - sizey) - offsety)
						* DestRect.h / sizey) + DestRect.y ;
				x = (x * ptsx) / winx ;
				y = (y * ptsy) / winy ;
				SDL_WarpMouseInWindow (hwndProg, x, y) ;
				}
				SDL_SemPost (Sema4) ;
				break ;

				case EVT_QUIT :
				exitcode = (size_t) ev.user.data2 ;
				running = 0 ;
				break ;

				case EVT_OSK :
				if (ev.user.data1)
				    {
					if (!SDL_IsTextInputActive ())
						SDL_StartTextInput () ;
				    }
				else
				    {
					if (SDL_IsTextInputActive ())
						SDL_StopTextInput () ;
				    }
				break ;

				case WMU_REALLOC :
#ifdef __WINDOWS__
				if ((ev.user.data1 < userRAM) ||
				    (ev.user.data1 > (userRAM + MaximumRAM)) ||
				    (NULL == VirtualAlloc (userRAM, ev.user.data1 - userRAM,
						MEM_COMMIT, PAGE_EXECUTE_READWRITE)))
					iResult = 0 ;
#else
				if ((ev.user.data1 < userRAM) ||
				    (ev.user.data1 > (userRAM + MaximumRAM)))
					iResult = 0 ;
#endif
				else
				{
					if ((ev.user.data2) && (ev.user.data1 > userTOP))
						userTOP = ev.user.data1 ;
					iResult = (size_t) ev.user.data1 ;
				}
				SDL_SemPost (Sema4) ;
				break ;

				case WMU_WAVEOPEN :
				hwo = SDL_OpenAudioDevice (NULL, 0, &want, &have, 0) ;
				SDL_PauseAudioDevice(hwo, 0) ;
				SDL_SemPost (Sema4) ;
				break ;

				case WMU_WAVECLOSE :
				if (hwo)
					SDL_CloseAudioDevice (hwo) ;
				hwo = 0 ;
				break ;

				case EVT_TIMER :
				SDL_RemoveTimer (UserTimerID) ;
				UserTimerID = SDL_AddTimer ((intptr_t) ev.user.data1, UserTimerCallback, 0) ;
				break ;

				case WMU_TIMER :
				if (nUserEv <= 0)
					flip7 () ;
				if (timtrp)
				{
					putevt (timtrp, WM_TIMER, 0, 0) ;
					flags |= ALERT ;
				}
				break ;

				case EVT_FSSYNC:
#ifdef __EMSCRIPTEN__
				EM_ASM(
				    FS.syncfs(function (err) {});
				);
#endif
				break ;

			}
			break ;

		case SDL_KEYDOWN:
			c = ev.key.keysym.sym ;

			switch (c)
			    {
				case SDLK_HOME :
				c = 130 ;
				break ;

				case SDLK_END :
				c = 131 ;
				break ;

				case SDLK_PAGEUP :
				c = 132 ;
				break ;

				case SDLK_PAGEDOWN :
				c = 133 ;
 				break ;

				case SDLK_INSERT :
				c = 134 ;
				break ;

				case SDLK_DELETE :
				c = 135 ;
				break ;

				case SDLK_LEFT :
				c = 136 ;
				break ;

				case SDLK_RIGHT :
				c = 137 ;
				break ;

				case SDLK_DOWN :
				c = 138 ;
				break ;

				case SDLK_UP :
				c = 139 ;
				break ;

				case SDLK_TAB :
				if (ev.key.keysym.mod & KMOD_SHIFT)
					c = 155 ;
				else if (ev.key.keysym.mod & KMOD_CTRL)
					{
					CaptureScreen () ;
					c = 0 ;
					}
				else
					c = 9 ;
				break ;

				case SDLK_BACKSPACE :
				c = 8 ;
				break ;

				case SDLK_MENU :
				c = 9 ;
				break ;

				case SDLK_RETURN :
				case SDLK_SELECT :
				case SDLK_KP_ENTER :
				c = 13 ;
				break ;

				case SDLK_AUDIOPLAY :
				c = 32 ;
				break ;

				case SDLK_ESCAPE :
				case SDLK_AC_BACK : // Android 'back' button
				c = 27 ;
				if ((flags & ESCDIS) == 0)
				    {
					flags |= ESCFLG ;
					c = 0 ;
				    }
				break ;

				default:
				if ((c >= SDLK_F1) && (c <= SDLK_F12))
				    {
					c = c - SDLK_F1 + 145 ;
					if (ev.key.keysym.mod & KMOD_SHIFT)
						c += 16 ;
					if (ev.key.keysym.mod & KMOD_CTRL)
						c += 32 ;
				    }
				else if ((c >= SDLK_F13) && (c <= SDLK_F15))
				    {
					c = c - SDLK_F13 + 157 ;
					if (ev.key.keysym.mod & KMOD_SHIFT)
						c += 16 ;
					if (ev.key.keysym.mod & KMOD_CTRL)
						c += 32 ;
				    }
				else if ((c >= SDLK_a) && (c <= SDLK_z) &&
					 (ev.key.keysym.mod & KMOD_CTRL))
					c = c - SDLK_a + 1 ;
				else if (ev.key.keysym.scancode == SDL_SCANCODE_AUDIOREWIND)
					c = 128 ;
				else if (ev.key.keysym.scancode == SDL_SCANCODE_AUDIOFASTFORWARD)
					c = 129 ;
				else
					c = 0 ;

			    }

			if ((c >= 130) && (c <= 133) && (ev.key.keysym.mod & KMOD_CTRL))
				c += 26 ;
			if ((c >= 136) && (c <= 137) && (ev.key.keysym.mod & KMOD_CTRL))
				c -= 8 ;

			if (c) putkey (c) ;
			break ;

#ifdef __IPHONEOS__
		case SDL_MOUSEBUTTONUP:
			if ((flags & ESCDIS) == 0)
			    {
				SDL_Point pt = {ev.button.x * winx / ptsx, ev.button.y * winy / ptsy} ;
				if (SDL_PointInRect (&pt, &backbutton))
					flags |= ESCFLG ;
			    }
			break ;
#endif

		case SDL_MOUSEBUTTONDOWN:
			if (moutrp)
			    {
				int x = (ev.button.x * winx) / ptsx ; // SDL 2.0.8 and later
				int y = (ev.button.y * winy) / ptsy ;
				x = (x - DestRect.x) * sizex / DestRect.w ;
				y = (y - DestRect.y) * sizey / DestRect.h ;

				switch (ev.button.button)
				    {
					case SDL_BUTTON_LEFT :
					putevt (moutrp, WM_LBUTTONDOWN, 1, y << 16 | x) ;
					break ;

					case SDL_BUTTON_MIDDLE :
					putevt (moutrp, WM_MBUTTONDOWN, 16, y << 16 | x) ;
					break ;

					case SDL_BUTTON_RIGHT :
					putevt (moutrp, WM_RBUTTONDOWN, 2, y << 16 | x) ;
					break ;
				    }
				flags |= ALERT ;
			    }
			break ;

		case SDL_FINGERDOWN:
		case SDL_FINGERUP:
		case SDL_FINGERMOTION:
			if (moutrp && (sysflg & 2))
			{
				int x = (ev.tfinger.x * winx - DestRect.x) * sizex / DestRect.w ;
				int y = (ev.tfinger.y * winy - DestRect.y) * sizey / DestRect.h ;

				putevt (moutrp, ev.type, ev.tfinger.fingerId, y << 16 | x) ;
				flags |= ALERT ;
			}
			break ;

		case SDL_MULTIGESTURE :
			if (sysflg & 4) // Can be used to disable pan/zoom
			{
				if (!systrp) break ; // Not moutrp to avoid flood
				int x = (ev.mgesture.x * ptsx - DestRect.x) * sizex / DestRect.w ;
				int y = (ev.mgesture.y * ptsy - DestRect.y) * sizey / DestRect.h ;
				int z = ev.mgesture.dDist * 0x10000 ;

				putevt (systrp, ev.type, z, y << 16 | x) ;
				flags |= ALERT ;
				break ;
			}
			if (fabs(ev.mgesture.dDist) > 0.002)
			{
				volatile unsigned int temp = zoom ; // force aligned access
				temp = temp * (1.0 + ev.mgesture.dDist) /
					      (1.0 - ev.mgesture.dDist) ;
				zoom = temp ;
			}
			if (fabs(ev.mgesture.x - oldx) < 0.05)
			{
				volatile int temp = panx ; // force aligned access
				temp -= winx * (ev.mgesture.x - oldx) / scale ;
				panx = temp ;
				bChanged = 1 ;
			}
			if (fabs(ev.mgesture.y - oldy) < 0.05)
			{
				volatile int temp = pany ; // force aligned access
				temp -= winy * (ev.mgesture.y - oldy) / scale ;
				pany = temp ;
				bChanged = 1 ;
			}
			oldx = ev.mgesture.x ;
			oldy = ev.mgesture.y ;
			break ;

		case SDL_WINDOWEVENT:
			if (clotrp && (ev.window.event == SDL_WINDOWEVENT_CLOSE))
				{
					putevt (clotrp, WM_CLOSE, 0, ev.window.windowID) ;
					flags |= ALERT ;
					break ;
				}
			if (siztrp)
			{
				ev.window.data1 *= winx / ptsx ;
				ev.window.data2 *= winy / ptsy ;
				switch (ev.window.event)
				{
					case SDL_WINDOWEVENT_MOVED :
					    putevt (siztrp, WM_MOVE, ev.window.windowID,
						(ev.window.data2 << 16) | ev.window.data1) ;
					break ;

					case SDL_WINDOWEVENT_RESIZED :
					    putevt (siztrp, WM_SIZE, ev.window.windowID,
						(ev.window.data2 << 16) | ev.window.data1) ;
					break ;
				}
				flags |= ALERT ;
			}
			break ;

		case SDL_MOUSEWHEEL:
			if (ev.wheel.y > 0)
				putkey (0x8D) ;
			if (ev.wheel.y < 0)
				putkey (0x8C) ;
			break ;

		case SDL_APP_DIDENTERFOREGROUND:
			if (siztrp)
			{
				// Signal 'restored from background'
				putevt (siztrp, WM_SIZE, 0, (winy << 16) | winx) ; 
				flags |= ALERT ;
			}
			break ;
		}
		return 1 ; // Must not yield if outstanding events
	}

	return ((unsigned int)(now - lastusrev) < BUSYT) ; // Yield if idle
}
