/******************************************************************\
*       BBC BASIC Minimal Console Version                         *
*       Copyright (C) R. T. Russell, 2021                         *
*                                                                 *
*       bbccos.c: Command Line Interface, ANSI VDU drivers        *
*       Version 0.32a, 02-Mar-2021                                *
\*****************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "bbccon.h"

#ifdef __WIN32__
#include <io.h>
typedef int timer_t ;
#define myfseek _fseeki64
#else
#define myfseek fseek
#endif

#ifdef __APPLE__
#include <dispatch/dispatch.h>
typedef dispatch_source_t timer_t ;
#endif

#undef MAX_PATH
#define NCMDS 39	// number of OSCLI commands
#define POWR2 32	// largest power-of-2 less than NCMDS
#define COPYBUFLEN 4096	// length of buffer used for *COPY command
#define _S_IWRITE 0x0080
#define _S_IREAD 0x0100
#define MAX_PATH 260
#define NUMMODES 10

// External routines:
void trap (void) ;
int oskey (int) ;
void error (int, const char *) ;
void oswrch (unsigned char) ;
void crlf (void) ;
void outchr (unsigned char) ;
void text (const char*) ;
void listline (signed char*, int*) ;
void quiet (void) ;
timer_t StartTimer (int) ;
void StopTimer (timer_t) ;
void SystemIO (int) ;
int stdin_handler (int*, int*) ;
int getkey (unsigned char *) ;

// Global variables:
extern timer_t UserTimerID ;

static short modetab[NUMMODES][5] =
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
        {640,512,16,16,16}     // MODE 9
} ;

static char *cmds[NCMDS] = {
		"bye", "cd", "chdir", "copy", "del", "delete", "dir",
		"dump", "era", "erase", "esc", "exec", "float", "fx",
		"help", "hex", "input", "key", "list", "load", "lock", "lowercase",
		"md", "mkdir", "output", "quit", "rd", "refresh",
		"ren", "rename", "rmdir", "run", "save", "spool", "spoolon",
		"timer", "tv", "type", "unlock"} ;

enum {
		BYE, CD, CHDIR, COPY, DEL, DELETE, DIRCMD,
		DUMP, ERA, ERASE, ESC, EXEC, FLOAT, FX,
		HELP, HEX, INPUT, KEY, LIST, LOAD, LOCK, LOWERCASE,
		MD, MKDIR, OUTPUT, QUIT, RD, REFRESH,
		REN, RENAME, RMDIR, RUN, SAVE, SPOOL, SPOOLON,
		TIMER, TV, TYPE, UNLOCK} ;

// Change to a new screen mode:
static void newmode (short wx, short wy, short cx, short cy, short nc, signed char bc) 
{
	printf ("\033[8;%d;%dt", wy/cy, wx/cx) ;

	if (bc & 1)
		printf ("\033(0") ;
	else
		printf ("\033(B") ;

	if (bc & 0x80)
		printf ("\033[30m\033[107m") ;
	else
		printf ("\033[37m\033[40m") ;

	printf ("\033[H\033[J") ;

	scroln = 0 ;
}

//VDU 22,n - MODE n
static void modechg (char al) 
{
	short wx, wy, cx, cy, nc ;

	if (al >= NUMMODES)
		return ;

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
	case 1:		// cursor on/off
		if (a)
			printf ("\033[?25h") ;
		else
			printf ("\033[?25l") ;
		break ;

	case 16:	// enable line wrap
		cmcflg = (cmcflg & b) ^ a ;
		break;

	case 22:	// user-defined mode
		newmode (a + 256*b, c + 256*d, e, f, g, h) ;
		if (h & 8)
			vflags |= UTF8 ;
		else
			vflags &= ~UTF8 ;
		break ;
	}
}

static void newline (int *px, int *py)
{
	int oldrow = *py ;
	printf ("\012") ;
	if (!stdin_handler (NULL, py)) *py += 1 ;
	if (*px)
		printf ("\033[%i;%iH", *py + 1, *px + 1) ;
	if ((oldrow == *py) && (scroln & 0x80) && (--scroln == 0x7F))
	    {
		unsigned char ch ;
		scroln = 0x7F + oldrow ;
		do
		    {
			usleep (5000) ;
			stdin_handler (NULL, NULL) ;
		    } 
		while ((getkey (&ch) == 0) && ((flags & (ESCFLG | KILL)) == 0)) ;
	    }
}

/*****************************************************************\
*       VDU codes                                                 *
\*****************************************************************/

