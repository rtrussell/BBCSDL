/******************************************************************\
*       BBC BASIC for SDL 2.0 (32-bits or 64-bits)                 *
*       Copyright (c) R. T. Russell, 2000-2021                     *
*                                                                  *
*       BBC.h constant, variable and structure declarations        *
*       Version 1.25b, 22-Aug-2021                                 *
\******************************************************************/

// Constants:
#define STACK_NEEDED 512
#define ACCSLEN 65536 // Must be the same in bbcsdl.h and bbccon.h

// Sentinels:
#define CALCHK	0xC3414C43
#define DIMCHK	0xC4494D43
#define FNCHK	0xC64E4348
#define FORCHK	0xC64F5243
#define GOSCHK	0xC74F5343
#define LDCHK	0xCC444348
#define LOCCHK	0xCC4F4343
#define ONCHK	0xCF4E4348
#define PROCHK	0xD0524F43
#define REPCHK	0xD2455043
#define RETCHK	0xD2455443
#define WHICHK	0xD7484943

// Tokens:
#define TAND	-128
#define TDIV	-127
#define TEOR	-126
#define TMOD	-125
#define TOR	-124
#define TERROR	-123
#define TLINE	-122
#define TOFF	-121
#define TSTEP	-120
#define TSPC	-119
#define TTAB	-118
#define TELSE	-117
#define TTHEN	-116
#define TLINO	-115
#define TOPENIN	-114
#define TPTRR	-113

#define TPAGER	-112
#define TTIMER	-111
#define TLOMEMR	-110
#define THIMEMR	-109
#define TABS	-108
#define TACS	-107
#define TADVAL	-106
#define TASC	-105
#define TASN	-104
#define TATN	-103
#define TBGET	-102
#define TCOS	-101
#define TCOUNT	-100
#define TDEG	-99
#define TERL	-98
#define TERR	-97

#define TEVAL	-96
#define TEXP	-95
#define TEXTR	-94
#define TFALSE	-93
#define TFN	-92
#define TGET	-91
#define TINKEY	-90
#define TINSTR	-89
#define TINT	-88
#define TLEN	-87
#define TLN	-86
#define TLOG	-85
#define TNOT	-84
#define TOPENUP	-83
#define TOPENOUT -82
#define TPI	-81

#define TPOINT	-80
#define TPOS	-79
#define TRAD	-78
#define TRND	-77
#define TSGN	-76
#define TSIN	-75
#define TSQR	-74
#define TTAN	-73
#define TTO	-72
#define TTRUE	-71
#define TUSR	-70
#define TVAL	-69
#define TVPOS	-68
#define TCHR	-67
#define TGETS	-66
#define TINKEYS	-65

#define TLEFT	-64
#define TMID	-63
#define TRIGHT	-62
#define TSTR	-61
#define TSTRING	-60
#define TEOF	-59
#define TSUM	-58
#define TWHILE	-57
#define TCASE	-56
#define TWHEN	-55
#define TOF	-54
#define TENDCASE -53
#define TOTHERWISE -52
#define TENDIF	-51
#define TENDWHILE -50
#define TPTRL	-49

#define TPAGEL	-48
#define TTIMEL	-47
#define TLOMEML	-46
#define THIMEML	-45
#define TSOUND	-44
#define TBPUT	-43
#define TCALL	-42
#define TCHAIN	-41
#define TCLEAR	-40
#define TCLOSE	-39
#define TCLG	-38
#define TCLS	-37
#define TDATA	-36
#define TDEF	-35
#define TDIM	-34
#define TDRAW	-33

#define TEND	-32
#define TENDPROC -31
#define TENVEL	-30
#define TFOR	-29
#define TGOSUB	-28
#define TGOTO	-27
#define TGCOL	-26
#define TIF	-25
#define TINPUT	-24
#define TLET	-23
#define TLOCAL	-22
#define TMODE	-21
#define TMOVE	-20
#define TNEXT	-19
#define TON	-18
#define TVDU	-17

#define TPLOT	-16
#define TPRINT	-15
#define TPROC	-14
#define TREAD	-13
#define TREM	-12
#define TREPEAT	-11
#define TREPORT	-10
#define TRESTOR	-9
#define TRETURN	-8
#define TRUN	-7
#define TSTOP	-6
#define TCOLOUR	-5
#define TTRACE	-4
#define TUNTIL	-3
#define TWIDTH	-2
#define TOSCLI	-1

#define TCIRCLE	1
#define TELLIPSE 2
#define TFILL	3
#define TMOUSE	4
#define TORIGIN	5
#define TQUIT	6
#define TRECT	7
#define TSWAP	8
#define TSYS	9
#define TTINT	10
#define TWAIT	11
#define TINSTALL 12
#define TPRIVATE 14
#define TBY	15
#define TEXIT	16

#define FUNTOK	TLINO // first function token
#define TOKLO	TPTRR
#define TOKHI	THIMEMR
#define OFFSIT	TPTRL-TPTRR

// Non-token statements:
// * star command
// = return from function
// ( label
// : separator
// [ assembler

// Bit names:
#define BIT0	0x01
#define BIT1	0x02
#define BIT2	0x04
#define BIT3	0x08
#define BIT4	0x10
#define BIT5	0x20
#define BIT6	0x40
#define BIT7	0x80

