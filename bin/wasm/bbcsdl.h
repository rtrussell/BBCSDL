/******************************************************************\
*       BBC BASIC for SDL 2.0 (Emscripten / Web Assembly)          *
*       Copyright (c) R. T. Russell, 2000-2020                     *
*                                                                  *
*       BBCSDL.H constant definitions                              *
*       Version 1.15w 30-Aug-2020                                  *
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
#define EVT_OSWORD	0x200F  // OSWORD call
#define EVT_TIMER	0x2010	// Set timer to new period
#define EVT_FSSYNC	0x2011  // Sync filesystem (Emscripten)

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

// Bits in flags byte:

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
extern SDL_Rect ClipRect ;
extern int bChanged ;
extern SDL_Texture *TTFcache[65536] ;

// Static variables:

extern int stavar[] ;		// Static integer variables
#define accs (*(char **)((char*)stavar +  332))		// String accumulator
#define buff (*(char **)((char*)stavar + 336))		// Temporary line buffer
#define vcount (*(unsigned int *)((char*)stavar + 380))	// Character count since newline
#define curlin (*(heapptr *)((char*)stavar + 384))
#define timtrp (*(heapptr *)((char*)stavar + 388))
#define clotrp (*(heapptr *)((char*)stavar + 392))
#define siztrp (*(heapptr *)((char*)stavar + 396))
#define systrp (*(heapptr *)((char*)stavar + 400))
#define moutrp (*(heapptr *)((char*)stavar + 404))
#define prand  (*(unsigned int*)((char*)stavar + 412))	// Pseudo-random number
#define liston (*(unsigned char *)((char*)stavar + 419))// *FLOAT/*HEX/*LOWERCASE/OPT
#define path   (*(char**)((char*)stavar + 420))		// File path buffer
#define keystr (*(char ***)((char*)stavar + 424))	// Pointers to user *KEY strings
#define keybdq (*(char **)((char*)stavar + 428))	// Keyboard queue (indirect) 
#define eventq (*(int **)((char*)stavar + 432))		// Event queue (indirect)
#define keyptr (*(unsigned char **)((char*)stavar + 436))// Pointer to *KEY string
#define usrchr (*(char **)((char*)stavar + 440))	// Pointer to user-defined chars
#define lstopt (*(char *)((char*)stavar + 444))
#define sclflg (*(char *)((char*)stavar + 445))
#define optval (*(unsigned char *)((char*)stavar + 446))// I/O redirection
#define farray (*(unsigned char *)((char*)stavar + 447))// @hfile%() number of dimensions
#define fasize (*(unsigned int *)((char*)stavar + 448))	// @hfile%() number of elements
#define filbuf ((void**)((char*)stavar + 452))		// @hfile%(0)
#define fcbtab ((FCB *)((char*) stavar + 504))		// fcbtab[]
#define spchan (*(SDL_RWops **)((char*)stavar + 536))	// SPOOL channel
#define exchan (*(SDL_RWops **)((char*)stavar + 540))	// EXEC channel
#define sacc   ((unsigned int *)((char*)stavar + 544))	// Sound DDS accumulators
#define smix   ((short *)((char*)stavar + 560))		// Stereo mix for each channel
#define datend ((char*)stavar + 576)			// End of initialised variables

// VDU variables allocated in bbdata_wasm32.c:

extern int vduvar[] ;		// VDU variables
#define origx  (*(int *)((char*)vduvar + 0))	 	// Graphics x-origin (BASIC units)
#define origy  (*(int *)((char*)vduvar + 4))	 	// Graphics y-origin (BASIC units)
#define lastx  (*(int *)((char*)vduvar + 8))	 	// Current x-coordinate (pixels)
#define lasty  (*(int *)((char*)vduvar + 12))	 	// Current y-coordinate (pixels)
#define prevx  (*(int *)((char*)vduvar + 16))	 	// Previous x-coordinate (pixels)
#define prevy  (*(int *)((char*)vduvar + 20))	 	// Previous y-coordinate (pixels)
#define textwl (*(int *)((char*)vduvar + 24))	 	// Text window left (pixels)
#define textwr (*(int *)((char*)vduvar + 28))	 	// Text window right (pixels)
#define textwt (*(int *)((char*)vduvar + 32))	 	// Text window top (pixels)
#define textwb (*(int *)((char*)vduvar + 36))	 	// Text window bottom (pixels)
#define pixelx (*(int *)((char*)vduvar + 40))	 	// Width of a graphics 'dot'
#define pixely (*(int *)((char*)vduvar + 44))	 	// Height of a graphics 'dot'
#define textx  (*(int *)((char*)vduvar + 48))	 	// Text caret x-position (pixels)
#define texty  (*(int *)((char*)vduvar + 52))	 	// Text caret y-position (pixels)
#define hfont  (*(TTF_Font **)((char*)vduvar + 56))	// Handle of current font
#define hrect  (*(SDL_Rect **)((char*)vduvar + 60))	// Pointer to clipping rect
#define forgnd (*(short *)((char*)vduvar + 64))	 	// Graphics foreground colour/action
#define bakgnd (*(short *)((char*)vduvar + 66))		// Graphics background colour/action
#define cursa  (*(unsigned char *)((char*)vduvar + 68))	// Start (top) line of caret
#define cursb  (*(unsigned char *)((char*)vduvar + 69))	// Finish (bottom) line of caret
#define txtfor (*(char *)((char*)vduvar + 70))	 	// Text foreground colour index
#define txtbak (*(char *)((char*)vduvar + 71))	 	// Text background colour index
#define modeno (*(signed char *)((char*)vduvar + 72))	// MODE number (can be -1)
#define colmsk (*(char *)((char*)vduvar + 73))	 	// Mask for maximum number of colours
#define vflags (*(unsigned char *)((char*)vduvar + 74))	// VDU drivers flags byte
#define scroln (*(signed char*)((char*)vduvar + 75)) 	// Scroll counter in paged mode
#define cursx  (*(unsigned char *)((char*)vduvar + 76))	// Cursor (caret) width
#define lthick (*(unsigned char *)((char*)vduvar + 77))	// Line thickness
#define cmcflg (*(char *)((char*)vduvar + 78))		// Cursor movement control
#define tweak  (*(signed char *)((char*)vduvar + 79))	// Character spacing adjustment

#define sndqw ((unsigned char *)((char*)vduvar + 80))	// Sound queue write pointers
#define sndqr ((unsigned char *)((char*)vduvar + 84))	// Sound queue read pointers
#define eenvel ((signed char *)((char*)vduvar + 88))	// Sound envelope numbers
#define escale ((unsigned char *)((char*)vduvar + 92)) 	// Envelope scalers
#define epsect ((unsigned char *)((char*)vduvar + 96)) 	// Envelope pitch section
#define easect ((unsigned char *)((char*)vduvar + 100))	// Envelope amplitude section
#define epitch ((unsigned char *)((char*)vduvar + 104))	// Envelope pitch (frequency)
#define ecount ((unsigned char *)((char*)vduvar + 108))	// Envelope count
#define soundq ((unsigned char *)((char*)vduvar + 112))	// 4*SOUNDQL = 80
#define vduq   ((unsigned char *)((char*)vduvar + 192))	// VDU queue (different from asm version)
#define queue  (*(unsigned char *)((char*)vduvar + 202))// VDU queue status
#define kbdqw  (*(unsigned char *)((char*)vduvar + 203))// Keyboard queue write pointer
#define kbdqr  (*(unsigned char *)((char*)vduvar + 204))// Keyboard queue read pointer
#define evtqw  (*(unsigned char *)((char*)vduvar + 205))// Event queue write pointer
#define evtqr  (*(unsigned char *)((char*)vduvar + 206))// Event queue read pointer
#define keyexp (*(unsigned char *)((char*)vduvar + 207))// *KEY expansion counter
#define sizex  (*(int *)((char*)vduvar + 208))	 	// Total width of client area (pixels)
#define sizey  (*(int *)((char*)vduvar + 212))	 	// Total height of client area (pixels)
#define charx  (*(int *)((char*)vduvar + 216))	 	// Average character width (pixels)
#define chary  (*(int *)((char*)vduvar + 220))	 	// Average character height (pixels)
#define prchx  (*(int *)((char*)vduvar + 224))	 	// Average character width (printer)
#define prchy  (*(int *)((char*)vduvar + 228))	 	// Average character height (printer)
#define timoff (*(int *)((char*)vduvar + 232))		// Offset to add to TickCount
#define envels (*(signed char **)((char*)vduvar + 236))	// Pointer to ENVELOPEs (16 x 16)
#define waves  (*(short **)((char*)vduvar + 240))	// Pointer to SOUND waveforms
#define elevel ((unsigned char*)((char*)vduvar+244))	// Envelope level (amplitude)
#define prntx  (*(int *)((char*)vduvar + 248))		// Horizontal printing position
#define prnty  (*(int *)((char*)vduvar + 252))		// Vertical printing position

extern unsigned char bbcfont[] ;
extern unsigned short ttxtfont[] ;
extern int lastick ;		// To test for TIME wrapping

// System variables:

extern int sysvar[] ;		// @ variables linked list
#define memhdc (*(SDL_Renderer **)((char*)sysvar + 12))	// @memhdc%
#define wParam (*(int *)((char*)sysvar + 44))		// @wparam%
#define lParam (*(int *)((char*)sysvar + 60))		// @lparam%
#define hwndProg (*(SDL_Window **)((char*)sysvar + 92))	// @hwnd%
#define offsetx (*(int *)((char*)sysvar + 104))		// @ox%
#define offsety (*(int *)((char*)sysvar + 116))		// @oy%
#define iMsg   (*(int *)((char*)sysvar + 148))		// @msg%
#define tempo  (*(unsigned char *)((char*)sysvar + 196))// @flags%
#define sysflg (*(char *)((char*)sysvar + 197))		// *SYS flag
#define reflag (*(char *)((char*)sysvar + 198))		// *REFRESH flag
#define flags  (*(unsigned char *)((char*)sysvar + 199))// BASIC's Boolean flags byte
#define voices ((unsigned char *)((char*)sysvar + 212)) // Voice (waveform) for each channel
#define zoom   (*(unsigned int *)((char*)sysvar + 228))	// @zoom%
#define hwo    (*(SDL_AudioDeviceID *)((char*)sysvar + 376)) // @hwo%
#define platform (*(unsigned int *)((char*)sysvar + 396)) // SDL version and OS platform
#define chrmap (*(short **)((char*)sysvar + 412))		// @chrmap%
#define panx   (*(int *)((char*)sysvar + 428))		// @panx%
#define pany   (*(int *)((char*)sysvar + 444))		// @pany%
#define breakpt (*(heapptr *)((char*)sysvar + 496))	// @brkpt%
#define breakhi (*(heapptr *)((char*)sysvar + 512))	// @brkhi%

// Declared in bbcsdl.c:
extern unsigned int palette[256] ;
extern size_t iResult ;		// Result from user event
extern int nUserEv ;		// Number of pending user events
extern int OSKtime ;		// On-screen keyboard timeout
extern SDL_sem *Sema4 ;		// Semaphore for user event wait
extern SDL_mutex *Mutex ;	// Mutex to protect event queue
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
