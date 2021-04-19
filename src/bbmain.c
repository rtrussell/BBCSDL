/*****************************************************************\
*       32-bit or 64-bit BBC BASIC Interpreter                    *
*       (C) 2017-2021  R.T.Russell  http://www.rtrussell.co.uk/   *
*                                                                 *
*       The name 'BBC BASIC' is the property of the British       *
*       Broadcasting Corporation and used with their permission   *
*                                                                 *
*       bbmain.c: Immediate mode, error handling, variable lookup *
*       Version 1.22a, 18-Apr-2021                                *
\*****************************************************************/

#include <stdio.h>
#include <string.h>
#include <setjmp.h>
#include "BBC.h"

// Routines in bbcmos:
void oswrch (unsigned char) ;	// Write to display or other output stream (VDU)
void osline (char *) ;		// Read line of input
void reset (void) ;		// Prepare for reporting an error
void faterr (const char *) ;	// Report a 'fatal' error message
void trap (void) ;		// Test for ESCape
void osload (char*, void*, int) ; // Load a file to memory
void ossave (char*, void*, int) ; // Save a file from memory
int osopen (int, char *) ;	// Open a file
unsigned char osbget (int, int*) ; // Read a byte from a file
void osshut (int) ;		// Close file(s)

// Routines in bbccli:
void oscli (char*) ;            // Command Line Interface

// Routines in bbexec:
VAR xeq (void) ;		// Execute program

// Routines in bbeval:
long long itemi (void);		// Return an integer numeric item
long long expri (void);		// Evaluate an integer numeric expression
long long loadi (void *, unsigned char) ;

// Global jump buffer:
jmp_buf env ;

#ifdef __llvm__
signed char *esi ;		// Program pointer
heapptr *esp ;			// Stack pointer
#endif

// List of immediate mode commands:

static const signed char comnds[] = {
	0x18,'A','U','T','O',
	0x19,'D','E','L','E','T','E',
	0x1A,'E','D','I','T',
	0x1B,'L','I','S','T',
	0x1C,'L','O','A','D',
	0x1D,'N','E','W',
	0x1E,'R','E','N','U','M','B','E','R',
	0x1F,'S','A','V','E',
	0x00,0x7F } ;
	
// List of token values and associated keywords.
// If a keyword is followed by a space it will only
// match the word followed immediately by a delimiter.

static const signed char keywds[] = {
	TAND,'A','N','D',
	TABS,'A','B','S',
	TACS,'A','C','S',
	TADVAL,'A','D','V','A','L',
	TASC,'A','S','C',
	TASN,'A','S','N',
	TATN,'A','T','N',
	TBGET,'B','G','E','T',' ',
	TBPUT,'B','P','U','T',' ',
	TBY,'B','Y',' ',
	TCOLOUR,'C','O','L','O','U','R',
	TCOLOUR,'C','O','L','O','R',
	TCALL,'C','A','L','L',
	TCASE,'C','A','S','E',
	TCHAIN,'C','H','A','I','N',
	TCHR,'C','H','R','$',
	TCLEAR,'C','L','E','A','R',' ',
	TCLOSE,'C','L','O','S','E',' ',
	TCLG,'C','L','G',' ',
	TCLS,'C','L','S',' ',
	TCOS,'C','O','S',
	TCOUNT,'C','O','U','N','T',' ',
	TCIRCLE,'C','I','R','C','L','E',
	TDATA,'D','A','T','A',
	TDEG,'D','E','G',
	TDEF,'D','E','F',
	TDIV,'D','I','V',
	TDIM,'D','I','M',
	TDRAW,'D','R','A','W',
	TENDPROC,'E','N','D','P','R','O','C',' ',
	TENDWHILE,'E','N','D','W','H','I','L','E',' ',
	TENDCASE,'E','N','D','C','A','S','E',' ',
	TENDIF,'E','N','D','I','F',' ',
	TEND,'E','N','D',' ',
	TENVEL,'E','N','V','E','L','O','P','E',
	TELSE,'E','L','S','E',
	TEVAL,'E','V','A','L',
	TERL,'E','R','L',' ',
	TERROR,'E','R','R','O','R',
	TEOF,'E','O','F',' ',
	TEOR,'E','O','R',
	TERR,'E','R','R',' ',
	TEXIT,'E','X','I','T',' ',
	TEXP,'E','X','P',
	TEXTR,'E','X','T',' ',
	TELLIPSE,'E','L','L','I','P','S','E',
	TFOR,'F','O','R',
	TFALSE,'F','A','L','S','E',' ',
	TFILL,'F','I','L','L',
	TFN,'F','N',
	TGOTO,'G','O','T','O',
	TGETS,'G','E','T','$',
	TGET,'G','E','T',
	TGOSUB,'G','O','S','U','B',
	TGCOL,'G','C','O','L',
	THIMEMR,'H','I','M','E','M',' ',
	TINPUT,'I','N','P','U','T',
	TIF,'I','F',
	TINKEYS,'I','N','K','E','Y','$',
	TINKEY,'I','N','K','E','Y',
	TINT,'I','N','T',
	TINSTR,'I','N','S','T','R','(',
	TINSTALL,'I','N','S','T','A','L','L',
	TLINE,'L','I','N','E',
	TLOMEMR,'L','O','M','E','M',' ',
	TLOCAL,'L','O','C','A','L',
	TLEFT,'L','E','F','T','$','(',
	TLEN,'L','E','N',
	TLET,'L','E','T',
	TLOG,'L','O','G',
	TLN,'L','N',
	TMID,'M','I','D','$','(',
	TMODE,'M','O','D','E',
	TMOD,'M','O','D',
	TMOVE,'M','O','V','E',
	TMOUSE,'M','O','U','S','E',
	TNEXT,'N','E','X','T',
	TNOT,'N','O','T',
	TON,'O','N',
	TOFF,'O','F','F',' ',
	TOF,'O','F',' ',
	TORIGIN,'O','R','I','G','I','N',
	TOR,'O','R',
	TOPENIN,'O','P','E','N','I','N',
	TOPENOUT,'O','P','E','N','O','U','T',
	TOPENUP,'O','P','E','N','U','P',
	TOSCLI,'O','S','C','L','I',
	TOTHERWISE,'O','T','H','E','R','W','I','S','E',
	TPRINT,'P','R','I','N','T',
	TPAGER,'P','A','G','E',' ',
	TPRIVATE,'P','R','I','V','A','T','E',
	TPTRR,'P','T','R',' ',
	TPI,'P','I',' ',
	TPLOT,'P','L','O','T',
	TPOINT,'P','O','I','N','T','(',
	TPROC,'P','R','O','C',
	TPOS,'P','O','S',' ',
	TQUIT,'Q','U','I','T',' ',
	TRETURN,'R','E','T','U','R','N',' ',
	TREPEAT,'R','E','P','E','A','T',
	TREPORT,'R','E','P','O','R','T',' ',
	TREAD,'R','E','A','D',
	TREM,'R','E','M',
	TRUN,'R','U','N',' ',
	TRAD,'R','A','D',
	TRESTOR,'R','E','S','T','O','R','E',
	TRIGHT,'R','I','G','H','T','$','(',
	TRND,'R','N','D',' ',
	TRECT,'R','E','C','T','A','N','G','L','E',
	TSTEP,'S','T','E','P',
	TSGN,'S','G','N',
	TSIN,'S','I','N',
	TSQR,'S','Q','R',
	TSPC,'S','P','C',
	TSTR,'S','T','R','$',
	TSTRING,'S','T','R','I','N','G','$','(',
	TSOUND,'S','O','U','N','D',
	TSTOP,'S','T','O','P',' ',
	TSUM,'S','U','M',
	TSWAP,'S','W','A','P',
	TSYS,'S','Y','S',
	TTAN,'T','A','N',
	TTAB,'T','A','B','(',
	TTHEN,'T','H','E','N',
	TTIMER,'T','I','M','E',' ',
	TTINT,'T','I','N','T',
	TTO,'T','O',
	TTRACE,'T','R','A','C','E',
	TTRUE,'T','R','U','E',' ',
	TUNTIL,'U','N','T','I','L',
	TUSR,'U','S','R',
	TVDU,'V','D','U',
	TVAL,'V','A','L',
	TVPOS,'V','P','O','S',' ',
	TWHILE,'W','H','I','L','E',
	TWHEN,'W','H','E','N',
	TWAIT,'W','A','I','T',' ',
	TWIDTH,'W','I','D','T','H',
	THIMEML,'H','I','M','E','M',' ',
	TLOMEML,'L','O','M','E','M',' ',
	TPAGEL,'P','A','G','E',' ',
	TPTRL,'P','T','R',' ',
	TTIMEL,'T','I','M','E',' ',
	0x00,0x7F} ;

