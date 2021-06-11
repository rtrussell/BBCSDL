/******************************************************************\
*       BBC BASIC for SDL 2.0 (Emscripten / Web Assembly)          *
*       Copyright (c) R. T. Russell, 2000-2021                     *
*                                                                  *
*       BBC.h constant and variable declarations                   *
*       Version 1.23a, 06-Jun-2021                                 *
\******************************************************************/

// Constants:
#define STACK_NEEDED 512

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

// A variant holds an 80-bit long double, a 64-bit long long or a string descriptor.
// n.b. GCC pads a long double to 16 bytes (128 bits) for alignment reasons but only
// the least-significant 80-bits need to be stored on the heap, in files etc.
// When a long double is 64-bits rather than 80-bits (e.g. ARM) it will be necessary
// to force the type word (.i.t or .s.t member) to a value other than 0 or -1. 
typedef union __attribute__ ((packed)) __attribute__ ((aligned (4))) tagVAR
{
#if defined(__arm__) || defined(__aarch64__) || defined(__EMSCRIPTEN__)
	double f ;
#else
        long double f ;
#endif
        struct
        {
          long long n ;
          short t ; // = 0
        } i ;
        struct
        {
          heapptr p ; // Assumed to be 32 bits
          unsigned int l ; // Must be unsigned for overflow tests in 'math'
          short t ; // = -1
        } s ;
	struct
	{
	  double d ;
	  short t ; // unused (loadn/storen only)
	} d ;
} VAR, *LPVAR ; 

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
register signed char *esi asm ("r10") ;	// Program pointer
register heapptr *esp asm ("r11") ;	// Stack pointer
#endif
#endif

// Data locations (defined in bbcdata):
extern int stavar[] ;		// Static integer variables
#define dynvar ((heapptr *)((char*)stavar + 108))	// Linked-list pointers
#define fnptr  ((heapptr *)((char*)stavar + 324))	// Pointer to user FuNctions
#define proptr ((heapptr *)((char*)stavar + 328))	// Pointer to user PROCedures
#define accs   (*(char **)((char*)stavar + 332))	// String accumulator
#define buff   (*(char **)((char*)stavar + 336))	// Temporary line buffer
#define vpage  (*(heapptr *)((char*)stavar + 340))	// Value of PAGE
#define tracen (*(unsigned short *)((char*)stavar + 344))	// TRACE maximum line number
#define lomem  (*(heapptr *)((char*)stavar + 348))	// Pointer to base of heap
#define pfree  (*(heapptr *)((char*)stavar + 352))	// Pointer to free space
#define himem  (*(heapptr *)((char*)stavar + 356))	// Pointer to top of stack
#define libase (*(heapptr *)((char*)stavar + 360))	// Base of libraries 
#define errtxt (*(const char **)((char*)stavar + 364))	// Most recent error message
#define onersp (*(heapptr **)((char*)stavar + 368))	// ON ERROR LOCAL stack pointer
#define errtrp (*(heapptr *)((char*)stavar + 372))	// Pointer to ON ERROR handler
#define datptr (*(heapptr *)((char*)stavar + 376))	// DATA pointer
#define vcount (*(unsigned int *)((char*)stavar + 380))	// Character count since newline
#define curlin (*(heapptr *)((char*)stavar + 384))	// Pointer to current line
#define timtrp (*(heapptr *)((char*)stavar + 388))	// Pointer to ON TIME handler
#define clotrp (*(heapptr *)((char*)stavar + 392))	// Pointer to ON CLOSE handler
#define siztrp (*(heapptr *)((char*)stavar + 396))	// Pointer to ON MOVE handler
#define systrp (*(heapptr *)((char*)stavar + 400))	// Pointer to ON SYS handler
#define moutrp (*(heapptr *)((char*)stavar + 404))	// Pointer to ON MOUSE handler
#define errlin (*(heapptr *)((char*)stavar + 408))	// Pointer to error line
#define prand  (*(unsigned int *)((char*)stavar + 412))	// Pseudo-random number
#define vwidth (*(unsigned char *)((char*)stavar + 417))// Width for auto-newline
#define errnum (*(unsigned char *)((char*)stavar + 418))// Error code number
#define liston (*(unsigned char *)((char*)stavar + 419))// *FLOAT/*HEX/*LOWERCASE/OPT
#define lstopt (*(char *)((char*)stavar + 444))		// LISTO value

extern node *flist[] ;		// String free-lists
extern STR tmps ;		// Temporary string descriptor
extern unsigned char fvtab[] ;	// Table of 'fast' variable types

extern int vduvar[] ;		// VDU variables
#define modeno (*(char *)((char*)vduvar + 72))		// MODE number
#define evtqw (*(unsigned char *)((char*)vduvar + 205))	// Event queue write pointer
#define evtqr (*(unsigned char *)((char*)vduvar + 206))	// Event queue read pointer

extern int sysvar[] ;		// @ variables linked list
#define memhdc (*(size_t *)((char*)sysvar + 12))	// SDL Renderer
#define flags  (*(unsigned char *)((char*)sysvar + 183))// BASIC's Boolean flags byte
#define link00 (*(int *)((char*)sysvar + 490))		// Terminating link in @ list
#define diradr (*(heapptr *)((char*)sysvar + 228))
#define dirlen (*(int *)((char*)sysvar + 232))
#define libadr (*(heapptr *)((char*)sysvar + 248))
#define liblen (*(int *)((char*)sysvar + 252))
#define cmdadr (*(heapptr *)((char*)sysvar + 268))
#define cmdlen (*(int *)((char*)sysvar + 272))
#define usradr (*(heapptr *)((char*)sysvar + 288))
#define usrlen (*(int *)((char*)sysvar + 292))
#define tmpadr (*(heapptr *)((char*)sysvar + 308))
#define tmplen (*(int *)((char*)sysvar + 312))

// Defined in bbcsdl.c:
extern char *szCmdLine ;	// @cmd$
extern char *szLoadDir ;	// @dir$
extern char *szLibrary ;	// @lib$
extern char *szUserDir ;	// @usr$
extern char *szTempDir ;	// @tmp$
extern const char szNotice [] ;
extern void *progRAM ;
extern void *userRAM ;