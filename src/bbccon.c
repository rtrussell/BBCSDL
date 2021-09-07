/******************************************************************\
*       BBC BASIC Minimal Console Version                          *
*       Copyright (C) R. T. Russell, 2021                          *

        Modified 2021 by Eric Olson and Memotech-Bill for
        Raspberry Pico

*       bbccon.c Main program, Initialisation, Keyboard handling   *
*       Version 0.36a, 22-Aug-2021                                 *
\******************************************************************/

#define _GNU_SOURCE
#define __USE_GNU
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include "bbccon.h"
#include "align.h"
#define ESCTIME 200  // Milliseconds to wait for escape sequence
#define QRYTIME 1000 // Milliseconds to wait for cursor query response
#define QSIZE 16     // Twice longest expected escape sequence

#ifdef _WIN32
#define HISTORY 100  // Number of items in command history
#include <windows.h>
#include <conio.h>
typedef int timer_t ;
#undef WM_APP
#define myftell _ftelli64
#define myfseek _fseeki64
#define PLATFORM "Win32"
#define realpath(N,R) _fullpath((R),(N),_MAX_PATH)
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x4
#define DISABLE_NEWLINE_AUTO_RETURN 0x8
#define ENABLE_VIRTUAL_TERMINAL_INPUT 0x200
BOOL WINAPI K32EnumProcessModules (HANDLE, HMODULE*, DWORD, LPDWORD) ;
#else
# ifdef PICO
#define HISTORY 10  // Number of items in command history
#include <hardware/flash.h>
#include <hardware/exception.h>
#include <hardware/watchdog.h>
#include <pico/bootrom.h>
#include <pico/stdlib.h>
#include <pico/time.h>
#ifdef STDIO_USB
#include "tusb.h"
#endif
#define WM_TIMER 275
#include "lfswrap.h"
extern char __StackLimit;
# define PLATFORM "Pico"
# else
#define HISTORY 100  // Number of items in command history
#include <termios.h>
#include <signal.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include "dlfcn.h"
#define PLATFORM "Linux"
#define WM_TIMER 275
#define myftell ftell
#define myfseek fseek
# endif
#endif

#ifdef __WIN64__
#undef PLATFORM
#define PLATFORM "Win64"
#endif

#ifdef __APPLE__
#include <dispatch/dispatch.h>
#include <mach-o/dyld.h>
typedef dispatch_source_t timer_t ;
dispatch_queue_t timerqueue ;
#undef PLATFORM
#define PLATFORM "MacOS"
#endif

#undef MAX_PATH
#ifndef MAX
#define MAX(X,Y) (((X) > (Y)) ? (X) : (Y))
#endif

// Program constants:
#define SCREEN_WIDTH  640
#define SCREEN_HEIGHT 500
#define MAX_PATH 260
#define AUDIOLEN 441 * 4

// Global variables (external linkage):

void *userRAM = NULL ;
void *progRAM = NULL ;
void *userTOP = NULL ;
const int bLowercase = 0 ;    // Dummy
const char szVersion[] = "Altered Basic "PLATFORM" Console "VERSION;
const char szNotice[] = 
	"Created by Eric Olson with help\r\n"
	"using altered source from BBC BASIC\r\n"
	"(C) Copyright R. T. Russell, "YEAR"\r\n"
	"Modified by Memotech-Bill" ;
char *szLoadDir ;
char *szLibrary ;
char *szUserDir ;
char *szTempDir ;
char *szCmdLine ;
int MaximumRAM = MAXIMUM_RAM ;
timer_t UserTimerID ;
unsigned int palette[256] ;
#ifdef PICO
void *libtop;
#endif

// Array of VDU command lengths:
static int vdulen[] = {
   0,   1,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   1,   2,   5,   0,   0,   1,   9,   8,   5,   0,   1,   4,   4,   0,   2 } ;

// Keycode translation table:
static unsigned char xkey[64] = {
   0, 139, 138, 137, 136,   0, 131,   0, 130,   0,   0,   0,   0,   0,   0,   0,
 145, 146, 147, 148,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0, 134, 135,   0, 132, 133,   0,   0,   0,   0,   0,   0,   0,   0, 149,
   0, 150, 151, 152, 153, 154,   0, 155, 156,   0,   0,   0,   0,   0,   0,   0 } ;

// Declared in bbcans.c:
void oscli (char *) ;
void xeqvdu (int, int, int) ;
char *setup (char *, char *, char *, char, unsigned char *) ;

// Declared in bbmain.c:
void error (int, const char *) ;
void text (const char *) ;	// Output NUL-terminated string
void crlf (void) ;		// Output a newline

// Declared in bbeval.c:
unsigned int rnd (void) ;	// Return a pseudo-random number

// Interpreter entry point:
int basic (void *, void *, void *) ;

// Forward references:
unsigned char osbget (void*, int*) ;
void osbput (void*, unsigned char) ;
void quiet (void) ;

// Dummy functions:
void gfxPrimitivesSetFont(void) { } ;
void gfxPrimitivesGetFont(void) { } ;

// File scope variables:
static unsigned char inputq[QSIZE] ;
static volatile int inpqr = 0, inpqw = 0 ;

// Put to STDIN queue:
static int putinp (unsigned char inp)
{
	unsigned char bl = inpqw ;
	unsigned char al = (bl + 1) % QSIZE ;
#ifdef STDIO_UART
	static unsigned char bFirst = 1;
	if ( bFirst )
	    {
	    bFirst = 0;
	    if ( inp == 0xFF ) return 0;
	    }
#endif
	if (al != inpqr)
	    {
		inputq[bl] = inp ;
		inpqw = al ;
		return 1 ;
	    }
	return 0 ;
}

#ifdef PICO
inline static void myPoll(){
    static int c = PICO_ERROR_TIMEOUT;
    for(;;){
        if ((c != PICO_ERROR_TIMEOUT) && (putinp(c) == 0))
            break;
        c=getchar_timeout_us(0);
        if (c == PICO_ERROR_TIMEOUT)
            break;
    }
}
#endif

// Get from STDIN queue:
static int getinp (unsigned char *pinp)
{
#ifdef PICO
	myPoll();
#endif
	unsigned char bl = inpqr ;
	if (bl != inpqw)
	    {
		*pinp = inputq[bl] ;
		inpqr = (bl + 1) % QSIZE ;
		return 1 ;
	    }
	return 0 ;
}

#ifdef _WIN32
#define RTLD_DEFAULT (void *)(-1)
static void *dlsym (void *handle, const char *symbol)
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

static DWORD WINAPI myThread (void *parm)
{
	int ch ;
	long unsigned int nread ;

	while (ReadFile (GetStdHandle(STD_INPUT_HANDLE), &ch, 1, &nread, NULL))
	    {
		if (nread)
			while (putinp (ch) == 0)
				sleep (0) ;
		else
			sleep (0) ;
	    }
	return 0 ;
}

#else

void *myThread (void *parm)
{
	int ch ;
	size_t nread ;

	do
	    {
		nread = read (STDIN_FILENO, &ch, 1) ;
		if (nread)
			while (putinp (ch) == 0)
				usleep (1) ;
		else
			usleep (1) ;
	    }
	while (nread >= 0) ;
	return 0 ;
}
#endif

#ifdef __linux__
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