// Error messages:

static char* errwds[] = {
	"No room", 		// 0
	"Jump out of range", 	// 1
	"Bad immediate constant", // 2
	"Multiple label", 	// 3
	"Mistake", 		// 4
	"Missing ,", 		// 5
	"Type mismatch", 	// 6
	"Not in a function", 	// 7
	"Address out of range",	// 8
	"Missing \"", 		// 9
	"Bad DIM statement", 	// 10
	"DIM space", 		// 11
	"Not in a FN or PROC", 	// 12
	"Not in a procedure", 	// 13
	"Bad use of array", 	// 14
	"Bad subscript", 	// 15
	"Syntax error", 	// 16
	"Escape", 		// 17
	"Division by zero", 	// 18
	"String too long", 	// 19
	"Number too big", 	// 20
	"Negative root", 	// 21
	"Logarithm range", 	// 22
	"Accuracy lost", 	// 23
	"Exponent range", 	// 24
	"Bad MODE", 		// 25
	"No such variable", 	// 26
	"Missing )", 		// 27
	"Bad hex or binary", 	// 28
	"No such FN/PROC", 	// 29
	"Bad call", 		// 30
	"Incorrect arguments", 	// 31
	"Not in a FOR loop", 	// 32
	"Can't match FOR",	// 33
	"Bad FOR variable", 	// 34
	"STEP cannot be zero", 	// 35
	"Missing TO", 		// 36
	"Missing OF", 		// 37
	"Not in a subroutine", 	// 38
	"ON syntax", 		// 39
	"ON range", 		// 40
	"No such line", 	// 41
	"Out of data", 		// 42
	"Not in a REPEAT loop",	// 43
	"WHEN/OTHERWISE not first", // 44
	"Missing #", 		// 45
	"Not in a WHILE loop", 	// 46
	"Missing ENDCASE", 	// 47
	"OF not last", 		// 48
	"Missing ENDIF", 	// 49
	"Bad MOUSE variable", 	// 50
	"No such system call", 	// 51
	"Bad library", 		// 52
	"Size mismatch", 	// 53
	"DATA not LOCAL", 	// 54
	"Missing \\", 		// 55
	"Bad use of structure",	// 56
        ""} ;

// list1 is tokens that can be followed by an encoded line number:
static signed char list1[] = {TGOTO, TGOSUB, TRESTOR, TTRACE, TTHEN, TELSE, 0} ;

// list2 is tokens that switch the lexical analyser to 'left' mode:
static signed char list2[] = {TTHEN, TELSE, TREPEAT, TERROR, TCLOSE, TMOUSE, TMOVE, TSYS, ':', 0} ;

// Test a character for valid in, or terminating, a variable name.
// Permissible characters are: '{', '.', '#', '$', '%', '&', '(', 0-9, '@', '_', '`', A-Z and a-z.
// Test most likely matches first, n.b. individual compares are faster than 'strchr'.
int range0 (char c)
{
	return (((c >= '_') && (c <= '{')) ||
	        ((c >= '@') && (c <= 'Z')) || 
	        ((c >= '0') && (c <= '9')) ||
		((c >= '#') && (c <= '&')) ||
	         (c == '(') || (c == '.')) ;
}

// Test a character for valid in a variable name.
// Permissible characters are: 0-9, '@', '_', '`', A-Z and a-z.
int range1 (char c)
{
	return (((c >= '_') && (c <= 'z')) ||
	        ((c >= '@') && (c <= 'Z')) ||
	        ((c >= '0') && (c <= '9'))) ;
}

// Test a character for valid as the first character of a variable name.
// Permissible characters are: '@', '_', '`', A-Z and a-z.
int range2 (char c)
{
	return (((c >= '_') && (c <= 'z')) ||
	        ((c >= '@') && (c <= 'Z'))) ;
}

// Handle error condition
// If NULL supplied as msg, look up message from code
// If ON ERROR is active, execution continues
// Error code zero signifies a 'fatal' error
// Error code < 0 signifies QUIT (negative exitcode)
// Error code 256 signifies return to immediate mode
void error (int code, const char * msg)
{
	if ((code < 0) || (code >= 256))
		longjmp (env, code) ; // Quit or immediate mode
	if (msg == NULL)
	    {
		if (code <= 56)
			msg = errwds [code] ;
		else
			msg = "Unknown error" ;
	    }
	errnum = code ;
	errtxt = msg ;
	errlin = curlin ;
	if (code != 0)
		longjmp (env, code) ;
	faterr (msg) ;
	longjmp (env, -1) ;
}

// Return next non-space character (handling line continuation)
signed char nxt (void)
{
	unsigned char al ;
	while ((al = *(unsigned char *)esi) == ' ')
		esi++ ;
	if (al == '\\')
	    {
		esi = (signed char *) memchr((const char *) esi, 0x0D, 256) + 4 ;
		curlin = esi - (signed char *) zero ;
		if (*esi++ != '\\')
			error (55, NULL) ; // 'Missing \'
		return nxt () ;
	    }
	return al ;
}

// Simple search for token at start of line:
signed char *search (signed char *edx, signed char token)
{
	int ll ;
	while (((ll = (int)*(unsigned char *)edx) != 0) && (*((signed char *)edx+3) != token))
		edx += ll ;
	if (ll)
		return edx + 3 ;
	return NULL ; 
}

// Encode line number into pseudo binary form.
static char * encode (unsigned short lino, char *ebx)
{
	unsigned char al = lino & 0xC0 ;
	unsigned char ah = (lino >> 8) & 0xC0 ;
	lino = (lino & 0x3F3F) | 0x4040 ;
	*ebx++ = TLINO ;
	*ebx++ = ((al | (ah >> 2)) >> 2) ^ 0x54 ;
	*ebx++ = lino & 0xFF ;
	*ebx++ = lino >> 8 ;
	return ebx ;
}

