/******************************************************************\
*       BBC BASIC Minimal Console Version                          *
*       Copyright (c) R. T. Russell, 2000-2021                     *
*                                                                  *
*       bbccon.h constant definitions                              *
*       Version v0.36, 21-Aug-2021                                 *
\******************************************************************/

// System constants :

#define YEAR    "2021"          // Copyright year
#define VERSION "v0.36c"         // Version string
#ifdef PICO
#define ACCSLEN 1024            // Must be the same in BBC.h
#define DEFAULT_RAM PAGE_OFFSET+0x20000  // Initial amount of RAM to allocate
#else
#define	ACCSLEN 65536		// Must be the same in BBC.h
#define DEFAULT_RAM PAGE_OFFSET+0x200000 // Initial amount of RAM to allocate
#endif
#define PAGE_OFFSET ACCSLEN + 0x1300     // Offset of PAGE from memory base
#define MINIMUM_RAM PAGE_OFFSET+0x20000  // Minimum amount of RAM to allocate
#define MAXIMUM_RAM 0x10000000  // Maximum amount of RAM to allocate

#if (PAGE_OFFSET < 0x10000) && (defined(__x86_64__) || defined(__aarch64__))
#error "PAGE must be at least 64K above memory base on 64-bit platforms"
#endif

#define MAX_PORTS	4	// Maximum number of port channels
#define MAX_FILES	8	// Maximum number of file channels
#define MAX_LINE_LEN   2304     // At least 252*RECTANGLE + 4
#define	AUDIOLEN	441 * 4	// Length of audio block in bytes
#define	SOUNDQE         4       // Number of bytes per sound entry
#define SOUNDQL         5*SOUNDQE // Number of bytes per channel
#define MAX_TEMPO	10	// Maximum (slowest) *TEMPO setting

// Bit names:

#define	BIT0		0x01
#define	BIT1		0x02
#define	BIT2		0x04
#define	BIT3		0x08
#define	BIT4		0x10
#define	BIT5		0x20
#define	BIT6		0x40
#define	BIT7		0x80

// Bits in _flags byte (must be the same as in BBCEQUS.INC):

#define	IOFLAG		BIT0	// Insert/overtype
#define	EGAFLG		BIT1	// EGA-compatible modes (*EGA [ON])
#define	CGAFLG		BIT2	// CGA-compatible modes (*EGA OFF)
#define	PTFLAG		BIT3	// VDU 2 active
#define	HRGFLG		BIT4	// VDU 5 active
#define	VDUDIS		BIT5	// VDU 21 active
#define	UFONT		BIT6	// User font selected
#define	UTF8		BIT7	// UTF-8 mode selected

// Bits in _flags byte:

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

// Structures:
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

extern char colmsk ; 	// Mask for maximum number of colours
extern unsigned char vflags ;	// VDU drivers flags byte
extern signed char scroln ; 	// Scroll counter in paged mode
extern unsigned char cmcflg ;
extern char sclflg ;
extern char reflag, sysflg ;
extern unsigned char bbcfont[] ;
extern unsigned short ttxtfont[] ;

extern char **keystr ;	// Pointers to user *KEY strings
extern unsigned char flags ;	// BASIC's Boolean flags byte
extern unsigned char tempo ;
extern heapptr timtrp, clotrp, siztrp, systrp, moutrp ;
extern heapptr curlin ;		// Pointer to current line
extern heapptr breakpt ;	// Pointer to breakpoint start
extern heapptr breakhi ;	// Pointer to breakpoint end

//
extern int datend[] ;		// End of initialised variables
extern int stavar[] ;		// Static integer variables
extern FILE *exchan ;		// EXEC channel
extern FILE *spchan ;		// SPOOL channel
extern char *accs ;		// String accumulator
extern char *buff ;		// Temporary string buffer
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
extern unsigned int lastick ;	// To test for TIME wrapping
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
extern unsigned int vcount ;    // Character count since newline

// Declared in bbccon.c:
extern void *userRAM ;		// Base of user memory
extern void *progRAM ;		// Default LOMEM
extern void *userTOP ;		// Default HIMEM
extern const char szVersion[] ;	// Initial announcement
extern const char szNotice[] ;	// Copyright string
extern int bChanged ;		// Display refresh required
extern unsigned int platform ;	// OS platform
extern unsigned int palette[256] ;
