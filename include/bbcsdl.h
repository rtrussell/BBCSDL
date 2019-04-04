/******************************************************************\
*       BBC BASIC for SDL 2.0 (64-bit)                             *
*       Copyright (c) R. T. Russell, 2000-2019                     *
*                                                                  *
*       BBCSDL.H constant definitions                              *
*       Version 1.02a 26-Mar-2019                                  *
\******************************************************************/

// System constants :

#define PAGE_OFFSET    0x31C00  // Must be the same in BBCEQUS.INC
#define PALETTE_SIZE     16     // Must be the same in BBCEQUS.INC
#define XSCREEN        2048     // Must be the same in BBCEQUS.INC
#define YSCREEN        2048     // Must be the same in BBCEQUS.INC
#define MAX_PORTS	4	// Maximum number of port channels
#define MAX_FILES	8	// Maximum number of file channels
#define MAX_LINE_LEN   2304     // At least 252*RECTANGLE + 4
#define MARGINL        1000     // Default left margin (mm * 100)
#define MARGINR        1000     // Default right margin (mm * 100)
#define MARGINT        1000     // Default top margin (mm * 100)
#define MARGINB        1000     // Default bottom margin (mm * 100)
#define	SCREEN_WIDTH	640	// Initial width
#define	SCREEN_HEIGHT	500	// Initial height
#define	AUDIOLEN	441 * 4	// Length of audio block in bytes
#define	SOUNDQE         4       // Number of bytes per sound entry
#define SOUNDQL         5*SOUNDQE // Number of bytes per channel
#define MAX_EVENTS	512	// Maximum SDL events to queue
#define MAX_TEMPO	10	// Maximum (slowest) *TEMPO setting

// User-defined message IDs:

#define	WM_APP		0x8000
#define WMU_BYE         WM_APP+3  // must be the same in BBCEQUS.INC
#define WMU_REALLOC     WM_APP+4  // must be the same in BBCEQUS.INC
#define WMU_WAVEOPEN    WM_APP+5  // must be the same in BBCEQUS.INC
#define WMU_WAVECLOSE   WM_APP+6  // must be the same in BBCEQUS.INC
#define WMU_PLAYMIDI    WM_APP+7  // must be the same in BBCEQUS.INC
#define WMU_TIMER       WM_APP+26

// Custom user-event IDs:

#define EVT_VDU		0x2000	// Send a VDU command
#define EVT_COPYKEY	0x2001	// Handle 'copy key' actions
#define EVT_TINT	0x2002	// Get RGB pixel value
#define EVT_DISPLAY	0x2003	// *DISPLAY command
#define EVT_PIXELS	0x2004	// Read back pixels
#define EVT_CARET	0x2005	// Get text caret coordinates
#define EVT_FONT	0x2006	// Open a font
#define EVT_CHAR	0x2007	// Get character at text x,y
#define EVT_WIDTH	0x2008	// Get width of a string
#define EVT_REFLAG	0x2009	// Update refresh flag
#define EVT_SYSCALL	0x200A	// Call in GUI thread context
#define EVT_QUIT	0x200B	// Terminate with exit code
#define EVT_MOUSE	0x200C	// Get mouse position/buttons
#define EVT_MOUSETO	0x200D	// Move mouse pointer
#define EVT_OSK		0x200E	// En/disable On Screen Keyboard

// Bit names:

#define	BIT0		0x01
#define	BIT1		0x02
#define	BIT2		0x04
#define	BIT3		0x08
#define	BIT4		0x10
#define	BIT5		0x20
#define	BIT6		0x40
#define	BIT7		0x80

// Bits in [vflags]:

#define	IOFLAG		BIT0	// Insert/overtype
#define	EGAFLG		BIT1	// EGA-compatible modes (*EGA [ON])
#define	CGAFLG		BIT2	// CGA-compatible modes (*EGA OFF)
#define	PTFLAG		BIT3	// VDU 2 active
#define	HRGFLG		BIT4	// VDU 5 active
#define	VDUDIS		BIT5	// VDU 21 active
#define	UFONT		BIT6	// User font selected
#define	UTF8		BIT7	// UTF-8 mode selected

// Bits in _flags byte (must be the same as in BBCEQUS.INC):

#define ESCFLG          0x80
#define ESCDIS          0x40
#define ALERT           0x20
#define FLASH           0x10
#define PHASE           0x08    
#define PAUSE           0x04
#define SSTEP           0x02
#define KILL            0x01