// Execute a VDU command:
//          0  ^
//          0  | ev.user.code
//          V  |
//          h  v
//          g  ^
//          f  | ev.user.data1
//          e  |
//          d  v
//          c  ^
//          b  | ev.user.data2
//          a  |
//          n  v

void xeqvdu (int code, int data1, int data2)
{
	int vdu = code >> 8 ;
	static int col = 0, row = 0 ;
	static int rhs = 999 ;

#ifdef __WIN32__
	if (!_isatty (_fileno (stdin)) || !_isatty (_fileno (stdout)))
#else
	if (!isatty (STDIN_FILENO) || !isatty (STDOUT_FILENO))
#endif
	    {
		fwrite (&vdu, 1, 1, stdout) ;
		return ;
	    }

	if ((vflags & VDUDIS) && (vdu != 6))
		return ;

	if ((rhs == 999) && (stdin_handler (&col, &row)))
	    {
		printf ("\033[%i;999H", row + 1) ;
		stdin_handler (&rhs, NULL) ;
		printf ("\033[%i;%iH", row + 1, col + 1) ;
	    }

	switch (vdu)
	    {
		case 2: // PRINTER ON
			printf ("\033[5i") ;
		  	break ;

		case 3: // PRINTER OFF
			printf ("\033[4i") ;
			break ;

		case 4: // LO-RES TEXT
			vflags &= ~HRGFLG ;
			break ;

		case 5: // HI-RES TEXT
			vflags |= HRGFLG ;
			break ;

		case 6: // ENABLE VDU DRIVERS
			vflags &= ~VDUDIS ;
			break ;

		case 7: // BELL
			fwrite (&vdu, 1, 1, stdout) ;
			break ;

		case 8: // LEFT
			if (col == 0)
			    {
				col = rhs ;
				if (row == 0)
					printf ("\033M") ;
				else
					row -- ;
				printf ("\033[%i;%iH", row + 1, col + 1) ;
			    }
			else
			    {
				col-- ;
				fwrite (&vdu, 1, 1, stdout) ;
			    }
			break ;

		case 9: // RIGHT
			if (col == rhs)
			    {
				printf ("\015") ;
				col = 0 ; // in case of aborted cursor query
				newline (&col, &row) ;
			    }
			else
			    {
				col++ ;
				printf ("\033[C") ;
			    }
			break ;

		case 10: // LINE FEED
			newline (&col, &row) ;
			break ;

		case 11: // UP
			printf ("\033M") ;
			if (!stdin_handler (&col, &row))
				row -= 1 ;
			break ;

		case 12: // CLEAR SCREEN
			printf ("\033[H\033[J") ;
			col = 0 ;
			row = 0 ;
			break ;

		case 13: // RETURN
			fwrite (&vdu, 1, 1, stdout) ;
			col = 0 ;
			break ;

		case 14: // PAGING ON
			scroln = 0x80 + row ;
			break ;

		case 15: // PAGING OFF
			scroln = 0 ;
			break ;

		case 17: // COLOUR n
			vdu = 30 + (code & 7) ;
			if (code & 8)
				vdu += 60 ;
			if (code & 0x80)
				vdu += 10 ;
			printf ("\033[%im", vdu) ;
			break ;

		case 20: // RESET COLOURS
			printf ("\033[37m\033[40m") ;
			break ;

		case 21: // DISABLE VDU DRIVERS
			vflags |= VDUDIS ;
			break ;

		case 22: // MODE CHANGE
			modechg (code & 0x7F) ;
			col = 0 ;
			row = 0 ;
			rhs = 999 ;
			break ;

		case 23: // DEFINE CHARACTER ETC.
			defchr (data2 & 0xFF, (data2 >> 8) & 0xFF, (data2 >> 16) & 0xFF,
				(data2 >> 24) & 0xFF, data1 & 0xFF, (data1 >> 8) & 0xFF,
				(data1 >> 16) & 0xFF, (data1 >> 24) & 0xFF, code & 0xFF) ;
			if ((data2 & 0xFF) == 22)
			    {
				col = 0 ;
				row = 0 ;
				rhs = 999 ;
			    }
			break ;

		case 26: // RESET VIEWPORTS
			col = 0 ;
			row = 0 ;
			rhs = 999 ;
			break ;

		case 27: // SEND NEXT TO OUTC
			if (col > rhs)
			    {
				printf ("\015") ;
				col = 0 ;
				newline (&col, &row) ;
			    }
			fwrite (&code, 1, 1, stdout) ;
			if ((col == rhs) && ((cmcflg & 1) == 0))
			    {
				printf ("\015") ;
				col = 0 ;
				newline (&col, &row) ;
			    }
			else if (!(vflags & UTF8) || ((signed char)(code & 0xFF) >= -64))
				col++ ;
			break ;

		case 30: // CURSOR HOME
			printf ("\033[H") ;
			col = 0 ;
			row = 0 ;
			scroln &= 0x80 ;
			break ;

		case 31: // TAB(X,Y)
			col = data1 >> 24 & 0xFF ;
			row = code & 0xFF ;
			printf ("\033[%i;%iH", row + 1, col + 1) ;
			break ;

		case 127: // DEL
			xeqvdu (0x0800, 0, 0) ;
			xeqvdu (0x2000, 0, 0) ;
			xeqvdu (0x0800, 0, 0) ;
			break ;

		default:
			if (vdu >= 0x20)
			    {
				if (col > rhs)
				    {
					printf ("\015") ;
					col = 0 ;
					newline (&col, &row) ;
				    }
				fwrite (&vdu, 1, 1, stdout) ;
				if ((col == rhs) && ((cmcflg & 1) == 0))
				    {
					printf ("\015") ;
					col = 0 ;
					newline (&col, &row) ;
				    }
				else if (!(vflags & UTF8) || ((signed char)vdu >= -64))
					col++ ;
			    }
	    }
	fflush (stdout) ;
}

