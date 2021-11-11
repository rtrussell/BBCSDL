/*****************************************************************\
*       32-bit BBC BASIC Interpreter                              *
*       (c) 2018-2021  R.T.Russell  http://www.rtrussell.co.uk/   *
*                                                                 *
*       bbasmb.c: Simple ARM 4 assembler                          *
*       Version 1.24a, 12-Jul-2021                                *
\*****************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include "BBC.h"

#ifndef __WINDOWS__
#define stricmp strcasecmp
#define strnicmp strncasecmp
#endif

#if defined(__x86_64__) || defined(__aarch64__)
#define OC ((unsigned int) stavar[15] + (void *)((long long) stavar[13] << 32)) 
#define PC ((unsigned int) stavar[16] + (void *)((long long) stavar[17] << 32)) 
#else
#define OC (void *) stavar[15]
#define PC (void *) stavar[16]
#endif

// External routines:
void newlin (void) ;
void *getput (unsigned char *) ;
void error (int, const char *) ;
void token (signed char) ;
void text (const char*) ;
void crlf (void) ;
void comma (void) ;
void spaces (int) ;
int range0 (char) ;
signed char nxt (void) ;

long long itemi (void) ;
long long expri (void) ;
VAR expr (void) ;
VAR exprs (void) ;
VAR loadn (void *, unsigned char) ;
void storen (VAR, void *, unsigned char) ;

// Routines in bbcmos.c:
void *sysadr (char *) ;
unsigned char osrdch (void) ;
void oswrch (unsigned char) ;
int oskey (int) ;
void osline (char *) ;
int osopen (int, char *) ;
void osshut (int) ;
unsigned char osbget (int, int *) ;
void osbput (int, unsigned char) ;
long long getptr (int) ;
void setptr (int, long long) ;
long long getext (int) ;
void oscli (char *) ;
int osbyte (int, int) ;
void osword (int, void *) ;

static char *mnemonics[] = {
		"adc", "add", "adr", "align", "and", "bic", "blx", "bl", "bx", "b", "clz",
		"cmn", "cmp", "db", "dcb", "dcd", "dcs", "dcw", "eor", "equb", "equd", "equq",
		"equs", "equw", "ldm", "ldr", "mla", "mov", "mrs", "msr", "mul", "mvn",
		"nop", "opt", "orr", "pop", "push", "rsb", "rsc", "sbc", "smlal", "smull",
		"stm", "str", "sub", "swi", "swp", "swpb", "teq", "tst", "umlal", "umull" } ;

static unsigned char opcodes[] = {
		0x0A, 0x08, 0x00, 0xFF, 0x00, 0x1C, 0x12, 0xB0, 0x12, 0xA0, 0x16,
		0x17, 0x15, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x02, 0xFF, 0xFF, 0xFF,
		0xFF, 0xFF, 0x81, 0x01, 0x02, 0x1A, 0x10, 0x12, 0x00, 0x1E,
		0x1A, 0xFF, 0x18, 0x8B, 0x92, 0x06, 0x0E, 0x0C, 0x0E, 0x0C,
		0x80, 0x00, 0x04, 0xF0, 0x10, 0x10, 0x13, 0x11, 0x0A, 0x08 } ;

enum {		ADC, ADD, ADR, ALIGN, AND, BIC, BLX, BL, BX, B, CLZ,
		CMN, CMP, DB, DCB, DCD, DCS, DCW, EOR, EQUB, EQUD, EQUQ,
		EQUS, EQUW, LDM, LDR, MLA, MOV, MRS, MSR, MUL, MVN,
		NOP, OPT, ORR, POP, PUSH, RSB, RSC, SBC, SMLAL, SMULL,
		STM, STr, SUB, SWI, SWP, SWPB, TEQ, TST, UMLAL, UMULL } ;

static char *conditions[] = {
		"al", "cc", "cs", "eq", "ge", "gt", "hi", "hs",
		"le", "lo", "ls", "lt", "mi", "ne", "pl", "vc", "vs" } ;

static char *collisions[] = { "e", "o", "s", "t" } ;

static unsigned char ccodes[] = {
		0b1110, 0b0011, 0b0010, 0b0000, 0b1010, 0b1100, 0b1000, 0b0010,
		0b1101, 0b0011, 0b1001, 0b1011, 0b0100, 0b0001, 0b0101, 0b0111, 0b0110 } ;

static char *suffices[] = { "bt", "b", "d", "h", "sb", "sh", "t" } ;

static char *stackops[] = { "da", "ia", "db", "ib", "fa", "fd", "ea", "ed" } ;

static char *registers[] = {
		"lr", "pc", "r0", "r10", "r11", "r12", "r13", "r14", "r15", "r1", 
		"r2", "r3", "r4", "r5", "r6", "r7", "r8", "r9", "sp" } ;

static unsigned char regno[] = {
		14, 15, 0, 10, 11, 12, 13, 14, 15, 1, 2, 3, 4, 5, 6, 7, 8, 9, 13 } ;

static char *shifts[] = { "lsl", "lsr", "asr", "ror", "rrx", "asl" } ;

static char *oslist[] = {
		"osrdch", "oswrch", "oskey", "osline", "oscli", "osopen", "osbyte", "osword",
		"osshut", "osbget", "osbput", "getptr", "setptr", "getext" } ;

static void *osfunc[] = {
		osrdch, oswrch, oskey, osline, oscli, osopen, osbyte, osword, 
		osshut, osbget, osbput, getptr, setptr, getext } ;

static int lookup (char **arr, int num)
{
	int i, n ;
	for (i = 0; i < num; i++)
	    {
		n = strlen (*(arr + i)) ;
		if (strnicmp ((const char *)esi, *(arr + i), n) == 0)
			break ;
	    }
	if (i >= num)
		return -1 ;
	esi += n ;
	return i ;
}

static unsigned char reg (void)
{
	int i ;
	nxt () ;
	i = lookup (registers, sizeof(registers) / sizeof(registers[0])) ;
	if (i < 0)
	    {
		i = itemi() ;
		if ((i < 0) || (i > 15))
			error (16, NULL) ; // 'Syntax error'
		return i ;
	    }
	return regno[i] ;
}

static unsigned char shift (void)
{
	int i ;
	nxt () ;
	i = lookup (shifts, sizeof(shifts) / sizeof(shifts[0])) ;
	if (i < 0)
		error (16, NULL) ; // 'Syntax error'
	return i % 5 ;
}

static unsigned char stackop (int mnemonic) 
{
	int i = lookup (stackops, sizeof(stackops) / sizeof(stackops[0])) ;
	if (i < 0)
		error (16, NULL) ; // 'Syntax error'
	if ((mnemonic == STM) && (i >= 4))
		i ^= 3 ; // invert 
	return i & 3 ;
}

static unsigned char shiftcount (void)
{
	int n = expri () ;
	if ((n < 0) || (n > 31))
		error (16, NULL) ; // 'Syntax error'
	return n ;
}

static int immrot (unsigned int n) 
{
	int rotate = 0 ;
	while (n > 255)
	    {
		n = (n << 2) | ((n & 0xC0000000) >> 30) ;
		rotate += 1 ;
		if (rotate >= 16)
		    {
			if ((liston & BIT5) == 0)
				break ;
			error (2, NULL) ; // 'Bad immediate constant'
		    }
	    }
	return n | (rotate << 8) ;
}

static int reglist (void) 
{
	int temp = 0 ;
	if (nxt () != '{')
		error (16, NULL) ; // 'Syntax error'
	do
	    {
		unsigned char tmp ;
		esi++ ;
		tmp = reg () ;
		temp |= 1 << tmp ;
		if (nxt () == '-')
		    {
			unsigned char last ;
			esi++ ;
			last = reg () ;
			while (++tmp <= last)
				temp |= 1 << tmp ;
		    }
	    }
	while (nxt () == ',') ;
	if (*esi != '}')
		error (16, NULL) ; // 'Syntax error'
	esi++ ;
	return temp ;
}

static int shifter_operand (void)
{
	int bits, temp ;
	if (nxt () == '#')
	    {
		esi++ ;
		return immrot (expri ()) | 0x2000000 ;
	    }
	bits = reg () ;
	if (nxt () != ',')
		return bits ;
	esi++ ; // bump past comma
	temp = shift () ;
	if (temp == 4)
		return bits | 0x60 ; // RRX
	bits |= temp << 5 ;
	if (nxt () == '#')
	    {
		esi++ ;
		return bits | shiftcount () << 7 ;
	    }
	return bits | (reg () << 8) | BIT4 ;
}

static int offset (unsigned char *pimm, unsigned char *pplus)
{
	*pimm = 0 ;
	*pplus = 1 ;

	if (nxt () == '#')
	    {
		esi++ ;
		*pimm = 1 ;
	    }

	if (nxt () == '+')
		esi++ ;
	else if (*esi == '-')
	    {
		esi++ ;
		*pplus = 0 ;
	    }

	if (*pimm)
		return expri () ;
	return reg () ;
}

static void tabit (int x)
{
	if (vcount == x) 
		return ;
	if (vcount > x)
		crlf () ;
	spaces (x - vcount) ;
}

static void poke (void *p, int n) 
{
	char *d ;
	if (liston & BIT6)
	    {
		d = OC ;
		stavar[15] += n ;
	    }
	else
		d = PC ;

	stavar[16] += n ;
	memcpy (d, p, n) ;
}

static void *align (void)
{
	while (stavar[16] & 3)
	    {
		stavar[16]++ ;
		if (liston & BIT6)
			stavar[15]++ ;
	    } ;
	return PC ;
}

void assemble (void)
{
	signed char al ;
	signed char *oldesi = esi ;
	int init = 1 ;
	void *oldpc = PC ;

	while (1)
	    {
		int mnemonic, condition, instruction = 0 ;
		unsigned char ccode ;

		if (liston & BIT7)
		    {
			int tmp ;
			if (liston & BIT6)
				tmp = stavar[15] ;
			else
				tmp = stavar[16] ;
			if (tmp >= stavar[12])
				error (8, NULL) ; // 'Address out of range'
		    }

		al = nxt () ;
		esi++ ;

		switch (al) 
		    {
			case 0:
				esi-- ;
				liston = (liston & 0x0F) | 0x30 ;
				return ;

			case ']':
				liston = (liston & 0x0F) | 0x30 ;
				return ;

			case 0x0D:
				newlin () ;
				if (*esi == 0x0D)
					break ;
			case ':':
				if (liston & BIT4)
				    {
					void *p ;
					int n = PC - oldpc ;
					if (liston & BIT6)
						p = OC - n ;
					else
						p = PC - n ;

					do
					    {
						unsigned int i = *(unsigned int *)p ;
#if (defined (_WIN32)) && (__GNUC__ < 9)
						sprintf (accs, "%08I64X ", (long long) (size_t) oldpc) ;
#else
						sprintf (accs, "%08llX ", (long long) (size_t) oldpc) ;
#endif
						switch (n)
						    {
							case 0: break ;
							case 1:	i &= 0xFF ;
							case 2: i &= 0xFFFF ;
							case 3: i &= 0xFFFFFF ;
							case 4: sprintf (accs + 9, "%0*X ", n*2, i) ;
								break ;
							default: sprintf (accs + 9, "%08X ", i) ;
						    }
						if (n > 4)
						    {
							n -= 4 ;
							p += 4 ;
							oldpc += 4 ;
						    }
						else 
							n = 0 ;

						text (accs) ;

						if (*oldesi == '.')
						    {
							tabit (18) ;
							do	
							    {
								token (*oldesi++ ) ;
							    }
							while (range0(*oldesi)) ;
							while (*oldesi == ' ') oldesi++ ;
						    }
						tabit (30) ;
						while ((*oldesi != ':') && (*oldesi != 0x0D)) 
							token (*oldesi++) ;
						crlf () ;
					    }
					while (n) ;
				    }
				nxt () ;
#ifdef __arm__
				if ((liston & BIT6) == 0)
					__builtin___clear_cache (oldpc, PC) ; 
#endif
				oldpc = PC ;
				oldesi = esi ;
				break ;

			case ';':
			case TREM:
				while ((*esi != 0x0D) && (*esi != ':')) esi++ ;
				break ;

			case '.':
				if (init)
					oldpc = align () ;
				{
				VAR v ;
				unsigned char type ;
				void *ptr = getput (&type) ;
				if (ptr == NULL)
					error (16, NULL) ; // 'Syntax error'
				if (type >= 128)
					error (6, NULL) ; // 'Type mismatch'
				if ((liston & BIT5) == 0)
				    {
					v = loadn (ptr, type) ;
					if (v.i.n)
						error (3, NULL) ; // 'Multiple label'
				    }
				v.i.t = 0 ;
				v.i.n = (intptr_t) PC ;
				storen (v, ptr, type) ;
				}
				break ;

			default:
				esi-- ;
				mnemonic = lookup (mnemonics, sizeof(mnemonics)/sizeof(mnemonics[0])) ;

				condition = lookup (conditions, 
						    sizeof(conditions) / sizeof(conditions[0])) ;

				if ((condition == -1) && (mnemonic == BL))
				    {
					condition = lookup (collisions, 
							sizeof (collisions) / sizeof(collisions[0])) ;
					if (condition >= 0)
					    {
						mnemonic = B ;
						condition += 8 ; // ble, blo, bls, blt
					    }
				    }

				if (condition == -1)
					ccode = 0b1110 ;
				else
					ccode = ccodes[condition] ;

				if (mnemonic != OPT)
					init = 0 ;

				switch (mnemonic)
				    {
					case OPT:
						liston = (liston & 0x0F) | (expri () << 4) ;
						continue ;

					case DB:
						{
						VAR v = expr () ;
						if (v.s.t == -1)
						    {
							if (v.s.l > 256)
								error (19, NULL) ; // 'String too long'
							poke (v.s.p + zero, v.s.l) ;
							continue ;
						    }
						if (v.i.t)
							v.i.n = v.f ;
						poke (&v.i.n, 1) ;
						continue ;
						}

					case DCB:
					case EQUB:
						{
						int n = expri () ;
						poke (&n, 1) ;
						continue ; // n.b. not break
						}
 
					case DCW:
					case EQUW:
						{
						int n = expri () ;
						poke (&n, 2) ;
						continue ; // n.b. not break
						}

					case DCD:
					case EQUD:
					case EQUQ:
						{
						VAR v = expr () ;
						long long n ;
						if (v.s.t == -1)
						    {
							signed char *oldesi = esi ;
							int i ;
							memcpy (accs, v.s.p + zero, v.s.l) ;
							*(accs + v.s.l) = 0 ;
							esi = (signed char *)accs ;
							i = lookup (oslist, sizeof(oslist) /
								sizeof(oslist[0])) ;
							esi = oldesi ;
							if (i >= 0)
								n = (size_t) osfunc[i] ;
							else
						 		n = (size_t) sysadr (accs) ;
							if (n == 0)
								error (51, NULL) ; // 'No such system call'
						    }
						else if (v.i.t == 0)
							n = v.i.n ;
						else
							n = v.f ;
						if (mnemonic == EQUQ)
						    {
							poke (&n, 8) ;
							continue ;
						    }
						instruction = (int) n ;
						}
						break ;

					case DCS:
					case EQUS:
						{
						VAR v = exprs () ;
						if (v.s.l > 256)
							error (19, NULL) ; // 'String too long'
						poke (v.s.p + zero, v.s.l) ;
						continue ;
						}

					case NOP:
						instruction = 0xE1A00000 ;
						break ;

					case ADR:
						{
						int offpc ;
						instruction = (ccode << 28) | reg () << 12 ;
						comma () ;
						offpc = (void *) (size_t) expri () - PC - 8 ;
						if (offpc >= 0)
							instruction |= 0x028F0000 | immrot (offpc) ;
						else
							instruction |= 0x024F0000 | immrot (-offpc) ;
						break ;
						}						
 
					case ADC:
					case ADD:
					case AND:
					case BIC:
					case EOR:
					case ORR:
					case RSB:
					case RSC:
					case SBC:
					case SUB:
						instruction = (ccode << 28) | (opcodes[mnemonic] << 20) ;
						if ((*esi == 's') || (*esi == 'S'))
						    {
							esi++ ;
							instruction |= 0x100000 ;
						    }
						instruction |= reg () << 12 ;
						comma () ;
						instruction |= reg () << 16 ;
						comma () ;
						instruction |= shifter_operand () ;
						break ;

					case CMN:
					case CMP:
					case TEQ:
					case TST:
						instruction = (ccode << 28) | (opcodes[mnemonic] << 20) ;
						if ((*esi == 's') || (*esi == 'S'))
							esi++ ;
						instruction |= reg () << 16 ;
						comma () ;
						instruction |= shifter_operand () ;
						break ;

					case MOV:
					case MVN:
						instruction = (ccode << 28) | (opcodes[mnemonic] << 20) ;
						if ((*esi == 's') || (*esi == 'S'))
						    {
							esi++ ;
							instruction |= 0x100000 ;
						    }
						instruction |= reg () << 12 ;
						comma () ;
						instruction |= shifter_operand () ;
						break ;

					case MUL:
					case MLA:
						instruction = (ccode << 28) | (opcodes[mnemonic] << 20) ;
						if ((*esi == 's') || (*esi == 'S'))
						    {
							esi++ ;
							instruction |= 0x100000 ;
						    }
						instruction |= reg () << 16 ;
						comma () ;
						instruction |= reg () | 0x90 ;
						comma () ;
						instruction |= reg () << 8 ;
						if (mnemonic == MLA)
						    {
							comma () ;
							instruction |= reg () << 12 ;
						    }
						break ;

					case SMLAL:
					case SMULL:
					case UMLAL:
					case UMULL:
						instruction = (ccode << 28) | (opcodes[mnemonic] << 20) ;
						if ((*esi == 's') || (*esi == 'S'))
						    {
							esi++ ;
							instruction |= 0x100000 ;
						    }
						instruction |= reg () << 12 ;
						comma () ;
						instruction |= reg () << 16 ;
						comma () ;
						instruction |= reg () | 0x90 ;
						comma () ;
						instruction |= reg () << 8 ;
						break ;

					case B:
					case BL:
						{
						int dest ;
						instruction = (ccode << 28) | (opcodes[mnemonic] << 20) ;
						dest = ((void *) (size_t) expri () - PC - 8) >> 2 ;
						if ((dest != (dest << 8 >> 8)) && ((liston & BIT5) != 0))
							error (1, NULL) ; // 'Jump out of range'
						instruction |= (dest & 0xFFFFFF) ;
						}
						break ;

					case BX:
						instruction = (ccode << 28) | 0x012FFF10 ;
						instruction |= reg () ;
						break ;

					case BLX:
						instruction = (ccode << 28) | 0x012FFF30 ;
						instruction |= reg () ;
						break ;

					case LDM:
					case STM:
						instruction = (ccode << 28) | (opcodes[mnemonic] << 20) ;
						instruction |= stackop (mnemonic) << 23 ;
						instruction |= reg () << 16 ;
						if (nxt () == '!')
						    {
							esi++ ;
							instruction |= 0x200000 ;
						    }
						comma () ;
						instruction |= reglist () ;
						break ;

					case PUSH:
					case POP:
						instruction = (ccode << 28) | (opcodes[mnemonic] << 20) ;
						instruction |= 0xD0000 | reglist () ;
						break ;

					case LDR:
					case STr:
						{
						int offreg = 0, suffix ;
						unsigned char imm = 1, add = 1 ;
						suffix = lookup (suffices, sizeof(suffices) /
							sizeof(suffices[0])) ;

						instruction = (ccode << 28) | (opcodes[mnemonic] << 20) ;
						instruction |= reg () << 12 ;
						comma () ;

						if (nxt () != '[')
						    {
							offreg = (void *) (size_t) expri () - PC - 8 ;
							if ((abs(offreg) > 0xFFF) && (liston & BIT5))
								error (8, NULL) ;
							if (offreg >= 0)
								instruction |= 0x58F0000 | (offreg & 0xFFF) ;
							else
								instruction |= 0x50F0000 | (-offreg & 0xFFF) ;
							break ;
						    }

						esi++ ;
						instruction |= reg () << 16 ;

						if (nxt () == ']')
							esi++ ;
						else
							instruction |= 0x1000000 ; // P bit

						if ((suffix < 2) || (suffix == 6))
						    { // Addressing mode 2 (nothing, BT, B, T)
							instruction |= 0x4000000 ;

							if ((suffix == 0) || (suffix == 6)) // BT, T
								instruction |= 0x200000 ; // W bit

							if ((suffix == 0) || (suffix == 1))
								instruction |= 0x400000 ; // B bit

							if (nxt () != ',')
							    {
								instruction |= 0x1800000 ;
								break ;
							    }
							esi++ ;

							offreg = offset (&imm, &add) ;
							if (imm)
								instruction |= offreg & 0xFFF ;
							else
								instruction |= offreg | 0x2000000 ;
							
							if (nxt () == ',')
							    {
								int n ;
								esi++ ;
								n = shift () ;
								if ((n >= 4) || (nxt () != '#'))
									error (16, NULL) ;
								esi++ ;
								instruction |= n << 5 ;
								instruction |= shiftcount () << 7 ;
							    }
						    }
						else
						    { // Addressing mode 3 (D, H, SB, SH) 2,3,4,5

							if ((mnemonic == LDR) && (suffix != 2))
								instruction |= 0x100000 ; // L bit
							if ((suffix == 2) || (suffix == 4) || 
									(suffix == 5))
								instruction |= 0x40 ; // S bit
							if ((suffix == 3) || (suffix == 5) ||
							    ((mnemonic != LDR) && (suffix == 2)))
								instruction |= 0x20 ; // H bit

							if (nxt () != ',')
							    {
								instruction |= 0x1C00090 ;
								break ;
							    }
							esi++ ;

							offreg = offset (&imm, &add) ;
							if (imm)
								instruction |= (offreg & 0x0F) |
								  ((offreg & 0xF0) << 4) | 0x400090 ;
							else
								instruction |= offreg | 0x90 ;

						    }

						if (add) 
							instruction |= 0x800000 ; // U bit

						if ((instruction & 0x1000000) == 0) // Test P bit
							break ;
						if (nxt () != ']')
							error (16, NULL) ; // 'Syntax error'
						esi++ ;

						if (nxt () == '!')
						    {
							esi++ ;
							instruction |= 0x200000 ; // W bit
						    }
						}
						break ;

					case CLZ:
						instruction = (ccode << 28) | (opcodes[mnemonic] << 20) ;
						instruction |= reg () << 12 ;
						comma () ;
						instruction |= reg () | 0xF0F10 ;
						break ;

					case SWI:
						instruction = (ccode << 28) | (opcodes[mnemonic] << 20) ;
						instruction |= expri () & 0xFFFFFF ;
						break ;

					case SWP:
					case SWPB:
						instruction = (ccode << 28) | (opcodes[mnemonic] << 20) ;
						if (mnemonic == SWPB)
							instruction |= 0x400000 ;
						instruction |= reg () << 12 ;
						comma () ;
						instruction |= reg () | 0x90 ;
						comma () ;
						if (*esi != '[')
							error (16, NULL) ; // 'Syntax error'
						esi++ ;
						instruction |= reg () << 16 ;
						if (nxt () != ']')
							error (16, NULL) ; // 'Syntax error'
						esi++ ;
						break ;

					case ALIGN:
						oldpc = align () ;
						if ((nxt() >= '1') && (*esi <= '9'))
						    {
							int n = expri () ;
							if ((n & (n - 1)) || (n & 0xFFFFFF03) || (n == 0))
								error (16, NULL) ; // 'Syntax error'
							instruction = 0xE1A00000 ;
							while (stavar[16] & (n - 1))
								poke (&instruction, 4) ; 
						    }
						continue ;

					default:
						error (16, NULL) ; // 'Syntax error'
				    }

				oldpc = align () ;

				poke (&instruction, 4) ;
		    }
	    } ;
}