// BASIC tokens:

#define TOK_ELSE       -117
#define TOK_THEN       -116
#define TOK_LINENO     -115
#define TOK_FN          -92
#define TOK_WHILE       -57
#define TOK_CASE        -56
#define TOK_WHEN        -55
#define TOK_ENDCASE     -53
#define TOK_OTHERWISE   -52
#define TOK_ENDIF       -51
#define TOK_ENDWHILE    -50
#define TOK_CALL        -42
#define TOK_DATA        -36
#define TOK_DEF         -35
#define TOK_FOR         -29
#define TOK_GOSUB       -28
#define TOK_GOTO        -27
#define TOK_IF          -25
#define TOK_LOCAL       -22
#define TOK_NEXT        -19
#define TOK_ON          -18
#define TOK_PROC        -14
#define TOK_READ        -13
#define TOK_REM         -12
#define TOK_REPEAT      -11
#define TOK_RUN          -7
#define TOK_UNTIL        -3
#define TOK_EXIT         16

#define TOKLO          -113   // first token with left and right forms
#define TOKHI          -109   // last token with left and right forms
#define OFFSIT           64   // offset from 'right' to 'left' form

// Special 32-bit 'pointer' type for BASIC's heap:
typedef unsigned int heapptr ;

// Structures and unions:
typedef struct tagPARM
{
	size_t i[16] ;
	double f[8] ;
} PARM, *LPPARM ;

typedef struct tagFCB
{
	unsigned char p ; // pointer
	unsigned char o ; // offset  (0-256)
	unsigned char w ; // written (0-256)
	signed char f ;   // bit0: offset<>0, bit7: written<>0
} FCB, *LPFCB ;

// Variables declared in bbcsdl.c:
extern SDL_Renderer *memhdc ;
extern SDL_Window *hwndProg ;
extern SDL_Rect ClipRect ;
extern int bChanged ;
extern unsigned int platform ;	// SDL version and OS platform
extern unsigned int palette[256] ;
extern SDL_Texture *TTFcache[65536] ;
extern short *chrmap ;
extern unsigned int zoom ;

// VDU variables declared in bbcdata.nas or bbcdat.s:
extern int origx ; 	// Graphics x-origin (BASIC units)
extern int origy ; 	// Graphics y-origin (BASIC units)
extern int lastx ; 	// Current x-coordinate (pixels)
extern int lasty ; 	// Current y-coordinate (pixels)
extern int prevx ; 	// Previous x-coordinate (pixels)
extern int prevy ; 	// Previous y-coordinate (pixels)
extern int textwl ; 	// Text window left (pixels)
extern int textwr ; 	// Text window right (pixels)
extern int textwt ; 	// Text window top (pixels)
extern int textwb ; 	// Text window bottom (pixels)
extern int pixelx ; 	// Width of a graphics 'dot'
extern int pixely ; 	// Height of a graphics 'dot'
extern int textx ; 	// Text caret x-position (pixels)
extern int texty ; 	// Text caret y-position (pixels)
extern TTF_Font *hfont ;// Handle of current font
extern SDL_Rect *hrect ;// Pointer to clipping rect

extern short forgnd ; 	// Graphics foreground colour/action
extern short bakgnd ;	// Graphics background colour/action

extern unsigned char cursa ; 	// Start (top) line of caret
extern unsigned char cursb ; 	// Finish (bottom) line of caret
extern char txtfor ; 	// Text foreground colour index
extern char txtbak ; 	// Text background colour index
extern signed char modeno ; 	// Mode number (can be -1)
extern char colmsk ; 	// Mask for maximum number of colours
extern unsigned char vflags ;	// VDU drivers flags byte
extern signed char scroln ; 	// Scroll counter in paged mode

extern int sizex ; 	// Total width of client area (pixels)
extern int sizey ; 	// Total height of client area (pixels)
extern int charx ; 	// Average character width (pixels)
extern int chary ; 	// Average character height (pixels)
extern int prchx ; 	// Average character width (printer)
extern int prchy ; 	// Average character height (printer)
extern int prntx ; 	// Horizontal printing position
extern int prnty ; 	// Vertical printing position
extern unsigned char lthick ; 	// Line thickness
extern signed char tweak ; // Character spacing adjustment
extern short cursx ; 	// Cursor (caret) width