// Put event into event queue, unless full:
int putevt (heapptr handler, int msg, int wparam, int lparam)
{
	unsigned char bl = evtqw ;
	unsigned char al = bl + 8 ;
	int index = bl >> 2 ;
	if ((al == evtqr) || (eventq == NULL))
		return 0 ;
	eventq[index] = lparam ;
	eventq[index + 1] = msg ;
	eventq[index + 64] = wparam ;
	eventq[index + 65] = handler ;
	evtqw = al ;
	return 1 ;
}

// Get event from event queue, unless empty:
static heapptr getevt (void)
{
	heapptr handler ;
	unsigned char al = evtqr ;
	int index = al >> 2 ;
	flags &= ~ALERT ;
	if ((al == evtqw) || (eventq == NULL))
		return 0 ;
	lParam = eventq[index] ;
	iMsg = eventq[index + 1] ;
	wParam = eventq[index + 64] ;
	handler = eventq[index + 65] ;
	al += 8 ;
	evtqr = al ;
	if (al != evtqw)
		flags |= ALERT ;
	return handler ;
}

// Put keycode to keyboard queue:
static int putkey (char key)
{
	unsigned char bl = kbdqw ;
	unsigned char al = bl + 1 ;
	if ((key == 0x1B) && !(flags & ESCDIS))
	    {
		flags |= ESCFLG ;
		return 0 ;
	    }
	if (al != kbdqr)
	    {
		keybdq[(int) bl] = key ;
		kbdqw = al ;
		return 1 ;
	    }
	return 0 ;
}

// Get keycode (if any) from keyboard queue:
int getkey (unsigned char *pkey)
{
#ifdef PICO
	myPoll();
#endif
	unsigned char bl = kbdqr ;
	if (bl != kbdqw)
	    {
		*pkey = keybdq[(int) bl] ;
		kbdqr = bl + 1 ;
		return 1 ;
	    }
	return 0 ;
}

// Get millisecond tick count:
unsigned int GetTicks (void)
{
#ifdef _WIN32
	return timeGetTime () ;
#else
# ifdef PICO
	absolute_time_t t=get_absolute_time();
	return to_ms_since_boot(t);
# else
	struct timespec ts ;
	if (clock_gettime (CLOCK_MONOTONIC, &ts))
		clock_gettime (CLOCK_REALTIME, &ts) ;
	return (ts.tv_sec * 1000) + (ts.tv_nsec / 1000000) ;
# endif
#endif
}

static int kbchk (void)
{
#ifdef PICO
	myPoll();
#endif
	return (inpqr != inpqw) ;
}

static unsigned char kbget (void)
{
	unsigned char ch = 0 ;
	getinp (&ch) ;
	return ch ;
}

static int kwait (unsigned int timeout)
{
	int ready ;
	timeout += GetTicks () ;
	while (!(ready = kbchk ()) && (GetTicks() < timeout))
		usleep (1) ;
	return ready ;
}

// Returns 1 if the cursor position was read successfully or 0 if it was aborted
// (in which case *px and *py will be unchanged) or if px and py are both NULL.
int stdin_handler (int *px, int *py)
{
	int wait = (px != NULL) || (py != NULL) ;
	static char report[16] ;
	static char *p = report, *q = report ;
	unsigned char ch ;

	if (wait)
	    {
		printf ("\033[6n") ;
		fflush (stdout) ;
	    }

	do
	    {
		if (kbchk ())
		    {
			ch = kbget () ;
				
			if (ch == 0x1B)
			    {
				if (p != report)
					q = p ;
				*p++ = 0x1B ;
			    }
			else if (p != report)
			    {
				if (p < report + sizeof(report))
					*p++ = ch ;
				if ((ch >= 'A') && (ch <= '~') && (ch != '[') && (ch != 'O') &&
					((*(q + 1) == '[') || (*(q + 1) == 'O')))
				    {
					p = q ;
					q = report ;
								
					if (ch == 'R')
					    {
						int row = 0, col = 0 ;
						if (sscanf (p, "\033[%d;%dR", &row, &col) == 2)
						    {
							if (px) *px = col - 1 ;
							if (py) *py = row - 1 ;
							return 1 ;
						    }
					    }
					if (ch == '~')
					    {
						int key ;
						if (sscanf (p, "\033[%d~", &key))
							putkey (xkey[key + 32]) ;
					    }
					else
						putkey (xkey[ch - 64]) ;
				    }
			    }
			else
			    {
				putkey (ch) ;
				if (((kbdqr - kbdqw - 1) & 0xFF) < 50)
				    {
					p = report ;
					q = report ;
					wait = 0 ; // abort
				    }
			    }
		    }
		if ((wait || (p != report)) && !kwait (wait ? QRYTIME : ESCTIME))
		    {
			q = report ;
			while (q < p) putkey (*q++) ;
			p = report ;
			q = report ;
			wait = 0 ; // abort
		    }
	    }
	while (wait || (p != report)) ;
	return 0 ;
}

// Get text cursor (caret) coordinates:
void getcsr(int *px, int *py)
{
	if (!stdin_handler (px, py))
	    {
		if (px != NULL) *px = -1 ; // Flag unable to read
		if (py != NULL) *py = -1 ;
	    }
}

// SOUND Channel,Amplitude,Pitch,Duration
void sound (short chan, signed char ampl, unsigned char pitch, unsigned char duration)
{
}

// ENVELOPE N,T,PI1,PI2,PI3,PN1,PN2,PN3,AA,AD,AS,AR,ALA,ALD
void envel (signed char *env)
{
}

// Disable sound generation:
void quiet (void)
{
}

// Get pixel RGB colour:
int vtint (int x, int y)
{
	error (255, "Sorry, not implemented") ;
	return -1 ;
}

// Get nearest palette index:
int vpoint (int x, int y)
{
	error (255, "Sorry, not implemented") ;
	return -1 ;
}

int vgetc (int x, int y)
{
	error (255, "Sorry, not implemented") ;
	return -1 ;
}

int osbyte (int al, int xy)
{
	error (255, "Sorry, not implemented") ;
	return -1 ;
}

void osword (int al, void *xy)
{
	error (255, "Sorry, not implemented") ;
	return ;
}

// Get string width in graphics units:
int widths (unsigned char *s, int l)
{
	error (255, "Sorry, not implemented") ;
	return -1 ;
}