// Search for a keyword
// Return token if found, unchanged character if not found
// If found, advance pointer past keyword
static signed char tokit (char **pesi, const signed char *ebx)
{
	signed char al, ah ;
	while (1)
	    {
		char *esi = *pesi ;
		signed char tok = *ebx++ ;
		al = *esi++ ;
		ah = *ebx++ ;
		if (al < ah) break ;
		if ((al == ah) || ((liston & BIT3) && ((al - 0x20) == ah)))
		    {
			signed char lc = al - ah ;
			do
			    {
				al = *esi++ ;
				ah = *ebx++ ;
				if ((ah == '(') || (ah == '$'))
					lc = 0 ;
			    }
			while (((al - lc) == ah) && (ah > ' ')) ;
			if ((al == '.') || (ah < ' ') || ((ah == ' ') && !range1(al)))
			    {
				if (al == '.')
					*pesi = esi - 1 ;
				else
					*pesi = esi - 2 ;
			  	return tok ;
			    }
		    }
		while (*ebx >= ' ') ebx++ ;
	    }
	return **pesi ;
}

// Lexical analysis:
char *lexan (char *esi, char *ebx, unsigned char mode)
{
	signed char al ;
	while (1)
	    {
		al = *esi ;
		if (al == 0x0D) break ;
		if (!range1(al)) mode &= ~(BIT3+BIT5) ;
		if ((al != ' ') && (al != ','))
		    {
			if ((al >= 'g') || (al == '@') || (al == '_') || (al == '`') ||
			((al >= 'G') && (((liston & BIT3) == 0) || (al < 'a'))))
				mode &= ~BIT3 ; // not in hex
			if (al == '"') mode ^= BIT7 ;
			if (mode & BIT4)
			    {
				mode &= ~BIT4 ;
				unsigned int lino = 0 ;
				int n = 0 ;
				if (al != '+')
					sscanf (esi, " %u%n", &lino, &n) ;
				esi += n ;
				if (lino)
				    {
					mode |= BIT4 ;
					encode (lino, ebx) ;
					ebx += 4 ;
				    }
				continue ;
			    }
			if (mode <= 1)
			    {
				if (mode == 1)	// left mode
				    {
					mode = 0 ; // right mode
					if (al == '*')
						mode |= BIT6 ;
					else if ((al >= 'A') && (al <= 'z'))
						al = tokit (&esi, keywds) ;
					if (al == TDATA)
						mode |= BIT6 ;
					else if ((al >= TOKLO) && (al <= TOKHI))
						al += OFFSIT ;
				    }
				else if ((al >= 'A') && (al <= 'z'))
					al = tokit (&esi, keywds) ;
				if (al == TREM) mode |= BIT6 ; // quit tokenising
				if ((al == TFN) || (al == TPROC) || range2(al)) mode |= BIT5 ;
				if (al == '&') mode |= BIT3 ; // in hex
				if (strchr((const char *)list1, al)) mode |= BIT4 ; // accept line number
				if (strchr((const char *)list2, al)) mode |= BIT0 ; // enter left mode
			    }
		    }
		*ebx++ = al ;
		esi++ ;
	    }
	*ebx++ = 0x0D ;
	return ebx ;
}

void crlf (void) ;

// Output a character:
void outchr (unsigned char al)
{
	oswrch (al) ;
	if (al == 0x0D) vcount = 0 ;
	if (al >= ' ')
	    {
		vcount += 1 ;
		if ((vwidth != 0) && (vcount == vwidth))
		    crlf () ;
	    }
}

// Output a character or keyword:
void token (signed char al)
{
	if (al >= ' ')
		outchr (al) ;
	else
	    {
		signed char *tok = (signed char*) strchr ((char *)keywds, al) ;
		if (tok != NULL)
			while (*++tok > ' ')
				outchr (*tok | ((liston & BIT3) << 2)) ;
	    }
}

// Output Carriage Return, Line Feed:
void crlf (void)
{
	outchr (0x0D) ;
	outchr (0x0A) ;
	vcount = 0 ;
}

// Output a NUL-terminated string:
void text (const char *txt)
{
	while (*txt)
		outchr (*txt++) ;
}

// List a program line without CRLF (entered with pointer to line number)
void listline (signed char *p, int *pindent)
{
	int n ;
	signed char al = 0 ;
	char number[7] ;
	unsigned char mode = BIT0 ; // set left
	unsigned short lino = *(unsigned short*) p ;
	if (lino)
		sprintf (number, "%5d", lino) ;
	else
		sprintf (number, "     ") ;
	text (number) ;
	p += 2 ;

	if (lstopt & 1)
		oswrch (32) ;

	if (strchr ("\355\375\316\315\213\311\314", *p))
		*pindent -= 1 ;
	if (*p == TENDCASE)
		*pindent -= 2 ;
	if (lstopt & 2)
		for (n = 0; n < *pindent * 2; n++)
			oswrch (' ') ;
	if (strchr ("\343\365\307\213\311\314", *p))
		*pindent += 1 ;
	if (*p == TCASE)
		*pindent += 2 ;

	while (*p != 0x0D)
	    {
		al = *p++ ;
		if ((al == '"') && !(mode & 0x60))
			mode ^= BIT7 ;
		if (mode & (BIT5 | BIT6 | BIT7))
			oswrch (al) ;
		else
		    {
			if ((al == '*') && (mode & BIT0))
				mode |= BIT4 ; // *command
			if ((al == TDATA) && (mode & BIT0))
				mode |= BIT5 ; // DATA
			if (al == TREM)
				mode |= BIT6 ; // REM
			if (al != ' ')
				mode &= ~(BIT0 | BIT1) ; // right mode, clear EXIT
			if (al == TEXIT)
				mode |= BIT1 ; // EXIT
			if (strchr((const char *)list2, al))
				mode |= BIT0 ;
			if (al == TLINO)
			    {
				unsigned char ah = *(unsigned char *)p++ ;
				lino = ((*(unsigned char *)p++) ^ ((ah << 2) & 0xC0)) ;
				lino += ((*(unsigned char *)p++) ^ ((ah << 4) & 0xC0)) * 256 ;
				sprintf (number, "%d", lino) ;
				text (number) ;
			    }
			else
				token (al) ;
		    }
		if (!(mode & 0xF2))
		    {
			if (strchr ("\343\365\307", *p))
				*pindent += 1 ;
			if (strchr ("\355\375\316", *p))
				*pindent -= 1 ;
			if (*p == TCASE)
				*pindent += 2 ;
		    }
	    }
	if (!(mode & 0xF2) && (al == TTHEN))
		*pindent += 1 ;
}