// Parse a filespec, return pointer to terminator.
// Note that source string is CR-terminated
char *setup (char *dst, char *src, char *ext, char term, unsigned char *pflag)
{
	unsigned char flag = 0 ;

	while (*src == ' ') src++ ;		// Skip leading spaces
	if ((*src == '"') && (term != '"'))
	{
		char *ret ;
		src++ ;
		ret = setup (dst, src, ext, '"', pflag) ;
		if (*ret++ != '"')
			error (253, "Bad string") ;
		while (*ret == ' ')
			ret++ ;
		return ret ;
	}

	while (1)
	{
		char ch = *src++ ;
		if ((ch == ',') || (ch == 0x0D) || (ch == term))
			break ;
		flag |= BIT0 ;			// Flag filename
		if (ch == '.')
			flag |= BIT7 ;		// Flag extension
		if ((ch == '/') || (ch == '\\'))
		{
			flag &= ~(BIT0 + BIT7) ;// Flag no extension, no filename
			flag |= BIT1 ;		// Flag path present
		}
		*dst++ = ch ;
	}

	if (flag & BIT7)
	{
		if ((*(dst-1) == '.') && (*(dst-2) != '.'))
			dst-- ;
	}
	else if (flag & BIT0)
	{
		strcpy (dst, ext) ;
		dst += strlen (ext) ;
	}

	if (pflag != NULL)
		*pflag = flag ;
	*dst = 0 ;
	return src - 1 ;
}

// Parse a *KEY string:
// Note that source string is CR-terminated
static int parse (char *dst, char *src, char term)
{
	int n = 0 ;

	while ((term != '"') && (*src == ' ')) src++ ;		// Skip leading spaces
	if (*src == '"')
		return parse (dst, src + 1, '"') ;
	while ((*src != 0x0D) && (*src != term))
	{
		char c, m = 0 ;
		while ((c = *src++) == '|')
		{
			c = *src++ ;
			if (c == '!')
			    {
				m = 0x80 ;
				continue ;
			    }
			else if ((c >= '?') && (c < '`'))
				c ^= 0x40 ;
			break ;
		}
		if (dst) *dst++ = c | m ;
		n++ ;
	}
	if (dst) *dst = 0 ;
	if ((term == '"') && (*src != term))
		error (253, "Bad string") ;
	return n ;
}

// Parse ON or OFF:
static int onoff (char *p)
{
	int n = 0 ;
	if ((*p & 0x5F) == 'O')
		p++ ;
	sscanf (p, "%x", &n) ;
	return !n ;
}