// ADVAL(n)
int adval (int n)
{
	if (n == -1)
	    	return (kbdqr - kbdqw - 1) & 0xFF ;
	error (255, "Sorry, not implemented") ;
	return -1 ;
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
#endif
	long long wrapper (volatile double a, volatile double b, volatile double c, volatile double d,
			volatile double e, volatile double f, volatile double g, volatile double h)
	{
		long long result ;
#ifdef _WIN32
		static void* savesp ;
		asm ("mov %%esp,%0" : "=m" (savesp)) ;
#endif
		result = APIfunc (p->i[0], p->i[1], p->i[2], p->i[3], p->i[4], p->i[5],
				p->i[6], p->i[7], p->i[8], p->i[9], p->i[10], p->i[11]) ;
#ifdef _WIN32
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
#endif
	double wrapper (volatile double a, volatile double b, volatile double c, volatile double d,
			volatile double e, volatile double f, volatile double g, volatile double h)
	{
		double result ;
#ifdef _WIN32
		static void* savesp ;
		asm ("mov %%esp,%0" : "=m" (savesp)) ;
#endif
		result = APIfunc (p->i[0], p->i[1], p->i[2], p->i[3], p->i[4], p->i[5],
				p->i[6], p->i[7], p->i[8], p->i[9], p->i[10], p->i[11]) ;
#ifdef _WIN32
		asm ("mov %0,%%esp" : : "m" (savesp)) ;
#endif
		return result ;
	}

	return wrapper (p->f[0], p->f[1], p->f[2], p->f[3], p->f[4], p->f[5], p->f[6], p->f[7]) ;
}
#pragma GCC reset_options
#endif

// Call a function in the context of the GUI thread:
size_t guicall (void *func, PARM *parm)
{
	return apicall_ (func, parm) ;
}

// Check for Escape (if enabled) and kill:
void trap (void)
{
	stdin_handler (NULL, NULL) ;

	if (flags & KILL)
		error (-1, NULL) ; // Quit
	if (flags & ESCFLG)
	    {
		flags &= ~ESCFLG ;
		kbdqr = kbdqw ;
		quiet () ;
		if (exchan)
		    {
			fclose (exchan) ;
			exchan = NULL ;
		    }
		error (17, NULL) ; // 'Escape'
	    }
}

// Test for escape, kill, pause, single-step, flash and alert:
heapptr xtrap (void)
{
	trap () ;
	if (flags & ALERT)
		return getevt () ;
	return 0 ;
}

// Report a 'fatal' error:
void faterr (const char *msg)
{
#ifdef PICO
	fprintf (stdout, "%s\r\n", msg) ;
#else
	fprintf (stderr, "%s\r\n", msg) ;
#endif
	error (256, "") ;
}

// Read keyboard or F-key expansion:
static int rdkey (unsigned char *pkey)
{
	if (keyptr)
	    {
		*pkey = *keyptr++ ;
		if (*keyptr == 0)
			keyptr = NULL ;
		return 1 ;
	    }

	while (getkey (pkey))
	    {
		int keyno = 0 ;

		if ((*pkey >= 128) && (*pkey < 156))
			keyno = *pkey ^ 144 ;
		if (keyno >= 24)
			keyno -= 12 ;
		if (*pkey == 127)
		    {
			*pkey = 8 ;
			keyno = 24 ;
		    }
		if (keyno)
		    {
			keyptr = *((unsigned char **)keystr + keyno) ;
			if (keyptr)
			    {
				*pkey = *keyptr++ ;
				if (*keyptr == 0)
					keyptr = NULL ;
			    }
		    }
		return 1 ;
	    }
	return 0 ;
}

// Wait a maximum period for a keypress, or test key asynchronously:
int oskey (int wait)
{
	if (wait >= 0)
	    {
		unsigned int start = GetTicks () ;
		while (1)
		    {
			unsigned char key ;
			trap () ;
			if (rdkey (&key))
				return (int) key ;
			if ((unsigned int)(GetTicks () - start) >= wait * 10)
				return -1 ;
			usleep (5000) ;
		    }
	    }

	if (wait == -256)
		return 's' ;

	return 0 ;
}

// Wait for keypress:
unsigned char osrdch (void)
{
	unsigned char key ;
	if (exchan)
	{
		if (fread (&key, 1, 1, exchan))
			return key ;
		fclose (exchan) ;
 		exchan = NULL ;
	}

	if (optval >> 4)
		return osbget ((void *)(size_t)(optval >> 4), NULL) ;

	while (!rdkey (&key))
	{
		usleep (5000) ;
		trap () ;
	}
	return key ;
}

// Output byte to VDU stream:
void oswrch (unsigned char vdu)
{
	unsigned char *pqueue = vduq ;

	if (optval & 0x0F)
	    {
		osbput ((void *)(size_t)(optval & 0x0F), vdu) ;
		return ;
	    }

	if (spchan)
	    {
		if (0 == fwrite (&vdu, 1, 1, spchan))
		    {
			fclose (spchan) ;
			spchan = NULL ;
		    }
	    }

	if (vduq[10] != 0)		// Filling queue ?
	    {
		vduq[9 - vduq[10]] = vdu ;
		vduq[10] -- ;
		if (vduq[10] != 0)
			return ;
		vdu = vduq[9] ;
		if (vdu >= 0x80)
		    {
			xeqvdu (vdu << 8, 0, 0) ;
			int ecx = (vdu >> 4) & 3 ;
			if (ecx == 0) ecx++ ;
			pqueue -= ecx - 9 ;
				for ( ; ecx > 0 ; ecx--)
					xeqvdu (*pqueue++ << 8, 0, 0) ;
			fflush (stdout) ;
			return ;
		    }
	    }
	else if (vdu >= 0x20)
	    {
		if ((vdu >= 0x80) && (vflags & UTF8))
		    {
			char ah = (vdu >> 4) & 3 ;
			if (ah == 0) ah++ ;
			vduq[10] = ah ;	// No. of bytes to follow
			vduq[9] = vdu ;
			return ;
		    }
		xeqvdu (vdu << 8, 0, 0) ;
		fflush (stdout) ;
		return ;
	    }
	else
	    {
		vduq[10] = vdulen[(int) vdu] ;
		vduq[9] = vdu ;
		if (vduq[10] != 0)
			return ;
	    }

// Mapping of VDU queue to UserEvent parameters,
// VDU 23 (defchr) has the most parameter bytes:
//
//          0  ^
// status-> 0  | ev.user.code
//          V  |
//          h  v
//          g  ^
//          f  | ev.user.data1
//          e  |
//          d  v
//          c  ^
//          b  | ev.user.data2
//          a  |
//  vduq->  n  v

	xeqvdu (*(int*)(pqueue + 8) & 0xFFFF, *(int*)(pqueue + 4), *(int*)pqueue) ;
}

// Prepare for outputting an error message:
void reset (void)
{
	vduq[10] = 0 ;	// Flush VDU queue
	keyptr = NULL ;	// Cancel *KEY expansion
	optval = 0 ;	// Cancel I/O redirection
	reflag = 0 ;	// *REFRESH ON
}

// Input and edit a string :
void osline (char *buffer)
{
	static char *history[HISTORY] = {NULL} ;
	static int empty = 0 ;
	int current = empty ;
	char *eol = buffer ;
	char *p = buffer ;
	*buffer = 0x0D ;
	int n ;

	while (1)
	    {
		unsigned char key ;

		key = osrdch () ;
		switch (key)
		    {
			case 0x0A:
			case 0x0D:
				n = (char *) memchr (buffer, 0x0D, 256) - buffer ;
				if (n == 0)
					return ;
				if ((current == (empty + HISTORY - 1) % HISTORY) &&
						(0 == memcmp (buffer, history[current], n)))
					return ;
				history[empty] = malloc (n + 1) ;
				memcpy (history[empty], buffer, n + 1) ;
				empty = (empty + 1) % HISTORY ;
				if (history[empty])
				    {
					free (history[empty]) ;
					history[empty] = NULL ;
				    }
				return ;

			case 8:
			case 127:
				if (p > buffer)
				    {
					char *q = p ;
					do p-- ; while ((vflags & UTF8) && (*(signed char*)p < -64)) ;
					oswrch (8) ;
					memmove (p, q, buffer + 256 - q) ;
				    }
				break ;

			case 21:
				while (p > buffer)
				    {
					char *q = p ;
					do p-- ; while ((vflags & UTF8) && (*(signed char*)p < -64)) ;
					oswrch (8) ;
					memmove (p, q, buffer + 256 - q) ;
				    }
				break ;

			case 130:
				while (p > buffer)
				    {
					oswrch (8) ;
					do p-- ; while ((vflags & UTF8) && (*(signed char*)p < -64)) ;
				    }
				break ;

			case 131:
				while (*p != 0x0D)
				    {
					oswrch (9) ;
					do p++ ; while ((vflags & UTF8) && (*(signed char*)p < -64)) ;
				    }
				break ;

			case 134:
				vflags ^= IOFLAG ;
				if (vflags & IOFLAG)
					printf ("\033[1 q") ;
				else
					printf ("\033[3 q\033[7 q") ;
				break ;

			case 135:
				if (*p != 0x0D)
				    {
					char *q = p ;
					do q++ ; while ((vflags & UTF8) && (*(signed char*)q < -64)) ;
					memmove (p, q, buffer + 256 - q) ;
				    }
				break ;

			case 136:
				if (p > buffer)
				    {
					oswrch (8) ;
					do p-- ; while ((vflags & UTF8) && (*(signed char*)p < -64)) ;
				    }
				break ;

			case 137:
				if (*p != 0x0D)
				    {
					oswrch (9) ;
					do p++ ; while ((vflags & UTF8) && (*(signed char*)p < -64)) ;
				    }
				break ;

			case 138:
			case 139:
				if (key == 138)
					n = (current + 1) % HISTORY ;
				else
					n = (current + HISTORY - 1) % HISTORY ;
				if (history[n])
				    {
					char *s = history[n] ;
					while (*p != 0x0D)
					    {
						oswrch (9) ;
						do p++ ; while ((vflags & UTF8) &&
							(*(signed char*)p < -64)) ;
					    }
					while (p > buffer)
					    {
						oswrch (127) ;
						do p-- ; while ((vflags & UTF8) &&
							 (*(signed char*)p < -64)) ;
				 	   }
					while ((*s != 0x0D) && (p < (buffer + 255)))
					    {
						oswrch (*s) ;
						*p++ = *s++ ;
					    }
					*p = 0x0D ;
					current = n ;
				    }
				break ;

			case 132:
			case 133:
			case 140:
			case 141:
				break ;

			case 9:
				key = ' ';
			default:
				if (p < (buffer + 255))
				    {
					if (key != 0x0A)
					    {
						oswrch (key) ;
					    }
					if (key >= 32)
					    {
						memmove (p + 1, p, buffer + 255 - p) ;
						*p++ = key ;
						*(buffer + 255) = 0x0D ;
						if ((*p != 0x0D) && (vflags & IOFLAG))
						    {
							char *q = p ;
							do q++ ; while ((vflags & UTF8) &&
								(*(signed char*)q < -64)) ;
							memmove (p, q, buffer + 256 - q) ;
						    }
					    }
				    }
		    }

		if (*p != 0x0D)
		    {
			oswrch (23) ;
			oswrch (1) ;
			for (n = 8 ; n != 0 ; n--)
				oswrch (0) ;
		    }
		n = 0 ;
		while (*p != 0x0D)
		    {
			oswrch (*p++) ;
			n++ ;
		    }
		for (int i = 0; i < (eol - p); i++)
			oswrch (32) ;
		for (int i = 0; i < (eol - p); i++)
			oswrch (8) ;
		eol = p ;
		while (n)
		    {
			if (!(vflags & UTF8) || (*(signed char*)p >= -64))
				oswrch (8) ;
			p-- ;
			n-- ;
		    }
		if (*p != 0x0D)
		    {
			oswrch (23) ;
			oswrch (1) ;
			oswrch (1) ;
			for (n = 7 ; n != 0 ; n--)
				oswrch (0) ;
		    }
	    }
}

// Get TIME
int getime (void)
{
	unsigned int n = GetTicks () ;
	if (n < lastick)
		timoff += 0x19999999 ;
	lastick = n ;
	return n / 10 + timoff ;
}

int getims (void)
{
	char *at ;
	time_t tt ;

	time (&tt) ;
	at = ctime (&tt) ;
	ISTORE(accs + 0, ILOAD(at + 0)) ; // Day-of-week
	ISTORE(accs + 4, ILOAD(at + 8)) ; // Day-of-month
	ISTORE(accs + 7, ILOAD(at + 4)) ; // Month
	ISTORE(accs + 11, ILOAD(at + 20)) ; // Year
	if (*(accs + 4) == ' ') *(accs + 4) = '0' ;
	memcpy (accs + 16, at + 11, 8) ; // Time
	accs[3] = '.' ;
	accs[15] = ',' ;
	return 24 ;
}

// Put TIME
void putime (int n)
{
	lastick = GetTicks () ;
	timoff = n - lastick / 10 ;
}

// Wait for a specified number of centiseconds:
// On some platforms specifying a negative value causes a task switch
void oswait (int cs)
{
	unsigned int start = GetTicks () ;
	if (cs < 0)
	    {
		sleep (0) ;
		return ;
	    }
	cs *= 10 ;
	do
	    {
		trap () ;
		usleep (1000) ;
	    }
	while ((unsigned int)(GetTicks () - start) < cs) ;
}


// MOUSE x%, y%, b%
void mouse (int *px, int *py, int *pb)
{
	if (px) *px = 0 ;
	if (py) *py = 0 ;
	if (pb) *pb = 0 ;
}

// MOUSE ON [type]
void mouseon (int type)
{
}

// MOUSE OFF
void mouseoff (void)
{
}

// MOUSE TO x%, y%
void mouseto (int x, int y)
{
}

// Get address of an API function:
void *sysadr (char *name)
{
#ifdef PICO
	extern void *sympico (char *name) ;
	return sympico (name) ;
#else
	void *addr = NULL ;
	if (addr != NULL)
		return addr ; 
	return dlsym (RTLD_DEFAULT, name) ;
#endif
}

// Call an emulated OS subroutine (if CALL or USR to an address < 0x10000)
int oscall (int addr)
{
	int al = stavar[1] ;
	void *xy = (void *) ((size_t)stavar[24] | ((size_t)stavar[25] << 8)) ;
	switch (addr)
	    {
		case 0xFFE0: // OSRDCH
			return (int) osrdch () ;

		case 0xFFE3: // OSASCI
			if (al != 0x0D)
			    {
				oswrch (al) ;
				return 0 ;
			    }

		case 0xFFE7: // OSNEWL
			crlf () ;
			return 0 ;

		case 0xFFEE: // OSWRCH
			oswrch (al) ;
			return 0 ;

		case 0xFFF1: // OSWORD
			memcpy (xy + 1, &bbcfont[*(unsigned char*)(xy) << 3], 8) ;
			return 0 ;

		case 0xFFF4: // OSBYTE
			return (vgetc (0x80000000, 0x80000000) << 8) ;

		case 0xFFF7: // OSCLI
			oscli (xy) ;
			return 0 ; 

		default:
			error (8, NULL) ; // 'Address out of range'
	    }
	return 0 ;
}

// Request memory allocation above HIMEM:
heapptr oshwm (void *addr, int settop)
{
#ifdef _WIN32
	if ((addr < userRAM) ||
	    (addr > (userRAM + MaximumRAM)) ||
	    (NULL == VirtualAlloc (userRAM, addr - userRAM,
			MEM_COMMIT, PAGE_EXECUTE_READWRITE)))
		return 0 ;
#else
	if ((addr < userRAM) ||
	    (addr > (userRAM + MaximumRAM)))
		return 0 ;
#endif
	else
	    {
		if (settop && (addr > userTOP))
			userTOP = addr ;
		return (size_t) addr ;
	    }
}

// Get a file context from a channel number:
static FILE *lookup (void *chan)
{
	FILE *file = NULL ;

	if ((chan >= (void *)1) && (chan <= (void *)(MAX_PORTS + MAX_FILES)))
		file = (FILE*) filbuf[(size_t)chan] ;
	else
		file = (FILE*) chan ;

	if (file == NULL)
		error (222, "Invalid channel") ;
	return file ;
}

// Load a file into memory:
void osload (char *p, void *addr, int max)
{
	int n ;
	FILE *file ;
	if (NULL == setup (path, p, ".bbc", '\0', NULL))
		error (253, "Bad string") ;
	file = fopen (path, "rb") ;
	if (file == NULL)
		error (214, "File or path not found") ;
	n = fread (addr, 1, max, file) ;
	fclose (file) ;
	if (n == 0)
		error (189, "Couldn't read from file") ;
}

// Save a file from memory:
void ossave (char *p, void *addr, int len)
{
	int n ;
	FILE *file ;
	if (NULL == setup (path, p, ".bbc", '\0', NULL))
		error (253, "Bad string") ;
	file = fopen (path, "w+b") ;
	if (file == NULL)
		error (214, "Couldn't create file") ;
	n = fwrite (addr, 1, len, file) ;
	fclose (file) ;
	if (n < len)
		error (189, "Couldn't write to file") ;
}

// Open a file:
void *osopen (int type, char *p)
{
	int chan, first, last ;
	FILE *file ;
	if (setup (path, p, ".bbc", '\0', NULL) == NULL)
		return 0 ;
	if (type == 0)
		file = fopen (path, "rb") ;
	else if (type == 1)
		file = fopen (path, "w+b") ;
	else
		file = fopen (path, "r+b") ;
	if (file == NULL)
		return NULL ;

	if (0 == memcmp (path, "/dev", 4))
	    {
		first = 1 ;
		last = MAX_PORTS ;
	    }
	else
	    {
		first = MAX_PORTS + 1 ;
		last = MAX_PORTS + MAX_FILES ;
	    }

	for (chan = first; chan <= last; chan++)
	    {
		if (filbuf[chan] == 0)
		    {
			filbuf[chan] = file ;
			if (chan > MAX_PORTS)
				*(int *)&fcbtab[chan - MAX_PORTS - 1] = 0 ;
			return (void *)(size_t)chan ;
		    }
	    }
	fclose (file) ;
	error (192, "Too many open files") ;
	return NULL ; // never happens
}

// Read file to 256-byte buffer:
static void readb (FILE *context, unsigned char *buffer, FCB *pfcb)
{
	int amount ;
	if (context == NULL)
		error (222, "Invalid channel") ;
//	if (pfcb->p != pfcb->o) Windows requires fseek to be called ALWAYS
	myfseek (context, (pfcb->p - pfcb->o) & 0xFF, SEEK_CUR) ;
#ifdef _WIN32
	long long ptr = myftell (context) ;
#endif
	amount = fread (buffer, 1, 256, context) ;
#ifdef _WIN32
	myfseek (context, ptr + amount, SEEK_SET) ; // filetest.bbc fix (32-bit)
#endif
	pfcb->p = 0 ;
	pfcb->o = amount & 0xFF ;
	pfcb->w = 0 ;
	pfcb->f = (amount != 0) ;
	while (amount < 256)
		buffer[amount++] = 0 ;
	return ;
}

// Write 256-byte buffer to file:
static int writeb (FILE *context, unsigned char *buffer, FCB *pfcb)
{
	int amount ;
	if (pfcb->f >= 0)
	    {
		pfcb->f = 0 ;
		return 0 ;
	    }
	if (context == NULL)
		error (222, "Invalid channel") ;
	if (pfcb->f & 1)
		myfseek (context, pfcb->o ? -pfcb->o : -256, SEEK_CUR) ;
#ifdef _WIN32
	long long ptr = myftell (context) ;
#endif
	amount = fwrite (buffer, 1, pfcb->w ? pfcb->w : 256, context) ;
#ifdef _WIN32
	myfseek (context, ptr + amount, SEEK_SET) ; // assemble.bbc asmtst64.bba fix
#endif
	pfcb->o = amount & 0xFF ;
	pfcb->w = 0 ;
	pfcb->f = 1 ;
	return (amount == 0) ;
}

// Close a single file:
static int closeb (void *chan)
{
	int result ;
	if ((chan > (void *)MAX_PORTS) && (chan <= (void *)(MAX_PORTS+MAX_FILES)))
	    {
		int index = (size_t) chan - MAX_PORTS - 1 ;
		unsigned char *buffer = (unsigned char *) filbuf[0] + index * 0x100 ;
		FCB *pfcb = &fcbtab[index] ;
		FILE *handle = (FILE *) filbuf[(size_t) chan] ;
		if (writeb (handle, buffer, pfcb))
			return 1 ;
	    }
	result = fclose (lookup (chan)) ;
	if ((chan >= (void *)1) && (chan <= (void *)(MAX_PORTS + MAX_FILES)))
		filbuf[(size_t)chan] = 0 ;
	return result ;
}

// Read a byte:
unsigned char osbget (void *chan, int *peof)
{
	unsigned char byte = 0 ;
	if (peof != NULL)
		*peof = 0 ;
	if ((chan > (void *)MAX_PORTS) && (chan <= (void *)(MAX_PORTS+MAX_FILES)))
	    {
		int index = (size_t) chan - MAX_PORTS - 1 ;
		unsigned char *buffer = (unsigned char *) filbuf[0] + index * 0x100 ;
		FCB *pfcb = &fcbtab[index] ;
		if (pfcb->p == pfcb->o)
		    {
			FILE *handle = (FILE *) filbuf[(size_t) chan] ;
			if (writeb (handle, buffer, pfcb))
				error (189, "Couldn't write to file") ;
			readb (handle, buffer, pfcb) ;
			if ((pfcb->f & 1) == 0)
			    {
				if (peof != NULL)
					*peof = 1 ;
				return 0 ;
			    } 
		    }
		return buffer[pfcb->p++] ;
	    }
	if ((0 == fread (&byte, 1, 1, lookup (chan))) && (peof != NULL))
		*peof = 1 ;
	return byte ;
}

// Write a byte:
void osbput (void *chan, unsigned char byte)
{
	if ((chan > (void *)MAX_PORTS) && (chan <= (void *)(MAX_PORTS+MAX_FILES)))
	    {
		int index = (size_t) chan - MAX_PORTS - 1 ;
		unsigned char *buffer = (unsigned char *) filbuf[0] + index * 0x100 ;
		FCB *pfcb = &fcbtab[index] ;
		if (pfcb->p == pfcb->o)
		    {
			FILE *handle = (FILE *) filbuf[(size_t) chan] ;
			if (writeb (handle, buffer, pfcb))
				error (189, "Couldn't write to file") ;
			readb (handle, buffer, pfcb) ;
		    }
		buffer[pfcb->p++] = byte ;
		pfcb->w = pfcb->p ;
		pfcb->f |= 0x80 ;
		return ;
	    }
	if (0 == fwrite (&byte, 1, 1, lookup (chan)))
		error (189, "Couldn't write to file") ;
}

// Get file pointer:
long long getptr (void *chan)
{
	myfseek (lookup (chan), 0, SEEK_CUR) ;
	long long ptr = myftell (lookup (chan)) ;
	if (ptr == -1)
		error (189, "Couldn't read file pointer") ;
	if ((chan > (void *)MAX_PORTS) && (chan <= (void *)(MAX_PORTS+MAX_FILES)))
	    {
		FCB *pfcb = &fcbtab[(size_t) chan - MAX_PORTS - 1] ;
		if (pfcb->o)
			ptr -= pfcb->o ;
		else if (pfcb->f & 1)
			ptr -= 256 ;
		if (pfcb->p)
			ptr += pfcb->p ;
		else if (pfcb->f & 0x81)
			ptr += 256 ;
	    }
	return ptr ;
}

// Set file pointer:
void setptr (void *chan, long long ptr)
{
	if ((chan > (void *)MAX_PORTS) && (chan <= (void *)(MAX_PORTS+MAX_FILES)))
	    {
		int index = (size_t) chan - MAX_PORTS - 1 ;
		unsigned char *buffer = (unsigned char *) filbuf[0] + index * 0x100 ;
		FCB *pfcb = &fcbtab[index] ;
		FILE *handle = (FILE *) filbuf[(size_t) chan] ;
		if (writeb (handle, buffer, pfcb))
			error (189, "Couldn't write to file") ;
		*(int *)pfcb = 0 ;
	    }
	if (-1 == myfseek (lookup (chan), ptr, SEEK_SET))
		error (189, "Couldn't set file pointer") ;
}

// Get file size:
long long getext (void *chan)
{
	long long newptr = getptr (chan) ;
	FILE *file = lookup (chan) ;
	myfseek (file, 0, SEEK_CUR) ;
	long long ptr = myftell (file) ;
	myfseek (file, 0, SEEK_END) ;
	long long size = myftell (file) ;
	if ((ptr == -1) || (size == -1))
		error (189, "Couldn't set file pointer") ;
	myfseek (file, ptr, SEEK_SET) ;
	if (newptr > size)
		return newptr ;
	return size ;
}

// Get EOF status:
long long geteof (void *chan)
{
	if ((chan > (void *)MAX_PORTS) && (chan <= (void *)(MAX_PORTS+MAX_FILES)))
	    {
		FCB *pfcb = &fcbtab[(size_t) chan - MAX_PORTS - 1] ;
		if ((pfcb->p != 0) && (pfcb->o == 0) && ((pfcb->f) & 1))
			return 0 ;
	    }
	return -(getptr (chan) >= getext (chan)) ;
}

// Close file (if chan = 0 all open files closed and errors ignored):
void osshut (void *chan)
{
	if (chan == NULL)
	    {
		int chan ;
		for (chan = MAX_PORTS + MAX_FILES; chan > 0; chan--)
		    {
			if (filbuf[chan])
				closeb ((void *)(size_t)chan) ; // ignore errors
		    }
		return ;
	    }
	if (closeb (chan))
		error (189, "Couldn't close file") ;
}

// Start interpreter:
int entry (void *immediate)
{
        // memset (&stavar[1], 0, (char *)datend - (char *)&stavar[1]) ;

	accs = (char*) userRAM ;		// String accumulator
	buff = (char*) accs + ACCSLEN ;		// Temporary string buffer
	path = (char*) buff + 0x100 ;		// File path
	keystr = (char**) (path + 0x100) ;	// *KEY strings
	keybdq = (char*) keystr + 0x100 ;	// Keyboard queue
	eventq = (void*) keybdq + 0x100 ;	// Event queue
	filbuf[0] = (eventq + 0x200 / 4) ;	// File buffers n.b. pointer arithmetic!!

	farray = 1 ;				// @hfile%() number of dimensions
	fasize = MAX_PORTS + MAX_FILES + 4 ;	// @hfile%() number of elements

#ifndef _WIN32
	vflags = UTF8 ;				// Not |= (fails on Linux build)
#endif

	prand = (unsigned int) GetTicks () ;	/// Seed PRNG
	*(unsigned char*)(&prand + 1) = (prand == 0) ;
	rnd () ;				// Randomise !

	memset (keystr, 0, 256) ;
	xeqvdu (0x1700, 0, 0x1F) ;		// initialise VDU drivers
	spchan = NULL ;
	exchan = NULL ;

	if (immediate)
	    {
		text (szVersion) ;
		crlf () ;
		text (szNotice) ;
		crlf () ;
	    }
	return basic (progRAM, userTOP, immediate) ;
}

#ifdef _WIN32
static void UserTimerProc (UINT uUserTimerID, UINT uMsg, void *dwUser, void *dw1, void *dw2)
{
	if (timtrp)
		putevt (timtrp, WM_TIMER, 0, 0) ;
	flags |= ALERT ; // Always, for periodic ESCape detection
	return ;
}

timer_t StartTimer (int period)
{
	return timeSetEvent (period, 0, (LPTIMECALLBACK) UserTimerProc, 0, TIME_PERIODIC) ; 
}

void StopTimer (timer_t timerid)
{
	timeKillEvent (timerid) ;
}

void SystemIO (int flag)
{
	if (!flag)
		SetConsoleMode (GetStdHandle(STD_INPUT_HANDLE), ENABLE_VIRTUAL_TERMINAL_INPUT) ; 
}
#endif

#ifdef __linux__
static void UserTimerProc (int sig, siginfo_t *si, void *uc)
{
	if (timtrp)
		putevt (timtrp, WM_TIMER, 0, 0) ;
	flags |= ALERT ; // Always, for periodic ESCape detection
}

timer_t StartTimer (int period)
{
	timer_t timerid ;
	struct sigevent evp = {0} ;
	struct sigaction sig ;
	struct itimerspec tm ;

	tm.it_interval.tv_nsec = (period % 1000) * 1000000 ;
	tm.it_interval.tv_sec = (period / 1000) ;
	tm.it_value = tm.it_interval ;

	sig.sa_handler = (void *) &UserTimerProc ;
	sigemptyset (&sig.sa_mask) ;
	sig.sa_flags = 0 ;

	evp.sigev_value.sival_ptr = &timerid ;
	evp.sigev_notify = SIGEV_SIGNAL ;
	evp.sigev_signo = SIGALRM ;

	timer_create (CLOCK_MONOTONIC, &evp, &timerid) ;
	timer_settime (timerid, 0, &tm, NULL) ;
	sigaction (SIGALRM, &sig, NULL) ;

	return timerid ;
}

void StopTimer (timer_t timerid)
{
	timer_delete (timerid) ;
}

void SystemIO (int flag)
{
	struct termios tmp ;
	tcgetattr (STDIN_FILENO, &tmp) ;
	if (flag)
	    {
		tmp.c_lflag |= ISIG ;
		tmp.c_oflag |= OPOST ;
	    }
	else
	    {
		tmp.c_lflag &= ~ISIG ;
		tmp.c_oflag &= ~OPOST ;
	    }
	tcsetattr (STDIN_FILENO, TCSAFLUSH, &tmp) ;
}
#endif

#ifdef PICO
static bool UserTimerProc (struct repeating_timer *prt)
    {
//    myPoll ();
    if (timtrp)
	putevt (timtrp, WM_TIMER, 0, 0) ;
    flags |= ALERT ; // Always, for periodic ESCape detection
    return true;
    }

static struct repeating_timer s_rpt_timer;
timer_t StartTimer (int period)
    {
    timer_t dummy;
    add_repeating_timer_ms (period, UserTimerProc, NULL, &s_rpt_timer);
    return dummy;
    }

void StopTimer (timer_t timerid)
    {
    cancel_repeating_timer (&s_rpt_timer);
    return;
    }

void SystemIO (int flag) {
	return;
}
#endif

#ifdef __APPLE__
static void UserTimerProc (dispatch_source_t timerid)
{
	if (timtrp)
		putevt (timtrp, WM_TIMER, 0, 0) ;
	flags |= ALERT ; // Always, for periodic ESCape detection
}

timer_t StartTimer (int period)
{
	dispatch_source_t timerid ;
	dispatch_time_t start ;

	timerid = dispatch_source_create (DISPATCH_SOURCE_TYPE_TIMER, 0, 0, timerqueue) ;

	dispatch_source_set_event_handler (timerid, ^{UserTimerProc(timerid);}) ;
	start = dispatch_time (DISPATCH_TIME_NOW, period * 1000000) ;
	dispatch_source_set_timer (timerid, start, period * 1000000, 0) ;
	dispatch_resume (timerid) ;

	return timerid ;
}

void StopTimer (timer_t timerid)
{
	dispatch_source_cancel (timerid) ;
}

void SystemIO (int flag)
{
	struct termios tmp ;
	tcgetattr (STDIN_FILENO, &tmp) ;
	if (flag)
	    {
		tmp.c_lflag |= ISIG ;
		tmp.c_oflag |= OPOST ;
	    }
	else
	    {
		tmp.c_lflag &= ~ISIG ;
		tmp.c_oflag &= ~OPOST ;
	    }
	tcsetattr (STDIN_FILENO, TCSAFLUSH, &tmp) ;
}
#endif

static void SetLoadDir (char *path)
{
	char temp[MAX_PATH] ;
	char *p ;
	strcpy (temp, path) ;
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

#ifdef _WIN32
	strcat (szLoadDir, "\\") ;
#else
	strcat (szLoadDir, "/") ;
#endif
}

#ifdef PICO
void sigbus(void){
	printf("SIGBUS exception caught...\n");
	for(;;);
}
#endif

int main (int argc, char* argv[])
{
#ifdef PICO
    stdio_init_all();
# ifdef FREE
	exception_set_exclusive_handler(HARDFAULT_EXCEPTION,sigbus);
# else
	extern void __attribute__((used,naked)) HardFault_Handler(void);
	exception_set_exclusive_handler(HARDFAULT_EXCEPTION,HardFault_Handler);
# endif
#ifdef STDIO_USB
	printf("Waiting for usb host");
	while (!tud_cdc_connected()) {
		printf(".");
		sleep_ms(500);
	}
#else
	// Wait for UART connection
	const uint LED_PIN = PICO_DEFAULT_LED_PIN;
	gpio_init(LED_PIN);
	gpio_set_dir(LED_PIN, GPIO_OUT);
	for (int i = 10; i > 0; --i )
	    {
	    gpio_put(LED_PIN, 1);
	    sleep_ms(500);
	    gpio_put(LED_PIN, 0);
	    sleep_ms(500);
	    printf ("%d seconds to start\n", i);
	    }
#endif
	char *cmdline[]={"bbcbasic",0};
	argc=1; argv=cmdline;
	printf("\n");
	mount ();
#endif
int i ;
char *env, *p, *q ;
int exitcode = 0 ;
void *immediate ;
FILE *ProgFile, *TestFile ;
char szAutoRun[MAX_PATH + 1] ;

#ifdef _WIN32
int orig_stdout = -1 ;
int orig_stdin = -1 ;
HANDLE hThread = NULL ;

	platform = 0 ;

	// Allocate the program buffer
	// First reserve the maximum amount:

	char *pstrVirtual = NULL ;

	while ((MaximumRAM >= DEFAULT_RAM) &&
		(NULL == (pstrVirtual = (char*) VirtualAlloc (NULL, MaximumRAM,
						MEM_RESERVE, PAGE_EXECUTE_READWRITE))))
		MaximumRAM /= 2 ;

	// Now commit the initial amount to physical RAM:

	if (pstrVirtual != NULL)
		userRAM = (char*) VirtualAlloc (pstrVirtual, DEFAULT_RAM,
						MEM_COMMIT, PAGE_EXECUTE_READWRITE) ;

#endif

#ifdef __linux__
static struct termios orig_termios ;
pthread_t hThread = 0 ;

	platform = 1 ;

	void *base = NULL ;

	while ((MaximumRAM >= MINIMUM_RAM) && (NULL == (base = mymap (MaximumRAM))))
		MaximumRAM /= 2 ;

	// Now commit the initial amount to physical RAM:

	if (base != NULL)
		userRAM = mmap (base, MaximumRAM, PROT_EXEC | PROT_READ | PROT_WRITE, 
					MAP_FIXED | MAP_PRIVATE | MAP_ANON, -1, 0) ;

#endif

#ifdef __APPLE__
static struct termios orig_termios ;
pthread_t hThread = NULL ;

	platform = 2 ;

	while ((MaximumRAM >= MINIMUM_RAM) &&
				((void*)-1 == (userRAM = mmap ((void *)0x10000000, MaximumRAM, 
						PROT_EXEC | PROT_READ | PROT_WRITE, 
						MAP_PRIVATE | MAP_ANON, -1, 0))) &&
				((void*)-1 == (userRAM = mmap ((void *)0x10000000, MaximumRAM, 
						PROT_READ | PROT_WRITE, 
						MAP_PRIVATE | MAP_ANON, -1, 0))))
		MaximumRAM /= 2 ;
#endif

#ifdef PICO
	platform = 1 ;
	MaximumRAM = MINIMUM_RAM;
	userRAM = &__StackLimit;
	if (userRAM + MaximumRAM > (void *)0x20040000) userRAM = 0 ;
/*
	The address 0x20040000 is 8K less than total RAM on the Pico to
	leave space for the current stack when zeroing.  This only works
	for a custom linker script that allocates most of the RAM to the
	stack.  For the default script userRAM = malloc(MaximumRAM);
*/
	if (userRAM) bzero(userRAM,MaximumRAM) ;
#endif

	if ((userRAM == NULL) || (userRAM == (void *)-1))
	    {
#ifdef PICO
		fprintf(stdout, "Couldn't allocate memory\r\n") ;
		sleep(5);
#else
		fprintf(stderr, "Couldn't allocate memory\r\n") ;
#endif
		return 9 ;
	    }

#if defined __x86_64__ || defined __aarch64__ 
	platform |= 0x40 ;
#endif

	if (MaximumRAM > DEFAULT_RAM)
		userTOP = userRAM + DEFAULT_RAM ;
	else
		userTOP = userRAM + MaximumRAM ;
	progRAM = userRAM + PAGE_OFFSET ; // Will be raised if @cmd$ exceeds 255 bytes
	szCmdLine = progRAM - 0x100 ;     // Must be immediately below default progRAM
	szTempDir = szCmdLine - 0x100 ;   // Strings must be allocated on BASIC's heap
	szUserDir = szTempDir - 0x100 ;
	szLibrary = szUserDir - 0x100 ;
	szLoadDir = szLibrary - 0x100 ;

// Get path to executable:

#ifdef _WIN32
	if (GetModuleFileName(NULL, szLibrary, 256) == 0)
#endif
#ifdef __linux__
	i = readlink ("/proc/self/exe", szLibrary, 255) ;
	if (i > 0)
		szLibrary[i] = '\0' ;
	else
#endif
#ifdef __APPLE__
	i = 256 ;
	if (_NSGetExecutablePath(szLibrary, (unsigned int *)&i))
#endif
	    {
		p = realpath (argv[0], NULL) ;
		if (p)
		    {
			strncpy (szLibrary, p, 256) ;
			free (p) ;
		    }
	    }

	strcpy (szAutoRun, szLibrary) ;

	q = strrchr (szAutoRun, '/') ;
	if (q == NULL) q = strrchr (szAutoRun, '\\') ;
	p = strrchr (szAutoRun, '.') ;

	if (p > q) *p = '\0' ;
	strcat (szAutoRun, ".bbc") ;

	TestFile = fopen (szAutoRun, "rb") ;
	if (TestFile != NULL)
		fclose (TestFile) ;
	else if ((argc >= 2) && (*argv[1] != '-'))
		strcpy (szAutoRun, argv[1]) ;

	strcpy (szCmdLine, szAutoRun) ;

	if (argc >= 2)
	    {
		*szCmdLine = 0 ;
		for (i = 1; i < argc; i++)
		    {
			if (i > 1) strcat (szCmdLine, " ") ;
			strcat (szCmdLine, argv[i]) ;
		    }
		progRAM = (void *)(((intptr_t) szCmdLine + strlen(szCmdLine) + 256) & -256) ;
	    }

	if (*szAutoRun && (NULL != (ProgFile = fopen (szAutoRun, "rb"))))
	    {
		fread (progRAM, 1, userTOP - progRAM, ProgFile) ;
		fclose (ProgFile) ;
		immediate = NULL ;
	    }
	else
	    {
		immediate = (void *) 1 ;
		*szAutoRun = '\0' ;
	    }

#ifdef PICO
	strcpy(szTempDir, "/tmp");
	strcpy(szUserDir, "/user");
	strcpy(szLibrary, "/lib");
	strcpy(szLoadDir, "/");
	mkdir(szTempDir, 0770);
	mkdir(szUserDir, 0770);
	mkdir(szLibrary, 0770);
	strcat(szTempDir, "/");
	strcat(szUserDir, "/");
	strcat(szLibrary, "/");
#else
	env = getenv ("TMPDIR") ;
	if (!env) env = getenv ("TMP") ;
	if (!env) env = getenv ("TEMP") ;
	if (env) strcpy (szTempDir, env) ;
	else strcpy (szTempDir, "/tmp") ;

	env = getenv ("HOME") ;
	if (!env) env = getenv ("APPDATA") ;
	if (!env) env = getenv ("HOMEPATH") ;
	if (env) strcpy (szUserDir, env) ;

	p = strrchr (szLibrary, '/') ;
	if (p == NULL) p = strrchr (szLibrary, '\\') ;
	if (p)
		*p = '\0' ;

# ifdef _WIN32
	strcat (szTempDir, "\\") ;
	strcat (szLibrary, "\\lib\\") ;
	strcat (szUserDir, "\\bbcbasic\\") ;
	mkdir (szUserDir) ;
# else
	strcat (szTempDir, "/") ;
	strcat (szLibrary, "/lib/") ;
	strcat (szUserDir, "/bbcbasic/") ;
	mkdir (szUserDir, 0777) ;
# endif

	SetLoadDir (szAutoRun) ;
#endif

	if (argc < 2)
		*szCmdLine = 0 ;
	else if (TestFile == NULL)
		chdir (szLoadDir) ;

	// Set console for raw input and ANSI output:
#ifdef _WIN32
	// n.b.  Description of DISABLE_NEWLINE_AUTO_RETURN at MSDN is completely wrong!
	// What it actually does is to disable converting LF into CRLF, not wrap action.
	if (GetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), (LPDWORD) &orig_stdout)) 
		SetConsoleMode (GetStdHandle(STD_OUTPUT_HANDLE), orig_stdout | ENABLE_WRAP_AT_EOL_OUTPUT |
                                ENABLE_VIRTUAL_TERMINAL_PROCESSING | DISABLE_NEWLINE_AUTO_RETURN) ;
	if (GetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), (LPDWORD) &orig_stdin)) 
		SetConsoleMode (GetStdHandle(STD_INPUT_HANDLE), ENABLE_VIRTUAL_TERMINAL_INPUT) ; 
	hThread = CreateThread (NULL, 0, myThread, 0, 0, NULL) ;