// Flag bits:
#define ESCFLG	BIT7	// ESCape key pressed 
#define ESCDIS	BIT6	// ESCape key disabled (*ESC OFF)
#define ALERT	BIT5	// Pending event interrupt
#define FLASH	BIT4	// MODE 7 flash update needed
#define PHASE	BIT3    // MODE 7 flash phase
#define PAUSE	BIT2	// In debug paused state
#define SSTEP	BIT1	// Single-step requested
#define KILL	BIT0	// Program wants to terminate

// Special 32-bit 'pointer' type for BASIC's heap:
#define STRIDE sizeof(void *) / sizeof(heapptr)
typedef unsigned int heapptr ;

// Structures and unions:
typedef struct tagPARM
{
	size_t i[16] ;
	double f[8] ;
} PARM, *LPPARM ;

// String descriptor:
typedef struct __attribute__ ((packed)) __attribute__ ((aligned (4))) tagSTR
{
	heapptr p ; // Assumed to be 32 bits
	int l ;
} STR, *LPSTR ;

// Structure for linked list of string free space
struct node
{
	struct node *next ;
	char *data ;
} ;
typedef struct node node ;

// Base address for 32-bit offsets into heap:
#if defined(__x86_64__) || defined(__aarch64__)
#define zero userRAM
#define TMASK 31
#define STYPE 16
#define ATYPE 40
#else
#define zero (void*) 0
#define TMASK 15
#define STYPE 24
#define ATYPE 4
#endif

// Register globals:
#ifdef __llvm__
extern signed char *esi ;		// Program pointer
extern heapptr *esp ;			// Stack pointer
#else
#ifdef __i386__
register signed char *esi asm ("esi") ;	// Program pointer
register heapptr *esp asm ("edi") ;	// Stack pointer
#endif
#ifdef __arm__
register signed char *esi asm ("r10") ;	// Program pointer
register heapptr *esp asm ("r11") ;	// Stack pointer
#endif
#ifdef __x86_64__
register signed char *esi asm ("r12") ;	// Program pointer
register heapptr *esp asm ("r13") ;	// Stack pointer
#endif
#ifdef __aarch64__
register signed char *esi asm ("x27") ;	// Program pointer
register heapptr *esp asm ("x28") ;	// Stack pointer
#endif
#endif

// Data locations (defined in bbdata_xxx_xx.nas):
extern unsigned char errnum ;	// Error code number
extern char *accs ;		// String accumulator
extern char *buff ;		// Temporary line buffer
extern heapptr *onersp ;	// ON ERROR LOCAL stack pointer
extern heapptr vpage ;		// Value of PAGE
extern heapptr errlin ;		// Pointer to error line
extern heapptr curlin ;		// Pointer to current line
extern heapptr errtrp ;		// Pointer to ON ERROR handler
extern heapptr timtrp ;		// Pointer to ON TIME handler
extern heapptr clotrp ;		// Pointer to ON CLOSE handler
extern heapptr siztrp ;		// Pointer to ON MOVE handler
extern heapptr systrp ;		// Pointer to ON SYS handler
extern heapptr moutrp ;		// Pointer to ON MOUSE handler
extern heapptr libase ;		// Base of libraries 
extern heapptr datptr ;		// DATA pointer
extern heapptr lomem ;		// Pointer to base of heap
extern heapptr pfree ;		// Pointer to free space
extern heapptr himem ;		// Pointer to top of stack
extern const char *errtxt ;	// Most recent error message
extern int stavar[] ;		// Static integer variables
extern heapptr dynvar[] ;	// Linked-list pointers
extern heapptr fnptr[] ;	// Pointer to user FuNctions
extern heapptr proptr[] ;	// Pointer to user PROCedures
extern node *flist[] ;		// String free-lists
extern STR tmps ;		// Temporary string descriptor
extern unsigned char liston ;	// *FLOAT/*HEX/*LOWERCASE/OPT
extern unsigned char lstopt ;	// LISTO value (indentation)
extern unsigned int vcount ;    // Character count since newline
extern unsigned char vwidth ;	// Width for auto-newline
extern int link00 ;		// Terminating link in @ list
extern heapptr vduptr ;		// @vdu{} structure pointer
extern heapptr vdufmt ;		// @vdu{} structure format
extern unsigned char evtqw ;	// Event queue write pointer
extern unsigned char evtqr ;	// Event queue read pointer
extern void *sysvar ;		// Start of @ variables linked list
extern unsigned short tracen ;	// TRACE maximum line number
extern unsigned char flags ;	// BASIC's Boolean flags byte
extern unsigned int prand ;	// Pseudo-random number
extern unsigned char fvtab[] ;	// Table of 'fast' variable types
extern char modeno ;		// MODE number
extern size_t memhdc ;		// SDL Renderer
extern heapptr cmdadr, diradr, libadr, usradr, tmpadr ;
extern int     cmdlen, dirlen, liblen, usrlen, tmplen ;

// Defined in bbcsdl.c:
extern char *szCmdLine ;	// @cmd$
extern char *szLoadDir ;	// @dir$
extern char *szLibrary ;	// @lib$
extern char *szUserDir ;	// @usr$
extern char *szTempDir ;	// @tmp$
extern const char szNotice [] ;
extern void *userRAM ;

// Defined in bbccon.c
#ifdef PICO
extern void *libtop;
#endif

#include <align.h>
