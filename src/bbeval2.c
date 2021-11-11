/*****************************************************************\
*       32-bit or 64-bit BBC BASIC Interpreter                    *
*       (C) 2017-2021  R.T.Russell  http://www.rtrussell.co.uk/   *
*                                                                 *
*       The name 'BBC BASIC' is the property of the British       *
*       Broadcasting Corporation and used with their permission   *
*                                                                 *
*       bbeval.c: Expression evaluation, functions and arithmetic *
*       Version 1.25a, 07-Oct-2021                                *
\*****************************************************************/

#define __USE_MINGW_ANSI_STDIO 1

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include <errno.h>
#include <setjmp.h>
#include "BBC.h"

#if defined __arm__ || defined __aarch64__ || defined __EMSCRIPTEN__
#define powl pow
#define sqrtl sqrt
#define sinl sin
#define cosl cos
#define tanl tan
#define asinl asin
#define acosl acos
#define atanl atan
#define expl exp
#define logl log
#define fabsl fabs
#define truncl trunc
#define EFORMAT "%.*E"
#define FFORMAT "%.*f"
#define GFORMAT "%.*G"
#else
#define EFORMAT "%.*LE"
#define FFORMAT "%.*Lf"
#define GFORMAT "%.*LG"
#endif

// Routines in bbmain:
void check (void) ;		// Check for running out of memory
int range0 (char) ;		// Test char for valid in a variable name
signed char nxt (void) ;	// Skip spaces, handle line continuation
void error (int, const char*) ;	// Process an error
void outchr (unsigned char) ;	// Output a character
void text (const char *) ;	// Output NUL-terminated string
void crlf (void) ;		// Output a newline
unsigned short report (void) ;	// Put error message in string accumulator
char *moves (STR *, int) ;	// Move string into temporary buffer
void fixs (VAR) ;		// Copy to string accumulator
heapptr *pushs (VAR) ;		// Push string on stack
void comma (void) ;		// Check for comma
void braket (void) ;		// Check for closing parenthesis
char *alloct (int) ;		// Allocate a temporary string buffer
int arrlen (void **) ;		// Count elements in an array
void *getvar (unsigned char *) ;	// Get a variable's pointer and type
void *getput (unsigned char *) ;	// Get, and if necessary create, var
void *putvar (void *ptr, unsigned char*) ;
char *lexan (char *, char *, unsigned char) ;
signed char *gettop (signed char *, unsigned short *) ;
unsigned short setlin (signed char *, char **) ;
signed char *search (signed char *, signed char) ;
char *allocs (STR *, int) ;

// Routines in bbexec:
void modify (VAR, void *, unsigned char, signed char) ;
void modifs (VAR, void *, unsigned char, signed char) ;
void storen (VAR, void *, unsigned char) ;
void procfn (signed char) ;	// User-defined PROC or FN
VAR xeq (void) ;		// Execute

// Routines in bbcmos:
unsigned char osrdch (void) ;	// Get character from console input
int oskey (int) ;		// Wait for character or test key
int getime (void) ;		// Return centisecond count
int getims (void) ;		// Get clock time string to accs
int vtint (int, int) ;		// Get RGB pixel colour or -1
int vpoint (int, int) ;		// Get palette index or -1
void getcsr (int*, int*) ;	// Get text cursor (caret) coords
int vgetc (int, int) ;		// Get character at specified coords
int oscall (int) ;		// Call an emulated OS function
int widths (char *, int) ;	// Get string width in graphics units
int adval (int) ;		// ADVAL function
void *osopen (int, char *) ;	// Open a file
unsigned char osbget (void*, int*) ; // Get a byte from a file
long long getptr (void*) ;	// Get file pointer
long long getext (void*) ;	// Get file length
long long geteof (void*) ;	// Get EOF status
void *sysadr (char *) ;		// Get the address of an API function

// Global jump buffer:
extern jmp_buf env ;

#if defined __arm__ || defined __aarch64__ || defined __EMSCRIPTEN__
static void setfpu(void) {}
static double xpower[9] = {1.0e1, 1.0e2, 1.0e4, 1.0e8, 1.0e16, 1.0e32, 1.0e64,
			   1.0e128, 1.0e256} ;
#else
static void setfpu(void) { unsigned int mode = 0x37F; asm ("fldcw %0" : : "m" (*&mode)); } 
static long double xpower[13] = {1.0e1L, 1.0e2L, 1.0e4L, 1.0e8L, 1.0e16L, 1.0e32L, 1.0e64L,
				1.0e128L, 1.0e256L, 1.0e512L, 1.0e1024L, 1.0e2048L, 1.0e4096L} ;
#endif

// Get possibly-parenthesised variable:
static void *getvbr(unsigned char *ptype)
{
	if (nxt () == '(')
	    {
		void *ptr ;
		esi++ ;
		ptr = getvbr (ptype) ;
		braket () ;
		return ptr ;
	    }
	return getvar (ptype) ;
}

// Convert a numeric value to a NUL-terminated decimal string:
int str (VAR v, char *dst, int format)
{
	int n ;
	char *p ;
	char fmt[5] = "%d" ;
	int width = format & 0xFF ;
        int prec = (format & 0xFF00) >> 8 ;

	switch (format & 0x30000)
	    {
		case 0x10000:
			if (prec) prec-- ;
			if (v.i.t == 0) v.f = v.i.n ;
			n = sprintf(dst, EFORMAT, prec, v.f) ;
			strcpy (fmt, "%-3d") ;
			break ;

		case 0x20000:
			if (v.i.t == 0) v.f = v.i.n ;
			n = sprintf(dst, FFORMAT, prec, v.f) ;
			break ;

		default:
		if (prec == 0) prec = 9 ;
		if (v.i.t == 0)
		    {
			n = sprintf(dst, "%lld", v.i.n) ; // ARM (no 80-bit float)
			if (n <= prec) break ;
			v.f = v.i.n ;
		    }
		n = sprintf(dst, GFORMAT, prec, v.f) ;
	    }

	p = strchr (dst, 'E') ;
	if (p)
	    {
		sprintf (p + 1, fmt, atoi (p + 1)) ;
		n = strlen (dst) ;
	    }

	if (n < width) 
	    {
		memmove (dst + width - n, dst, n + 1) ;
		memset (dst, ' ', width - n) ;
		n = width ;
	    }

	if (format & 0x800000)
	    {
		p = strchr (dst, '.') ;
		if (p) *p = ',' ;
	    }
	return n ;
}

// Convert a numeric value to a NUL-terminated hexadecimal string:
int strhex (VAR v, char *dst, int field)
{
	char fmt[12] ;
	long long n ;

#ifdef _WIN32
	sprintf (fmt, "%%%uI64X", field) ;
#else
	sprintf (fmt, "%%%ullX", field) ;
#endif

	if (v.i.t)
	    {
		long long t = v.f ;
		if (t != truncl (v.f))
			error (20, NULL) ; // 'Number too big'
		v.i.n = t ;
	    }

	n = v.i.n ; // copy because v is effectively passed-by-reference
	if ((liston & BIT2) == 0)
		n &= 0xFFFFFFFF ;
	return sprintf(dst, fmt, n) ;
}