// Search for top of program
// Return pointer to terminating NUL
// If NULL is returned 'bad program'.
signed char *gettop (signed char *ebx, unsigned short *reserved)
{
	if (reserved != NULL) *reserved = 0 ;
	int n ;
	do
	    {
		if ((n = (int)*(unsigned char *)ebx) == 0)
			return ebx ;
		ebx += n ;
		if (n == 3)
		    {
			if (reserved != NULL)
				*reserved = * ((unsigned short *) ebx - 1) ;
			if (*ebx == 0) return ebx ;
			return NULL ;
		    }
	    }
	while (*(ebx - 1) == 0x0D) ;
	return NULL ;
}

// Clear all dynamic variables including functions and procedures
// Make space for 'fast' variables if appropriate
void clear (void)
{
	unsigned short fastvars ;
	signed char *ebx = vpage + (signed char *) zero ;
	signed char *top = gettop (ebx, &fastvars) ;
	if (top == NULL)
	    {
		error (0, "Bad program") ;
		return ;
	    }
	*(top+1) = 0xFF ;
	*(top+2) = 0xFF ;
	lomem = top + 3 - (signed char *) zero ;
	if (fastvars)
		lomem = (lomem + 7) & -8 ; // align
	memset (lomem + zero, 0, 4 * fastvars) ;
	pfree = lomem + 4 * fastvars ;
	memset (dynvar, 0, 4 * (54 + 2)) ;
	memset (flist, 0, sizeof(void *) * 33 + 8) ;
	link00 = 0 ;
}

// Find the line containing a particular address
// Return NULL if not found
static signed char* findlin (signed char *ebx, signed char *edx, char **pebp)
{
	int n  = 0 ;
	if (pebp != NULL) *pebp = NULL ;
	while ((edx >= ebx) && (n = (int)*(unsigned char *)ebx) != 0) // must compare first
	    {
		if ((*((signed char *)ebx+3) == 0) && (pebp != NULL))
			*pebp = (char *) ebx + 4 ; // library name
		ebx += n ;
	    }
	if (n == 0)
		return NULL ;
	return (ebx - n) ;
}

// Search user's program and libraries for
// the line containing a particular address
// If the address is not within the user's program
// or libraries, return zero.
unsigned short setlin (signed char *edx, char **pebp)
{
	signed char *tmp = findlin(vpage + zero, edx, NULL) ;
	if ((tmp == NULL) && (libase != 0))
		tmp = findlin(libase + zero, edx, pebp) ;
	if (tmp == NULL)
		return 0 ;
	return *(unsigned short *)(tmp + 1) ;
}

// Find a specified numbered line in the program by
// searching from the beginning.  The performance of
// GOTO, GOSUB and RESTORE is critically dependent
// on the speed of this routine.
// Can optionally be entered with a target address.
signed char * findl (unsigned int edx)
{
	signed char *ebx = vpage + (signed char *) zero ;
	if (*ebx == 0)
		return NULL ; // No program
	if ((edx + (signed char *) zero) >= ebx)
	    {
		ebx = edx + (signed char *) zero ;
		int n = (int)*(unsigned char *)ebx ;
		if (*(ebx + n - 1) == 0x0D)
			return ebx ;
		return NULL ;
	    }
	edx &= 0xFFFF ;
	while (edx > *(unsigned short *)(ebx + 1))
		ebx += (int)*(unsigned char *)ebx ; 
	if (edx == *(unsigned short *)(ebx + 1))
		return ebx ;
	return NULL ;
}

// Clear ON ERROR, ON event pointers etc.
void clrtrp (void)
{
	if (errtrp >= vpage)
		errtrp = 0 ; // ON ERROR
	onersp = NULL ;
	timtrp = 0 ; // ON TIME
	clotrp = 0 ; // ON CLOSE
	siztrp = 0 ; // ON MOVE
	systrp = 0 ; // ON SYS
	moutrp = 0 ; // ON MOUSE
	evtqw = 0 ; // Flush event queue...
	evtqr = 0 ; // ... after xxxtrp
}

// Prepare an error message in the string accumulator
unsigned short report (void)
{
	char *module = NULL ;
	unsigned short lino ;
	strcpy (accs, errtxt) ;
	lino = setlin (errlin + zero, &module) ;
	if (module != NULL)
	    {
		strcat (accs, " in module ") ;
		strncat (accs, module, (char *)memchr(module, 0x0D, 255) - module) ;
	    }
	return lino ;
}

// Free old string (if any) pointed to by descriptor,
// allocate space for new string and update descriptor:
char * allocs (unsigned int *ps, int len)
{
	char *addr ;
	node *head ;
	int new = 0, old = 0, size ;

	if (len)
		new = 32 - __builtin_clz (len) ;
	if (*(ps+1))
		old = 32 - __builtin_clz (*(ps+1)) ;
	*(ps+1) = len ;

// if old and new strings have the same allocation, just change the length:

	if (old == new)
	    {
		return *ps + (char *) zero ; 
	    }

	size = ((1 << new) - 1) ; // new allocation

// the allocations differ: so first see if new allocation is in free list;
// if it is, just swap with old allocation:

	if (flist[new] != NULL)
	    {
		head = flist[new] ;
		flist[new] = head->next ; // remove from 'new' list
		head->next = flist[old] ;
		flist[old] = head ; 	  // insert into 'old' list
		addr = head->data ;
		head->data = *ps + (char *) zero ; 
		*ps = addr - (char *) zero ;
		return addr ;
	    }

// new allocation is not in free list, see if we can expand into heap.
// It is extremely important that a block in the free list is used *
// IN PREFERENCE TO expanding into the heap.

	if (old && ((*ps + (1 << old) - 1) == pfree))
	    {
		addr = *ps + (char *) zero ;
		if (size > ((char *)esp - addr - STACK_NEEDED))
			error (0, NULL) ; // 'No room'
		pfree = addr + size - (char *) zero ;
		return addr ;
	    }

// add old allocation to the free list (unless zero):

	if (old)
	    {
		if (flist[0]) // spare node available?
		    {
			head = flist[0] ; 
			flist[0] = head->next ;
		    }
		else
		    {
			addr = ((pfree + 3) & -4) + (char *) zero ;
			pfree = addr + sizeof (node) - (char *) zero ;
			head = (node *) addr ;
		    }
		head->data = *ps + (char *) zero ;
		head->next = flist[old] ;
		flist[old] = head ;
	    }

// allocate new string space from the heap:

	addr = ((pfree + 1) & -2) + (char *) zero ; // Unicode align
	if (size > ((char *)esp - addr - STACK_NEEDED))
		error (0, NULL) ; // 'No room'
	pfree = addr + size - (char *) zero ;
	*ps = addr - (char *) zero ;
	return addr ;
}

// Allocate memory for a temporary string:
//  For lengths < 65536 use the string accumulator
//  For lengths >= 65536 allocate from the heap
char *alloct (int len) 
{
	if (len < 65536)
		return accs ;
	return allocs ((unsigned int *)&tmps, len) ;
}

// Move string into a temporary buffer (with optional offset):
char *moves (STR *ps, int offset)
{
	char *dst = alloct (offset + ps->l) ;
 	memmove (dst + offset, ps->p + zero, ps->l) ;
	return dst ;
}

// Copy string to string accumulator and append CR:
void fixs (VAR v)
{
	if (v.s.l > 65535)
		error (19, NULL) ; // 'String too long'
	memmove (accs, v.s.p + zero, v.s.l) ;
	*(accs + v.s.l) = 0x0D ;
}

