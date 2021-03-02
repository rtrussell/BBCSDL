/*****************************************************************\
*       32-bit or 64-bit BBC BASIC for SDL 2.0                    *
*       (C) 2017-2021  R.T.Russell  http://www.rtrussell.co.uk/   *
*                                                                 *
*       The name 'BBC BASIC' is the property of the British       *
*       Broadcasting Corporation and used with their permission   *
*                                                                 *
*       bbccli.c: Command Line Interface (OS emulation)           *
*       This module runs in the context of the interpreter thread *
*       Version 1.20a, 02-Mar-2021                                *
\*****************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "SDL2/SDL.h"
#include "SDL_ttf.h"
#include "bbcsdl.h"
#include "SDL_stbimage.h"

#undef MAX_PATH
#define NCMDS 51	// number of OSCLI commands
#define POWR2 32	// largest power-of-2 less than NCMDS
#define COPYBUFLEN 4096	// length of buffer used for *COPY command
#define _S_IWRITE 0x0080
#define _S_IREAD 0x0100
#define MAX_PATH 260

// External routines:
void trap (void) ;
void error (int, const char *) ;
void oswrch (unsigned char) ;
void pushev (int code, void *data1, void *data2) ;
int waitev (void) ;
Uint32 UserTimerCallback(Uint32, void *) ;
void crlf (void) ;
void outchr (unsigned char) ;
void text (const char*) ;
void listline (signed char*, int*) ;
void quiet (void) ;
void getcsr (int*, int*) ;

static char *cmds[NCMDS] = {
		"bye", "cd", "chdir", "copy", "del", "delete", "dir", "display",
		"dump", "ega", "era", "erase", "esc", "exec", "float", "font", "fx",
		"gsave", "help", "hex", "input", "key", "list", "load", "lock", "lowercase",
		"md", "mdisplay", "mkdir", "noega", "osk", "output", "quit", "rd", "refresh",
		"ren", "rename", "rmdir", "run", "save", "screensave", "spool", "spoolon",
		"stereo", "sys", "tempo", "timer", "tv", "type", "unlock", "voice"} ;

enum {
		BYE, CD, CHDIR, COPY, DEL, DELETE, DIRCMD, DISPLAY,
		DUMP, EGA, ERA, ERASE, ESC, EXEC, FLOAT, FONT, FX,
		GSAVE, HELP, HEX, INPUT, KEY, LIST, LOAD, LOCK, LOWERCASE,
		MD, MDISPLAY, MKDIR, NOEGA, OSK, OUTPUT, QUIT, RD, REFRESH,
		REN, RENAME, RMDIR, RUN, SAVE, SCREENSAVE, SPOOL, SPOOLON,
		STEREO, SYS, TEMPO, TIMER, TV, TYPE, UNLOCK, VOICE} ;

static int BBC_RWclose (struct SDL_RWops* context)
{
	int ret = SDL_RWclose (context) ;
	pushev (EVT_FSSYNC, NULL, NULL) ;
	return ret ;
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

	while ((term != '"') && (*src == ' ')) src++ ;	// Skip leading spaces
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
	char path1[MAX_PATH], path2[MAX_PATH] ;
	SDL_RWops *srcfile, *dstfile ;
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
			setup (path1, p, "", ' ', &flag) ;
			if (flag == 0)
			    {
				getcwd (path1, MAX_PATH) ;
				text (path1) ;
				crlf () ;
				return ;
			    }
			if (chdir (path1))
				error (206, "Bad directory") ;
			return ;

		case COPY:			// *COPY oldfile newfile
			p = setup (path1, p, ".bbc", ' ', NULL) ;
			srcfile = SDL_RWFromFile (path1, "rb") ;
			if (srcfile == NULL)
				error (214, "File or path not found") ;
			setup (path1, p, ".bbc", ' ', NULL) ;
			dstfile = SDL_RWFromFile (path1, "wb") ;
			if (dstfile == NULL)
			    {
				SDL_RWclose (srcfile) ;
				error (189, SDL_GetError ()) ;	// SDL error
			    }
			p = malloc (COPYBUFLEN) ;
			do
			    {
				n = SDL_RWread (srcfile, p, 1, COPYBUFLEN) ;
				if (n == 0)
					break ;
			    }
			while (SDL_RWwrite (dstfile, p, 1, n)) ;
			free (p) ;
			SDL_RWclose (srcfile) ;
			BBC_RWclose (dstfile) ;
			if (n)
				error (189, SDL_GetError ()) ;
			return ;

		case DEL:			// *DEL filename
		case DELETE:			// *DELETE filename
		case ERA:			// *ERA filename
		case ERASE:			// *ERASE filename
			setup (path1, p, ".bbc", ' ', NULL) ;
			if (remove (path1))
				error (254, "Bad command") ;	// Bad command
			return ;

		case DIRCMD:
			setup (path1, p, ".bbc", ' ', &flag) ;
			if ((flag & BIT0) == 0)
				strcat (path1, "*.bbc") ;
			if (flag & BIT1)
			    {
				p = path1 + strlen (path1) ;
				q = path1 ;
				while ((*p != '/') && (*p != '\\')) p-- ;
				if ((p == path1) || (*(p - 1) == ':'))
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
#ifdef __WINDOWS__
				dd = '\\' ;
#else
				dd = '/' ;
#endif
				p = path1 ;
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

		case DISPLAY:		// *DISPLAY bmpfile [xpos,ypos[,width,height[,keycol]]]
			    {
				int col = 0 ;
				SDL_Rect rect = {0, 0, 0, 0} ;
				SDL_Surface *bmp ;

				p = setup (path1, p, ".bmp", ' ', NULL) ;

				p += cpy - cmd ;
				sscanf (p, "%i, %i, %i, %i, %x",
					&rect.x, &rect.y, &rect.w, &rect.h, &col) ;
				if ((rect.h == 0) && (col == 0))
				    {
					rect.w = 0 ;
					sscanf (p, "%i, %i, %x", &rect.x, &rect.y, &col) ;
				    }
				rect.w /= 2 ;
				rect.h /= 2 ;

				srcfile = SDL_RWFromFile (path1, "rb") ;
				if (srcfile == 0)
					error (214, "File or path not found") ;
				bmp = SDL_LoadBMP_RW (srcfile, 0) ;
				if (bmp)
					SDL_RWclose (srcfile) ;
				else
					bmp = STBIMG_Load_RW (srcfile, 1) ;
				if (bmp == NULL)
					error (189, SDL_GetError ()) ;
				if (col)
					SDL_SetColorKey (bmp, 1, col) ;

				pushev (EVT_DISPLAY, &rect, bmp) ;
				if (0 == waitev ())
					error (189, SDL_GetError ()) ;
				return ;
			    }

		case EGA:
			vflags |= CGAFLG ;
			if (onoff (p))
				vflags |= EGAFLG ;
			else
				vflags &= ~EGAFLG ;
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
				SDL_RWclose (exchan) ;
				exchan = NULL ;
			    }
			setup (path1, p, ".bbc", ' ', NULL) ;
			if (*path1 == 0)
				return ;
			exchan = SDL_RWFromFile (path1, "rb") ;
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

		case FONT:			// *FONT [filename,[size[,BIQU]]]
			    {
				int size = 0, style = 0 ;
				unsigned char attr = 0 ;
				unsigned char flag ;
				p = setup (path1, p, ".ttf", ' ', &flag) ;
				if (*p == ',') p++ ;
				if (*p != 0x0D)
					size = (strtol (p, &p, 10) * 21845 + 8192) >> 14 ;
				if ((size == 0) && flag) size = abs (chary) ;
				while (*p == ' ') p++ ;
				if (*p == ',') p++ ;
				while (*p != 0x0D) attr |= *p++ ;
				if ((attr & BIT2) && (attr & BIT4))
					style |= TTF_STYLE_UNDERLINE ;
				if ((attr & BIT4) && !(attr & BIT2))
					style |= TTF_STYLE_STRIKETHROUGH ;
				if (attr & BIT3)
					style |= TTF_STYLE_ITALIC ;
				if (attr & BIT1)
					style |= TTF_STYLE_BOLD ;
				pushev (EVT_FONT, path1, (void *)(intptr_t)((style << 16) | size)) ;
				if ((waitev() == 0) && (size != 0))
					error (246, "No such font") ;
				return ;
			    }

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

		case GSAVE:			// *GSAVE bmpfile [xpos,ypos[,width,height]]
		case SCREENSAVE:
			    {
				int bfSize ;
				SDL_Rect rect = {0, 0, 0, 0} ;

				p = setup (path1, p, ".bmp", ' ', NULL) ;

				p += cpy - cmd ;
				sscanf (p, "%i, %i, %i, %i",
					&rect.x, &rect.y, &rect.w, &rect.h) ;
				rect.w /= 2 ;
				rect.h /= 2 ;

				if ((rect.w == 0) || (rect.h == 0))
				    {
					getcsr (NULL, NULL) ;
					rect.w = sizex ;
					rect.h = sizey ;
				    }

				bfSize = rect.h * ((rect.w * 3 + 3) & -4) + 14 + 40 ;
				p = malloc (bfSize) ;
				if (p == NULL)
					error (254, "Bad command") ;
				memset (p, 0, 54) ;
				* (short*) p = 0x4D42 ;		// bfType = 'BM'
				* (int*) (p + 2) = bfSize ;	// bfSize
				* (int*) (p + 10) = 54 ;	// bfOffBits
				* (int*) (p + 14) = 40 ;	// biSize
				* (int*) (p + 18) = rect.w ;	// biWidth
				* (int*) (p + 22) = rect.h ;	// biHeight (bottom-up)
				* (short*) (p + 26) = 1 ;	// biPlanes
				* (short*) (p + 28) = 24 ;	// biBitCount

				pushev (EVT_PIXELS, &rect, p + 54) ;
				waitev () ;

				dstfile = SDL_RWFromFile (path1, "wb") ;
				if (dstfile == NULL)
				    {
					free (p) ;
					error (189, SDL_GetError ()) ;
				    }
				n = SDL_RWwrite (dstfile, p, 1, bfSize) ;
				free (p) ;
				if (n != bfSize)
					error (189, SDL_GetError ()) ;
				BBC_RWclose (dstfile) ;
				return ;
			    }

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
			setup (path1, p, ".bbc", ' ', NULL) ;
			srcfile = SDL_RWFromFile (path1, "rb") ;
			if (srcfile == NULL)
				error (214, "File or path not found") ;
			b = 0 ;
			while (1)
			    {
				unsigned char al ;
				if (flags & (ESCFLG | KILL))
				    {
					SDL_RWclose (srcfile) ;
					trap () ;
				    }
				n = SDL_RWread (srcfile, &al, 1, 1) ;
				if (n && al)
				    {
					SDL_RWread (srcfile, path2, 1, al - 1) ;
					listline ((signed char *)path2, &b) ;
					crlf () ;
				    }
				else
					break ;
			    } ;
			SDL_RWclose (srcfile) ;
			return ;

		case LOAD:		// *LOAD filename hexaddr [+maxlen]
			p = setup (path1, p, ".bbc", ' ', NULL) ;
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
			srcfile = SDL_RWFromFile (path1, "rb") ;
			if (srcfile == NULL)
				error (214, "File or path not found") ;
			if (0 == SDL_RWread(srcfile, q, 1, n))
				error (189, SDL_GetError ()) ;
			SDL_RWclose (srcfile ) ;
			return ;

		case LOCK:
			setup (path1, p, ".bbc", ' ', NULL) ;
			if (0 != chmod (path1, _S_IREAD))
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
			setup (path1, p, "", ' ', NULL) ;
#ifdef __WINDOWS__
			if (0 != mkdir (path1))
#else
			if (0 != mkdir (path1, 0777))
#endif
				error (254, "Bad command") ;
			return ;

		case MDISPLAY:		// *MDISPLAY hexaddr [xpos,ypos[,width,height[,keycol]]]
			    {
				int col = 0 ;
				void *addr = NULL ;
				SDL_Rect rect = {0, 0, 0, 0} ;
				SDL_Surface *bmp ;

				p += cpy - cmd ;
				sscanf (p, "%p %i, %i, %i, %i, %x",
					&addr, &rect.x, &rect.y, &rect.w, &rect.h, &col) ;
				if ((rect.h == 0) && (col == 0))
				    {
					rect.w = 0 ;
					sscanf (p, "%x %i, %i, %x", (int*) &addr, &rect.x, &rect.y, &col) ;
				    }
				rect.w /= 2 ;
				rect.h /= 2 ;

				srcfile = SDL_RWFromMem (addr, *(int*)(addr + 2)) ;
				if (srcfile == 0)
					error (189, SDL_GetError ()) ;
				bmp = SDL_LoadBMP_RW (srcfile, 0) ;
				if (bmp)
					SDL_RWclose (srcfile) ;
				else
					bmp = STBIMG_Load_RW (srcfile, 1) ;
				if (bmp == NULL)
					error (189, SDL_GetError ()) ;
				if (col)
					SDL_SetColorKey (bmp, 1, col) ;

				pushev (EVT_DISPLAY, &rect, bmp) ;
				if (0 == waitev ())
					error (189, SDL_GetError ()) ;
				return ;
			    }

		case NOEGA:
			vflags &= ~(CGAFLG + EGAFLG) ;
			return ;

		case OSK:
			if (onoff (p))
				pushev (EVT_OSK, (void *) 1, NULL) ;
			else
				pushev (EVT_OSK, NULL, NULL) ;
			return ;

		case RD:
		case RMDIR:
			setup (path1, p, "", ' ', NULL) ;
			if (0 != rmdir (path1))
				error (254, "Bad command") ;
			return ;

		case REFRESH:
			while (*p == ' ') p++ ;
			if (*p != 0x0D)
			    {
				if (onoff (p))
					pushev (EVT_REFLAG, NULL, NULL) ;
				else
					pushev (EVT_REFLAG, (void *)2, NULL) ;
				waitev () ;
				return ;
			    }
			pushev (EVT_REFLAG, (void *)0x201, NULL) ;
			waitev () ;
			while (reflag & 1)
				SDL_Delay (1) ;
			return ;

		case REN:
		case RENAME:
			p = setup (path1, p, ".bbc", ' ', NULL) ;
			setup (path2, p, ".bbc", ' ', NULL) ;
			dstfile = SDL_RWFromFile (path2, "rb") ;
			if (dstfile != NULL)
			    {
				BBC_RWclose (dstfile) ;
				error (196, "File exists") ;
			    }
			if (0 != rename (path1, path2))
				error (196, "File exists") ;
			return ;

		case RUN:
			strncpy (path1, p, MAX_PATH - 1) ;
			q = memchr (path1, 0x0D, MAX_PATH) ;
			if (q != NULL) *q = 0 ;
			q = path1 + strlen (path1) - 1 ;
			if (*q == ';')
				*q = '&' ;
#ifdef __IPHONEOS__
			error (255, "Unsupported") ;
#else
			if (0 != system (path1))
				error (254, "Bad command") ;
#endif
			return ;

		case SAVE:		// *SAVE filename hexaddr +hexlen
			p = setup (path1, p, ".bbc", ' ', NULL) ;
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
			dstfile = SDL_RWFromFile (path1, "wb") ;
			if (dstfile == NULL)
				error (189, SDL_GetError ()) ;
			if (0 == SDL_RWwrite(dstfile, q, 1, n))
				error (189, SDL_GetError ()) ;
			BBC_RWclose (dstfile ) ;
			return ;

		case SPOOL:
			if (spchan != NULL)
			    {
				BBC_RWclose (spchan) ;
				spchan = NULL ;
			    }
			while (*p == ' ') p++ ;
			if (*p == 0x0D)
				return ;
			setup (path1, p, ".bbc", ' ', NULL) ;
			spchan = SDL_RWFromFile (path1, "wb") ;
			if (spchan == NULL)
				error (189, SDL_GetError ()) ;
			return ;

		case SPOOLON:
			if (spchan != NULL)
			    {
				BBC_RWclose (spchan) ;
				spchan = NULL ;
			    }
			while (*p == ' ') p++ ;
			if (*p == 0x0D)
				return ;
			setup (path1, p, ".bbc", ' ', NULL) ;
			spchan = SDL_RWFromFile (path1, "ab") ;
			if (spchan == NULL)
				error (189, SDL_GetError ()) ;
			return ;

		case STEREO:
			b = 0 ;
			n = 0 ;
			sscanf (p, "%i,%i", &b, &n) ;
			b &= 3 ;
			smix[b]     = 0x4000 - (n << 7) ;
			smix[b + 4] = 0x4000 + (n << 7) ;
			return ;

		case SYS:
			n = 0 ;
			sscanf (p, "%i", &n) ;
			sysflg = n ;
			return ;

		case TEMPO:
			n = 0 ;
			sscanf (p, "%i", &n) ;
			if (((n & 0x3F) <= MAX_TEMPO) && ((n & 0x3F) > 0))
				tempo = n ;
			return ;

		case TIMER:
			n = 0 ;
			sscanf (p, "%i", &n) ;
			if (n == 0)
				return ;
			pushev (EVT_TIMER, (void *)(intptr_t) n, NULL) ;
			return ;

		case TV:
			return ;		// ignored

		case TYPE:
			setup (path1, p, ".bbc", ' ', NULL) ;
			srcfile = SDL_RWFromFile (path1, "rb") ;
			if (srcfile == NULL)
				error (214, "File or path not found") ;
			do
			    {
				char ch ;
				if (flags & (ESCFLG | KILL))
				    {
					SDL_RWclose (srcfile) ;
					crlf () ;
					trap () ;
				    }
				n = SDL_RWread (srcfile, &ch, 1, 1) ;
				oswrch (ch) ;
			    }
			while (n) ;
			SDL_RWclose (srcfile) ;
			crlf () ; // Zero COUNT
			return ;

		case UNLOCK:
			setup (path1, p, ".bbc", ' ', NULL) ;
			if (0 != chmod (path1, _S_IREAD | _S_IWRITE))
				error (254, "Bad command") ;
			return ;

		case VOICE:
			b = 0 ;
			n = 0 ;
			sscanf (p, "%i,%i", &b, &n) ;
			voices[b & 3] = n & 7 ;
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
			p = setup (path1, p, ".bbc", ' ', NULL) ;
			srcfile = SDL_RWFromFile (path1, "rb") ;
			if (srcfile == NULL)
				error (214, "File or path not found") ;
			b = 0 ;
			h = 0 ;
			if (*p != 0x0D)
			    {
				long long s = strtoll (p, &p, 16) ;
				if ((s != 0) && (-1 == SDL_RWseek (srcfile, s, RW_SEEK_SET)))
					error (189, SDL_GetError ()) ;
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
				unsigned char dbuff[16] ;
				if (flags & (ESCFLG | KILL))
				    {
					SDL_RWclose (srcfile) ;
					trap () ;
				    }
				n = SDL_RWread (srcfile, dbuff, 1, 16 - (b & 15)) ;
				if (n <= 0) break ;
				if ((h > 0) && (n > h)) n = h ; 
				memset (path1, ' ', 80) ;
				sprintf (path1, "%08X  ", b) ;
				for (i = 0; i < n; i++)
				    {
					sprintf (path1 + 10 + 3 * i, "%02X ", dbuff[i]) ;
					if ((dbuff[i] >= ' ') && (dbuff[i] <= '~'))
						path1[59+i] = dbuff[i] ;
					else
						path1[59+i] = '.' ;
				    }
					path1[10 + 3 * n] = ' ' ; path1[75] = 0 ;
				text (path1) ;
				crlf () ;
				b += n ;
				h -= n ;
			    }
			while (h) ;
			SDL_RWclose (srcfile) ;
			return ;
	} ;

	error (254, "Bad command") ;
}