#else
# ifndef PICO
	tcgetattr (STDIN_FILENO, &orig_termios) ;
	struct termios raw = orig_termios ;
	raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON) ;
	raw.c_oflag &= ~OPOST ;
	raw.c_cflag |= CS8 ;
	raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG) ;
	raw.c_cc[VMIN] = 0 ;
	raw.c_cc[VTIME] = 1 ;
	tcsetattr (STDIN_FILENO, TCSAFLUSH, &raw) ;
        pthread_create(&hThread, NULL, &myThread, NULL);
# endif
#endif

#ifdef __APPLE__
	timerqueue = dispatch_queue_create ("timerQueue", 0) ;
#endif

	UserTimerID = StartTimer (250) ;

	flags = 0 ;

	exitcode = entry (immediate) ;

	if (UserTimerID)
		StopTimer (UserTimerID) ;

#ifdef __APPLE__
        dispatch_release (timerqueue) ;
#endif

#ifdef _WIN32
	if (_isatty (_fileno (stdout)))
		printf ("\033[0m\033[!p") ;
	CloseHandle (hThread) ;
	if (orig_stdout != -1)
		SetConsoleMode (GetStdHandle(STD_OUTPUT_HANDLE), orig_stdout) ;
	if (orig_stdin != -1)
		SetConsoleMode (GetStdHandle(STD_INPUT_HANDLE), orig_stdin) ;
#else
# ifdef PICO
	printf ("\033[0m\033[!p") ;
	printf ("\nExiting with code %d...rebooting...\n",
		exitcode);
	watchdog_reboot(0,0,50);
    for(;;) sleep(5);
# else
	if (isatty (STDOUT_FILENO))
		printf ("\033[0m\033[!p") ;
	tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios) ;
# endif
#endif

	exit (exitcode) ;
}