// Push a string onto the stack, returning the original stack pointer:
heapptr *pushs (VAR v)
{
	heapptr *oldesp = esp ;
	if (v.s.l > ((char *)esp - (char *)zero - pfree - STACK_NEEDED))
		error (0, NULL) ; // 'No room'
	esp = (heapptr *)(((size_t)esp - v.s.l) & -4) ;
	memmove (esp, v.s.p + zero, v.s.l) ;
	return oldesp ;
}

// Check for running out of memory:
void check (void)
{
	if ((pfree + STACK_NEEDED + (char *) zero) > (char *)esp)
		error (0, NULL) ; // 'No room'
}

void comma (void)
{
	if (nxt () != ',')
		error (5, NULL) ; // 'Missing ,'
	esi++ ;
}

void braket (void)
{
	if (nxt () != ')')
		error (27, NULL) ; // 'Missing )'
	esi++ ;
}

// Count number of elements in an array and return pointer to the first:
int arrlen (void **pebx)
{
	int dims ;
	unsigned char *ebx = *(unsigned char **)pebx ;
	int edx = 1 ;
	if ((ebx < (unsigned char*)2) || (*ebx == 0))
		error(14, NULL) ; // 'Bad use of array'
	dims = *ebx++ ;
	while (dims--)
	    {
		edx *= *(unsigned int *)ebx ;
		ebx += 4 ;
	    }
	*pebx = ebx ;
	return edx ;
} 

// Process array subscripts
// Returns offset into array data
static int getsub (void **pebx, unsigned char *ptype)
{
	int dims ;
	unsigned char *ebx = *(unsigned char **)pebx ;
	int edx = 0 ;
	if ((ebx < (unsigned char*)2) || (*ebx == 0))
		error(14, NULL) ; // 'Bad use of array'
	dims = *ebx++ ;
	while (dims--)
	    {
		unsigned long long n = expri () ;
		int ecx = *(unsigned int *)ebx ;
		ebx += 4 ;
		if (n >= ecx)
			error (15, NULL) ; // 'Subscript out of range'
		edx = edx * ecx + n ;
		if (dims) comma () ; else braket () ;
	    }
	*pebx = ebx ;
	edx *= (*ptype & TMASK) ;
	return edx ;
}

// Make struct.array&() look like a NUL-terminated string:
static int getsbs (void *ebx, unsigned char *ptype)
{
	if (nxt () == ')') 
	    {
		if (*ptype != 1)
			error (15, NULL) ;
		esi++ ; // skip )
		*ptype = 130 ; 
		return 0 ;
	    }
	return getsub (&ebx, ptype) ;
}

// Create a new variable and initialise to zero.
// Called only after getvar() has discovered variable does not exist.
// Returns pointer to variable
// Types are: 1 = unsigned byte
//            4 = signed integer
//            8 = 64-bit floating point
//           10 = 80-bit floating point
//           16 = structure (64-bit) 
//           24 = structure (32-bit)
//           40 = 64-bit signed integer
//          136 = (moveable) string
void *create (unsigned char **pedi, unsigned char *ptype)
{
	int i, size ;
	signed char al ;
	unsigned char *edi = *pedi ;

	while (range1 (al = *esi++))
		*edi++ = al ;
	switch (al) 
	    {
		case '%':
			if (*esi == al)
			    {
				esi++ ;
				*edi++ = al ;
				*ptype = 40 ;
				break ;
			    }
			*ptype = 4;
			break ;
		case '$':
			*ptype = 136 ;
			break ;
		case '#':
			*ptype = 8 ;
			break ;
		case '&':
			*ptype = 1 ;
			break ;
		case '{':
		case '.':
			*ptype = STYPE ;
			al = '{' ;
			break ;
		default:
			*ptype = 10 ;
			esi-- ;
	    }
	if (*ptype != 10)
		*edi++ = al ; // type character
	size = *ptype & TMASK ;
	if (*esi == '(')
	    {
		*edi++ = '(' ;
		size = 8 ; // array pointer
	    }
	for (i = 0; i <= size; i++)
		*edi++ = 0 ; // terminate and initialise

	*pedi = edi ;
	return edi - size ;
}

// As create but handle whole array and whole structure
void * putvar (void *ebx, unsigned char *ptype)
{
	unsigned char *edi = pfree + (unsigned char *) zero ;

	*(heapptr *)edi = *(heapptr *) ebx ;
	*(heapptr *)ebx = edi - (unsigned char *) zero ;
	edi += 4 ;

	ebx = create (&edi, ptype) ;
	pfree = edi - (unsigned char *) zero ;

	check () ;
	if (*esi == '(')
	    {
		esi++ ;
		if (nxt () != ')')
			error (14, NULL) ; // 'Bad use of array'
		esi++ ;
		*ptype |= BIT6 ; // Flag whole array
	    }
	if ((*ptype & BIT4) && (*esi == '}'))
		esi++ ;
	return ebx ;
}

// As create but for creating FN and PROC entries :
void * putdef (void *ebx)
{
	unsigned char type ;
	unsigned char *edi = pfree + (unsigned char *) zero ;

	*(heapptr *)edi = *(heapptr *) ebx ; 
	*(heapptr *)ebx = edi - (unsigned char *) zero ;
	edi += 4 ;

	ebx = create (&edi, &type) ;
	pfree = ebx + sizeof(void *) - zero ; // room for FN/PROC pointer

	check () ;
	return ebx ;
}

// Scan linked-list for variable etc. (used for regular dynamic variables,
// system variables, structure members, function and procedure definitions).
// If found move to head of list unless sysvar or structure (base link = 0).
// Return pointer to terminator character:
static void *scanll (heapptr *base, signed char *edi)
{
	signed char al ;
	signed char *save = esi ;
	void *prev = NULL, *this ;
	int next ; // n.b. signed for relative links

	if (base && (edi < ((signed char *) zero + 6)))
		return NULL ; // not found

	do
	    {
		this = edi ;
		edi += 4 ;  // skip link
		while (*esi++ == *edi++) ;
		esi-- ; edi-- ;
		al = *edi ; // first character not to match
		if (((al == 0) && !range0(*esi)) ||	// full match
                    ((al == 0) && (*(esi-1) == '(')) ||	// array
		    ((al == 0) && (*esi != '%') && (*esi != '(') && !range1(*(esi-1))) || // PRINT a#b
		    ((al == '{') && (*++edi == 0) && (*esi == '.')) || // structure member
		    ((al == '%') && (base == NULL) && (*(esi-1) == '%') && (*esi != '(') && (*++edi == 0)) ||
		    ((al == '%') && (base == NULL) && (*(esi-1) == '%') && (*esi == '(') && 
						(*++edi == '(') && (*++edi == 0)))
		    {
			if (base && prev && ((this - zero) != *base))
			    {
				next = *(heapptr *)base ;
				*(heapptr *)base = this - zero ;
				*(heapptr *)prev = *(heapptr *)this ;
				*(heapptr *)this = next ;
			    }
			if (*(esi-1) == '(')
				esi-- ;
			return edi + 1 ;
		    }
		esi = save ;
		prev = this ;
		next = *(int *)prev ;
		if (base)
			edi = next + (signed char *) zero ;
		else
			edi = this + next ;
	    }
	while (next) ;
	return NULL ; // not found
}