// Multiply by an integer-power of 10:
#if defined __arm__ || defined __aarch64__ || defined __EMSCRIPTEN__
static double xpow10 (double n, int p)
{
	int f = 0, i = 0 ;

	if (p >= 512)
		error (20, NULL) ; // 'Number too big'
	if (p <= -512)
		return 0.0L ;
#else
static long double xpow10 (long double n, int p)
{
	int f = 0, i = 0 ;
	setfpu () ;

	if (p >= 8192)
		error (20, NULL) ; // 'Number too big'
	if (p <= -8192)
		return 0.0L ;
#endif

	if (p < 0)
	    {
		p = -p ;
		f = 1 ;
	    }

	while (p)
	    {
		if (p & 1)
		    {
			if (f)
				n /= xpower[i] ;
			else
				n *= xpower[i] ;
		    }
		i++ ;
		p = p >> 1 ;
	    }
	return n ;
}

// Get an unsigned integer from a string:

static unsigned long long number (int *pcount, int *ptrunc)
{
	unsigned long long n = 0 ;
	while (1)
	    {
		char al = *esi ;
		if ((al < '0') || (al > '9'))
			break ;
		esi++ ;
		(*pcount)++ ;
		if ((n > 0x1999999999999999L) || ((n == 0x1999999999999999L) && 
				((al > '5') || *ptrunc)))
			(*ptrunc)++ ;
		else
			n = n * 10 + (al - '0') ;
	    }
	return n ;
}

// Get an unsigned numeric constant:
VAR con (void)
{
	VAR v ;
	unsigned long long i = 0, f = 0 ;
	int e = 0, ni, nf = 0, ne = 0, nt = 0 ;
	setfpu () ;

	i = number (&ni, &nt) ;
	v.i.n = i ;
	v.i.t = 0 ;
	if ((*esi == '.') || (v.i.n < 0) || (nt != 0))
	    {
		v.i.t = 1 ; // ARM
		v.f = i ; // integer overflow
	    }

	if (nt != 0)
	    {
		v.f = xpow10 (v.f, nt) ;
		nt = 0 ;
	    }

	if (*esi == '.')
	    {
		esi++ ;
		f = number (&nf, &nt) ;
		v.f += xpow10 (f, nt - nf) ;
	    }

	if ((*esi == 'E') || ((liston & BIT3) && (*esi == 'e')))
	    {
		int neg = 0 ;

		esi++ ;
		if (*esi == '-')
		    {
			esi++ ;
			neg = 1 ;
		    }
		else if (*esi == '+')
			esi++ ;
		e = number (&ne, &nt) ;
		if (neg)
			e = -e ;

		if (v.i.t == 0)
		    {
			v.i.t = 1 ; // ARM
			v.f = i ;
		    }
		v.f = xpow10 (v.f, e) ;
		if ((v.i.t == 0) || (v.i.t == (short)0x8000))
			v.f = 0.0L ; // underflow
		if (isinf(v.f) || (v.i.t == -1))
			error (20, NULL) ; // 'Number too big'
	    }

	if (*esi == '#') esi++ ;
	return v ;
}

// Get a string constant (quoted string):
VAR cons (void)
{
	VAR v ;
	signed char al ;
	char *p = accs ;
	while (1)
	    {
		al = *esi++ ;
		if (al == 0x0D)
			error (9, NULL) ; // 'Missing "'
		if ((al == '"') && (*esi++ != '"'))
		    {
			esi-- ;
			v.s.p = accs - (char *) zero ;
			v.s.l = p - accs ;
			v.s.t = -1 ;
			break ;
		    }
		*p++ = al ;
	    }
	return v ;
}

// Load a numeric variable:
// type is 1, 4, 5, 8, 10, 36 or 40
VAR loadn (void *ptr, unsigned char type)
{
	VAR v ;
	switch (type)
	    {
		case 1:
			v.i.t = 0 ;
			v.i.n = *(unsigned char*)ptr ;
			break ;

		case 4:
			v.i.t = 0 ;
			v.i.n = ILOAD(ptr) ;
			break ;

		case 5:
			{
			int ecx = *((unsigned char*)ptr + 4) ;
			int edx = ILOAD(ptr) ;
			int sign = (edx < 0) ;
			if (ecx == 0)
			    {
				v.i.t = 0 ;
				v.i.n = edx ;
				break ;
			    }
			ecx += 895 ;
			edx = edx << 1 ;
			ecx = (ecx << 20) | ((unsigned int)edx >> 12) ;
			edx = edx << 20 ;
			if (sign) ecx |= 0x80000000 ;
			v.s.p = edx ;
			v.s.l = ecx ;
			v.i.t = 1 ; // ARM
			v.f = v.d.d ;
			}
			break ;

		case 8:
			if (ILOAD((char *) ptr + 4) == 0)
			    {
				v.i.t = 0 ;
				v.i.n = ILOAD(ptr) ; // 64-bit variant
				break ;
			    }
			{
			// v.s.p = *(heapptr*)ptr ;
			// v.s.l = *(int *)((char *)ptr + 4) ;
			memcpy (&v.s.p, ptr, 8) ; // may be unaligned
			v.i.t = 1 ; // ARM
			v.f = v.d.d ;
			}
			break ;

		case 10:
			// v.s.p = *(heapptr*)ptr ;
			// v.s.l = *(int *)((char *)ptr + 4) ;
			// v.i.t = *(short *)((char *)ptr + 8) ;
			memcpy (&v.s.p, ptr, 10) ; // may be unaligned
			break ;

		case 40:
			v.i.t = 0 ;
			// v.s.p = *(heapptr*)ptr ;
			// v.s.l = *(int *)((char *)ptr + 4) ;
			memcpy (&v.s.p, ptr, 8) ; // may be unaligned
			break ;

		case 36:
			v.i.t = 0 ;
			v.i.n = (intptr_t) VLOAD(ptr) ;
			break ;

		default:
			v.i.t = 0 ;
			v.i.n = 0 ;
			error (6, NULL) ; // 'Type mismatch'
	    }
	return v ;
}

// Load a string variable:
// type is 128, 130 or 136
VAR loads (void *ptr, unsigned char type)
{
	VAR v ;
	switch (type)
	    {
		case 128:
			v.s.l = memchr (ptr, 0x0D, 0x10000) - ptr ;
			if (v.s.l > 0xFFFF)
				error (19, NULL) ; // 'String too long'
			if ((ptr < zero) || ((ptr + v.s.l) > (zero + 0xFFFFFFFF)))
			    {
				// Don't use alloct because it will put string in accs
				v.s.p = allocs (&tmps, v.s.l) - (char *) zero ;
				memcpy (v.s.p + zero, ptr, v.s.l) ;
			    }
			else
				v.s.p = ptr - zero ;
			break ;

		case 130:
			v.s.l = strlen (ptr) ;
			if ((ptr < zero) || ((ptr + v.s.l) > (zero + 0xFFFFFFFF)))
			    {
				// Don't use alloct because it will put string in accs
				v.s.p = allocs (&tmps, v.s.l) - (char *) zero ;
				memcpy (v.s.p + zero, ptr, v.s.l) ;
			    }
			else
				v.s.p = ptr - zero ;
			break ;

		case 136:
			v.s.p = ULOAD(ptr) ;
			v.s.l = ILOAD(ptr + 4) ;
			break ;

		default:
			error (6, NULL) ; // 'Type mismatch'
	    }
	v.s.t = -1 ;
	return v ;
}

// Load an integer numeric:
long long loadi (void *ptr, unsigned char type)
{
	VAR v = loadn (ptr, type) ;
	if (v.i.t != 0)
	    {
		long long t = v.f ;
		if (t != truncl (v.f))
			error (20, NULL) ; // 'Number too big'
		v.i.n = t ;
	    }
	return v.i.n ;
}

void xfix (VAR *px)
{
	if (px->i.t)
	    {
		long long t = px->f ;
		if (t != truncl (px->f))
			error (20, NULL) ; // 'Number too big'
		px->i.n = t ;
		px->i.t = 0 ;
	    }
}

static void fix2 (VAR *px, VAR *py)
{
	if (px->i.t)
	    {
		long long t = px->f ;
		if (t != truncl (px->f))
			error (20, NULL) ; // 'Number too big'
		px->i.n = t ;
		px->i.t = 0 ;
	    }
	if (py->i.t)
	    {
		long long t = py->f ;
		if (t != truncl (py->f))
			error (20, NULL) ; // 'Number too big'
		py->i.n = t ; 
		py->i.t = 0 ;
	    }
}

void xfloat (VAR *px)
{
	if (px->i.t == 0)
	    {
		px->i.t = 1 ; // ARM
		px->f = px->i.n ;
	    }
}

static void float2 (VAR *px, VAR *py)
{
	if (px->i.t == 0)
	    {
		px->i.t = 1 ; // ARM
		px->f = px->i.n ;
	    }
	if (py->i.t == 0)
	    {
		py->i.t = 1 ; // ARM
		py->f = py->i.n ;
	    }
}

// Return a pseudo-random integer:
unsigned int rnd (void)
{
	unsigned int ecx = *(unsigned char*)(&prand + 1) ;
	unsigned int edx = prand ;
	unsigned int eax = (edx >> 1) | (ecx << 31) ;
	*(unsigned char*)(&prand + 1) = (ecx & ~1) | (edx & 1) ; // Preserve bits 1-7
	eax = eax ^ (edx << 12) ;
	edx = eax ^ (eax >> 20) ;
	prand = edx ;
	return edx ;
}

VAR math (VAR x, signed char op, VAR y)
{
	errno = 0 ;
	setfpu () ;
	switch (op)
	    {
		case '+':
			if ((x.i.t == 0) && (y.i.t == 0))
			    {
#if defined __GNUC__ && __GNUC__ < 5
				long long sum = x.i.n + y.i.n ;
				if (((int)(x.s.l ^ y.s.l) < 0) || ((sum ^ x.i.n) >= 0))
#else
				long long sum ;
				if (! __builtin_saddll_overflow (x.i.n, y.i.n, &sum))
#endif
				    {
					x.i.n = sum ;
					return x ;
				    }
			    }
			float2 (&x, &y) ;
			x.f += y.f ;
			break ;

		case '-':
			if ((x.i.t == 0) && (y.i.t == 0))
			    {
#if defined __GNUC__  && __GNUC__ < 5
				long long dif = x.i.n - y.i.n ;
				if (((int)(x.s.l ^ y.s.l) >= 0) || ((dif ^ x.i.n) >= 0))
#else
				long long dif ;
				if (! __builtin_ssubll_overflow (x.i.n, y.i.n, &dif))
#endif
				    {
					x.i.n = dif ;
					return x ;
				    }
			    }
			float2 (&x, &y) ;
			x.f -= y.f ;
			break ;

		case '*':
			if (x.i.n == 0)
				return x ;
			if (y.i.n == 0)
				return y ;
			if ((x.i.t == 0) && (y.i.t == 0))
			    {
#if defined __GNUC__  && __GNUC__ < 5
				long long prod = x.i.n * y.i.n ;
				if ((x.i.n != 0x8000000000000000) && (y.i.n != 0x8000000000000000) &&
					((__builtin_clzll(x.i.n) + __builtin_clzll(~x.i.n) +
					  __builtin_clzll(y.i.n) + __builtin_clzll(~y.i.n)) > 63))
#else
				long long prod ;
				if (! __builtin_smulll_overflow (x.i.n, y.i.n, &prod))
#endif
				    {
					x.i.n = prod ;
					return x ;
				    }
			    }
			float2 (&x, &y) ;
			x.f *= y.f ;
			break ;

		case '/':
			float2 (&x, &y) ;
			x.f /= y.f ;
			break ;

		case '^':
			if (y.i.t == 0)
			    {
				if (y.i.n == 0)
				    {
					y.i.n = 1 ;
					return y ;
				    }
				int n ;
				if (y.i.n == -1)
					n = 64 ;
				else
					n = __builtin_clzll(y.i.n) + __builtin_clzll(~y.i.n) ;
				if (n > 32)
				    {
					VAR v ;
					int yi = y.i.n ;
					v.i.t = 0 ;
					v.i.n = 1 ;

					if (yi < 0)
					    {
						yi = -yi ;
						x = math (v, '/', x) ;
					    }
					yi = yi << (n - 33) ;
					n = 65 - n ;
					while (n--)
					    {
						v = math (v, '*', v) ;
						if (yi < 0)
							v = math (v, '*', x) ;
						yi = yi << 1 ;
					    }
					return v ;
				    }
			    }
			float2 (&x, &y) ;
			x.f = powl(x.f, y.f) ;
			break ;

		case TDIV:
			fix2 (&x, &y) ;
			if (y.i.n == 0) error (18, NULL) ; // 'Division by zero'
			x.i.n /= y.i.n ;
			return x ;

		case TMOD:
			fix2 (&x, &y) ;
			if (y.i.n == 0) error (18, NULL) ; // 'Division by zero'
			x.i.n %= y.i.n ;
			return x ;

		case TAND:
			fix2(&x, &y) ;
			x.i.n &= y.i.n ;
			return x ;

		case TOR:
			fix2(&x, &y) ;
			x.i.n |= y.i.n ;
			return x ;

		case TEOR:
			fix2(&x, &y) ;
			x.i.n ^= y.i.n ;
			return x ;

		case TSUM:
			fix2(&x, &y) ;
			x.i.n += y.i.n ;
			return x ;

		case '<':
			if ((x.i.t == 0) && (y.i.t == 0))
				x.i.n = -(x.i.n < y.i.n) ;
			else
			    {
				float2 (&x, &y) ;
				x.i.n = -(x.f < y.f) ;
				x.i.t = 0 ; // must come after!
			    }
			return x ;

		case '=':
			if ((x.i.t == 0) && (y.i.t == 0))
				x.i.n = -(x.i.n == y.i.n) ;
			else
			    {
				float2 (&x, &y) ;
				x.i.n = -(x.f == y.f) ;
				x.i.t = 0 ; // must come after!
			    }
			return x ;

		case '>':
			if ((x.i.t == 0) && (y.i.t == 0))
				x.i.n = -(x.i.n > y.i.n) ;
			else
			    {
				float2 (&x, &y) ;
				x.i.n = -(x.f > y.f) ;
				x.i.t = 0 ; // must come after!
			    }
			return x ;

		case 'y':
			if ((x.i.t == 0) && (y.i.t == 0))
				x.i.n = -(x.i.n <= y.i.n) ;
			else
			    {
				float2 (&x, &y) ;
				x.i.n = -(x.f <= y.f) ;
				x.i.t = 0 ; // must come after!
			    }
			return x ;

		case 'z':
			if ((x.i.t == 0) && (y.i.t == 0))
				x.i.n = -(x.i.n != y.i.n) ;
			else
			    {
				float2 (&x, &y) ;
				x.i.n = -(x.f != y.f) ;
				x.i.t = 0 ; // must come after!
			    }
			return x ;

		case '{':
			if ((x.i.t == 0) && (y.i.t == 0))
				x.i.n = -(x.i.n >= y.i.n) ;
			else
			    {
				float2 (&x, &y) ;
				x.i.n = -(x.f >= y.f) ;
				x.i.t = 0 ; // must come after!
			    }
			return x ;

		case 4:
			fix2 (&x, &y) ;
			if (y.i.n > 63)
			    {
				x.i.n = 0 ;
				return x ;
			    }
			x.i.n = x.i.n << y.i.n ;
			if ((liston & BIT2) == 0)
				x.i.n = (x.i.n << 32) >> 32 ;
			return x ;

		case 5:
			fix2 (&x, &y) ;
			if (y.i.n > 63)
			    {
				x.i.n = 0 ;
				return x ;
			    }
			x.i.n = x.i.n << y.i.n ;
			return x ;

		case 6:
			fix2 (&x, &y) ;
			if (y.i.n > 63)
				y.i.n = 63 ;
			x.i.n = x.i.n >> y.i.n ;
			return x ;

		case 7:
			fix2 (&x, &y) ;
			if (y.i.n > 63)
			    {
				x.i.n = 0 ;
				return x ;
			    }
			if (((liston & BIT2) == 0) && (y.i.n != 0))
				x.i.n &= 0xFFFFFFFF ;
			x.i.n = (unsigned long long) x.i.n >> y.i.n ;
			return x ;
	    }

	if (isinf(x.f) || isnan(x.f) || (x.i.t == -1) || errno)
	    {
		if (op == '/')
			error (18, NULL) ; // 'Division by zero'
		else if (op == '^')
			error (22, NULL) ; // 'Logarithm range'
		else
			error (20, NULL) ; // 'Number too big'
	    }

	if ((x.i.t == 0) || (x.i.t == (short) 0x8000))
		x.f = 0.0 ; // Underflow

	return x ;
}

// Get a possibly signed numeric constant from string accumulator:
VAR val (void)
{
	VAR v ;
	signed char *tmpesi = esi ;
	esi = (signed char *) accs ;
	while (*esi == ' ') esi++ ;
	if (*esi == '-')
	    {
		VAR z = {0} ;
		esi++ ;
		v = math (z, '-', con ()) ;
	    }
	else
	    {
		if (*esi == '+') esi++ ;
		v = con () ;
	    }
	esi = tmpesi ;
	return v ;
}

VAR item (void) ;
VAR expr (void) ;

VAR items (void)
{
	VAR v = item () ;
	if (v.s.t != -1)
		error (6, NULL) ; // 'Type mismatch'
	return v ;
}

static VAR itemn (void)
{
	VAR v = item () ;
	if (v.s.t == -1)
		error (6, NULL) ; // 'Type mismatch'
	return v ;
}

static VAR itemf (void)
{
	VAR v = item () ;
	if (v.s.t == -1)
		error (6, NULL) ; // 'Type mismatch'
	if (v.i.t == 0)
	    {
		v.i.t = 1 ; // ARM
		v.f = v.i.n ;
	    }
	return v ;
}

long long itemi (void)
{
	VAR v = item () ;
	if (v.s.t == -1)
		error (6, NULL) ; // 'Type mismatch'
	if (v.i.t != 0)
	    {
		long long t = v.f ;
		if (t != truncl (v.f))
			error (20, NULL) ; // 'Number too big'
		v.i.n = t ;
	    }
	return v.i.n ;
}

VAR exprs (void)
{
	VAR v = expr () ;
	if (v.s.t != -1)
		error (6, NULL) ; // 'Type mismatch'
	return v ;
}

VAR exprn (void)
{
	VAR v = expr () ;
	if (v.s.t == -1)
		error (6, NULL) ; // 'Type mismatch'
	return v ;
}

long long expri (void)
{
	VAR v = expr () ;
	if (v.s.t == -1)
		error (6, NULL) ; // 'Type mismatch'
	if (v.i.t != 0)
	    {
		long long t = v.f ;
		if (t != truncl (v.f))
			error (20, NULL) ; // 'Number too big'
		v.i.n = t ;
	    }
	return v.i.n ;
}

void *channel (void)
{
	if (nxt () != '#')
		error (45, NULL) ; // 'Missing #'
	esi++ ;
	return (void *) (size_t) itemi () ;
}

static int dimfunc (void)
{
	int d, n ;
	void *ptr ;
	unsigned char type ;
	if (nxt () == '(')
	    {
		esi++ ;
		n = dimfunc () ;
		braket () ;
		return n ;
	    }
	ptr = getvar (&type) ;
	if (ptr == NULL)
		error (16, NULL) ; // 'Syntax error'
	if (type == 0)
		error (26, NULL) ; // 'No such variable'
	if ((type & (BIT4 | BIT6)) == 0)
		error (6, NULL) ; // 'Type mismatch'
	if ((type & BIT6) == 0)
	    {
		ptr = VLOAD(ptr) ; // Structure format ptr
		if (ptr < (void *)2)
			error (56, NULL) ; // 'Bad use of structure'
		return ILOAD(ptr) ;
	    }
	ptr = VLOAD(ptr) ;
	if (ptr < (void *)2)
		error (14, NULL) ; // 'Bad use of array'
	d = *(unsigned char *)ptr ;
	if (*esi != ',')
		return d;
	esi++ ;
	n = expri () - 1 ;
	if ((n < 0) || (n >= d))
		error (15, NULL) ; // 'Bad subscript'
	return ILOAD(ptr + 1 + n * 4) - 1 ;
}

/************************* Parenthesised expression ****************************/

static VAR item_BRACKET (void)
    {
    VAR v = expr ();
    if (nxt () != ')')
        error (27, NULL); // 'Missing )'
    esi++;
    return v;
    }

/************************************* FN **************************************/

static VAR item_TFN (void)
    {
    int errcode = 0;
    jmp_buf savenv;
    heapptr* savesp;

    procfn (TFN);

    memcpy (&savenv, &env, sizeof(env));
    savesp = esp;
    errcode = (setjmp (env)); // <0 : QUIT, >0 : error, 256: END
    if (errcode)
        {
        if ((errcode < 0) || (errcode > 255) || (errtrp == 0) ||
            (onersp == NULL) || (onersp > savesp))
            {
            memcpy (&env, &savenv, sizeof(env));
            longjmp (env, errcode);
            }
        esi = errtrp + (signed char *) zero;
        esp = onersp;
        }

    VAR v = xeq ();
    memcpy (&env, &savenv, sizeof(env));
    return v;
    }

/************************************* USR *************************************/

static VAR item_TUSR (void)
    {
    int (*func) (int,int,int,int,int,int);
    size_t n = itemi ();
    VAR v;

    v.i.t = 0;
    if ((n >= 0x8000) && (n <= 0xFFFF))
        v.i.n = oscall (n);
    else
        {
        func = (void *) n;
        v.i.n = func (stavar[1], stavar[2], stavar[3],
            stavar[4], stavar[5], stavar[6]);
        }
    return v;
    }

/************************************ FALSE ************************************/

static VAR item_TFALSE (void)
    {
    VAR v;
    v.i.n = 0;
    v.i.t = 0;
    return v;
    }

/************************************ TRUE *************************************/

static VAR item_TTRUE (void)
    {
    VAR v;
    v.i.n = -1;
    v.i.t = 0;
    return v;
    }

/*********************************** OPENIN ************************************/

static VAR item_TOPENIN (void)
    {
    VAR v = items ();
    fixs (v);
    v.i.t = 0;
    v.i.n = (size_t) osopen (0, accs);
    return v;
    }

/*********************************** OPENOUT ***********************************/

static VAR item_TOPENOUT (void)
    {
    VAR v = items ();
    fixs (v);
    v.i.t = 0;
    v.i.n = (size_t) osopen (1, accs);
    return v;
    }

/*********************************** OPENUP ************************************/

static VAR item_TOPENUP (void)
    {
    VAR v = items ();
    fixs (v);
    v.i.t = 0;
    v.i.n = (size_t) osopen (2, accs);
    return v;
    }

/************************************* PTR *************************************/

static VAR item_TPTRR (void)
    {
    VAR v;
    if (*esi == '(')
        {
        void *ptr;
        unsigned char type;
        esi++;
        nxt ();
        ptr = getvar (&type);
        if (ptr == NULL)
            error (16, NULL); // 'Syntax error'
        if (type == 0)
            error (26, NULL); // 'No such variable'
        braket ();
        v.i.t = 0;
        if (type == 136)
            v.i.n = ULOAD(ptr) + (size_t) zero;
        else if ((type == 36) || (type & 0x40))
            v.i.n = TLOAD(ptr);
        else if (type == STYPE)
            v.i.n = TLOAD(ptr + sizeof (void *));
        else
            error (6, NULL); // 'Type mismatch'
        }
    else
        {
        void *n = channel ();
        v.i.t = 0;
        v.i.n = getptr (n);
        }
    return v;
    }

/************************************* EXT *************************************/

static VAR item_TEXTR (void)
    {
    VAR v;
    void *n = channel ();
    v.i.t = 0;
    v.i.n = getext (n);
    return v;
    }

/************************************* EOF *************************************/

static VAR item_TEOF (void)
    {
    VAR v;
    void *n = channel ();
    v.i.t = 0;
    v.i.n = geteof (n);
    return v;
    }

/************************************ BGET *************************************/

static VAR item_TBGET (void)
    {
    VAR v;
    void *n = channel ();
    v.i.t = 0;
    v.i.n = osbget (n, NULL);
    return v;
    }

/************************************ PAGE *************************************/

static VAR item_TPAGER (void)
    {
    VAR v;
    v.i.n = vpage + (size_t) zero;
    v.i.t = 0;
    return v;
    }

/************************************* TOP *************************************/

static VAR item_TTO (void)
    {
    if ((*esi == 'P') || (((liston & BIT3) != 0) && (*esi == 'p')))
        {
        VAR v;
        esi++;
        v.i.n = (size_t) gettop (vpage + (signed char *) zero, NULL) + 3;
        v.i.t = 0;
        return v;
        }
    else
        error (16, NULL); // 'Syntax error'
    }

/************************************ LOMEM ************************************/

static VAR item_TLOMEMR (void)
    {
    VAR v;
    v.i.n = lomem + (size_t) zero;
    v.i.t = 0;
    return v;
    }

/************************************* END *************************************/

static VAR item_TEND (void)
    {
    VAR v;
    v.i.n = pfree + (size_t) zero;
    v.i.t = 0;
    return v;
    }

/************************************ HIMEM ************************************/

static VAR item_THIMEMR (void)
    {
    VAR v;
    v.i.n = himem + (size_t) zero;
    v.i.t = 0;
    return v;
    }

/************************************ MODE *************************************/

static VAR item_TMODE (void)
    {
    VAR v;
    v.i.n = modeno;
    v.i.t = 0;
    return v;
    }

/************************************ TIME *************************************/

static VAR item_TTIMER (void)
    {
    VAR v;
    if (*esi == '$')
        {
        esi++;
        v.s.t = -1;
        v.s.p = accs - (char *) zero;
        v.s.l = getims ();
        return v;
        }
    v.i.n = (int) getime (); // Must not overflow 32-bit int
    v.i.t = 0;
    return v;
    }

/************************************ ADVAL ************************************/

static VAR item_TADVAL (void)
    {
    VAR v;
    v.i.n = adval (itemi ());
    v.i.t = 0;
    return v;
    }

/************************************ COUNT ************************************/

static VAR item_TCOUNT (void)
    {
    VAR v;
    v.i.n = vcount;
    v.i.t = 0;
    return v;
    }

/************************************ WIDTH ************************************/

static VAR item_TWIDTH (void)
    {
    VAR v;
    if (*esi == '(')
        {
        esi++;
        VAR v = exprs ();
        braket ();
        if (v.s.l > ((char *)esp - (char *)zero - pfree - STACK_NEEDED))
            error (0, NULL); // 'No room'
        v.i.n = widths (v.s.p + (char *) zero, v.s.l);
        v.i.t = 0;
        return v;
        }
    v.i.n = vwidth;
    v.i.t = 0;
    return v;
    }

/************************************* DIM *************************************/

static VAR item_TDIM (void)
    {
    VAR v;
    v.i.t = 0;
    v.i.n = dimfunc ();
    return v;
    }

/************************************* GET *************************************/

static VAR item_TGET (void)
    {
    VAR v;
    if (*esi == '(')
        {
        int x, y;
        esi++;
        x = expri ();
        comma ();
        y = expri ();
        braket ();
        v.i.n = vgetc (x, y);
        v.i.t = 0;
        return v;
        }
    v.i.n = osrdch ();
    v.i.t = 0;
    return v;
    }

/************************************ GET$ *************************************/

static VAR item_TGETS (void)
    {
    VAR v;
    if (*esi == '#')
        {
        char *p;
        int term = 0x100;
        int count = 0x7FFFFFFF;
        void *chan = channel ();
        if (nxt () == TBY)
            {
            esi++;
            term = -1;
            count = itemi ();
            }
        else if (*esi == TTO)
            {
            esi++;
            term = itemi ();
            }
        allocs (&tmps, 0); // Free tmps (may change pfree)	
        p = pfree + (char *) zero;
        while (count--)
            {
            int eof;
            char al = osbget (chan, &eof);
            if (eof) break;
            if (term >= 0)
                {
                if (al == (term & 0xFF)) break;
                if ((al == 0x0D) && (term & 0x8100)) break;
                if ((al == 0x0A) && (term & 0x100)) break;
                }
            *p++ = al;
            if (p > ((char *) esp - STACK_NEEDED))
                error (0, NULL); // 'No room'
            }
        v.s.t = -1;
        v.s.p = pfree;
        v.s.l = p - (char *) zero - pfree;
        v.s.p = moves ((STR *) &v, 0) - (char *) zero;
        return v;
        }
    if (*esi == '(')
        {
        int c, x, y;
        esi++;
        x = expri ();
        comma ();
        y = expri ();
        braket ();
        c = vgetc (x, y);
        ISTORE(accs, c);
        v.s.t = -1;
        v.s.p = accs - (char *) zero;
        if (c < 0)
            v.s.l = 0;
        else
            v.s.l = strlen (accs);
        return v;
        }
    *accs = osrdch ();
    v.s.t = -1;
    v.s.p = accs - (char *) zero;
    v.s.l = 1;
    return v;
    }

/************************************ INKEY ************************************/

static VAR item_TINKEY (void)
    {
    VAR v;
    v.i.t = 0;
    v.i.n = oskey (itemi ());
    return v;
    }

/*********************************** INKEY$ ************************************/

static VAR item_TINKEYS (void)
    {
    VAR v;
    int n = oskey (itemi ());
    *accs = n;
    v.s.t = -1;
    v.s.p = accs - (char *) zero;
    v.s.l = (n != -1);
    return v;
    }

/************************************* POS *************************************/

static VAR item_TPOS (void)
    {
    VAR v;
    int x;
    getcsr (&x, NULL);
    v.i.t = 0;
    v.i.n = x;
    return v;
    }

/************************************ VPOS *************************************/

static VAR item_TVPOS (void)
    {
    VAR v;
    int y;
    getcsr (NULL, &y);
    v.i.t = 0;
    v.i.n = y;
    return v;
    }

/************************************ TINT *************************************/

static VAR item_TTINT (void)
    {
    VAR v;
    int p, x, y;
    if (*esi++ != '(')
        error (16, NULL); // 'Syntax error'
    x = expri ();
    comma ();
    y = expri ();
    braket ();
    p = vtint (x, y);
    v.i.t = 0;
    v.i.n = p;
    return v;
    }

/*********************************** TPOINT ************************************/

static VAR item_TPOINT (void)
    {
    VAR v;
    int p, x, y;
    x = expri ();
    comma ();
    y = expri ();
    braket ();
    p = vpoint (x, y);
    v.i.t = 0;
    v.i.n = p;
    return v;
    }

/************************************* ASC *************************************/

static VAR item_TASC (void)
    {
    VAR v;
    v = items ();
    if (v.s.l)
        v.i.n = *(v.s.p + (unsigned char *) zero);
    else
        v.i.n = -1;
    v.i.t = 0;
    return v;
    }

/************************************* ERR *************************************/

static VAR item_TERR (void)
    {
    VAR v;
    v.i.n = errnum;
    v.i.t = 0;
    return v;
    }

/************************************* ERL *************************************/

static VAR item_TERL (void)
    {
    VAR v;
    v.i.n = setlin (errlin + (signed char *) zero , NULL);
    v.i.t = 0;
    return v;
    }

/************************************* PI **************************************/

static VAR item_TPI (void)
    {
    VAR v;
    if (*esi == '#') esi++;
#if defined __arm__ || defined __aarch64__ || defined __EMSCRIPTEN__
    v.i.n = 0x400921FB54442D18L;
    v.i.t = 1;
#else
    v.i.n = 0xC90FDAA22168C235L;
    v.i.t = 0x4000;
#endif
    return v;
    }

/************************************* SIN *************************************/

static VAR item_TSIN (void)
    {
    VAR v = itemf ();
    v.f = sinl (v.f);
	if (isinf(v.f) || isnan(v.f) || (v.i.t == -1) || errno)
        error (20, NULL) ; // 'Number too big'
	if (v.i.t == 0)
		v.i.n = 0 ; // Underflow
    return v;
    }

/************************************* COS *************************************/

static VAR item_TCOS (void)
    {
    VAR v = itemf ();
    v.f = cosl (v.f);
	if (isinf(v.f) || isnan(v.f) || (v.i.t == -1) || errno)
        error (20, NULL) ; // 'Number too big'
	if (v.i.t == 0)
		v.i.n = 0 ; // Underflow
    return v;
    }

/************************************* TAN *************************************/

static VAR item_TTAN (void)
    {
    VAR v = itemf ();
    v.f = tanl (v.f);
	if (isinf(v.f) || isnan(v.f) || (v.i.t == -1) || errno)
        error (20, NULL) ; // 'Number too big'
	if (v.i.t == 0)
		v.i.n = 0 ; // Underflow
    return v;
    }

/************************************* ASN *************************************/

static VAR item_TASN (void)
    {
    VAR v = itemf ();
    v.f = asinl (v.f);
	if (isinf(v.f) || isnan(v.f) || (v.i.t == -1) || errno)
        error (20, NULL) ; // 'Number too big'
	if (v.i.t == 0)
		v.i.n = 0 ; // Underflow
    return v;
    }

/************************************* ACS *************************************/

static VAR item_TACS (void)
    {
    VAR v = itemf ();
    v.f = acosl (v.f);
	if (isinf(v.f) || isnan(v.f) || (v.i.t == -1) || errno)
        error (20, NULL) ; // 'Number too big'
	if (v.i.t == 0)
		v.i.n = 0 ; // Underflow
    return v;
    }

/************************************* ATN *************************************/

static VAR item_TATN (void)
    {
    VAR v = itemf ();
    v.f = atanl (v.f);
	if (isinf(v.f) || isnan(v.f) || (v.i.t == -1) || errno)
        error (20, NULL) ; // 'Number too big'
	if (v.i.t == 0)
		v.i.n = 0 ; // Underflow
    return v;
    }

/************************************* DEG *************************************/

static VAR item_TDEG (void)
    {
    VAR xdeg;
#if defined __arm__ || defined __aarch64__ || defined __EMSCRIPTEN__
    xdeg.i.n = 0x404CA5DC1A63C1F8L;
    xdeg.i.t = 1;
#else
    xdeg.i.n = 0xE52EE0D31E0FBDC3L;
    xdeg.i.t = 0x4004;
#endif
    return math (itemf(), '*', xdeg);
    }

/************************************* RAD *************************************/

static VAR item_TRAD (void)
    {
    VAR xrad;
#if defined __arm__ || defined __aarch64__ || defined __EMSCRIPTEN__
    xrad.i.n = 0x3F91DF46A2529D39L;
    xrad.i.t = 1;
#else
    xrad.i.n = 0x8EFA351294E9C8AEL;
    xrad.i.t = 0x3FF9;
#endif
    return math (itemf(), '*', xrad);
    }

/************************************* EXP *************************************/

static VAR item_TEXP (void)
    {
    VAR v = itemf ();
    v.f = expl (v.f);
    if (isinf (v.f))
        error (24, NULL); // 'Exponent range'
	if (v.i.t == 0)
		v.i.n = 0 ; // Underflow
    return v;
    }

/************************************* LN **************************************/

static VAR item_TLN (void)
    {
    VAR v = itemf ();
    v.f = logl (v.f);
    if (isnan (v.f) || isinf (v.f))
        error (22, NULL); // 'Logarithm range'
	if (v.i.t == 0)
		v.i.n = 0 ; // Underflow
    return v;
    }

/************************************* LOG *************************************/

static VAR item_TLOG (void)
    {
    VAR loge;
#if defined __arm__ || defined __aarch64__ || defined __EMSCRIPTEN__
    loge.i.n = 0x3FDBCB7B1526E50EL;
    loge.i.t = 1;
#else
    loge.i.n = 0xDE5BD8A937287195L;
    loge.i.t = 0x3FFD;
#endif
    VAR v = itemf ();
    v.f = logl (v.f );
    if (isnan (v.f) || isinf (v.f))
        error (22, NULL); // 'Logarithm range'
    return math (v, '*', loge);
    }

/************************************* SQR *************************************/

static VAR item_TSQR (void)
    {
    VAR v = itemf ();
    v.f = sqrtl (v.f);
	if (isinf(v.f) || isnan(v.f) || (v.i.t == -1) || errno)
        error (21, NULL) ; // 'Negative root'
	if (v.i.t == 0)
		v.i.n = 0 ; // Underflow
    return v;
    }

/************************************* MOD *************************************/

static VAR item_TMOD (void)
    {
    VAR v;
    int i, count;
    unsigned char type;
    void *ptr = getvbr (&type);
    if (ptr == NULL)
        error (16, NULL); // 'Syntax error'
    if (type == 0)
        error (26, NULL); // 'No such variable'
    if ((type >= 128) || (type < 64))
        error (6, NULL); // 'Type mismatch'
    ptr = VLOAD(ptr);
    count = arrlen (&ptr);
    v.i.t = 0;
    v.i.n = 0;
    type &= ~BIT6;
    for (i = 0; i < count; i++)
        {
        VAR x = loadn (ptr, type); // n.b. type can be 40
        v = math (v, '+', math (x, '*', x));
        ptr += type & TMASK;
        }
    if (v.i.t == 0)
        {
        v.i.t = 1; // ARM
        v.f = v.i.n;
        }
    v.f = sqrtl (v.f);
    return v;
    }

/************************************* SUM *************************************/

static VAR item_TSUM (void)
    {
    VAR v;
    VAR x;
    int count;
    unsigned char type;
    void *ptr;
    int sumlen = (nxt () == TLEN);
    if (sumlen) esi++;

    ptr = getvbr (&type);
    if (ptr == NULL)
        error (16, NULL); // 'Syntax error'
    if ((type & 64) == 0)
        error (6, NULL); // 'Type mismatch'
    ptr = VLOAD(ptr);
    count = arrlen (&ptr);
    if (type < 128)
        {
        if (sumlen)
            error (6, NULL); // 'Type mismatch'
        v.i.t = 0;
        v.i.n = 0;
        type &= ~BIT6;
        while (count--)
            {
            v = math (v, '+', loadn (ptr, type)); // n.b. type can be 40
            ptr += type & TMASK;
            }
        }
    else if (sumlen)
        {
        int n = 0;
        type &= ~BIT6;
        while (count--)
            {
            x = loads (ptr, type);
            ptr += 8;
            n += x.s.l;
            }
        v.i.t = 0;
        v.i.n = n;
        }
    else
        {
        int i, n = 0;
        void *tmp = ptr;
        type &= ~BIT6;
        for (i = 0; i < count; i++)
            {
            x = loads (tmp, type);
            tmp += 8;
            n += x.s.l;
            }
        v.s.t = -1;
        v.s.l = n;
        v.s.p = alloct (n) - (char *) zero;
        n = 0;
        while (count--)
            {
            x = loads (ptr, type);
            ptr += 8;
            if (x.s.l)
                memcpy (v.s.p + n + zero, x.s.p + zero, x.s.l);
            n += x.s.l;
            }
        }
    return v;
    }

/************************************* ABS *************************************/

static VAR item_TABS (void)
    {
    VAR v = itemn ();
    if (v.i.t)
        v.f = fabsl (v.f); // both double and long double
    else if (v.i.n == 0x8000000000000000)
        {
        v.i.t = 1; // ARM
        v.f = 9223372036854775808.0L;
        }
    else
        v.i.n = llabs (v.i.n);
    return v;
    }

/************************************* SGN *************************************/

static VAR item_TSGN (void)
    {
    VAR v = itemn ();
    if (v.i.t)
        v.i.n = (v.f > 0) - (v.f < 0);
    else
        v.i.n = (v.i.n > 0) - (v.i.n < 0);
    v.i.t = 0;
    return v;
    }

/************************************* INT *************************************/

static VAR item_TINT (void)
    {
    long long tmp;
    VAR v = itemf ();
    v.f = floorl (v.f);
    tmp = v.f;
    if (v.f != tmp)
        return v;
    v.i.n = tmp;
    v.i.t = 0;
    return v;
    }

/************************************* NOT *************************************/

static VAR item_TNOT (void)
    {
    VAR v;
    v.i.t = 0;
    v.i.n = ~itemi ();
    return v;
    }

/************************************* LEN *************************************/

static VAR item_TLEN (void)
    {
    VAR v = items ();
    v.i.n = v.s.l;
    v.i.t = 0;
    return v;
    }

/************************************* RND *************************************/

static VAR item_TRND (void)
    {
    VAR v;
    v.i.t = 0;
    if (*esi == '(')
        {
        int n;
        esi++;
        n = expri ();
        braket ();

        if (n == 0)
            {
            unsigned int edx = (prand >> 16) | (prand << 16);
            v.i.t = 1; // ARM
            v.f = (double)edx / 4294967296.0L;
            }
        else if (n == 1)
            {
            unsigned int edx = rnd ();
            edx = (edx >> 16) | (edx << 16);
            v.i.t = 1; // ARM
            v.f = (double)edx / 4294967296.0L;
            }
        else if (n > 1)
            v.i.n = (rnd() % n) + 1;
        else 
            {
            prand = (unsigned int) n;
            *(unsigned char*)(&prand + 1) = (n & 0x80000) == 0;
            v.i.n = n;
            }
        return v;
        }
    v.i.n = (signed int) rnd ();
    return v;
    }

/************************************* VAL *************************************/

static VAR item_TVAL (void)
    {
    VAR v = items ();
    fixs (v);
    return val ();
    }

/************************************ EVAL *************************************/

static VAR item_TEVAL (void)
    {
    signed char *oldesi;
    heapptr *oldesp;
    VAR v = items ();
    fixs (v);
    lexan (accs, accs, 0); // assumes string gets no longer
    v.s.p = accs - (char *) zero;
    v.s.l += 1;
    oldesp = pushs (v);
    oldesi = esi;
    esi = (signed char *) esp;
    v = expr ();
    esi = oldesi;
    esp = oldesp;
    return v;
    }

/************************************ STR$ *************************************/

static VAR item_TSTR (void)
    {
    VAR v;
    if (nxt () == '~')
        {
        esi++;
        v.s.l = strhex (itemn (), accs, 0);
        }
    else
        {
        v = itemn ();
        if (stavar[0] & 0xFF000000)
            v.s.l = str (v, accs, stavar[0] & 0xFFFF00);
        else
            v.s.l = str (v, accs, 0x900);
        }
    v.s.p = accs - (char *) zero;
    v.s.t = -1;
    return v;
    }

/************************************ CHR$ *************************************/

static VAR item_TCHR (void)
    {
    VAR v;
    *accs = itemi ();
    v.s.t = -1;
    v.s.p = accs - (char *) zero;
    v.s.l = 1;
    return v;
    }

/************************************ MID$ *************************************/

static VAR item_TMID (void)
    {
    heapptr *oldesp;
    char * tmp;
    int s, n;

    VAR v = exprs ();
    n = v.s.l;
    comma ();
    oldesp = pushs (v);

    s = expri ();
    if (s < 1)
        s = 1;

    if (*esi == ',')
        {
        esi++;
        n = expri ();
        if (n < 0)
            n = v.s.l;
        }
    braket ();

    if (n > ((int)v.s.l - s + 1))
        n = (int)v.s.l - s + 1;
    if (n < 0)
        n = 0;
    if (n == 0)
        {
        esp = oldesp;
        v.s.p = 0;
        v.s.l = 0;
        return v;
        }

    v.s.p = (char *) esp - (char *) zero + s - 1;
    v.s.l = n;
    tmp = moves ((STR*) &v, 0);
    esp = oldesp;
    v.s.p = tmp - (char *) zero;
    return v;
    }

/*********************************** LEFT$ *************************************/

static VAR item_TLEFT (void)
    {
    heapptr *oldesp;
    char * tmp;
    int n;

    VAR v = exprs ();
    if (*esi != ',')
        {
        braket ();
        if (v.s.l)
            v.s.l -= 1;
        return v;
        }

    esi++;
    oldesp = pushs (v);
    n = expri ();
    braket ();

    if ((n < 0) || (n > v.s.l))
        n = v.s.l;

    v.s.p = (char *) esp - (char *) zero;
    v.s.l = n;
    tmp = moves ((STR*) &v, 0);
    esp = oldesp;
    v.s.p = tmp - (char *) zero;
    return v;
    }

/*********************************** RIGHT$ ************************************/

static VAR item_TRIGHT (void)
    {
    heapptr *oldesp;
    char * tmp;
    int n;

    VAR v = exprs ();
    if (*esi != ',')
        {
        braket ();
        if (v.s.l > 1)
            {
            v.s.p += (int)v.s.l - 1;
            v.s.l = 1;
            }
        return v;
        }

    esi++;
    oldesp = pushs (v);
    n = expri ();
    braket ();

    if ((n < 0) || (n > v.s.l))
        n = v.s.l;

    v.s.p = (char *) esp - (char *) zero + v.s.l - n;
    v.s.l = n;
    tmp = moves ((STR*) &v, 0);
    esp = oldesp;
    v.s.p = tmp - (char *) zero;
    return v;
    }

/*********************************** STRING$ ***********************************/

static VAR item_TSTRING (void)
    {
    char *tmp, *p;
    long long size, n = expri ();
    comma ();
    VAR v = exprs ();
    braket ();
    if ((n <= 0) || (v.s.l == 0))
        {
        v.s.p = 0;
        v.s.l = 0;
        return v;
        }
    size = n * v.s.l;
    if (size > 0x7FFFFFFF)
        error (19, NULL); // 'String too long'
    tmp = alloct (size);
    for (p = tmp; n--; p += v.s.l)
        memcpy (p, v.s.p + zero , v.s.l);
    v.s.p = tmp - (char *) zero;
    v.s.l = size;
    return v;
    }

/*********************************** INSTR *************************************/

static VAR item_TINSTR (void)
    {
    heapptr *oldesp;
    int n = 0;
    char *p;
    VAR x = exprs ();
    oldesp = pushs (x);
    x.s.p = (char *) esp - (char *) zero;
    comma ();
    VAR v = exprs ();
    pushs (v);
    v.s.p = (char *) esp - (char *) zero;
    if (*esi == ',')
        {
        esi++;
        n = expri () - 1;
        if (n < 0)
            n = 0;
        }
    braket ();

    if ((x.s.l == 0) || (v.s.l == 0) || ((n + v.s.l) > x.s.l))
        {
        v.i.t = 0;
        v.i.n = 0;
        esp = oldesp;
        return v;
        }

    p = x.s.p + n + (char *) zero;
    while (p != NULL)
        {
        if (0 == memcmp (p, v.s.p + zero, v.s.l))
            {
            v.i.t = 0;
            v.i.n = p - (char *) zero - x.s.p + 1;
            esp = oldesp;
            return v;
            }
        p = memchr (p + 1, *(v.s.p + (char *) zero),
            x.s.p + x.s.l - v.s.l + (char *) zero - p);
        }
    v.i.t = 0;
    v.i.n = 0;
    esp = oldesp;
    return  v;
    }

/*********************************** REPORT ************************************/

static VAR item_TREPORT (void)
    {
    VAR v;
    if (nxt () != '$')
        error (16, NULL); // Syntax error
    esi++;
    report ();
    v.s.t = -1;
    v.s.p = accs - (char *) zero;
    v.s.l = strlen (accs);
    return v;
    }

/******************************* Line number ***********************************/

static VAR item_TLINO (void)
    {
    VAR v;
    unsigned char ah = *(unsigned char *)esi++;
    v.i.t = 0;
    v.i.n = ((*(unsigned char *)esi++) ^ ((ah << 2) & 0xC0));
    v.i.n += ((*(unsigned char *)esi++) ^ ((ah << 4) & 0xC0)) * 256; 
    return v;
    } 

/************************************ SYS **************************************/

static VAR item_TSYS (void)
    {
    VAR v = items ();
    if (v.s.l > 255)
        error (19, NULL); // 'String too long'
    memcpy (accs, v.s.p + zero, v.s.l);
    *(accs + v.s.l) = 0;
    v.i.t = 0;
    v.i.n = (intptr_t) sysadr (accs);
    return v;
    }

/*********************************** PROC **************************************/

static VAR item_TPROC (void)
    {
    VAR v;
    error (16, NULL); // 'Syntax error'
    return v;
    }

/*************************** Hexadecimal constant ******************************/

static VAR item_HEX (void)
    {
    VAR v;
    signed char *p = esi;
    v.i.n = 0;
    while (1)
        {
        signed char x = *esi;
        signed char d;
        if ((x >= '0') && (x <= '9')) d = x - 48;
        else if ((x >= 'A') && (x <= 'F')) d = x - 55;
        else if ((x >= 'a') && (x <= 'f')) d = x - 87;
        else break;
        v.i.n = (v.i.n << 4) | d;
        esi++;
        }
    if (p == esi)
        error (28, NULL); // 'Bad hex or binary'
    if ((liston & BIT2) == 0)
        v.i.n = (v.i.n << 32) >> 32;
    v.i.t = 0;
    return v;
    }

/****************************** Binary constant ********************************/

static VAR item_BIN (void)
    {
    VAR v;
    signed char al = *esi++;
    v.i.n = 0;
    if ((al != '0') && (al != '1'))
        error (28, NULL); // 'Bad hex or binary'
    while ((al == '0') || (al == '1'))
        {
        v.i.n = (v.i.n << 1) | (al - '0');
        al = *esi++;
        } 
    if ((liston & BIT2) == 0)
        v.i.n = (v.i.n << 32) >> 32;
    v.i.t = 0;
    esi--;
    return v;
    }

/******************************** Unary minus **********************************/

static VAR item_MINUS (void)
    {
    VAR v = itemn ();
    if (v.i.t)
        v.f = -v.f; // Both double and long-double
    else if (v.i.n == 0x8000000000000000)
        {
        v.i.t = 1; // ARM
        v.f = 9223372036854775808.0L;
        }
    else
        v.i.n = -v.i.n;
    return v;
    }

/***************************** Address-of operator *****************************/

static VAR item_ADDR (void)
    {
    VAR v;
    unsigned char type;
    void *ptr = getput (&type);
    v.i.t = 0;
    v.i.n = (size_t) ptr;
    return v;
    }

/************************ Decimal constant *************************************/

static VAR item_CONST (void)
    {
    --esi;
    return con ();
    }

/************************ Variable *********************************************/

static VAR item_DEFAULT (void)
    {
    VAR v;
    esi--;
    unsigned char type;
    signed char *savesi = esi;
    void *ptr = getvar (&type);
    if (ptr == NULL)
        error (16, NULL); // 'Syntax error'
    if (type == 0)
        {
        signed char *edi = vpage + (signed char *) zero;
        if ((liston & BIT5) == 0)
            {
            while (range0 (*++esi));
            v.i.t = 0;
#if defined(__x86_64__) || defined(__aarch64__)
            v.i.n = (unsigned int) stavar[16] + ((long long) stavar[17] << 32);
#else
            v.i.n = stavar[16];
#endif
            return v;
            }
        while ((*(unsigned char*)edi != 0) &&
            ((edi = search (edi, '(')) != NULL))
            {
            signed char *oldesi = esi;
            esi = edi + 1;
            edi -= 3;
            ptr = getvar (&type);
            if ((ptr != NULL) && (type == 0))
                ptr = putvar (ptr, &type);
            if (type)
                {
                v.i.t = 0;
                v.i.n = edi - (signed char *) zero;
                storen (v, ptr, type);
                }
            esi = oldesi;
            edi += (int)*(unsigned char*)edi;
            }
        esi = savesi;
        ptr = getvar (&type);
        if (type == 0)
            error (26, NULL); // 'No such variable'
        }
    if (type & BIT6)
        error (14, NULL); // 'Bad use of array'
    if (type & BIT4)
        {
        v.i.t = 0;
        v.i.n = (intptr_t) VLOAD(ptr + sizeof(void *)); // GCC extension: sizeof(void) = 1
        return v;
        }
    if (type < 128)
        return loadn (ptr, type);
    return loads (ptr, type);
    }

typedef VAR (*item_func)(void);

static const item_func item_list[] =
    {
    item_DEFAULT,   // TAND	        -128
	item_DEFAULT,	// TDIV	        -127
	item_DEFAULT,	// TEOR	        -126
	item_TMOD,	    // TMOD	        -125
	item_DEFAULT,	// TOR	        -124
	item_DEFAULT,	// TERROR       -123
	item_DEFAULT,	// TLINE        -122
	item_DEFAULT,	// TOFF	        -121
	item_DEFAULT,	// TSTEP        -120
	item_DEFAULT,	// TSPC	        -119
	item_DEFAULT,	// TTAB	        -118
	item_DEFAULT,	// TELSE        -117
	item_DEFAULT,	// TTHEN        -116
    item_TLINO,     // TLINO        -115
    item_TOPENIN,   // TOPENIN      -114
    item_TPTRR,     // TPTRR        -113
    item_TPAGER,    // TPAGER       -112
    item_TTIMER,    // TTIMER       -111
    item_TLOMEMR,   // TLOMEMR      -110
    item_THIMEMR,   // THIMEMR      -109
    item_TABS,      // TABS         -108
    item_TACS,      // TACS         -107
    item_TADVAL,    // TADVAL       -106
    item_TASC,      // TASC         -105
    item_TASN,      // TASN         -104
    item_TATN,      // TATN         -103
    item_TBGET,     // TBGET        -102
    item_TCOS,      // TCOS         -101
    item_TCOUNT,    // TCOUNT       -100
    item_TDEG,      // TDEG         -99
    item_TERL,      // TERL         -98
    item_TERR,      // TERR         -97
    item_TEVAL,     // TEVAL        -96
    item_TEXP,      // TEXP         -95
    item_TEXTR,     // TEXTR        -94
    item_TFALSE,    // TFALSE       -93
    item_TFN,       // TFN          -92
    item_TGET,      // TGET         -91
    item_TINKEY,    // TINKEY       -90
    item_TINSTR,    // TINSTR       -89
    item_TINT,      // TINT         -88
    item_TLEN,      // TLEN         -87
    item_TLN,       // TLN          -86
    item_TLOG,      // TLOG         -85
    item_TNOT,      // TNOT         -84
    item_TOPENUP,   // TOPENUP      -83
    item_TOPENOUT,  // TOPENOUT     -82
    item_TPI,       // TPI          -81
    item_TPOINT,    // TPOINT       -80
    item_TPOS,      // TPOS         -79
    item_TRAD,      // TRAD         -78
    item_TRND,      // TRND         -77
    item_TSGN,      // TSGN         -76
    item_TSIN,      // TSIN         -75
    item_TSQR,      // TSQR         -74
    item_TTAN,      // TTAN         -73
    item_TTO,       // TTO          -72
    item_TTRUE,     // TTRUE        -71
    item_TUSR,      // TUSR         -70
    item_TVAL,      // TVAL         -69
    item_TVPOS,     // TVPOS        -68
    item_TCHR,      // TCHR         -67
    item_TGETS,     // TGETS        -66
    item_TINKEYS,   // TINKEYS      -65
    item_TLEFT,     // TLEFT        -64
    item_TMID,      // TMID         -63
    item_TRIGHT,    // TRIGHT       -62
    item_TSTR,      // TSTR         -61
    item_TSTRING,   // TSTRING      -60
    item_TEOF,      // TEOF         -59
    item_TSUM,      // TSUM         -58
    item_DEFAULT,   // TWHILE       -57
    item_DEFAULT,   // TCASE        -56
    item_DEFAULT,   // TWHEN        -55
    item_DEFAULT,   // TOF	        -54
    item_DEFAULT,   // TENDCASE     -53
    item_DEFAULT,   // TOTHERWISE   -52
    item_DEFAULT,   // TENDIF	    -51
    item_DEFAULT,   // TENDWHILE    -50
    item_DEFAULT,   // TPTRL	    -49
    item_DEFAULT,   // TPAGEL	    -48
    item_DEFAULT,   // TTIMEL	    -47
    item_DEFAULT,   // TLOMEML	    -46
    item_DEFAULT,   // THIMEML	    -45
    item_DEFAULT,   // TSOUND	    -44
    item_DEFAULT,   // TBPUT	    -43
    item_DEFAULT,   // TCALL	    -42
    item_DEFAULT,   // TCHAIN	    -41
    item_DEFAULT,   // TCLEAR	    -40
    item_DEFAULT,   // TCLOSE	    -39
    item_DEFAULT,   // TCLG	        -38
    item_DEFAULT,   // TCLS	        -37
    item_DEFAULT,   // TDATA	    -36
    item_DEFAULT,   // TDEF	        -35
    item_TDIM,      // TDIM	        -34
    item_DEFAULT,   // TDRAW	    -33
    item_TEND,      // TEND	        -32
    item_DEFAULT,   // TENDPROC     -31
    item_DEFAULT,   // TENVEL	    -30
    item_DEFAULT,   // TFOR	        -29
    item_DEFAULT,   // TGOSUB	    -28
    item_DEFAULT,   // TGOTO	    -27
    item_DEFAULT,   // TGCOL	    -26
    item_DEFAULT,   // TIF	        -25
    item_DEFAULT,   // TINPUT	    -24
    item_DEFAULT,   // TLET	        -23
    item_DEFAULT,   // TLOCAL	    -22
    item_TMODE,     // TMODE	    -21
    item_DEFAULT,   // TMOVE	    -20
    item_DEFAULT,   // TNEXT	    -19
    item_DEFAULT,   // TON	        -18
    item_DEFAULT,   // TVDU	        -17
    item_DEFAULT,   // TPLOT	    -16
    item_DEFAULT,   // TPRINT	    -15
    item_TPROC,     // TPROC	    -14
    item_DEFAULT,   // TREAD	    -13
    item_DEFAULT,   // TREM	        -12
    item_DEFAULT,   // TREPEAT	    -11
    item_TREPORT,   // TREPORT	    -10
    item_DEFAULT,   // TRESTOR	    -9
    item_DEFAULT,   // TRETURN	    -8
    item_DEFAULT,   // TRUN	        -7
    item_DEFAULT,   // TSTOP	    -6
    item_DEFAULT,   // TCOLOUR	    -5
    item_DEFAULT,   // TTRACE	    -4
    item_DEFAULT,   // TUNTIL	    -3
    item_TWIDTH,    // TWIDTH	    -2
    item_DEFAULT,   // TOSCLI	    -1
    item_DEFAULT,   //              0
    item_DEFAULT,   // TCIRCLE	    1
    item_DEFAULT,   // TELLIPSE     2
    item_DEFAULT,   // TFILL	    3
    item_DEFAULT,   // TMOUSE	    4
    item_DEFAULT,   // TORIGIN	    5
    item_DEFAULT,   // TQUIT	    6
    item_DEFAULT,   // TRECT	    7
    item_DEFAULT,   // TSWAP	    8
    item_TSYS,      // TSYS	        9
    item_TTINT,     // TTINT	    10
    item_DEFAULT,   // TWAIT	    11
    item_DEFAULT,   // TINSTALL     12
    item_DEFAULT,   //              13
    item_DEFAULT,   // TPRIVATE     14
    item_DEFAULT,   // TBY	        15
    item_DEFAULT,   // TEXIT	    16
    item_DEFAULT,   //              17
    item_DEFAULT,   //              18
    item_DEFAULT,   //              19
    item_DEFAULT,   //              20
    item_DEFAULT,   //              21
    item_DEFAULT,   //              22
    item_DEFAULT,   //              23
    item_DEFAULT,   //              24
    item_DEFAULT,   //              25
    item_DEFAULT,   //              26
    item_DEFAULT,   //              27
    item_DEFAULT,   //              28
    item_DEFAULT,   //              29
    item_DEFAULT,   //              30
    item_DEFAULT,   //              31
    item_DEFAULT,   // ' '          32
    item_DEFAULT,   // '!'          33
    cons,           // '"'          34
    item_DEFAULT,   // '#'          35
    item_DEFAULT,   // '$'          36
    item_BIN,       // '%'          37
    item_HEX,       // '&'          38
    item_DEFAULT,   // '''          39
    item_BRACKET,   // '('          40
    item_DEFAULT,   // ')'          41
    item_DEFAULT,   // '*'          42
    itemn,          // '+'          43
    item_DEFAULT,   // ','          44
    item_MINUS,     // '-'          45
    item_CONST,     // '.'          46
    item_DEFAULT,   // '/'          47
    item_CONST,     // '0'          48
    item_CONST,     // '1'          49
    item_CONST,     // '2'          50
    item_CONST,     // '3'          51
    item_CONST,     // '4'          52
    item_CONST,     // '5'          53
    item_CONST,     // '6'          54
    item_CONST,     // '7'          55
    item_CONST,     // '8'          56
    item_CONST,     // '9'          57
    item_DEFAULT,   // ':'          58
    item_DEFAULT,   // ';'          59
    item_DEFAULT,   // '<'          60
    item_DEFAULT,   // '='          61
    item_DEFAULT,   // '>'          62
    item_DEFAULT,   // '?'          63
    item_DEFAULT,   // '@'          64
    item_DEFAULT,   // 'A'          65
    item_DEFAULT,   // 'B'          66
    item_DEFAULT,   // 'C'          67
    item_DEFAULT,   // 'D'          68
    item_DEFAULT,   // 'E'          69
    item_DEFAULT,   // 'F'          70
    item_DEFAULT,   // 'G'          71
    item_DEFAULT,   // 'H'          72
    item_DEFAULT,   // 'I'          73
    item_DEFAULT,   // 'J'          74
    item_DEFAULT,   // 'K'          75
    item_DEFAULT,   // 'L'          76
    item_DEFAULT,   // 'M'          77
    item_DEFAULT,   // 'N'          78
    item_DEFAULT,   // 'O'          79
    item_DEFAULT,   // 'P'          80
    item_DEFAULT,   // 'Q'          81
    item_DEFAULT,   // 'R'          82
    item_DEFAULT,   // 'S'          83
    item_DEFAULT,   // 'T'          84
    item_DEFAULT,   // 'U'          85
    item_DEFAULT,   // 'V'          86
    item_DEFAULT,   // 'W'          87
    item_DEFAULT,   // 'X'          88
    item_DEFAULT,   // 'Y'          89
    item_DEFAULT,   // 'Z'          90
    item_DEFAULT,   // '['          91
    item_DEFAULT,   // '\'          92
    item_DEFAULT,   // ']'          93
    item_ADDR,      // '^'          94
    item_DEFAULT,   // '_'          95
    item_DEFAULT,   // '`'          96
    item_DEFAULT,   // 'a'          97
    item_DEFAULT,   // 'b'          98
    item_DEFAULT,   // 'c'          99
    item_DEFAULT,   // 'd'          100
    item_DEFAULT,   // 'e'          101
    item_DEFAULT,   // 'f'          102
    item_DEFAULT,   // 'g'          103
    item_DEFAULT,   // 'h'          104
    item_DEFAULT,   // 'i'          105
    item_DEFAULT,   // 'j'          106
    item_DEFAULT,   // 'k'          107
    item_DEFAULT,   // 'l'          108
    item_DEFAULT,   // 'm'          109
    item_DEFAULT,   // 'n'          110
    item_DEFAULT,   // 'o'          111
    item_DEFAULT,   // 'p'          112
    item_DEFAULT,   // 'q'          113
    item_DEFAULT,   // 'r'          114
    item_DEFAULT,   // 's'          115
    item_DEFAULT,   // 't'          116
    item_DEFAULT,   // 'u'          117
    item_DEFAULT,   // 'v'          118
    item_DEFAULT,   // 'w'          119
    item_DEFAULT,   // 'x'          120
    item_DEFAULT,   // 'y'          121
    item_DEFAULT,   // 'z'          122
    item_DEFAULT,   // '{'          123
    item_DEFAULT,   // '|'          124
    item_DEFAULT,   // '}'          125
    item_DEFAULT,   // '~'          126
    item_DEFAULT,   //              127
    };

VAR item (void)
    {
    item_func fn = item_DEFAULT;
    VAR v ;
    signed char al = nxt () ;
#if PICO_STACK_CHECK & 0x02
    if((&al < (signed char *)libtop + 0x800) && (&al >= (signed char *)userRAM))
        {
        error(0, "Processor stack too deep!");
        }
#endif
    errno = 0 ;
    esi++ ;
#if 1
    return item_list[(int)al + 0x80]();
#else
    if (( al >= TLINO ) && ( al <= TSUM ))
        {
        fn = item_list[al - TLINO];
        }
    else if (((al >= '0') && (al <= '9')) || (al == '.'))
        {
        --esi;
        fn = con;
        }
    else
        {
        /*
        switch (al)
            {
            case TMOD:      fn = item_TMOD;     break;  // -125     1
            case TDIM:      fn = item_TDIM;     break;  // -34      2
            case TEND:      fn = item_TEND;     break;  // -32      3
            case TMODE:     fn = item_TMODE;    break;  // -21      4
            case TPROC:     fn = item_TPROC;    break;  // -14      5
            case TREPORT:   fn = item_TREPORT;  break;  // -10      6
            case TWIDTH:    fn = item_TWIDTH;   break;  // -2       7
            case TSYS:      fn = item_TSYS;     break;  // 9        8
            case TTINT:     fn = item_TTINT;    break;  // 10       9
            case '"':       fn = cons;          break;  // 34       10
            case '%':       fn = item_BIN;      break;  // 37       11
            case '&':       fn = item_HEX;      break;  // 38       12
            case '(':       fn = item_BRACKET;  break;  // 40       13
            case '+':       fn = itemn;         break;  // 43       14
            case '-':       fn = item_MINUS;    break;  // 45       15
            case '^':       fn = item_ADDR;     break;  // 94       16
            default:        fn = item_DEFAULT;  break;
        */
        // Bisection search
        if ( al == TSYS ) fn = item_TSYS;
        else if ( al < TSYS )
            {
            if ( al == TMODE ) fn = item_TMODE;
            else if ( al < TMODE )
                {
                if ( al == TDIM ) fn = item_TDIM;
                else if  ( al < TDIM )
                    {
                    if ( al == TMOD ) fn = item_TMOD;
                    }
                else
                    {
                    if ( al == TEND ) fn = item_TEND;
                    }
                }
            else
                {
                if ( al == TREPORT ) fn = item_TREPORT;
                else if ( al < TREPORT )
                    {
                    if ( al == TPROC ) fn = item_TPROC;
                    }
                else
                    {
                    if ( al == TWIDTH ) fn = item_TWIDTH;
                    }
                }
            }
        else
            {
            if ( al == '&' ) fn = item_HEX;
            else if ( al < '&' )
                {
                if ( al == '"' ) fn = cons;
                else if ( al < '"' )
                    {
                    if ( al == TTINT ) fn = item_TTINT;
                    }
                else
                    {
                    if ( al == '%' ) fn = item_BIN;
                    }
                }
            else
                {
                if ( al == '+' ) fn = itemn;
                else if ( al < '+' )
                    {
                    if ( al == '(' ) fn = item_BRACKET;
                    }
                else
                    {
                    if ( al == '-' ) fn = item_MINUS;
                    else if ( al == '^' ) fn = item_ADDR;
                    }
                }
            }
        }
    return fn ();
#endif
    }

// Check for relational or shift operator.
//           <   0x3C   =   0x3D   >   0x3E
//           <=  0x79   <>  0x7A   >=  0x7B
//           <<  0x04   <<< 0x05   >>  0x06   >>> 0x07

static signed char relop (void)
{
	unsigned char al, op = *esi ;
	if ((op != '<') && (op != '=') && (op != '>'))
		return 0 ;
	esi++ ;
 	al = nxt () ;
	if ((al != '<') && (al != '=') && (al != '>'))
		return op ; // < = >
	esi++ ;
	if (al != op)
		return op + al ; // <= <> >=
	if (al == '=')
		return op ; // == treated as =
	al = nxt () ;
	if (al == op)
	    {
		esi++ ;
		op += 1 ;
	    }
	return (op & 7) ; // << <<< >> >>>
}

// Evaluate an expression:
// Hierarchy: (1) Variables, functions, constants, parenthesized expressions.
//            (2) ^
//            (3) * / MOD DIV
//            (4) + -
//            (5) = <> <= >= > < << >> >>>
//            (6) AND
//            (7) EOR OR

// Level 5: ^:
static VAR expr5 (void)
{
	VAR x = item () ;
	signed char op = nxt () ;
	if (x.s.t == -1)
		return x ; // string
	while (1) 
	    {
		if (op == '^')
		    {
			esi++ ;
			VAR y = item () ;
			if (y.s.t == -1)
				error (6, NULL) ; // 'Type mismatch'
			x = math (x, op, y) ;
		    }
		else
			break ;
		op = nxt () ;
	    }
	return x;
}

// Level 4: *, /, MOD, DIV:
static VAR expr4 (void)
{
	VAR x = expr5 () ;
	if (x.s.t == -1)
		return x ; // string
	while (1) 
	    {
		signed char op = *esi ;
		if ((op == '*') || (op == '/') || (op == TMOD) || (op == TDIV))
		    {
			esi++ ;
			VAR y = expr5 () ;
			if (y.s.t == -1)
				error (6, NULL) ; // 'Type mismatch'
			x = math (x, op, y) ;
		    }
		else
			break ;
	    }
	return x;
}

// Level 3: +, -, SUM:
static VAR expr3 (void)
{
	VAR x = expr4 () ;
	if (x.s.t == -1)
	    {
		while (1)
		    {
			signed char op = *esi ;
			if (op == '+') // concatenate strings
			    {
				char *tmp ;
				heapptr *oldesp ;
				esi++ ;
				oldesp = pushs (x) ;
				VAR y = expr4 () ;
				if (y.s.t != -1)
					error (6, NULL) ; // 'Type mismatch'
				tmp = moves ((STR *) &y, x.s.l) ;
				memmove (tmp, esp, x.s.l) ;
				esp = oldesp ;
				x.s.p = tmp - (char *) zero ;
				x.s.l += y.s.l ;
			    }
			else
				break ;
		    }
		return x ;
	    }
	while (1) 
	    {
		signed char op = *esi ;
		if ((op == '+') || (op == '-') || (op == TSUM))
		    {
			esi++ ;
			VAR y = expr4 () ;
			if (y.s.t == -1)
				error (6, NULL) ; // 'Type mismatch'
			x = math (x, op, y) ;
		    }
		else
			break ;
	    }
	return x;
}

// Level 2: =, <, >, <>, <=, >=, <<, >>, >>>:
// (these operators do not chain)
static VAR expr2 (void)
{
	VAR x = expr3 () ;
	signed char op = relop () ;
	if (op == 0)
		return x ;
	if (x.s.t == -1)
	    {
		VAR y ;
		int n, r ;
		heapptr *oldesp ;

		oldesp = pushs (x) ;
		y = expr3 () ;
		if (y.s.t != -1)
			error (6, NULL) ; // 'Type mismatch'

		n = x.s.l ;
		if (n > y.s.l)
			n = y.s.l ;
		r = memcmp (esp, y.s.p + zero, n) ;
		if (r == 0)
			r = (x.s.l > y.s.l) - (x.s.l < y.s.l) ;
		esp = oldesp ;

		switch (op)
		    {
			case '<':
				x.i.n = -(r < 0) ;
				break ;
			case '=':
				x.i.n = -(r == 0) ;
				break ;
			case '>':
				x.i.n = -(r > 0) ;
				break ;
			case 'y':
				x.i.n = -(r <= 0) ;
				break ;
			case 'z':
				x.i.n = -(r != 0) ;
				break ;
			case '{':
				x.i.n = -(r >= 0) ;
				break ;
			default:
				error (16, NULL) ; // Syntax error
		    }
		x.i.t = 0 ;
	    }
	else
	    {
		VAR y = expr3 () ;
		if (y.s.t == -1)
			error (6, NULL) ; // 'Type mismatch'
		x = math (x, op, y) ;
	    }
	return x;
}

// Level 1: AND:
static VAR expr1 (void)
{
	VAR x = expr2 () ;
	if (x.s.t == -1)
		return x ; // string
	while (1)
	    {
		signed char op = *esi ;
		if (op == TAND)
		    {
			esi++ ;
			VAR y = expr2 () ;
			if (y.s.t == -1)
				error (6, NULL) ; // 'Type mismatch'
			x = math (x, op, y) ;
		    }
		else
			break ;
	    }
	return x;
}

// Level 0: OR, EOR:
VAR expr (void)
{
	VAR x = expr1 () ;
	if (x.s.t == -1)
		return x ; // string
	while (1) 
	    {
		signed char op = *esi ;
		if ((op == TOR) || (op == TEOR))
		    {
			esi++ ;
			VAR y = expr1 () ;
			if (y.s.t == -1)
				error (6, NULL) ; // 'Type mismatch'
			x = math (x, op, y) ;
		    }
		else
			break ;
	    }
	return x;
}

// Evaluate an array expression (strictly left-to-right):
int expra (void *ebp, int ecx, unsigned char type)
{
	int i ;
	signed char op ;
	unsigned char type2 ;
	void *ptr ;

	if (nxt () == TEVAL)
	    {
		int count ;
		signed char *tmpesi ;
		esi++ ;
		VAR v = items () ;
		fixs (v) ;
		lexan (accs, buff, 0) ;
		tmpesi = esi ;
		esi = (signed char *) buff ;
		count = expra (ebp, ecx, type) ; // recursive call
		esi = tmpesi ;
		if (nxt () != ',')
			return count ;
	    }
	else
	    {
		op = '=' ; // for initial assignment
		while (1)
		    {
			void *savebp = ebp ;
			signed char *savesi = esi ;
			signed char saveop = op ;

			if ((nxt () == '-') && (op == '=')) // unary minus
			    {
				esi++ ;
				nxt () ;
				op = '-' ;
			    }

			ptr = getvar (&type2) ;
			if ((ptr != NULL) && (type2 & BIT6)) // RHS is an array
			    {
				if (ptr < (void *)2)
					error (14, NULL) ; // 'Bad use of array'
				ptr = VLOAD(ptr) ;

				if (nxt () == '.') // dot product
					break ;

				if ((type != type2) || (ecx != arrlen(&ptr)))
					error (6, NULL) ; // 'Type mismatch'
				if (type < 128) // numeric array
				    {
					for (i = 0; i < ecx; i++)
					    {
						VAR v = loadn (ptr, type & ~BIT6) ;
						modify (v, ebp, type & ~BIT6, op) ;
						ebp += type & TMASK ; // GCC extension
						ptr += type & TMASK ; // GCC extension
					    }
				    }
				else // string array
				    {
					for (i = 0; i < ecx; i++)
					    {
						modifs (NLOAD(ptr), ebp, type & ~BIT6, op) ;
						ebp += 8 ; // GCC extension (void has size 1)
						ptr += 8 ; // GCC extension (void has size 1)
					    }
				    }
			    }
			else // RHS is not an array
			    {
				VAR v ;
				esi = savesi ;
				op = saveop ;

				if (type < 128) // numeric non-array
				    {
					switch (op)
					    {
						case TOR:
						case TAND:
						case TEOR:
							v = expr3 () ;
							break ;

						case '+':
						case '-':
							v = expr4 () ;
							break ;

						default:
							v = expr5 () ;
					    }
					if (v.s.t == -1)
						error (6, NULL) ; // 'Type mismatch'

					for (i = 0; i < ecx; i++)
					    {
						modify (v, ebp, type & ~BIT6, op) ;
						ebp += type & TMASK ; // GCC extension
						if (nxt () == ',') break ;
					    }
				    }
				else // string non-array
				    {
					heapptr *savesp ;
					v = items () ;
					savesp = pushs (v) ;
					v.s.p = (char *) esp - (char *) zero ;
					for (i = 0; i < ecx; i++)
					    {
						modifs (v, ebp, type & ~BIT6, op) ;
						ebp += 8 ; // GCC extension
						if (nxt () == ',') break ;
					    }
					esp = savesp ;
				    }
			    }

			ebp = savebp ;
			op = nxt () ;
			switch (op)
			    {
				// TODO enforce left-to-right priority
				case TOR:
				case TAND:
				case TEOR:
				case '+':
				case '-':
				case '*':
				case '/':
				case TDIV:
				case TMOD:
					esi++ ;
					continue ;
			    }
			break ;
		    }
	    }

	if (*esi == '.')
	    {
		int i, j, k, dimsl, dimsr, size ;
		int rowsl, colsl, rowsr, colsr ;
		unsigned char type3 ;
		void *rhs ;

		esi++ ; // skip dot
		nxt () ;
		rhs = getvar (&type3) ;
		if ((rhs == NULL) || (ptr == NULL))
			error (16, NULL) ; // 'Syntax error'
		if (type3 == 0)
			error (26, NULL) ; // 'No such variable'
		if ((type != type2) || (type != type3) || (type3 & BIT7))
			error (6, NULL) ; // 'Type mismatch'
		if (rhs < (void *)2)
			error (14, NULL) ; // 'Bad use of array'
		rhs = VLOAD(rhs) ;

		dimsl = *(unsigned char *)ptr ;
		dimsr = *(unsigned char *)rhs ;
		if ((dimsl > 2) || (dimsr > 2))
			error (6, NULL) ; // 'Type mismatch'

		if (dimsl == 2)
		    {
			rowsl = ILOAD(ptr + 1) ; // GCC extension: sizeof(void) = 1
			colsl = ILOAD(ptr + 5) ; // GCC extension: sizeof(void) = 1
			ptr += 9 ;
		    }
		else
		    {
			rowsl = 1 ;
			colsl = ILOAD(ptr + 1) ;
			ptr += 5 ;
		    }

		if (dimsr == 2)
		    {
			rowsr = ILOAD(rhs + 1) ; // GCC extension: sizeof(void) = 1
			colsr = ILOAD(rhs + 5) ; // GCC extension: sizeof(void) = 1
			rhs += 9 ;
		    }
		else
		    {
			colsr = 1 ;
			rowsr = ILOAD(rhs + 1) ;
			rhs += 5 ;
		    }

		if ((colsl != rowsr) || (ecx != (colsr * rowsl)))
			error (6, NULL) ; // 'Type mismatch'

		type &= ~BIT6 ;
		size = type & TMASK ;
		for (i = 0; i < rowsl; i++)
		    {
			void *oldrhs = rhs ;
			for (j = 0; j < colsr; j++)
			    {
				void *oldptr = ptr ;
				void *oldrhs = rhs ;
				for (k = 0; k < colsl; k++)
				    {
					modify (math (loadn (ptr,type), '*', loadn (rhs,type)), 
						ebp, type, '+') ;
					ptr += size ;
					rhs += size * colsr ;
				    }
				ebp += size ;
				rhs = oldrhs + size ;
				ptr = oldptr ;
			    }
			rhs = oldrhs ;
			ptr += size * colsl ;
		    }
		return ecx ;
	    }

	i = 0 ;
	while ((nxt () == ',') && (++i < ecx)) // list of initialisers supplied
	    {
		VAR v ;
		esi++ ;
		if (type < 128)
		    {
			v = exprn () ;
			ebp += type & TMASK ; // GCC extension
			modify (v, ebp, type & ~BIT6, '=') ;
		    }
		else
		    {
			v = exprs () ;
			ebp += 8 ; // GCC extension
			modifs (v, ebp, type & ~BIT6, '=') ;
		    }
	    }

	if (i) ecx = i + 1 ;
	return ecx ;
}

// Function aliases:
int str00 (VAR v, char *dst, int format)
{
	return str (v, dst, format) ;
}