// Wildcard string compare
// First string may contain ? and * wildcards
static int wild (char *ebx, char *edx)
{
	char *ecx = NULL ;

	if (*ebx == 0)
		return 1 ;		// Empty string matches everything
	if (strcmp (ebx, "*.*") == 0)
		return 1 ;		// "*.*" matches everything

	while (1)
	{
		char al = 0x20 | *ebx++ ;
		char ah = 0x20 | *edx++ ;

		if (al == '?')
		{
			if (*(edx - 1) == 0)
				return 0 ;
			continue ;
		}

		if (al == ah)
		{
			if (*(ebx - 1) == 0)
				return 1 ;
			continue ;
		}

		if (al != '*')
		{
			if (ecx == NULL)
				return 0 ;
			ebx = ecx ;
			edx-- ;
		}

		ecx = ebx ;
		if (*(edx - 1) == 0)
			return 0 ;
		al = *ebx++ ;
		if (al == 0)
			return 1 ;
		al |= 0x20 ;

		do
		{
			ah = *edx++ ;
			if (ah == 0)
				return 0 ;
			ah |= 0x20 ;
		}
		while (al != ah) ;
	}
	return 0 ;
}

void oscli (char *cmd)
{
	int b = 0, h = POWR2, n ;
	char cpy[256] ;
	char path[MAX_PATH], path2[MAX_PATH] ;
	FILE *srcfile, *dstfile ;
	DIR *d ;
	char *p, *q, dd ;
	unsigned char flag ;

	while (*cmd == ' ') cmd++ ;

	if ((*cmd == 0x0D) || (*cmd == '|'))
		return ;

	q = memchr (cmd, 0x0D, 256) ;
	if (q == NULL)
		error (204, "Bad name") ;
	memcpy (cpy, cmd, q - cmd) ;
	cpy[q - cmd] = 0 ;
	p = cpy ;
	while ((*p = tolower (*p)) != 0) p++ ;

	do
	    {
		if (((b + h) < NCMDS) && ((strcmp (cpy, cmds[b + h])) >= 0))
			b += h ;
		h /= 2 ;
	    }
	while (h > 0) ;

	n = strchr(cpy, '.') - cpy ;
	if ((n > 0) && ((b + 1) < NCMDS) &&
			(n <= strlen (cmds[b + 1])) &&
			(strncmp (cpy, cmds[b + 1], n) == 0))
		b++ ;

	p = cpy ;
	q = cmds[b] ;
	while (*p && *q && (*p == *q))
	    {
		p++ ;
		q++ ;
	    }

 	if (*p == '.')
		p++ ;
	else if (*q)
	    {
		b = RUN ;
		p = cpy ;
		if ((*p == '*') || (*p == '/'))
			p++ ;
	    }

	if (n == 0)
		b = DIRCMD ;

	p += cmd - cpy ;
	while (*p == ' ') p++ ;		// Skip leading spaces

	switch (b)
	    {
		case BYE:			// *BYE
		case QUIT:			// *QUIT
			error (-1, NULL) ;

		case CD:			// *CD [directory]
		case CHDIR:			// *CHDIR [directory]
			setup (path, p, "", ' ', &flag) ;
			if (flag == 0)
			    {
				getcwd (path, MAX_PATH) ;
				text (path) ;
				crlf () ;
				return ;
			    }
			if (chdir (path))
				error (206, "Bad directory") ;
			return ;

		case COPY:			// *COPY oldfile newfile
			p = setup (path, p, ".bbc", ' ', NULL) ;
			srcfile = fopen (path, "rb") ;
			if (srcfile == NULL)
				error (214, "File or path not found") ;
			setup (path, p, ".bbc", ' ', NULL) ;
			dstfile = fopen (path, "wb") ;
			if (dstfile == NULL)
			    {
				fclose (srcfile) ;
				error (189, "Couldn't close file") ;
			    }
			p = malloc (COPYBUFLEN) ;
			do
			    {
				n = fread (p, 1, COPYBUFLEN, srcfile) ;
				if (n == 0)
					break ;
			    }
			while (fwrite (p, 1, n, dstfile)) ;
			free (p) ;
			fclose (srcfile) ;
			fclose (dstfile) ;
			if (n)
				error (189, "Couldn't copy file") ;
			return ;

		case DEL:			// *DEL filename
		case DELETE:			// *DELETE filename
		case ERA:			// *ERA filename
		case ERASE:			// *ERASE filename
			setup (path, p, ".bbc", ' ', NULL) ;
			if (remove (path))
				error (254, "Bad command") ;	// Bad command
			return ;

		case DIRCMD:
			setup (path, p, ".bbc", ' ', &flag) ;
			if ((flag & BIT0) == 0)
				strcat (path, "*.bbc") ;
			if (flag & BIT1)
			    {
				p = path + strlen (path) ;
				q = path ;
				while ((*p != '/') && (*p != '\\')) p-- ;
				if ((p == path) || (*(p - 1) == ':'))
				    {
					dd = 0 ;
					*++p = 0 ;	// root
				    }
				else
				    {
					dd = *p ;
					*p++ = 0 ;	// not root
				    }
			    }
			else
			    {
				getcwd (path2, MAX_PATH) ;
#ifdef __WIN32__
				dd = '\\' ;
#else
				dd = '/' ;
#endif
				p = path ;
				q = path2 ;
			    }
			text ("Directory of ") ;
			text (q) ;
			outchr (dd) ;
			text (p) ;
			crlf () ;

			d = opendir (q) ;
			if (d == NULL)
				error (254, "Bad command") ;

			while (1)
			    {
				stdin_handler (NULL, NULL) ;
				if (flags & (ESCFLG | KILL))
				    {
					closedir (d) ;
					crlf () ;
					trap () ;
				    }
				struct dirent *r = readdir (d) ;
				if (r == NULL)
					break ;
				if (!wild (p, r -> d_name))
					continue ;
				outchr (' ') ;
				outchr (' ') ;
				text (r -> d_name) ;
				do
					outchr (' ') ;
				while ( (vcount != 0) && (vcount != 20) &&
					(vcount != 40) && (vcount < 60)) ;
				if (vcount > 60)
					crlf () ;
			    }
			closedir (d) ;
			crlf () ;
			return ;

		case ESC:
			if (onoff (p))
				flags &= ~ESCDIS ;
			else
				flags |= ESCDIS ;
			return ;

		case EXEC:
			if (exchan)
			    {
				fclose (exchan) ;
				exchan = NULL ;
			    }
			setup (path, p, ".bbc", ' ', NULL) ;
			if (*path == 0)
				return ;
			exchan = fopen (path, "rb") ;
			if (exchan == NULL)
				error (214, "File or path not found") ;
			return ;

		case FLOAT:
			n = 0 ;
			sscanf (p, "%i", &n) ;
			switch (n)
			    {
				case 40:
					liston &= ~(BIT0 + BIT1) ;
					break ;
				case 64:
					liston &= ~BIT1 ;
					liston |= BIT0 ;
					break ;
				case 80:
#if defined __arm__ || defined __aarch64__
					error (255, "Unsupported") ;
#else
					liston |= (BIT0 + BIT1) ;
#endif
					break ;
				default:
					error (254, "Bad command") ;
			    }
			return ;

		case FX:
			n = 0 ; b = 0 ;
			sscanf (p, "%i,%i", &n, &b) ;
			if (n == 15)
			    {
				if (b == 0)
					quiet () ;
				kbdqr = kbdqw ;
			    }
			else if (n == 21)
			    {
				if (b == 0)
					kbdqr = kbdqw ;
				else if ((b >= 4) && (b <= 7))
				    {
					sndqw[b - 4] = 0 ;
					sndqr[b - 4] = 0 ;
					eenvel[b - 4] = 0 ;
				    }
			    }
			return ;

		case HELP:
			text (szVersion) ;
			crlf () ;
			return ;

		case HEX:
			n = 0 ;
			sscanf (p, "%i", &n) ;
			switch (n)
			    {
				case 32:
					liston &= ~BIT2 ;
					break ;
				case 64:
					liston |= BIT2 ;
					break ;
				default:
					error (254, "Bad command") ;
			    }
			return ;

		case KEY:
			n = 0 ;
			if (*p != 0x0D)
				n = strtol (p, &p, 10) ;
			if ((n < 1) || (n > 24))
				error (251, "Bad key") ;
			if (*(keystr + n))
			    {
				free (*(keystr + n)) ;
				*(keystr + n) = NULL ;
			    }
			b = parse (NULL, p, 0) ;
			if (b == 0)
				return ;
			*(keystr + n) = malloc (b + 1) ;
			parse (*(keystr + n), p, 0) ;
			return ;

		case LIST:
			setup (path, p, ".bbc", ' ', NULL) ;
			srcfile = fopen (path, "rb") ;
			if (srcfile == NULL)
				error (214, "File or path not found") ;
			b = 0 ;
			while (1)
			    {
				unsigned char al ;
				stdin_handler (NULL, NULL) ;
				if (flags & (ESCFLG | KILL))
				    {
					fclose (srcfile) ;
					trap () ;
				    }
				n = fread (&al, 1, 1, srcfile) ;
				if (n && al)
				    {
					fread (path2, 1, al - 1, srcfile) ;
					listline ((signed char *)path2, &b) ;
					crlf () ;
				    }
				else
					break ;
			    } ;
			fclose (srcfile) ;
			return ;

		case LOAD:		// *LOAD filename hexaddr [+maxlen]
			p = setup (path, p, ".bbc", ' ', NULL) ;
			n = 0 ;
			q = 0 ;
			if (*p != 0x0D)
			    {
				q = (char *) (size_t) strtoll (p, &p, 16) ;
				while (*p == ' ') p++ ;
				if (*p == '+')
					n = strtol (p + 1, &p, 16) ;
				else
					n = (char *) (size_t) strtoll (p, &p, 16) - q ;
			    }
			if ((n <= 0) && ((q < (char *)userRAM) || (q >= (char *)userTOP)))
				error (8, NULL) ; // 'Address out of range'
			if (n <= 0)
				n = (char *)userTOP - q ;
			srcfile = fopen (path, "rb") ;
			if (srcfile == NULL)
				error (214, "File or path not found") ;
			if (0 == fread(q, 1, n, srcfile))
				error (189, "Couldn't read file") ;
			fclose (srcfile ) ;
			return ;

		case LOCK:
			setup (path, p, ".bbc", ' ', NULL) ;
			if (0 != chmod (path, _S_IREAD))
				error (254, "Bad command") ;
			return ;

		case LOWERCASE:
			if (onoff (p))
				liston |= BIT3 ;
			else
				liston &= ~BIT3 ;
			return ;

		case MD:
		case MKDIR:
			setup (path, p, "", ' ', NULL) ;
#ifdef __WIN32__
			if (0 != mkdir (path))
#else
			if (0 != mkdir (path, 0777))
#endif
				error (254, "Bad command") ;
			return ;

		case RD:
		case RMDIR:
			setup (path, p, "", ' ', NULL) ;
			if (0 != rmdir (path))
				error (254, "Bad command") ;
			return ;

		case REFRESH:
			return ;

		case REN:
		case RENAME:
			p = setup (path, p, ".bbc", ' ', NULL) ;
			setup (path2, p, ".bbc", ' ', NULL) ;
			dstfile = fopen (path2, "rb") ;
			if (dstfile != NULL)
			    {
				fclose (dstfile) ;
				error (196, "File exists") ;
			    }
			if (0 != rename (path, path2))
				error (196, "File exists") ;
			return ;

		case RUN:
			strncpy (path, p, MAX_PATH - 1) ;
			q = memchr (path, 0x0D, MAX_PATH) ;
			if (q != NULL) *q = 0 ;
			q = path + strlen (path) - 1 ;
			if (*q == ';')
				*q = '&' ;
			SystemIO (1) ;
			if (0 != system (path))
			    {
				SystemIO (0) ;
				error (254, "Bad command") ;
			    }
			SystemIO (0) ;
			return ;

		case SAVE:		// *SAVE filename hexaddr +hexlen
			p = setup (path, p, ".bbc", ' ', NULL) ;
			n = 0 ;
			q = 0 ;
			if (*p != 0x0D)
			    {
				q = (char *) (size_t) strtoll (p, &p, 16) ;
				while (*p == ' ') p++ ;
				if (*p == '+')
					n = strtol (p + 1, &p, 16) ;
				else
					n = (char *) (size_t) strtoll (p, &p, 16) - q ;
			    }
			if (n <= 0)
				error (254, "Bad command") ;
			dstfile = fopen (path, "wb") ;
			if (dstfile == NULL)
				error (189, "Couldn't create file") ;
			if (0 == fwrite(q, 1, n, dstfile))
				error (189, "Couldn't write file") ;
			fclose (dstfile ) ;
			return ;

		case SPOOL:
			if (spchan != NULL)
			    {
				fclose (spchan) ;
				spchan = NULL ;
			    }
			while (*p == ' ') p++ ;
			if (*p == 0x0D)
				return ;
			setup (path, p, ".bbc", ' ', NULL) ;
			spchan = fopen (path, "wb") ;
			if (spchan == NULL)
				error (189, "Couldn't create file") ;
			return ;

		case SPOOLON:
			if (spchan != NULL)
			    {
				fclose (spchan) ;
				spchan = NULL ;
			    }
			while (*p == ' ') p++ ;
			if (*p == 0x0D)
				return ;
			setup (path, p, ".bbc", ' ', NULL) ;
			spchan = fopen (path, "ab") ;
			if (spchan == NULL)
				error (189, "Couldn't open file") ;
			return ;

		case TIMER:
			n = 0 ;
			sscanf (p, "%i", &n) ;
			if (n == 0)
				return ;
			StopTimer (UserTimerID) ;
			UserTimerID = StartTimer (n) ; 
			return ;

		case TV:
			return ;		// ignored

		case TYPE:
			setup (path, p, ".bbc", ' ', NULL) ;
			srcfile = fopen (path, "rb") ;
			if (srcfile == NULL)
				error (214, "File or path not found") ;
			do
			    {
				char ch ;
				stdin_handler (NULL, NULL) ;
				if (flags & (ESCFLG | KILL))
				    {
					fclose (srcfile) ;
					crlf () ;
					trap () ;
				    }
				n = fread (&ch, 1, 1, srcfile) ;
				oswrch (ch) ;
			    }
			while (n) ;
			fclose (srcfile) ;
			crlf () ; // Zero COUNT
			return ;

		case UNLOCK:
			setup (path, p, ".bbc", ' ', NULL) ;
			if (0 != chmod (path, _S_IREAD | _S_IWRITE))
				error (254, "Bad command") ;
			return ;

		case INPUT:
			n = 0 ;
			sscanf (p, "%i", &n) ;
			optval = (optval & 0x0F) | (n << 4) ;
			return ;

		case OUTPUT:
			n = 0 ;
			sscanf (p, "%i", &n) ;
			optval = (optval & 0xF0) | (n & 0x0F) ;
			return ;

		case DUMP:
			p = setup (path, p, ".bbc", ' ', NULL) ;
			srcfile = fopen (path, "rb") ;
			if (srcfile == NULL)
				error (214, "File or path not found") ;
			b = 0 ;
			h = 0 ;
			if (*p != 0x0D)
			    {
				long long s = strtoll (p, &p, 16) ;
				if ((s != 0) && (-1 == myfseek (srcfile, s, SEEK_SET)))
					error (189, "Couldn't seek") ;
				while (*p == ' ') p++ ;
				if (*p == '+')
					h = strtol (p + 1, &p, 16) ;
				else
					h = strtoll (p, &p, 16) - s ;
				b = s & 0xFFFFFFFF ;
			    }
			do
			    {
				int i ;
				unsigned char buff[16] ;
				stdin_handler (NULL, NULL) ;
				if (flags & (ESCFLG | KILL))
				    {
					fclose (srcfile) ;
					trap () ;
				    }
				n = fread (buff, 1, 16 - (b & 15), srcfile) ;
				if (n <= 0) break ;
				if ((h > 0) && (n > h)) n = h ; 
				memset (path, ' ', 80) ;
				sprintf (path, "%08X  ", b) ;
				for (i = 0; i < n; i++)
				    {
					sprintf (path + 10 + 3 * i, "%02X ", buff[i]) ;
					if ((buff[i] >= ' ') && (buff[i] <= '~'))
						path[59+i] = buff[i] ;
					else
						path[59+i] = '.' ;
				    }
					path[10 + 3 * n] = ' ' ; path[75] = 0 ;
				text (path) ;
				crlf () ;
				b += n ;
				h -= n ;
			    }
			while (h) ;
			fclose (srcfile) ;
			return ;
		} ;

	error (254, "Bad command") ;
}