// Try to locate a function or procedure, or indirect call
void *getdef (unsigned char *found)
{
	void *ebx ;
	signed char al = *esi ;

	*found = 0 ;
	if (al == TFN)
		ebx = &fnptr[0] ;
	else if (al == TPROC)
		ebx = &proptr[0] ;
	else
		return NULL ;

	al = *++esi ;
	if (al == 0x18)
	    {
		unsigned short index = *(unsigned short *)(esi + 1) ;
		esi += 3 ;
		*found = 1 ;
		return lomem + index * 4 + zero ;
	    }
	if (range1(al))
	    {
		void *ptr = scanll (ebx, *(heapptr *)ebx + zero) ;
		if (ptr != NULL)
		    {
			*found = 1 ;
			return ptr ;
		    }
		return ebx ;
	    }
	if (al == '(')
	    {
		void *n = (void *) (size_t) itemi () ;
		if ((size_t) n < 0x10000)
			error (8, NULL) ; // 'Address out of range'
		*found = 1 ;
		return n ;
	    }
	return NULL ;
}

// Find variable/array type from suffix character(s):
static unsigned char getype (char *ptr)
{
	char al = *(ptr - 2) ; // type character
	char ah = *(ptr - 3) ; // to check for %%
	if (al == '(')
	    {
		al = *(ptr - 3) ; // array type
		ah = *(ptr - 4) ;
	    }
	switch (al)
	    {
		case '%':
			if (ah == '%')
				return 40 ;
			else
				return 4 ;

		case '#':
			return 8 ;

		case '$':
			return 136 ;

		case '{':
			return STYPE ; 

		case '&':
			return 1 ;
	    }
	return 10 ;
}

// Try to locate variable (etc.) in static or dynamic variables.
// If illegal initial character, return NULL.
// If not found, return pointer to linked-list base link and set type to 0.
// If found, return pointer to variable (etc.) and set type as appropriate.
// Types are:   1 = unsigned byte (a&)
//              4 = 32-bit signed integer (a%)
//              8 = 64-bit floating point (a#)
//             10 = 80-bit floating point (a)
//             16 = structure (a{ or a.) 64-bit
//             24 = structure (a{ or a.) 32-bit
//             36 = FN/PROC
//             40 = 64-bit signed integer (a%%)
//            136 = string (a$)
static void *locate (unsigned char *ptype)
{
	void *ebx, *edx ;
	char *ptr ;
	signed char al = *esi ;
	*ptype = 0 ;

	if (al < '@')
	    { // FN, PROC or fastvar
		if ((al >= 0x19) && (al <= 0x1F))
		    {
			*ptype = fvtab[(int)(al - 0x19)] ;
			ebx = lomem + (*(unsigned short *)(esi + 1) << 2) + zero ;
			esi += 3 ;
			return ebx ;
		    }
		ebx = getdef (ptype) ;
		if (ebx == NULL)
			return NULL ;
		if (*ptype == 0)
			error (29, NULL) ; // 'No such FN/PROC'
		*ptype = 36 ;
		return ebx ;
	    }
	if ((al <= 'Z') && (*(esi+1) == '%') && (*(esi+2) != '(') && (*(esi+2) != '%'))
	    { // Static integer variable
		esi += 2 ;
		*ptype = 4 ;
		return &stavar[al - '@'] ;
	    }
	if (al > 'Z')
	    {
		if ((al < '_') || (al > 'z'))
			return NULL ;
		al -= 4 ;
	    }
	if (al == '@')
	    {
		ebx = NULL ;
		edx = &sysvar ;
	    }
	else
	    {
		ebx = &dynvar[al - 'A'] ;
		edx = *(heapptr *) ebx + zero ;
	    }
	esi++ ;
	ptr = scanll (ebx, edx) ;
	if (ptr == NULL)
		return ebx ;

	*ptype = getype (ptr) ;
	return ptr ;
}

// Try to locate variable or FN/PROC in static or dynamic variables.
// If invalid, return NULL.
// If not found, return pointer to linked-list terminator and set type to 0.
// If found, return pointer to variable (etc.) and set type as appropriate.
// Types are: 1, 4, 5, 8, 10, 40 numeric
//            128, 130, 136 string
//            16/24, 80/88 structure, structure array
//            36, 100 function or procedure
//            65, 68, 72, 74, 104, 200 whole array
void * getvar (unsigned char *ptype)
{
	void *ebx ;
	signed char al = *esi ;
	*ptype = 0 ;

	if ((al == '$') || (al == '?') || (al == '!') || (al == '|') || (al == ']'))
	    {
		void *n ;
		esi++ ;
		if (al == '$')
		    {
			if (*esi == '$')
		  	    {
				esi ++ ;
				*ptype = 130 ;
			    }
			else
				*ptype = 128 ;
		    }
		else if (al == '?')
			*ptype = 1 ;
		else if (al == '!')
			*ptype = 4 ;
		else if (al == ']')
			*ptype = 40 ;
		else if ((liston & 3) == 0)
			*ptype = 5 ;
		else if ((liston & 3) == 1)
			*ptype = 8 ;
		else
			*ptype = 10 ;

		n = (void *) (size_t) itemi () ;
		if ((size_t) n < 0x10000)
		    {
			if ((size_t) n >= 0x400)
				error (8, NULL) ; // 'Address out of range'
			return (char *)&stavar[0] + (size_t) n ;
		    }
		return n ;
	    }

	ebx = locate (ptype) ;
	if ((*ptype == 0) || (ebx == NULL))
		return ebx ;

	if (*esi == '(')
	    {
		int ecx ;
		esi++ ;
		if (nxt () == ')')
		    {
			esi++ ;
			*ptype |= BIT6 ; // Flag whole array
			if ((*ptype & BIT4) && (*esi == '}'))
				esi++ ;
			return ebx ;
		    }
		if (*ptype == 36)
			return ebx ; // FNxxx( or PROCxxx( so not an array
		ebx = *(void **)ebx ;
		ecx = getsub (&ebx, ptype) ; // get array data pointer
		ebx += ecx ; // n.b. ebx is modified by getsub !
	    }

	if (*ptype & BIT4)
	    {
		void *ebp = *(void **)((int *)ebx + STRIDE) ;  // data pointer
		while (((al = *esi) == '.') || (al == '}'))
		    {
			if (al == '}')
				al = *++esi ;

			if (al == '.')
			    {
				signed char *edx = *(void **)ebx ; // template pointer
				if (edx == NULL)
					error (26, NULL); // 'No such variable'
				esi++ ; 
				edx += 4 ; 		    // skip size record
				ebx = scanll (NULL, edx) ;
				if (ebx == NULL)
					error (26, NULL); // 'No such variable'
				*ptype = getype (ebx) ;
				if (*ptype & BIT4) 
				    {
					ebp += *((int *)ebx + STRIDE) ;  // data pointer
					continue ; // recurse into nested structure
				    }
				ebp += *(int *) ebx ; // address data/array
				if (*esi == '(')
				    {
					esi++ ;
					ebx += 4 ; // n.b. GCC extension: sizeof(void) = 1
					ebx = ebp + getsbs (ebx, ptype) ;
				    }
				else
					ebx = ebp ;
			    }
			if (ebx < (void *)0x10000)
				error (56, NULL) ; // 'Bad use of structure
			break ;
		    }
	    }

	if ((((al = *esi) == '!') || (al == '?')) && (*ptype < 128))
	    {
		esi++ ;
		void *n = (void *) (size_t) loadi (ebx, *ptype) + itemi () ;
		if (al == '!')
			*ptype = 4 ;
		else
			*ptype = 1 ;
		if ((size_t) n < 0x10000)
			return (char *)&stavar[0] + (size_t) n ;
		return n ;
	    }

	return ebx ;
}

