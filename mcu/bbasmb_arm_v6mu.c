/*******************************************************************\
 *       32-bit BBC BASIC Interpreter                              *
 *       (c) 2018-2021  R.T.Russell  http://www.rtrussell.co.uk/   *
 *                                                                 *
 *       bbasmb_arm_v6m.c Assembler for ARMv6m thunb instructions  *
 *       Unified syntax                                            *
 *       Version 1.24a, 12-Jul-2021                                *
\*******************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <stdint.h>
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
void newlin (void);
void *getput (unsigned char *);
void error (int, const char *);
void token (signed char);
void text (const char*);
void crlf (void);
void comma (void);
void spaces (int);
int range0 (char);
signed char nxt (void);

long long itemi (void);
long long expri (void);
VAR expr (void);
VAR exprs (void);
VAR loadn (void *, unsigned char);
void storen (VAR, void *, unsigned char);

// Routines in bbcmos.c:
void *sysadr (char *);
unsigned char osrdch (void);
void oswrch (unsigned char);
int oskey (int);
void osline (char *);
int osopen (int, char *);
void osshut (int);
unsigned char osbget (int, int *);
void osbput (int, unsigned char);
long long getptr (int);
void setptr (int, long long);
long long getext (int);
void oscli (char *);
int osbyte (int, int);
void osword (int, void *);

enum {
    ASR, LSL, LSR,                                                  // 2-4: 2 Opcodes
    LDRB, STRB,                                                     // 5-6: 2 opcodes
    LDRH, STRH,                                                     // 7-8: 2 opcodes
    ADC, AND, BIC, CMN, EOR, MVN, ORR, REV16, REVSH, REV, ROR, SBC,
    SXTB, SXTH, TST, UXTB, UXTH,                                    // 9-25: <reg8>, <reg8>
    NOP, SEV, WFE, WFI, YIELD,                                      // 26-29: <opcode>
    BLX, BX,                                                        // 30-31: <reg8>
    DMB, DSB, ISB,                                                  // 32-34: 32-bit 0xF3BF____
    LDRSB, LDRSH,                                                   // 35-36: <reg8>, [<reg8>, <reg8>]
    CPSID, CPSIE,
    CMP, MOV,
    ADD, ADR, BKPT, B, LDM, LDR, MRS, MSR, MUL,
    POP, PUSH, RSB, STM, STr, SUB, SVC,                             // 37-54: Unique opcodes
    ALIGN, DB, DCB, DCD, DCS, DCW, EQUB, EQUD, EQUQ, EQUS, EQUW, OPT    // 55-66: Pseudo-ops
    };

static const char *mnemonics[] = {
    "asrs", "lsls", "lsrs",
    "ldrb", "strb",
    "ldrh", "strh",
    "adcs", "ands", "bics", "cmn", "eors", "mvns", "orrs", "rev16", "revsh", "rev", "rors", "sbcs",
    "sxtb", "sxth", "tst", "uxtb", "uxth",
    "nop", "sev", "wfe", "wfi", "yield",
    "blx", "bx",
    "dmb", "dsb", "isb",
    "ldrsb", "ldrsh",
    "cpsid", "cpsie",
    "cmp", "mov",
    "add", "adr", "bkpt", "b", "ldm", "ldr", "mrs", "msr", "muls",
    "pop", "push", "rsbs", "stm", "str", "sub", "svc",
    "align", "db", "dcb", "dcd", "dcs", "dcw", "equb", "equd", "equq", "equs", "equw", "opt"};

static const uint16_t opcodes[] = {
    0x1000,     // ASR \ <reg8>, <reg8>, #<imm5>
    0x0000,     // LSL |
    0x0800,     // LSR /
    0x7800,     // LDRB \ <reg8>, [<reg8>, #<imm5>]
    0x7000,     // STRB /
    0x8800,     // LDRH \ <reg8>, [<reg8>. #<imm5:1>]
    0x8000,     // STRH /
    0x4140,     // ADC   \ <reg8>, <reg8>
    0x4000,     // AND   |
    0x4380,     // BIC   |
    0x42C0,     // CMN   |
    0x4040,     // EOR   |
    0x43C0,     // MVN   |
    0x4300,     // ORR   |
    0xBA40,     // REV16 |
    0xBAC0,     // REVSH |
    0xBA00,     // REV   |
    0x41C0,     // ROR   |
    0x4180,     // SBC   |
    0xB240,     // SXTB  |
    0xB200,     // SXTH  |
    0x4200,     // TST   |
    0xB2C0,     // UXTB  |
    0xB280,     // UXTH  /
    0xBF00,     // NOP   \ <opcode>
    0xBF40,     // SEV   |
    0xBF20,     // WFE   |
    0xBF30,     // WFI   |
    0xBF10,     // YIELD /
    0x4780,     // BLX \ <reg8>
    0x4700,     // BX  /
    0x8F5F,     // DMB \ 32-bit 0xF3BF____
    0x8F4F,     // DSB |
    0x8F6F,     // ISB /
    0x5600,     // LDRSB \ <reg8>, [<reg8>, <reg8>]
    0x5700,     // LDRSH /
    0xB662,     // CPSID \ i
    0xB672      // CPSIE /
    };

static const uint16_t opcode2[] = {
    0x4100,     // ASR \ <reg8>, <reg8>
    0x4080,     // LSL |
    0x40C0,     // LSR /
    0x5C00,     // LDRB \ <reg8>, [<reg8>, <reg8>]
    0x5400,     // STRB /
    0x5A00,     // LDRH \ <reg8>, [<reg8>, <reg8>]
    0x5200,     // STRH /
    };

static const char *conditions[] = {
    "al", "cc", "cs", "eq", "ge", "gt", "hi", "hs",
    "le", "lo", "ls", "lt", "mi", "ne", "pl", "vc", "vs" };

static const unsigned char ccodes[] = {
    0b1110, 0b0011, 0b0010, 0b0000, 0b1010, 0b1100, 0b1000, 0b0010,
    0b1101, 0b0011, 0b1001, 0b1011, 0b0100, 0b0001, 0b0101, 0b0111, 0b0110 };

static const char *registers[] = {
    "lr", "pc", "r0", "r10", "r11", "r12", "r13", "r14", "r15", "r1", 
    "r2", "r3", "r4", "r5", "r6", "r7", "r8", "r9", "sp" };

static const unsigned char regno[] = {
    14, 15, 0, 10, 11, 12, 13, 14, 15, 1, 2, 3, 4, 5, 6, 7, 8, 9, 13 };

static const char *sysm[] = { "apsr", "iapsr", "eapsr", "xpsr", "ipsr", "epsr",
                              "iepsr", "msp", "psp", "primask", "control" };

static const char *oslist[] = {
    "osrdch", "oswrch", "oskey", "osline", "oscli", "osopen", "osbyte", "osword",
    "osshut", "osbget", "osbput", "getptr", "setptr", "getext" };

static const void *osfunc[] = {
    osrdch, oswrch, oskey, osline, oscli, osopen, osbyte, osword, 
    osshut, osbget, osbput, getptr, setptr, getext };

static const char *asmmsg[] = {
    "Invalid opcode",               // 101
    "Too many parameters",          // 102
    "Invalid register",             // 103
    "Low register required",        // 104
    "Invalid alignment",            // 105
    "Register / list conflict",     // 106
    "Invalid register list",        // 107
    "Status flags not set",         // 108
    "Invalid special register"      // 109
    };

static void asmerr (int ierr)
    {
    if ( liston & 0x20 )
        {
        const char *pserr = NULL;
        if ( ierr > 100 ) pserr = asmmsg[ierr - 101];
        error (ierr, pserr);
        }
    }

static int lookup (const char **arr, int num)
    {
	int i, n;
	for (i = 0; i < num; i++)
	    {
		n = strlen (*(arr + i));
		if (strnicmp ((const char *)esi, *(arr + i), n) == 0)
			break;
	    }
	if (i >= num)
		return -1;
	esi += n;
	return i;
    }

static int status (void)
    {
    if (( *esi == 's' ) || ( *esi == 'S' ))
        {
        ++esi;
        return 1;
        }
    return 0;
    }

static unsigned char reg (void)
    {
	int i;
	nxt ();
	i = lookup (registers, sizeof(registers) / sizeof(registers[0]));
	if (i < 0)
	    {
		i = itemi();
		if ((i < 0) || (i > 15))
			asmerr (103); // 'Invalid register'
		return i;
	    }
	return regno[i];
    }

static unsigned char reg8 (void)
    {
    int i = reg ();
    if ( i >= 8 ) asmerr (104); // 'Low register required'
    return i;
    }

static int reglist (void) 
    {
	int temp = 0;
	if (nxt () != '{')
		asmerr (16); // 'Syntax error'
	do
	    {
		unsigned char tmp;
		esi++;
		tmp = reg ();
		temp |= 1 << tmp;
		if (nxt () == '-')
		    {
			unsigned char last;
			esi++;
			last = reg ();
			while (++tmp <= last)
				temp |= 1 << tmp;
		    }
	    }
	while (nxt () == ',');
	if (*esi != '}')
		asmerr (16); // 'Syntax error'
	esi++;
	return temp;
    }

static int offset (int *pbImm)
    {
	*pbImm = 0;
	if (nxt () == '#')
	    {
		esi++;
        *pbImm = 1;
		return expri ();
	    }
	return reg ();
    }

static int offset2 (int *pbImm)
    {
    int or;
    if ( nxt () == ',' )
        {
        ++esi;
        or = offset (pbImm);
        }
    else
        {
        *pbImm = 1;
        or = 0;
        }
    return or;
    }

static void tabit (int x)
{
	if (vcount == x) 
		return ;
	if (vcount > x)
		crlf () ;
	spaces (x - vcount) ;
}

static void poke (const void *p, int n) 
    {
	char *d;
	if (liston & BIT6)
	    {
		d = OC;
		stavar[15] += n;
	    }
	else
		d = PC;

	stavar[16] += n;
	memcpy (d, p, n);
    }

static void *align (int n)
    {
	while (stavar[16] & (n-1))
	    {
		stavar[16]++;
		if (liston & BIT6)
			stavar[15]++;
	    };
	return PC;
    }

static inline int eol (char ch)
    {
    return (( ch == 0x0D ) || ( ch == ':' ) || ( ch == ';' ) || ( ch == TREM )) ? 1 : 0;
    }

void assemble (void)
    {
	signed char al;
	signed char *oldesi = esi;
	void *oldpc = align (4);

	while (1)
	    {
		int mnemonic, condition, instruction;

		if (liston & BIT7)
		    {
			int tmp;
			if (liston & BIT6)
				tmp = stavar[15];
			else
				tmp = stavar[16];
			if (tmp >= stavar[12])
				error (8, NULL); // 'Address out of range'
		    }

		al = nxt ();
		esi++;

		switch (al) 
		    {
			case 0:
				esi--;
				liston = (liston & 0x0F) | 0x30;
				return;

			case ']':
				liston = (liston & 0x0F) | 0x30;
				return;

			case 0x0D:
				newlin ();
				if (*esi == 0x0D)
					break;
			case ':':
				if (liston & BIT4)
				    {
					void *p;
					int n = PC - oldpc;
					if (liston & BIT6)
						p = OC - n;
					else
						p = PC - n;

					do
					    {
#if (defined (_WIN32)) && (__GNUC__ < 9)
						sprintf (accs, "%08I64X ", (long long) (size_t) oldpc);
#else
						sprintf (accs, "%08llX ", (long long) (size_t) oldpc);
#endif
                        char *ps = accs + 9;
						switch (n)
						    {
                            default:
                                sprintf (ps, "%02X ", *((unsigned char *)p));
                                ps += 3;
                                ++p;
                            case 3:
                                sprintf (ps, "%02X ", *((unsigned char *)p));
                                ps += 3;
                                ++p;
                            case 2:
                                sprintf (ps, "%02X ", *((unsigned char *)p));
                                ps += 3;
                                ++p;
                            case 1:
                                sprintf (ps, "%02X ", *((unsigned char *)p));
                                ps += 3;
                                ++p;
                                break;
						    }
						if (n > 4)
						    {
							n -= 4;
							oldpc += 4;
						    }
						else 
							n = 0;

						text (accs);

						if (*oldesi == '.')
						    {
							tabit (21);
							do	
							    {
								token (*oldesi++ );
							    }
							while (range0(*oldesi));
							while (*oldesi == ' ') oldesi++;
						    }
						tabit (30);
						while ((*oldesi != ':') && (*oldesi != 0x0D)) 
							token (*oldesi++);
						crlf ();
					    }
					while (n);
				    }
				nxt ();
#ifdef __arm__
				if ((liston & BIT6) == 0)
					__builtin___clear_cache (oldpc, PC); 
#endif
				oldpc = PC;
				oldesi = esi;
				break;

			case ';':
			case TREM:
				while ((*esi != 0x0D) && (*esi != ':')) esi++;
				break;

			case '.':
            {
            VAR v;
            unsigned char type;
            void *ptr = getput (&type);
            if (ptr == NULL)
                asmerr (16); // 'Syntax error'
            if (type >= 128)
                asmerr (6); // 'Type mismatch'
            if ((liston & BIT5) == 0)
                {
                v = loadn (ptr, type);
                if (v.i.n)
                    asmerr (3); // 'Multiple label'
                }
            v.i.t = 0;
            v.i.n = (intptr_t) PC;
            storen (v, ptr, type);
            break;
            }

			default:
				esi--;
				mnemonic = lookup (mnemonics, sizeof(mnemonics)/sizeof(mnemonics[0]));

                oldpc = align (2);

                instruction = opcodes[NOP];
				switch (mnemonic)
				    {
					case OPT:
						liston = (liston & 0x0F) | (expri () << 4);
						continue;

					case DB:
                    {
                    VAR v = expr ();
                    if (v.s.t == -1)
                        {
                        if (v.s.l > 256)
                            asmerr (19); // 'String too long'
                        poke (v.s.p + zero, v.s.l);
                        continue;
                        }
                    if (v.i.t)
                        v.i.n = v.f;
                    poke (&v.i.n, 1);
                    continue;
                    }

					case DCB:
					case EQUB:
                    {
                    int n = expri ();
                    poke (&n, 1);
                    continue; // n.b. not break
                    }
 
					case DCW:
					case EQUW:
                    {
                    int n = expri ();
                    poke (&n, 2);
                    continue; // n.b. not break
                    }

					case DCD:
					case EQUD:
					case EQUQ:
                    {
                    VAR v = expr ();
                    long long n;
                    if (v.s.t == -1)
                        {
                        signed char *oldesi = esi;
                        int i;
                        memcpy (accs, v.s.p + zero, v.s.l);
                        *(accs + v.s.l) = 0;
                        esi = (signed char *)accs;
                        i = lookup (oslist, sizeof(oslist) /
                            sizeof(oslist[0]));
                        esi = oldesi;
                        if (i >= 0)
                            n = (size_t) osfunc[i];
                        else
                            n = (size_t) sysadr (accs);
                        if (n == 0)
                            asmerr (51); // 'No such system call'
                        }
                    else if (v.i.t == 0)
                        n = v.i.n;
                    else
                        n = v.f;
                    if (mnemonic == EQUQ)   poke (&n, 8);
                    else                    poke (&n, 4);
                    }
                    continue; // n.b. not break

					case DCS:
					case EQUS:
                    {
                    VAR v = exprs ();
                    if (v.s.l > 256)
                        asmerr (19); // 'String too long'
                    poke (v.s.p + zero, v.s.l);
                    continue;
                    }

                    case ALIGN:
                        oldpc = align (2);
                        if ((nxt() >= '1') && (*esi <= '9'))
                            {
                            int n = expri ();
                            if ((n & (n - 1)) || (n & 0xFFFFFF03) || (n == 0))
                                asmerr (105); // 'invalid alignment'
                            instruction = opcodes[NOP];
                            while (stavar[16] & (n - 1))
                                poke (&instruction, 2); 
                            }
                        continue;

                    // Thumb Instructions

                    case MOV:
                        if ( status () )
                            {
                            int rd = reg8 ();
                            comma ();
                            int bImm;
                            int offreg = offset (&bImm);
                            if ( bImm )
                                {
                                if (( offreg < 0 ) || ( offreg > 0xFF ))
                                    asmerr (2); // 'Bad immediate constant'
                                instruction = 0x2000 | (( rd & 0x07 ) << 8 ) | ( offreg & 0xFF );
                                }
                            else
                                {
                                if ( offreg > 7 ) asmerr (104); // 'Low register required'
                                instruction = 0x0000 | (( offreg & 0x07 ) << 3 ) | rd;
                                }
                            }
                        else
                            {
                            int rd = reg ();
                            comma ();
                            instruction = 0x4600 | (( rd & 0x08 ) << 4 ) | ( reg () << 3 ) | ( rd & 0x07 );
                            }
                        break;
                        
					case CMP:
                    {
                    // <opcode> <reg8>, #<imm8>
                    // <opcode> <reg8>, <reg8>
                    // <opcode> <reg>, <reg>
                    int rn = reg ();
                    comma ();
                    int bImm;
                    int	offreg = offset (&bImm);
                    if ( bImm )
                        {
                        if ( rn > 7 ) asmerr (104); // 'Low register required'
                        if (( offreg < 0 ) || ( offreg > 0xFF ))
                            asmerr (2); // 'Bad immediate constant'
                        instruction = 0x2800 | ( rn << 8 ) | ( offreg & 0xFF );
                        }
                    else if (( rn < 8 ) && ( offreg < 8 ))
                        {
                        instruction = 0x4280 | ( offreg << 3 ) | rn;
                        }
                    else
                        {
                        instruction = 0x4500 | (( rn & 0x08 ) << 4 ) | ( reg () << 3 ) | ( rn & 0x07 );
                        }
                    break;
                    }

                    case ASR:
                    case LSL:
                    case LSR:
                    {
                    // <opcode> Rd, Rm, #<offset>
                    // <opcode> Rd, Rd, Rs
                    int rd = reg8 ();
                    comma ();
                    int rm = reg8 ();
                    comma ();
                    int bImm;
                    int	offreg = offset (&bImm);
                    if ( bImm )
                        {
                        if (( offreg < 0 ) || ( offreg > 31 )) asmerr (2); // 'Bad immediate constant'
                        instruction = opcodes[mnemonic] | ( offreg << 6 ) | ( rm << 3 ) | rd;
                        }
                    else if (( rm == rd ) && ( offreg < 8 ))
                        {
                        instruction = opcode2[mnemonic] | ( offreg << 3 ) | rd;
                        }
                    else
                        {
                        asmerr (103);   //'Invalid register'
                        }
                    break;
                    }

                    case LDRB:
                    case STRB:
                    {
                    // <opcode> <reg8>, [<reg8>, #<imm5>]
                    // <opcode> <reg8>, [<reg8>, <reg8>]
                    int rd = reg8 ();
                    comma ();
                    if ( nxt () == '[' ) ++esi;
                    else asmerr (16); // 'Syntax error'
                    int rn = reg8 ();
                    int bImm;
                    int offreg = offset2 (&bImm);
                    if ( bImm )
                        {
                        if (( offreg < 0 ) || ( offreg > 31 ))
                            asmerr (2); // 'Bad immediate constant'
                        instruction = opcodes[mnemonic] | (( offreg & 0x1F ) << 6 ) | ( rn << 3 ) | rd;
                        }
                    else
                        {
                        if ( offreg > 7 ) asmerr (104); // 'Low register required'
                        instruction = opcode2[mnemonic] | (( offreg & 0x07 ) << 6 ) | ( rn << 3 ) | rd;
                        }
                    if ( nxt () == ']' ) ++esi;
                    else asmerr (16); // 'Syntax error'
                    break;
                    }

                    case LDRH:
                    case STRH:
                    {
                    // LDRH <reg8>, [<reg8>, #<imm5:1>]
                    // LDRH <reg8>, [<reg8>, <reg8>]
                    int rd = reg8 ();
                    comma ();
                    if ( nxt () == '[' ) ++esi;
                    else asmerr (16); // 'Syntax error'
                    int rn = reg8 ();
                    int bImm;
                    int offreg = offset2 (&bImm);
                    if ( bImm )
                        {
                        if (( offreg < 0 ) || ( offreg > 62 ))
                            asmerr (2); // 'Bad immediate constant'
                        else if ( offreg & 0x01 )
                            asmerr (105);   // 'Invalid alignment'
                        instruction = opcodes[mnemonic] | (( offreg & 0x3E ) << 5 ) | ( rn << 3 ) | rd;
                        }
                    else
                        {
                        if ( offreg > 7 ) asmerr (104); // 'Low register required'
                        instruction = opcode2[mnemonic] | (( offreg & 0x07 ) << 6 ) | ( rn << 3 ) | rd;
                        }
                    if ( nxt () == ']' ) ++esi;
                    else asmerr (16); // 'Syntax error'
                    break;
                    }

					case ADC:
					case AND:
					case BIC:
					case EOR:
                    case ORR:
                    case ROR:
                    case SBC:
                    {
                    // <opcode> rd, rd, rn
                    int rd = reg8 ();
                    comma ();
                    if ( reg8 () != rd ) asmerr (103);  // 'Invalid register'
                    comma ();
                    instruction = opcodes[mnemonic] | ( reg8 () << 3 ) | rd;
                    break;
                    }
                    
					case CMN:
                    case MVN:
                    case REV:
                    case REV16:
                    case REVSH:
                    case SXTB:
                    case SXTH:
                    case TST:
                    case UXTB:
                    case UXTH:
                        // <opcode> Rd, Rn
                        instruction = opcodes[mnemonic] | reg8 ();
                        comma ();
                        instruction |= reg8 () << 3;
                        break;

					case NOP:
                    case SEV:
                    case WFE:
                    case WFI:
                    case YIELD:
						instruction = opcodes[mnemonic];
						break;

                    case BLX:
                    case BX:
                        // <opcode> <reg>
                        instruction = opcodes[mnemonic] | ( reg () << 3 );
                        break;

                    case DMB:
                    case DSB:
                    case ISB:
                        // <opcode32>
                        nxt ();
                        if ( !strnicmp ((const char *)esi, "sy", 2) ) esi += 2;
                        instruction = 0xF3BF;
                        poke (&instruction, 2);
                        instruction = opcodes[mnemonic];
                        break;

                    case LDRSB:
                    case LDRSH:
                        // <opcode> <reg8>, [<reg8>, <reg8>]
                        instruction = opcodes[mnemonic] | reg8 ();
                        comma ();
                        if ( nxt () == '[' ) ++esi;
                        else asmerr (16); // 'Syntax error'
                        instruction |= reg8 () << 3;
                        comma ();
                        instruction |= reg8 () << 6;
                        if ( nxt () == ']' ) ++esi;
                        else asmerr (16); // 'Syntax error'
                        break;

                    case ADD:
                        if ( status () )
                            {
                            int rd = reg8 ();
                            comma ();
                            int rn = reg8 ();
                            comma ();
                            int bImm;
                            int offreg = offset (&bImm);
                            if ( bImm )
                                {
                                if ( rd == rn )
                                    {
                                    if ( offreg >= 0 )
                                        {
                                        instruction = 0x3000 | ( rd << 8 ) | ( offreg & 0xFF );
                                        }
                                    else
                                        {
                                        offreg = -offreg;
                                        instruction = 0x3800 | ( rd << 8 ) | ( offreg & 0xFF );
                                        }
                                    if (offreg > 255) asmerr (2); // 'Bad immediate constant'
                                    }
                                else
                                    {
                                    if ( offreg >= 0 )
                                        {
                                        instruction = 0x1C00 | (( offreg & 0x07 ) << 6 ) | ( rn << 3 ) | rd;
                                        }
                                    else
                                        {
                                        offreg = -offreg;
                                        instruction = 0x1E00 | (( offreg & 0x07 ) << 6 ) | ( rn << 3 ) | rd;
                                        }
                                    if (offreg > 7) asmerr (2); // 'Bad immediate constant'
                                    }
                                }
                            else
                                {
                                if ( offreg > 7 ) asmerr (104); // 'Low register required'
                                instruction = 0x1800 | ( ( offreg & 0x07 ) << 6 ) | ( rn << 3 ) | rd;
                                }
                            }
                        else
                            {
                            int rd = reg ();
                            comma ();
                            int rn = reg ();
                            comma ();
                            int bImm;
                            int offreg = offset (&bImm);
                            if ( bImm )
                                {
                                if ( rn == 13 )
                                    {
                                    if ( rd == 13 )
                                        {
                                        if ( offreg >= 0 )
                                            {
                                            instruction = 0xB000 | (( offreg >> 2 ) & 0x7F );
                                            }
                                        else
                                            {
                                            offreg = - offreg;
                                            instruction = 0xB080 | (( offreg >> 2 ) & 0x7F );
                                            }
                                        if ( offreg > 508 ) asmerr (2); // 'Bad offregediate constant'
                                        else if ( offreg & 0x03 ) asmerr (105);   // 'Invalid alignment'
                                        }
                                    else
                                        {
                                        if ( rd > 7 ) asmerr (104); //'Low register required'
                                        if (( offreg < 0 ) || ( offreg > 1020 ))
                                            asmerr (2); // 'Bad offregediate constant'
                                        else if ( offreg & 0x03 )
                                            asmerr (105);   // 'Invalid alignment'
                                        instruction = 0xA800 | (( rd & 0x07 ) << 8 )
                                            | (( offreg >> 2 ) & 0xFF );
                                        }
                                    }
                                else if ( rn == 15 )
                                    {
                                    if ( rd > 7 ) asmerr (104); //'Low register required'
                                    if (( offreg < 0 ) || ( offreg > 1020 ))
                                        asmerr (2); // 'Bad offregediate constant'
                                    else if ( offreg & 0x03 )
                                        asmerr (105);   // 'Invalid alignment'
                                    instruction = 0xA000 | (( rd & 0x07 ) << 8 )
                                        | (( offreg >> 2 ) & 0xFF );
                                    }
                                else
                                    {
                                    asmerr (103);   // 'Invalid register'
                                    }
                                }
                            else
                                {
                                if ( rn != rd ) asmerr (103);   // 'Invalid register'
                                instruction = 0x4400 | (( rd & 0x08 ) << 4 ) | ( offreg << 3 )
                                    | ( rd & 0x07 );
                                }
                            }
                        break;

                    case SUB:
                        if ( status () )
                            {
                            int rd = reg8 ();
                            comma ();
                            int rn = reg8 ();
                            comma ();
                            int bImm;
                            int offreg = offset (&bImm);
                            if ( bImm )
                                {
                                if ( rd == rn )
                                    {
                                    if ( offreg >= 0 )
                                        {
                                        instruction = 0x3800 | ( rd << 8 ) | ( offreg & 0xFF );
                                        }
                                    else
                                        {
                                        offreg = - offreg;
                                        instruction = 0x3000 | ( rd << 8 ) | ( offreg & 0xFF );
                                        }
                                    if ( offreg > 255 ) asmerr (2); // 'Bad immediate constant'
                                    }
                                else
                                    {
                                    if ( offreg >= 0 )
                                        {
                                        instruction = 0x1E00 | (( offreg & 0x07 ) << 6 ) | ( rn << 3 ) | rd;
                                        }
                                    else
                                        {
                                        offreg = - offreg;
                                        instruction = 0x1C00 | (( offreg & 0x07 ) << 6 ) | ( rn << 3 ) | rd;
                                        }
                                    if ( offreg > 7 ) asmerr (2); // 'Bad immediate constant'
                                    }
                                }
                            else
                                {
                                if ( offreg > 7 ) asmerr (104); // 'Low register required'
                                instruction = 0x1A00 | (( offreg & 0x07 ) << 6 ) | ( rn << 3 ) | rd;
                                }
                            }
                        else
                            {
                            int rd = reg ();
                            comma ();
                            int rn = reg ();
                            comma ();
                            int bImm;
                            int offreg = offset (&bImm);
                            if (( rd != 13 ) || ( rn != 13 )) asmerr (103); // 'Invalid register'
                            if ( offreg >= 0 )
                                {
                                instruction = 0xB080 | (( offreg >> 2 ) & 0x7F );
                                }
                            else
                                {
                                offreg = - offreg;
                                instruction = 0xB000 | (( offreg >> 2 ) & 0x7F );
                                }
                            if ( offreg > 508 ) asmerr (2); // 'Bad offregediate constant'
                            else if ( offreg & 0x03 ) asmerr (105);   // 'Invalid alignment'
                            }
                        break;

					case ADR:
                    {
                    // ADR <reg8>, <label>
                    int offpc;
                    instruction = 0xA000 | reg8 () << 8;
                    comma ();
                    offpc = expri () - (((uint32_t)PC + 4) & 0xFFFFFFFC);
                    if (( offpc > 0x3FC ) || ( offpc < 0 ))
                        asmerr (8); // 'Address out of range'
                    else if (offpc & 0x03)
                        asmerr (105);   // 'Invalid allignment'
                    if (offpc >= 0)
                        instruction |= (offpc >> 2) & 0xFF;
                    break;
                    }

					case B:
                    {
                    condition = lookup (conditions, 
                        sizeof(conditions) / sizeof(conditions[0]));

                    if (condition == -1)
                        {
                        if (( *esi == 'l' ) || ( *esi == 'L' ))
                            {
                            // BL <label>
                            ++esi;
                            int dest = (void *) (size_t) expri () - PC - 4;
                            if ( dest & 0x01 ) asmerr (105);    // 'Invalid alignment'
                            dest >>= 1;
                            if (( dest > 0x00FFFFFF ) || ( dest <= (int) 0xFF000000 ))
                                asmerr (1); // 'Jump out of range'
                            int instlow = 0xF000 | (( dest >> 12 ) & 0x3FF );
                            instruction = 0xD000 | (( dest & 0x1000) << 1 ) | ( dest & 0xFFF );
                            if ( dest < 0 ) instlow ^= 0x400;
                            else instruction ^= 0x2800;
                            poke (&instlow, 2);
                            break;
                            }
                        condition = 0;
                        }
                    
                    int dest = (void *) (size_t) expri () - PC - 4;
                    if ( dest & 0x01 ) asmerr (105);    // 'Invalid alignment'
                    dest >>= 1;
                    if ( condition == 0 )
                        {
                        // B <label>
                        // BAL <label>
                        if (( dest < -1024 ) || ( dest >= 1024 )) asmerr (1); // 'Jump out of range'
                        instruction = 0xE000 | ( dest & 0x7FF );
                        }
                    else
                        {
                        // B<cond> <label>
                        if (( dest < -128 ) || ( dest >= 128 )) asmerr (1); // 'Jump out of range'
                        instruction = 0xD000 | ( ccodes[condition] << 8 ) | ( dest & 0xFF );
                        }
                    break;
                    }

                    case BKPT:
                        // BKPT <data>
                        if ( nxt () == '#' ) ++esi;
                        instruction = 0xBE00 | ( expri () & 0xFF );
                        break;

                    case CPSID:
                    case CPSIE:
                        nxt ();
                        if (( *esi == 'i' ) || ( *esi == 'I' )) ++esi;
                        else asmerr (16); // 'Syntax error' 
                        instruction = opcodes[mnemonic];
                        break;

					case LDM:
                    {
                    // LDM <reg8>!, <reg8 list>
                    // LDM <reg8>, <reg8 list>
                    if ( ! strnicmp ((const char *)esi, "ia", 2) ) esi += 2;
                    else if ( ! strnicmp ((const char *)esi, "fd", 2) ) esi += 2;
                    int rn = reg8 ();
                    instruction = 0xC800 | ( rn << 8 );
                    int bWB = 0;
                    if ( nxt () == '!' )
                        {
                        bWB = 1;
                        ++esi;
                        }
                    comma ();
                    int rl = reglist ();
                    instruction |= ( rl & 0xFF );
                    int bIL = ( instruction & ( 1 << rn ) ) > 0 ? 1 : 0;
                    if ( rl & 0xFF00 ) asmerr (107);    // 'Invalid register list'
                    else if ( bIL == bWB ) asmerr (106); // 'Register / list conflict'
                    break;
                    }

					case LDR:
                    {
                    // LDR <reg8>, [<reg8>, #<imm5>]
                    // LDR <reg8>, [SP, #<imm8>]
                    // LDR <reg8>, [PC, #<imm8>]
                    // LDR <reg8>, <label>
                    // LDR <reg8>, [<reg8>, <reg8>]
                    int rd = reg8 ();
                    comma ();
                    if ( nxt () == '[' )
                        {
                        ++esi;
                        int rn = reg ();
                        int bImm;
                        int	offreg = offset2 (&bImm);
                        if ( bImm )
                            {
                            if ( rn < 8 )
                                {
                                if ((offreg < 0) || (offreg > 124))
                                    asmerr (2); // 'Bad immediate constant'
                                else if (offreg & 0x03)
                                    asmerr (105);   // 'Invalid allignment'
                                instruction = 0x6800 | (( offreg & 0x7C ) << 4 ) | ( rn << 3 ) | rd;
                                }
                            else if (( rn == 13 ) || ( rn == 15 ))
                                {
                                if ((offreg < 0) || (offreg > 1020))
                                    asmerr (2); // 'Bad immediate constant'
                                else if (offreg & 0x03)
                                    asmerr (105);   // 'Invalid alignment'
                                instruction = ( rn == 15 ) ? 0x4800 : 0x9800;
                                instruction |= ( rd << 8 ) | (( offreg >> 2 ) & 0xFF );
                                }
                            else
                                {
                                asmerr (103); // 'Invalid register'
                                }
                            }
                        else
                            {
                            if (( rn > 7 ) || ( offreg > 7 )) asmerr (104); // 'Low register required'
                            instruction = 0x5800 | (( offreg & 0x07 ) << 6 ) | (( rn & 0x07 ) << 3 ) | rd;
                            }
                        if ( nxt () == ']' ) ++esi;
                        else asmerr (16); // 'Syntax error'
                        }
                    else
                        {
                        int offpc = expri () - (((uint32_t)PC + 4) & 0xFFFFFFFC);
                        if ((offpc < 0) || (offpc > 1020))
                            asmerr (8); // 'Address out of range'
                        else if (offpc & 0x03)
                            asmerr (105);   // 'Invalid allignment'
                        instruction = 0x4800 | ( rd << 8 ) | (( offpc >> 2 ) & 0xFF );
                        }
                    break;
                    }

                    case MRS:
                    {
                    instruction = 0x8000 | ( reg () << 8 );
                    comma ();
                    nxt ();
                    int sys = lookup (sysm, sizeof (sysm) / sizeof (sysm[0]));
                    if ( sys < 0 ) asmerr (109);    // 'Invalid special register'
                    if ( sys >= 4 ) ++sys;
                    if ( sys == 10 ) sys = 16;
                    else if ( sys == 11 ) sys = 20;
                    instruction |= sys;
                    uint16_t instlow = 0xF3EF;
                    poke (&instlow, 2);
                    break;
                    }

                    case MSR:
                    {
                    nxt ();
                    int sys = lookup (sysm, sizeof (sysm) / sizeof (sysm[0]));
                    if ( sys < 0 ) asmerr (109);    // 'Invalid special register'
                    if ( sys >= 4 ) ++sys;
                    if ( sys == 10 ) sys = 16;
                    else if ( sys == 11 ) sys = 20;
                    comma ();
                    instruction = 0xF380 | reg ();
                    poke (&instruction, 2);
                    instruction = 0x8800 | sys;
                    break;
                    }

                    case MUL:
                    {
                    // MULS rd, rn, rd
                    status ();
                    int rd = reg8 ();
                    comma ();
                    int rn = reg8 ();
                    comma ();
                    if ( reg8 () != rd ) asmerr (103); // 'Invalid register'
                    instruction = 0x4340 | ( rn << 3 ) | rd;
                    break;
                    }

                    case POP:
                    {
                    int rl = reglist ();
                    if ( rl & 0x7F00 ) asmerr (107); // 'Invalid register list'
                    instruction = 0xBC00 | ( rl & 0xFF );
                    if ( rl & 0x8000 ) instruction |= 0x100;
                    break;
                    }

                    case PUSH:
                    {
                    int rl = reglist ();
                    if ( rl & 0xBF00 ) asmerr (107); // 'Invalid register list'
                    instruction = 0xB400 | ( rl & 0xFF );
                    if ( rl & 0x4000 ) instruction |= 0x100;
                    break;
                    }

                    case RSB:
                        status ();
                        instruction = 0x4240 | reg8 ();
                        comma ();
                        instruction |= ( reg8 () << 3 );
                        comma ();
                        if ( nxt () == '#' )
                            {
                            ++esi;
                            int imm = expri ();
                            if ( imm != 0 ) asmerr (2); // 'Bad immediate constant'
                            }
                        else
                            {
                            asmerr (16); // 'Syntax error'
                            }
                        break;
                    
					case STM:
                    {
                    // STM <reg8>!, <reg8 list>
                    if ( ! strnicmp ((const char *)esi, "ia", 2) ) esi += 2;
                    else if ( ! strnicmp ((const char *)esi, "ei", 2) ) esi += 2;
                    int rn = reg8 ();
                    instruction = 0xC000 | ( rn << 8 );
                    if ( nxt () == '!' ) ++esi;
                    else asmerr (16); // 'Syntax error'
                    comma ();
                    int rl = reglist ();
                    if ( rl & 0xFF00 ) asmerr (107); // 'Invalid register list'
                    instruction |= rl;
                    break;
                    }

					case STr:
                    {
                    // LDR <reg8>, [<reg8>, #<imm5>]
                    // LDR <reg8>, [SP, #<imm8>]
                    // LDR <reg8>, [<reg8>, <reg8>]
                    int rd = reg8 ();
                    comma ();
                    if ( nxt () == '[' )
                        {
                        ++esi;
                        int rn = reg ();
                        int bImm;
						int	offreg = offset2 (&bImm);
                        if ( bImm )
                            {
                            if ( rn < 8 )
                                {
                                if ((offreg < 0) || (offreg > 124))
                                    asmerr (2); // 'Bad immediate constant'
                                else if (offreg & 0x03)
                                    asmerr (105);   // 'Invalid alignment'
                                instruction = 0x6000 | (( offreg & 0x7C ) << 4 ) | ( rn << 3 ) | rd;
                                }
                            else if ( rn == 13 )
                                {
                                if ((offreg < 0) || (offreg > 1020))
                                    asmerr (2); // 'Bad immediate constant'
                                else if (offreg & 0x03)
                                    asmerr (105);   // 'Invalid alignment'
                                instruction = 0x9000 | ( rd << 8 ) | (( offreg >> 2 ) & 0xFF );
                                }
                            else
                                {
                                asmerr (103); // Invalid register
                                }
                            }
                        else
                            {
                            if (( rn > 7 ) || ( offreg > 7 )) asmerr (104); // 'Low register required'
                            instruction = 0x5000 | (( offreg & 0x07 ) << 6 ) | (( rn & 0x07 ) << 3 ) | rd;
                            }
                        if ( nxt () == ']' ) ++esi;
                        else asmerr (16); // 'Syntax error'
                        }
                    else asmerr (16); // 'Syntax error'
                    break;
                    }

                    case SVC:
                    {
                    // SVC #<imm8>
                    if ( nxt () == '#' ) ++esi;
                    int imm = expri ();
                    if (( imm < 0 ) || ( imm > 0xFF ))
                        asmerr (2); // 'Bad immediate constant'
                    instruction = 0xDF00 | ( imm & 0xFF );
                    break;
                    }

                    default:
                        asmerr (101); // 'Invalid opcode'
                        while (! eol (*esi)) ++esi;
                    }

                poke (&instruction, 2);
                if (! eol (nxt ())) asmerr (102); // 'Too many parameters'
                while (! eol (*esi)) ++esi;
            }
        };
    }