// Other variables declared in bbcdata.nas or bbcdat.s:
extern char cmcflg ;
extern char sclflg ;
extern unsigned char bbcfont[] ;
extern unsigned short ttxtfont[] ;
extern char **keystr ;	// Pointers to user *KEY strings
extern unsigned char flags ;	// BASIC's Boolean flags byte
extern SDL_AudioDeviceID hwo ;
extern char reflag, sysflg ;
extern unsigned char tempo ;
extern int panx, pany ;
extern int offsetx, offsety ;
extern heapptr timtrp, clotrp, siztrp, systrp, moutrp ;
extern heapptr curlin ;		// Pointer to current line
extern heapptr breakpt ;	// Pointer to breakpoint start
extern heapptr breakhi ;	// Pointer to breakpoint end

//
extern int datend[] ;		// End of initialised variables
extern int stavar[] ;		// Static integer variables
extern SDL_RWops *exchan ;	// EXEC channel
extern SDL_RWops *spchan ;	// SPOOL channel
extern char* accs ;		// String accumulator
extern char* buffer ;		// Temporary string buffer
extern char* path ;		// File path buffer
extern signed char *envels ;	// Envelope storage (16 x 16)
extern short* waves ;
extern void* filbuf[] ;
extern FCB fcbtab[MAX_FILES] ;  // Table of FCBs
extern unsigned char *keyptr ;	// Pointer to *KEY string
extern char* usrchr ;		// User-defined characters (indirect)
extern char* keybdq ;		// Keyboard queue (indirect) 
extern int* eventq ;		// Event queue (indirect)
extern unsigned char vduq[] ;	// VDU queue (different from asm version)
extern unsigned char queue ;	// VDU queue status
extern unsigned char kbdqr ;	// Keyboard queue read pointer
extern unsigned char kbdqw ;	// Keyboard queue write pointer
extern unsigned char sndqr[4] ;	// Sound queue read pointers
extern unsigned char sndqw[4] ;	// Sound queue write pointers
extern signed char eenvel[4] ;	// Sound envelope numbers
extern unsigned char escale[4] ;// Envelope scalers
extern unsigned char epsect[4] ;// Envelope pitch section
extern unsigned char easect[4] ;// Envelope amplitude section
extern unsigned char epitch[4] ;// Envelope pitch (frequency)
extern unsigned char elevel[4] ;// Envelope level (amplitude)
extern unsigned char ecount[4] ;// Envelope count
extern unsigned char soundq[4*5*SOUNDQE] ;
extern unsigned int sacc[4] ;	// Sound DDS accumulators
extern unsigned char voices[4] ;// Voice (waveform) for each channel
extern short smix[8] ;		// Stereo mix for each channel
extern unsigned char evtqr ;	// Event queue read pointer
extern unsigned char evtqw ;	// Event queue write pointer
extern unsigned char flags ;	// Interpreter flags byte
extern int timoff ;		// TIME offset
extern int lastick ;		// To test for TIME wrapping
extern unsigned int prand ;	// Pseudo-random number
extern int iMsg ;		// Event message number
extern int wParam ;		// Event wParam value
extern int lParam ;		// Event lParam value
extern unsigned char tempo ;	// SOUND tempo
extern unsigned char farray ;	// @hfile%() number of dimensions
extern unsigned int fasize ;	// @hfile%() number of elements
extern unsigned char keyexp ;	// *KEY expansion counter
extern unsigned char optval ;	// I/O redirection
extern unsigned char liston ;	// *FLOAT/*HEX/*LOWERCASE/OPT
extern unsigned int count ;     // Character count since newline

// Declared in bbcsdl.c:
extern size_t iResult ;		// Result from user event
extern int nUserEv ;		// Number of pending user events
extern int OSKtime ;		// On-screen keyboard timeout
extern SDL_sem *Sema4 ;		// Semaphore for user event wait
extern void *userRAM ;		// Base of user memory
extern void *progRAM ;		// Default LOMEM
extern void *userTOP ;		// Default HIMEM
extern const char szVersion[] ;	// Initial announcement
extern const char szNotice[] ;	// Copyright string
extern int bChanged ;		// Display refresh required
extern SDL_Joystick *Joystick ;	// Handle to joystick
extern SDL_TimerID UserTimerID ;
extern int bBackground ;	// BBC BASIC in the background
extern int useGPA ;		// Use SDL_GL_GetProcAddress