// Get a variable pointer / type, creating it if necessary:
void *getput (unsigned char *ptype)
{
	void *ptr = getvar (ptype) ;
	if (ptr == NULL)
		error (16, NULL) ; // 'Syntax error'
	if (*ptype == 0)
		ptr = putvar (ptr, ptype) ;
	return ptr ;
}

// Called from DIM:
void * getdim (unsigned char *ptype)
{
	void *ebx ;
	unsigned char *edi = pfree + (unsigned char *) zero ;
	signed char *oldesi = esi ;
	char c = nxt () ;

	if ((c == '!') || (c == ']'))
		return getvar (ptype) ;

	*ptype = 0 ;

	ebx = locate (ptype) ;

	if (ebx == NULL)
		return ebx ;

	if ((*ptype) && (*esi != '(') && (*esi != '}'))
	    {
		esi = oldesi ;
		return getvar (ptype) ;
	    }

	if (*ptype)
		return ebx ;

	*(heapptr *)edi = *(heapptr *) ebx ;
	*(heapptr *)ebx = edi - (unsigned char *) zero ;
	edi += 4 ;

	ebx = create (&edi, ptype) ;
	pfree = edi - (unsigned char *) zero ;
	return ebx ;
}

// Get a range of line numbers [lo[,[hi]]]:
static void lrange (char *ptr, unsigned short *plo, unsigned short *phi)
{
	int n = 0 ;
	*plo = 0 ;
	*phi = 0 ;
	if (sscanf (ptr, "%hu ,%n%hu", plo, &n, phi) == 0)
		sscanf (ptr, " ,%n%hu", &n, phi) ;
	if ((*phi == 0) && (*plo == 0))	*phi = 0xFFFF ;
	if ((*phi == 0) && (n != 0))    *phi = 0xFFFF ;
}

// Replacement for strstr() which uses a string terminator other than NUL:
static char *strstrt (signed char *lookin, char *lookfor, char t)
{
	char *t1 = memchr (lookin, t, 256) ;
	char *t2 = memchr (lookfor, t, 256) ;
	if ((t1 == NULL) || (t2 == NULL)) return NULL ;
	*t1 = 0 ; *t2 = 0 ;
	char *result = strstr ((char *) lookin, lookfor) ;
	*t1 = t ; *t2 = t ;
	return result ;
}

// Fixup line-number cross-references in a line:
static void fixup (signed char *ptr, int nlines, unsigned short start, unsigned short step)
{
	signed char c ;
	int quote = 0 ;
	unsigned short n ;
	unsigned short lino = *(unsigned short *)(ptr + 1) ;
	ptr += 3 ;
	while ((c = *ptr++) != 0x0D)
	    {
		if (c == '"') quote = !quote ;
		if ((c == TLINO) && !quote) 
		    {
			int i ;
			unsigned char ah = *(unsigned char *)ptr++ ;
			n = ((*(unsigned char *)ptr++) ^ ((ah << 2) & 0xC0)) ;
			n += ((*(unsigned char *)ptr++) ^ ((ah << 4) & 0xC0)) * 256 ;

			for (i = 0; i < nlines; i++)
				if (n == *(unsigned short *)(lomem + zero + i * 2))
					break ;

			if (n == *(unsigned short *)(lomem + zero + i * 2))
				encode (start + i * step, (char *) ptr - 4) ;
			else
			    {
				sprintf (accs, "No such line %hu referenced in (new) "
						"line %hu\r\n", n, lino) ;
				text (accs) ;
			    }
		    }
	    }
}

