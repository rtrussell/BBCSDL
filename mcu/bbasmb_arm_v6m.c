/*****************************************************************\
 *       32-bit BBC BASIC Interpreter                              *
 *       (c) 2018-2021  R.T.Russell  http://www.rtrussell.co.uk/   *
 *                                                                 *
 *       bbasmb_arm_v6m.c Assembler for ARMv6m thunb instructions  *
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

enum {		CMP, MOV,                                                           // 3 opcodes
            ASR, LSL, LSR,                                                      // 2 Opcodes
            LDRB, STRB,                                                         // 2 opcodes
            LDRH, STRH,                                                         // 2 opcodes
            ADC, AND, BIC, CMN, EOR, MVN, ORR, REV16, REVSH, REV, ROR, SBC,
            SXTB, SXTH, TST, UXTB, UTXH,                                        // <reg8>, <reg8>
            NOP, SEV, WFE, WFI,                                                 // <opcode>
            BLX, BX,                                                            // <reg8>
            DMB, DSB, ISB,                                                      // 32-bit 0xF3BF____
            LDRSB, LDRSH,                                                       // <reg8>, [<reg8>, <reg8>]
            ADD, ADR, BKPT, BL, B, CPS, LDM, LDR, MRS, MSR, MUL,
            POP, PUSH, RSB, STM, STr, SUB, SVC,                                 // Unique opcodes
            ALIGN, DB, DCB, DCD, DCS, DCW, EQUB, EQUD, EQUQ, EQUS, EQUW, OPT    // Pseudo-ops
    };

static const char *mnemonics[] = {
    "cmp", "mov",
    "asr", "lsl", "lsr",
    "ldrb", "strb",
    "ldrh", "strh",
    "adc", "and", "bic", "cmn", "eor", "mvn", "orr", "rev16", "revsh", "rev", "ror", "sbc",
    "sxtb", "sxth", "tst", "uxtb", "utxh"
    "ldrb", "strb",
    "ldrh", "strh",
    "nop", "sev", "wfe", "wfi",
    "blx", "bx",
    "dmb", "dsb", "isb",
    "add", "adr", "bkpt", "bl", "b", "cps", "ldm", "ldr", "mrs", "msr", "mul",
    "pop", "push", "rsb", "stm", "str", "sub", "svc",
    "align", "db", "dcb", "dcd", "dcs", "dcw", "equb", "equd", "equq", "equs", "equw", "opt"};

static const uint16_t opcodes[] = {
    0x2100,     // CMP \ <reg8>, #<imm8>
    0x2000,     // MOV /
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
    0x4200,     // CMN   |
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
    0xBF00,     // NOP \ <opcode>
    0xBF40,     // SEV |
    0xBF20,     // WFE |
    0xBF30,     // WFI /
    0x4710,     // BLX \ <reg8>
    0x4700,     // BX  /
    0x8F5F,     // DMB \ 32-bit 0xF3BF____
    0x8F4F,     // DSB |
    0x8F6F,     // ISB /
    0x5600,     // LDRSB \ <reg8>, [<reg8>, <reg8>]
    0x5700      // LDRSH /
    };

static const uint16_t opcode2[] = {
    0x4280,     // CMP \ <reg8>, <reg8>
    0x0000,     // MOV /
    0x4100,     // ASR \ <reg8>, <reg8>
    0x4000,     // LSL |
    0x40C0,     // LSR /
    0x5C00,     // LDRB \ <reg8>, [<reg8>, <reg8>]
    0x5400,     // STRB /
    0x5A00,     // LDRH \ <reg8>, [<reg8>, <reg8>]
    0x5200,     // STRH /
    };

static const uint16_t opcode3[] = {
    0x4500,     // CMP \ <reg>, <reg>
    0x4600,     // MOV /
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

static unsigned char reg (void)
    {
	int i;
	nxt ();
	i = lookup (registers, sizeof(registers) / sizeof(registers[0]));
	if (i < 0)
	    {
		i = itemi();
		if ((i < 0) || (i > 15))
			error (16, NULL); // 'Syntax error'
		return i;
	    }
	return regno[i];
    }

static unsigned char reg8 (void)
    {
    int i = reg ();
    if ( i >= 8 ) error (16, NULL); // 'Syntax error'
    return i;
    }

static int reglist (void) 
    {
	int temp = 0;
	if (nxt () != '{')
		error (16, NULL); // 'Syntax error'
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
		error (16, NULL); // 'Syntax error'
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

static void *align (void)
    {
	while (stavar[16] & 3)
	    {
		stavar[16]++;
		if (liston & BIT6)
			stavar[15]++;
	    };
	return PC;
    }

void assemble (void)
    {
	signed char al;
	signed char *oldesi = esi;
	int init = 1;
	void *oldpc = PC;

	while (1)
	    {
		int mnemonic, condition, instruction = 0;
		unsigned char ccode;

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
						unsigned int i = *(unsigned int *)p;
#if (defined (_WIN32)) && (__GNUC__ < 9)
						sprintf (accs, "%08I64X ", (long long) (size_t) oldpc);
#else
						sprintf (accs, "%08llX ", (long long) (size_t) oldpc);
#endif
						switch (n)
						    {
							case 0: break;
							case 1:	i &= 0xFF;
							case 2: i &= 0xFFFF;
							case 3: i &= 0xFFFFFF;
							case 4: sprintf (accs + 9, "%0*X ", n*2, i);
								break;
							default: sprintf (accs + 9, "%08X ", i);
						    }
						if (n > 4)
						    {
							n -= 4;
							p += 4;
							oldpc += 4;
						    }
						else 
							n = 0;

						text (accs);

						if (*oldesi == '.')
						    {
							tabit (18);
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
				if (init)
					oldpc = align ();
                    {
                    VAR v;
                    unsigned char type;
                    void *ptr = getput (&type);
                    if (ptr == NULL)
                        error (16, NULL); // 'Syntax error'
                    if (type >= 128)
                        error (6, NULL); // 'Type mismatch'
                    if ((liston & BIT5) == 0)
                        {
                        v = loadn (ptr, type);
                        if (v.i.n)
                            error (3, NULL); // 'Multiple label'
                        }
                    v.i.t = 0;
                    v.i.n = (intptr_t) PC;
                    storen (v, ptr, type);
                    }
            break;

			default:
				esi--;
				mnemonic = lookup (mnemonics, sizeof(mnemonics)/sizeof(mnemonics[0]));

                if ( mnemonic == B )
                    {
                    condition = lookup (conditions, 
                        sizeof(conditions) / sizeof(conditions[0]));

                    if (condition == -1)
                        ccode = 0b1110;
                    else
                        ccode = ccodes[condition];
                    }

				if (mnemonic != OPT)
					init = 0;

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
                            error (19, NULL); // 'String too long'
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
                            error (51, NULL); // 'No such system call'
                        }
                    else if (v.i.t == 0)
                        n = v.i.n;
                    else
                        n = v.f;
                    if (mnemonic == EQUQ)
                        {
                        poke (&n, 8);
                        continue;
                        }
                    instruction = (int) n;
                    }
                    break;

					case DCS:
					case EQUS:
                    {
                    VAR v = exprs ();
                    if (v.s.l > 256)
                        error (19, NULL); // 'String too long'
                    poke (v.s.p + zero, v.s.l);
                    continue;
                    }

                    case ALIGN:
                        oldpc = align ();
                        if ((nxt() >= '1') && (*esi <= '9'))
                            {
                            int n = expri ();
                            if ((n & (n - 1)) || (n & 0xFFFFFF03) || (n == 0))
                                error (16, NULL); // 'Syntax error'
                            instruction = opcodes[NOP];
                            while (stavar[16] & (n - 1))
                                poke (&instruction, 2); 
                            }
                        continue;

                    // Thumb Instructions

					case CMP:
                    case MOV:
                    {
                    // <opcode> <reg8>, #<imm8>
                    // <opcode> <reg8>, <reg8>
                    // <opcode> <reg>, <reg>
                    int rn = reg ();
                    comma ();
                    if ( rn < 8 )
                        {
                        int bImm;
						int	offreg = offset (&bImm);
                        if ( bImm )
                            {
                            if (( offreg < 0 ) || ( offreg > 0xFF ))
                                error (2, NULL); // 'Bad immediate constant'
                            instruction = opcodes[mnemonic] | ( rn << 8 ) | ( offreg & 0xFF );
                            }
                        else if ( offreg < 8 )
                            {
                            instruction = opcode2[mnemonic] | ( offreg << 3 ) | rn;
                            }
                        else
                            {
                            instruction = opcode3[mnemonic] | ( offreg << 3 ) | rn;
                            }
                        }
                    else
                        {
                        instruction = opcode3[mnemonic] | 0x80 | ( reg () << 3 ) | ( rn & 0x07 );
                        }
                    break;
                    }

                    case ASR:
                    case LSL:
                    case LSR:
                    {
                    // <opcode> <reg8>,<reg8>,#<offset>
                    // <opcode> <reg8>,<reg8>,<reg8>
                    int rd = reg8 ();
                    comma ();
                    int rm = reg8 ();
                    if ( nxt () == ',' )
                        {
                        ++esi;
                        if ( nxt () != '#' ) error (2, NULL); // 'Bad immediate constant'
                        ++esi;
                        int shf = expri ();
                        if (( shf < 0 ) || ( shf > 31 )) error (2, NULL); // 'Bad immediate constant'
                        instruction = opcodes[mnemonic] | ( shf << 6 ) | ( rm << 3 ) | rd;
                        }
                    else
                        {
                        instruction = opcode2[mnemonic] | ( rm << 3 ) | rd;
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
                    else error (16, NULL); // 'Syntax error'
                    int rn = reg8 ();
                    int bImm;
                    int offreg = offset (&bImm);
                    if ( bImm )
                        {
                        if (( offreg < 0 ) || ( offreg > 31 ))
                            error (2, NULL); // 'Bad immediate constant'
                        instruction = opcodes[mnemonic] | (( offreg & 0x1F ) << 6 ) | ( rn << 3 ) | rd;
                        }
                    else
                        {
                        if ( offreg > 7 ) error (4, NULL); // Mistake
                        instruction = opcode2[mnemonic] | (( offreg & 0x07 ) << 6 ) | ( rn << 3 ) | rd;
                        }
                    }

                    case LDRH:
                    case STRH:
                    {
                    // LDRH <reg8>, [<reg8>, #<imm5:1>]
                    // LDRH <reg8>, [<reg8>, <reg8>]
                    int rd = reg8 ();
                    comma ();
                    if ( nxt () == '[' ) ++esi;
                    else error (16, NULL); // 'Syntax error'
                    int rn = reg8 ();
                    int bImm;
                    int offreg = offset (&bImm);
                    if ( bImm )
                        {
                        if (( offreg < 0 ) || ( offreg > 62 ) || ( offreg & 0x01 ))
                            error (2, NULL); // 'Bad immediate constant'
                        instruction = opcodes[mnemonic] | (( offreg & 0x3E ) << 5 ) | ( rn << 3 ) | rd;
                        }
                    else
                        {
                        if ( offreg > 7 ) error (4, NULL); // Mistake
                        instruction = opcode2[mnemonic] | (( offreg & 0x07 ) << 6 ) | ( rn << 3 ) | rd;
                        }
                    }
                    
					case ADC:
					case AND:
					case BIC:
					case CMN:
					case EOR:
                    case MVN:
                    case ORR:
                    case REV:
                    case REV16:
                    case REVSH:
                    case ROR:
                    case SBC:
                    case SXTB:
                    case SXTH:
                    case TST:
                    case UXTB:
                    case UTXH:
                        // <opcode> <reg8>, <reg8>
                        instruction = opcodes[mnemonic] | reg8 ();
						comma ();
						instruction |= reg8 () << 3;
                        break;

					case NOP:
                    case SEV:
                    case WFE:
                    case WFI:
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
                        poke (&opcodes[mnemonic], 2);
                        instruction = 0xF3BF;
                        break;

                    case LDRSB:
                    case LDRSH:
                        // <opcode> <reg8>, [<reg8>, <reg8>]
                        instruction = opcodes[mnemonic] | reg8 ();
                        comma ();
                        if ( nxt () == '[' ) ++esi;
                        else error (16, NULL); // 'Syntax error'
                        instruction |= reg8 () << 3;
                        comma ();
                        instruction |= reg8 () << 6;
                        break;

                    case ADD:
                    {
                    // ADD <reg8>, #<offset>
                    // ADD <reg8>, <reg8>, #<offset>
                    // ADD <reg8>, SP, #<offset>
                    // ADD <reg8>, PC, #<offset>
                    // ADD <reg8>, <reg8>, <reg8>
                    // ADD <reg8>, SP, <reg8>
                    // ADD <reg>,<reg>
                    int rd = reg ();
                    if ( rd < 8 )
                        {
                        int bImm;
						comma ();
						int	offreg = offset (&bImm);
                        if ( bImm )
                            {
                            // Add rd, #n
                            if ((offreg < 0) || (offreg > 255)) error (2, NULL); // 'Bad immediate constant'
                            instruction = 0x3000 | ( rd << 8 ) | ( offreg & 0xFF );
                            }
                        else
                            {
                            int rn = offreg;
                            if ( nxt () == ',' )
                                {
                                ++esi;
                                offreg = offset (&bImm);
                                if ( bImm )
                                    {
                                    // ADD rd, rn, #n
                                    if ( rn < 8 )
                                        {
                                        if ((offreg < 0) || (offreg > 7))
                                            error (2, NULL); // 'Bad immediate constant'
                                        instruction = 0x1C00 | (( offreg & 0x07 ) << 6 ) | ( rn << 3 ) | rd;
                                        }
                                    else if ( rn == 13 )
                                        {
                                        if ((offreg < 0) || (offreg > 1020) || (offreg & 0x03))
                                            error (2, NULL); // 'Bad immediate constant'
                                        instruction = 0xA100 | ((offreg >> 2) & 0xFF);
                                        }
                                    else error (4, NULL); // Mistake
                                    }
                                // ADD rd, rn, rm
                                else if ( rn < 8 )
                                    {
                                    if ( offreg > 7 ) error (4, NULL); // Mistake
                                    instruction = 0x1800 | ( ( offreg & 0x07 ) << 6 ) | ( rn << 3 ) | rd;
                                    }
                                else if ( rn == 13 )
                                    {
                                    if ( offreg != rd ) error (4, NULL); // Mistake
                                    instruction = 0x4468 | rd;
                                    }
                                else
                                    {
                                    error (4, NULL); // Mistake
                                    }
                                }
                            else
                                {
                                // ADD rd, rn
                                instruction = 0x4400 | ( rn << 3 ) | rd;
                                }
                            }
                        }
                    else
                        {
                        int bImm;
						comma ();
						int	offreg = offset (&bImm);
                        if ( bImm )
                            {
                            // ADD SP, #n
                            if ( rd != 13 ) error (4, NULL); // Mistake
                            if ((offreg < 0) || (offreg > 508) || (offreg & 0x03))
                                error (2, NULL); // 'Bad immediate constant'
                            instruction = 0xB000 | (( offreg >> 2 ) & 0x7F );
                            }
                        else if (( rd == 13 ) && ( offreg == 13 ) && ( nxt () == ',' ))
                            {
                            // ADD SP, SP, #n
                            ++esi;
                            if ( nxt () != '#' ) error (2, NULL); // 'Bad immediate constant'
                            int imm = expri ();
                            if (( imm < 0 ) || ( imm > 508 ) || ( imm & 0x03 ))
                                error (2, NULL); // 'Bad immediate constant'
                            instruction = 0xB000 | (( imm >> 2 ) & 0x7F );
                            }
                        else
                            {
                            // ADD rd, rn
                            instruction = 0x4400 | (( rd & 0x08 ) << 4 ) | ( offreg << 3 ) | ( rd & 0x07 );
                            }
                        }
                    break;
                    }

					case ADR:
                    {
                    // ADR <reg8>, <label>
                    int offpc;
                    instruction = opcodes[mnemonic] | reg8 () << 8;
                    comma ();
                    offpc = expri () - (((uint32_t)PC + 4) & 0xFFFFFFFC);
                    if (offpc >= 0)
                        instruction |= offpc >> 2;
                    if (( offpc & 0x03) || ( offpc < 0 ))
                        {
                        error (8, NULL); // 'Address out of range'
                        }
                    break;
                    }

					case B:
                    {
                    // B <label>
                    // B<cond> <label>
                    int dest = ((void *) (size_t) expri () - PC - 4) >> 1;
                    if ( ccode == 0b1110 )
                        {
                        if (( dest < -1024 ) || ( dest >= 1024 )) error (1, NULL); // 'Jump out of range'
                        instruction = 0xE000 | ( dest & 0x7FF );
                        }
                    else
                        {
                        if (( dest < -128 ) || ( dest >= 128 )) error (1, NULL); // 'Jump out of range'
                        instruction = 0xD000 | ( ccode << 8 ) | ( dest & 0xFF );
                        }
                    break;
                    }

                    case BKPT:
                        // BKPT <data>
                        instruction = instruction = opcodes[mnemonic] | ( expri () & 0xFF );
                        break;
                        
					case BL:
                    {
                    // BL <label>
                    int dest = ((void *) (size_t) expri () - PC - 4) >> 1;
                    if (( dest > 0x00FFFFFF ) || ( dest <= (int) 0xFF000000 ))
                        error (1, NULL); // 'Jump out of range'
                    instruction = 0xF000 | (( dest >> 12 ) & 0x3FF );
                    int instlow = 0xB000 | (( dest & 0x1000) << 1 ) | ( dest & 0xFFF );
                    if ( dest < 0 )
                        {
                        instruction ^= 0x400;
                        instlow ^= 0x2800;
                        }
                    poke (&instlow, 2);
                    break;
                    }

                    case CPS:
                        instruction = 0xB662 | (( expri () & 0x01 ) << 4 );
                        break;

					case LDM:
                    {
                    // LDM <reg8>!, <reg8 list>
                    // LDM <reg8>, <reg8 list>
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
                    if (( rl & 0xFF00 ) || ( bIL == bWB )) error (4, NULL); // Mistake
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
                        comma ();
                        int bImm;
						int	offreg = offset (&bImm);
                        if ( bImm )
                            {
                            if ( rn < 8 )
                                {
                                if ((offreg < 0) || (offreg > 124) || (offreg & 0x03))
                                    error (2, NULL); // 'Bad immediate constant'
                                instruction = 0x6800 | (( offreg & 0x7C ) << 4 ) | ( rn << 3 ) | rd;
                                }
                            else if (( rn == 13 ) || ( rn == 15 ))
                                {
                                if ((offreg < 0) || (offreg > 1020) || (offreg & 0x03))
                                    error (2, NULL); // 'Bad immediate constant'
                                instruction = ( rn == 15 ) ? 0x4800 : 0x9800;
                                instruction |= ( rd << 8 ) | (( offreg >> 2 ) & 0xFF );
                                }
                            else
                                {
                                error (4, NULL); // Mistake
                                instruction = opcodes[NOP];
                                }
                            }
                        else
                            {
                            if (( rn > 7 ) || ( offreg > 7 )) error (4, NULL); // Mistake
                            instruction = 0x5800 | (( offreg & 0x07 ) << 6 ) | (( rn & 0x07 ) << 3 ) | rd;
                            }
                        }
                    else
                        {
                        int offpc = expri () - (((uint32_t)PC + 4) & 0xFFFFFFFC);
                        if ((offpc < 0) || (offpc > 1020) || (offpc & 0x03))
                            error (8, NULL); // 'Address out of range'
                        instruction = 0x4800 | ( rd << 8 ) | (( offpc >> 2 ) & 0xFF );
                        }
                    break;
                    }

                    case MRS:
                    {
                    instruction = 0x1000 | ( reg () << 8 );
                    comma ();
                    int sys = lookup (sysm, sizeof (sysm) / sizeof (sysm[0]));
                    if ( sys == 10 ) sys = 16;
                    else if ( sys == 11 ) sys = 20;
                    instruction |= sys;
                    poke (&instruction, 2);
                    instruction = 0xF3EF;
                    break;
                    }

                    case MSR:
                    {
                    int sys = lookup (sysm, 12);
                    if ( sys == 10 ) sys = 16;
                    else if ( sys == 11 ) sys = 20;
                    instruction = 0x8800 | sys;
                    comma ();
                    instruction = 0xF380 | reg ();
                    break;
                    }

                    case MUL:
                    {
                    int rd = reg8 ();
                    comma ();
                    int rn = reg8 ();
                    comma ();
                    int rm = reg8 ();
                    if ( rm != rd ) error (4, NULL); // Mistake
                    instruction = 0x4340 | ( rn << 3 ) | rd;
                    break;
                    }

                    case POP:
                    {
                    int rl = reglist ();
                    if ( rl & 0x7F00 ) error (4, NULL); // Mistake
                    instruction = 0xBC00 | ( rl & 0xFF );
                    if ( rl & 0x8000 ) instruction |= 0x100;
                    break;
                    }

                    case PUSH:
                    {
                    int rl = reglist ();
                    if ( rl & 0xBF00 ) error (4, NULL); // Mistake
                    instruction = 0xB400 | ( rl & 0xFF );
                    if ( rl & 0x4000 ) instruction |= 0x100;
                    break;
                    }

                    case RSB:
                        instruction = 0x4240 | reg8 ();
                        comma ();
                        instruction |= ( reg8 () << 3 );
                        comma ();
                        if ( nxt () == '#' )
                            {
                            int imm = expri ();
                            if ( imm != 0 ) error (2, NULL); // 'Bad immediate constant'
                            }
                        else
                            {
                            error (16, NULL); // 'Syntax error'
                            }
                        break;
                    
					case STM:
                    {
                    // STM <reg8>!, <reg8 list>
                    int rn = reg8 ();
                    instruction = 0xC000 | ( rn << 8 );
                    if ( nxt () != '!' ) error (16, NULL); // 'Syntax error'
                    comma ();
                    int rl = reglist ();
                    if ( rl & 0xFF00 ) error (4, NULL); // Mistake
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
                        comma ();
                        int bImm;
						int	offreg = offset (&bImm);
                        if ( bImm )
                            {
                            if ( rn < 8 )
                                {
                                if ((offreg < 0) || (offreg > 124) || (offreg & 0x03))
                                    error (2, NULL); // 'Bad immediate constant'
                                instruction = 0x6000 | (( offreg & 0x7C ) << 4 ) | ( rn << 3 ) | rd;
                                }
                            else if ( rn == 13 )
                                {
                                if ((offreg < 0) || (offreg > 1020) || (offreg & 0x03))
                                    error (2, NULL); // 'Bad immediate constant'
                                instruction = 0x9000 | ( rd << 8 ) | (( offreg >> 2 ) & 0xFF );
                                }
                            else
                                {
                                error (4, NULL); // Mistake
                                instruction = opcodes[NOP];
                                }
                            }
                        else
                            {
                            if (( rn > 7 ) || ( offreg > 7 )) error (4, NULL); // Mistake
                            instruction = 0x5000 | (( offreg & 0x07 ) << 6 ) | (( rn & 0x07 ) << 3 ) | rd;
                            }
                        }
                    else error (16, NULL); // 'Syntax error'
                    break;
                    }

                    case SUB:
                    {
                    // SUB <reg8>, <reg8>, #<imm3>
                    // SUB <reg8>, #<imm8>
                    // SUB <reg8>, <reg8>, <reg8>
                    // SUB SP, SP, #<imm7>
                    int rd = reg ();
                    comma ();
                    if ( rd < 8 )
                        {
                        int bImm;
                        int offreg = offset (&bImm);
                        if ( bImm )
                            {
                            if (( offreg < 0 ) || ( offreg > 0xFF ))
                                error (2, NULL); // 'Bad immediate constant'
                            instruction = 0x3800 | ( rd << 8 ) | ( offreg & 0xFF );
                            }
                        else
                            {
                            int rn = offreg;
                            if ( rn > 7 ) error (4, NULL); // Mistake
                            rn &= 0x07;
                            comma ();
                            offreg = offset (&bImm);
                            if ( bImm )
                                {
                                if (( offreg < 0 ) || ( offreg > 7 ))
                                    error (2, NULL); // 'Bad immediate constant'
                                instruction = 0x1E00 | (( offreg & 0x07 ) << 6 ) | ( rn << 3 ) | rd;
                                }
                            else
                                {
                                if ( offreg > 7 ) error (4, NULL); // Mistake
                                instruction = 0x1A00 | (( offreg & 0x07 ) << 6 ) | ( rn << 3 ) | rd;
                                }
                            }
                        }
                    else if ( rd == 13 )
                        {
                        int rn = reg ();
                        if ( rn != 13 ) error (4, NULL); // Mistake
                        comma ();
                        if ( nxt () == '#' ) ++esi;
                        else error (16, NULL); // 'Syntax error'
                        int imm = expri ();
                        if (( imm < 0 ) || ( imm > 0x1FF ) || ( imm & 0x03 ))
                            error (2, NULL); // 'Bad immediate constant'
                        instruction = 0xB010 | (( imm >> 2 ) & 0x7F );
                        }
                    else
                        {
                        error (4, NULL); // Mistake
                        }
                    break;
                    }

                    case SVC:
                    {
                    // SVC #<imm8>
                    if ( nxt () == '#' ) ++esi;
                    else error (16, NULL); // 'Syntax error'
                    int imm = expri ();
                    if (( imm < 0 ) || ( imm > 0xFF ))
                        error (2, NULL); // 'Bad immediate constant'
                    instruction = 0xBF00 | ( imm & 0xFF );
                    break;
                    }

                    default:
                        error (16, NULL); // 'Syntax error'
                    }

                oldpc = align ();

                poke (&instruction, 2);
            }
        };
    }