// Main interpreter entry point:
int basic (void *ecx, void *edx, void *prompt)
{
	int errcode ;
	unsigned short autoinc = 0, autonum = 0 ;

	stavar[0] = 0x90A ;	// Initialise @%
	liston = 0x30 ;		// Initialise OPT/*FLOAT/*HEX/*LOWERCASE
	lstopt = 3 ;		// Initialise LISTO
	errtxt = szNotice ;
	cmdadr = szCmdLine - (char *) zero ;
	cmdlen = strlen (szCmdLine) ;
	diradr = szLoadDir - (char *) zero ;
	dirlen = strlen (szLoadDir) ;
	libadr = szLibrary - (char *) zero ;
	liblen = strlen (szLibrary) ;
	usradr = szUserDir - (char *) zero ;
	usrlen = strlen (szUserDir) ;
	tmpadr = szTempDir - (char *) zero ;
	tmplen = strlen (szTempDir) ;
	vpage = ecx - zero ;
	curlin = ecx - zero ;
	himem = edx - zero ;
	clear () ;
	datptr = search (vpage + (signed char *) zero, TDATA) - (signed char *) zero ;

	esi = vpage + 3 + (signed char *) zero ;
	esp = (heapptr *)((himem + (size_t) zero) & -4) ;

	errcode = (setjmp (env)) ; // >=0 = error, <0 = QUIT, 256 = END/STOP

	if (errcode < 0)
		return ~errcode ;

	if (errcode)
		prompt = (void *) 1 ;

	if ((errcode > 0) && (errcode < 256))
	    {
		esp = (heapptr *)((himem + (size_t) zero) & -4) ;
		if (errtrp)
		    {
			esi = errtrp + (signed char *) zero ;
			prompt = NULL ;
			if (onersp != NULL)
				esp = onersp ;
		    }
		else
		    {
			unsigned short lino ;
			tracen = 0 ;
			reset () ;
			crlf () ;
			lino = report () ;
			text (errtxt) ;
			if (lino)
			    {
				sprintf (accs, " at line %d", lino) ;
				text (accs) ;
			    }
			crlf () ;
			prompt = (void *) 1 ;
			autonum = 0 ;
			autoinc = 0 ;
		    }
	    }

	while (prompt)
	    {
		int n = 0 ;
		unsigned short lino = 0 ;

		esp = (heapptr *)((himem + (size_t) zero) & -4) ;
		if (autonum)
		    {
			sprintf (accs, "%5hu ", autonum) ;
			text (accs) ;
		    }			
		else if (autoinc)
			autoinc = 0 ;
		else
			oswrch ('>') ;

		liston = (liston & 0x0F) | 0x30 ;
		clrtrp () ;
		osline (accs) ;
		*(char *)(memchr (accs, 0x0D, 256) + 1) = 0 ; // Add NUL term for sscanf
		crlf () ;

		sscanf (accs, "%hu%n", &lino, &n) ;
		if (lino == 0)
		    {
			n = 0 ;
			lino = autonum ;
			autonum += autoinc ;
		    }
		while (*(accs + n) == 32) n++ ;
		*(lexan (accs + n, buff, 1)) = 0 ;	// Lexical analysis
		curlin = buff - (char *)zero ; // In case of error

		if (lino)
		    {
			signed char *tmp = vpage + (signed char *) zero ;
			clear () ;
			n = strlen (buff) + 3 ;
			while (lino > *(unsigned short *)(tmp + 1))
				tmp += (int)*(unsigned char *)tmp ; 
			if (lino == *(unsigned short *)(tmp + 1))
				memmove (tmp, tmp + *(unsigned char *)tmp,
				gettop (vpage + zero, NULL) - tmp + 3 - *(unsigned char *)tmp) ;
			if (n > 4)
			    {
				memmove (tmp + n, tmp, gettop (vpage + zero, NULL) - tmp + 3) ;
				*(unsigned char *)tmp = n ;
				*(unsigned short *)(tmp + 1) = lino ;
				memcpy (tmp + 3, buff, n - 3) ;
			    }
			clear () ; // essential to set lomem correctly
		    }
		else
		    {
			unsigned short lo, hi ;
			char *tmp = accs + n ;
			n = tokit (&tmp, comnds) ;
			tmp++ ;
			switch (n)
			    {
				case 0x18: // AUTO
					lrange (tmp, &autonum, &autoinc) ;
					if (autonum == 0) autonum = 10 ;
					if (autoinc == 0xFFFF) autoinc = 10 ;
					if (autoinc == 0) autoinc = 10 ;
					break ;

				case 0x19: // DELETE
					clear () ;
					lrange (tmp, &lo, &hi) ;
					if ((lo == 0) && (hi == 0xFFFF)) error (16, NULL) ;
					if (hi == 0) hi = lo ;
					esi = vpage + (signed char *) zero ;
					while (*esi && (*(unsigned short *)(esi + 1) < lo))
						esi += (int)*(unsigned char *)esi ;
					tmp = (char*) esi ;
					while (*esi && (*(unsigned short *)(esi + 1) <= hi))
						esi += (int)*(unsigned char *)esi ;
					memmove (tmp, esi, gettop (vpage + zero, NULL) - esi + 3) ;
					break ;

				case 0x1A: // EDIT
					lrange (tmp, &lo, &hi) ;
					if ((lo == 0) && (hi == 0xFFFF)) error (16, NULL) ;
					if (hi == 0) hi = lo ;
					esi = findl (lo) ;
					if (esi == NULL) error (41, NULL) ;
					strcpy (accs, "spool \042") ;
					strcat (accs, szTempDir) ;
					strcat (accs, "bbc.edit.tmp\042\015") ;
					oswrch (21) ;
					oscli (accs) ;
					while (*esi && (*(unsigned short *)(esi + 1) <= hi))
					    {
						n = 0 ;
						listline (esi + 1, &n) ;
						esi += *(unsigned char *)esi ;
					    }
					*(accs + 5) = 0x0D ;
					oscli (accs) ;
					oswrch (6) ;
					memcpy (accs, "exec  ", 6) ;
					oscli (accs) ;
					autoinc = 1 ; // suppress prompt
					break ;

				case 0x1B: // LIST[O]
					if ((*tmp == 'O') || (*tmp == 'o'))
					    {
						n = 0 ;
						sscanf (tmp + 1, "%u", &n) ;
						lstopt = n ;
						break ;
					    }
					lrange (tmp, &lo, &hi) ;
					if (hi == 0) hi = lo ;
					esi = vpage + (signed char *) zero ;
					n = 0 ;
					*(lexan (tmp, buff, 1)) = 0 ; // to support LISTIF
					tmp = strchr (buff, TIF) ;
					while ((tmp != NULL) && (*(++tmp) == ' ')) ;
					while (*esi && (*(unsigned short *)(esi + 1) < lo))
						esi += (int)*(unsigned char *)esi ; 
					while (*esi && (*(unsigned short *)(esi + 1) <= hi))
					    {
						trap () ;
						if ((tmp == NULL) || (strstrt (esi + 3, tmp, 13)))
						    {
							listline (esi + 1, &n) ;
							crlf () ;
						    }
						esi += *(unsigned char *)esi ;
					    }
					break ;

				case 0x1C: // LOAD
					memset (vpage + zero, 0, 256) ; // in case a short text file
					osload (tmp, vpage + zero, (void *)esp -
						 (vpage + zero) - STACK_NEEDED) ;
					esi = vpage + (signed char *) zero ;
					while (*esi)
					    {
						esi += (int)*(unsigned char *)esi ; 
						if (*(esi-1) != 0x0D) break ;
					    }
					if (*(esi-1) != 0x0D)
					    {
						int eof = 0 ;
						int file = osopen (0, tmp) ;
						esi = vpage + (signed char *) zero ;
						lino = 0 ;
						while (1)
						    {
							tmp = accs ; n = 65535 ;
							do *tmp = osbget (file, &eof) ;
							while (!eof && --n && (*tmp++ != 0x0A)) ;
							if (eof || (n <= 0)) break ;
							*(tmp - 1) = 0x0D ; *tmp = 0 ;
							tmp = accs ; lino++ ; n = 0 ;
							sscanf (tmp, "%hu%n", &lino, &n) ;
							tmp += n ;
							while ((*tmp == 32) || (*tmp == 9)) tmp++ ;
							n = lexan (tmp, (char *) esi + 3, 1)
								- (char *) esi ;
							if (n > 255) break ;
							*esi = n ;
							*(unsigned short *)(esi + 1) = lino ;
							esi += n ;
						    }
						osshut (file) ;
						*esi = 0 ;
					    }
					clear () ;
					break ;

				case 0x1D: // NEW 
					*(signed char *)(vpage + zero) = 0 ;
					break ;

				case 0x1E: // RENUMBER
					clear () ;
					lrange (tmp, &lo, &hi) ;
					if (lo == 0) lo = 10 ;
					if (hi == 0xFFFF) hi = 10 ;
					if (hi == 0) hi = 10 ;
					esi = vpage + (signed char *) zero ;
					n = 0 ;
					while (*esi)
					    {
						*(unsigned short *)(lomem + zero + 2*n) = 
						*(unsigned short *)(esi + 1) ;
						esi += *(unsigned char *)esi ;
						n++ ;
					    }
					if ((lo + n*hi - hi) > 65535) error (20, NULL) ; 
					esi = vpage + (signed char *) zero ;
					lino = lo ;
					while (*esi)
					    {
						unsigned char c = *(unsigned char *)esi ;
						*(unsigned short *)(esi + 1) = lino ;
						if (memchr (esi + 3, TLINO, c - 4))
							fixup (esi, n, lo, hi) ;
						lino += hi ;
						esi += c ;
					    }
					break ;

				case 0x1F: // SAVE
					clear () ;
					ossave (tmp, vpage + zero, gettop (vpage + zero, NULL) -
						(signed char *) (vpage + zero) + 3) ;
					break ;

				default:
					prompt = NULL ;
			    }
		    }

		esi = (signed char *) buff ;
	    }
	xeq () ;
	return 0 ;
}
