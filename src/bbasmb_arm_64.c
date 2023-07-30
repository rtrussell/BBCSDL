/*****************************************************************\
*       32-bit BBC BASIC Interpreter                              *
*       (c) 2018-2020  R.T.Russell  http://www.rtrussell.co.uk/   *
*       (c) 2021       Simon Willcocks simon.willcocks@gmx.de     *
*                                                                 *
*       bbasmb_arm_64.c: Simple ARM 4 assembler                   *
*       Version 0.01, 27 May 2021                                 *
*       Version 0.02, 12 Jul 2021                                 *
*       Version 0.03, 08 Nov 2021                                 *
*       Version 0.04, 06 Dec 2022                                 *
\*****************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include "BBC.h"

#ifdef __APPLE__
#include <libkern/OSCacheControl.h>
#endif

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

// Like comma:
static void hash (void)
{
    if ('#' != nxt ())
        error (16, "Missing #") ;
    esi++ ;
}

static void open_square (void)
{
    if ('[' != nxt ())
        error (16, "Missing [") ;
    esi++ ;
}

// Register code: bits 0-4: Number 0-31, bits 5,6: size 32 or 64 bits, bit 7: set if ZR

#define REGISTER_IS_ZERO        0x80

static inline int reg_size( unsigned reg )
{
    return reg & 0x60;
}

// Registers can be 64-bit Xnn, XZR, SP, LR, or 32-bit Wnn, WZR, 
static unsigned char reg (void)
{
    nxt () ;

    if (strnicmp ((const char *)esi, "LR", 2) == 0) {
        esi += 2;
        return 30 | 64;
    }
    if (strnicmp ((const char *)esi, "SP", 2) == 0) {
        esi += 2;
        return 31 | 64;
    }
    if (strnicmp ((const char *)esi, "WSP", 3) == 0) {
        esi += 3;
        return 31 | 32;
    }
    if (strnicmp ((const char *)esi, "XZR", 3) == 0) {
        esi += 3;
        return 31 | REGISTER_IS_ZERO | 64;
    }
    if (strnicmp ((const char *)esi, "WZR", 3) == 0) {
        esi += 3;
        return 31 | REGISTER_IS_ZERO | 32;
    }

    unsigned result = (esi[0] == 'x' || esi[0] == 'X') ? 64:32;

    if (esi[0] == 'x' || esi[0] == 'X'
     || esi[0] == 'w' || esi[0] == 'W')
    {
        unsigned char digit = esi[1] - '0';
        unsigned register_number = digit;

        if (digit < 10)
            {
            digit = esi[2] - '0';

            if (digit < 10)
                {
                register_number = register_number * 10 + digit;
                esi += 3;
                }
            else
                esi += 2;
            }

        if (register_number <= 30) // Number 31 is either SP or ZR
            {
                return result | register_number;
            }
    }

    error ( 16, "Bad register" );

    return 0;
}

static inline void optional_zero_offset (void)
{
    if (',' == nxt())
        {
        comma () ;
        hash () ;
        int imm = expri () ;
        if (imm != 0)
            error( 16, "If an offset is present, it must be 0" ) ;
        }
}

static inline void close_square (void)
{
    if (']' != nxt ())
        error (16, "Missing ]") ;
    esi++ ;
}

static inline void assembler_error (void)
{
    error( 16, "Something went wrong in the assembler, sorry" ) ;
}

static int is_sp( unsigned r )
{
    return ((r & 0x1f) == 31) && 0 == (r & REGISTER_IS_ZERO);
}

static int is_zero( unsigned r )
{
    return ((r & 0x1f) == 31) && 0 != (r & REGISTER_IS_ZERO);
}

// Mnemonics in here from the ARM documentation in UPPER case, assembler instructions in lower case
// They are listed in alphabetical order, which lookup relies on to avoid matching MOV before MOVK.
static char *mnemonics[] = {
"ABS", "ADC", "ADCS", "ADD", "ADDHN", "ADDHN2", "ADDP", "ADDS",
"ADDV", "ADR", "ADRP", "AESD", "AESE", "AESIMC", "AESMC", "align",
"AND", "ANDS", "ASR", "ASRV", "AT", "AUTDA", "AUTDB", "AUTDZA",
"AUTDZB", "AUTIA", "AUTIA1716", "AUTIASP", "AUTIAZ", "AUTIB", "AUTIB1716", "AUTIBSP",
"AUTIBZ", "AUTIZA", "AUTIZB", "B", "BCAX", "BFC", "BFI",
"BFM", "BFXIL", "BIC", "BICS", "BIF", "BIT", "BL", "BLR",
"BLRAA", "BLRAAZ", "BLRAB", "BLRABZ", "BR", "BRAA", "BRAAZ", "BRAB",
"BRABZ", "BRK", "BSL", "CAS", "CASA", "CASAB", "CASAH", "CASAL",
"CASALB", "CASALH", "CASB", "CASH", "CASL", "CASLB", "CASLH", "CASP",
"CASPA", "CASPAL", "CASPL", "CBNZ", "CBZ", "CCMN", "CCMP", "CINC",
"CINV", "CLREX", "CLS", "CLZ", "CMEQ", "CMGE", "CMGT", "CMHI",
"CMHS", "CMLE", "CMLT", "CMN", "CMP", "CMTST", "CNEG", "CNT",
"CRC32B", "CRC32CB", "CRC32CH", "CRC32CW", "CRC32CX", "CRC32H", "CRC32W", "CRC32X",
"CSEL", "CSET", "CSETM", "CSINC", "CSINV", "CSNEG", "db", "DC",
"dcb", "dcd", "DCPS1", "DCPS2", "DCPS3", "dcs", "dcw", "DMB",
"DRPS", "DSB", "DUP", "EON", "EOR", "EOR3", "equb", "equd",
"equq", "equs", "equw", "ERET", "ERETAA", "ERETAB", "ESB", "EXT",
"EXTR", "FABD", "FABS", "FACGE", "FACGT", "FADD", "FADDP", "FCADD",
"FCCMP", "FCCMPE", "FCMEQ", "FCMGE", "FCMGT", "FCMLA", "FCMLE", "FCMLT",
"FCMP", "FCMPE", "FCSEL", "FCVT", "FCVTAS", "FCVTAU", "FCVTL", "FCVTL2",
"FCVTMS", "FCVTMU", "FCVTN", "FCVTN2", "FCVTNS", "FCVTNU", "FCVTPS", "FCVTPU",
"FCVTXN", "FCVTXN2", "FCVTZS", "FCVTZU", "FDIV", "FJCVTZS", "FMADD", "FMAX",
"FMAXNM", "FMAXNMP", "FMAXNMV", "FMAXP", "FMAXV", "FMIN", "FMINNM", "FMINNMP",
"FMINNMV", "FMINP", "FMINV", "FMLA", "FMLAL", "FMLAL2", "FMLS", "FMLSL",
"FMLSL2", "FMOV", "FMSUB", "FMUL", "FMULX", "FNEG", "FNMADD", "FNMSUB",
"FNMUL", "FRECPE", "FRECPS", "FRECPX", "FRINTA", "FRINTI", "FRINTM", "FRINTN",
"FRINTP", "FRINTX", "FRINTZ", "FRSQRTE", "FRSQRTS", "FSQRT", "FSUB", "HINT",
"HLT", "HVC", "IC", "INS", "ISB", "LD1", "LD1R", "LD2",
"LD2R", "LD3", "LD3R", "LD4", "LD4R", "LDADD", "LDADDA", "LDADDAB",
"LDADDAH", "LDADDAL", "LDADDALB", "LDADDALH", "LDADDB", "LDADDH", "LDADDL", "LDADDLB",
"LDADDLH", "LDAPR", "LDAPRB", "LDAPRH", "LDAR", "LDARB", "LDARH", "LDAXP",
"LDAXR", "LDAXRB", "LDAXRH", "LDCLR", "LDCLRA", "LDCLRAB", "LDCLRAH", "LDCLRAL",
"LDCLRALB", "LDCLRALH", "LDCLRB", "LDCLRH", "LDCLRL", "LDCLRLB", "LDCLRLH", "LDEOR",
"LDEORA", "LDEORAB", "LDEORAH", "LDEORAL", "LDEORALB", "LDEORALH", "LDEORB", "LDEORH",
"LDEORL", "LDEORLB", "LDEORLH", "LDLAR", "LDLARB", "LDLARH", "LDNP", "LDP",
"LDPSW", "LDR", "LDRAA", "LDRAB", "LDRB", "LDRH", "LDRSB", "LDRSH",
"LDRSW", "LDSET", "LDSETA", "LDSETAB", "LDSETAH", "LDSETAL", "LDSETALB", "LDSETALH",
"LDSETB", "LDSETH", "LDSETL", "LDSETLB", "LDSETLH", "LDSMAX", "LDSMAXA", "LDSMAXAB",
"LDSMAXAH", "LDSMAXAL", "LDSMAXALB", "LDSMAXALH", "LDSMAXB", "LDSMAXH", "LDSMAXL", "LDSMAXLB",
"LDSMAXLH", "LDSMIN", "LDSMINA", "LDSMINAB", "LDSMINAH", "LDSMINAL", "LDSMINALB", "LDSMINALH",
"LDSMINB", "LDSMINH", "LDSMINL", "LDSMINLB", "LDSMINLH", "LDTR", "LDTRB", "LDTRH",
"LDTRSB", "LDTRSH", "LDTRSW", "LDUMAX", "LDUMAXA", "LDUMAXAB", "LDUMAXAH", "LDUMAXAL",
"LDUMAXALB", "LDUMAXALH", "LDUMAXB", "LDUMAXH", "LDUMAXL", "LDUMAXLB", "LDUMAXLH", "LDUMIN",
"LDUMINA", "LDUMINAB", "LDUMINAH", "LDUMINAL", "LDUMINALB", "LDUMINALH", "LDUMINB", "LDUMINH",
"LDUMINL", "LDUMINLB", "LDUMINLH", "LDUR", "LDURB", "LDURH", "LDURSB", "LDURSH",
"LDURSW", "LDXP", "LDXR", "LDXRB", "LDXRH", "LSL", "LSLV", "LSR",
"LSRV", "MADD", "MLA", "MLS", "MNEG", "MOV", "MOVI", "MOVK",
"MOVN", "MOVZ", "MRS", "MSR", "MSUB", "MUL", "MVN", "MVNI",
"NEG", "NEGS", "NGC", "NGCS", "NOP", "NOT", "opt", "ORN",
"ORR", "PACDA", "PACDB", "PACDZA", "PACDZB", "PACGA", "PACIA", "PACIA1716",
"PACIASP", "PACIAZ", "PACIB", "PACIB1716", "PACIBSP", "PACIBZ", "PACIZA", "PACIZB",
"PMUL", "PMULL", "PMULL2", "PRFM", "PSB", "PSBCSYNC", "RADDHN", "RADDHN2",
"RAX1", "RBIT", "RET", "RETAA", "RETAB", "REV", "REV16", "REV32",
"REV64", "ROR", "RORV", "RSHRN", "RSHRN2", "RSUBHN", "RSUBHN2", "SABA",
"SABAL", "SABAL2", "SABD", "SABDL", "SABDL2", "SADALP", "SADDL", "SADDL2",
"SADDLP", "SADDLV", "SADDW", "SADDW2", "SBC", "SBCS", "SBFIZ", "SBFM",
"SBFX", "SCVTF", "SDIV", "SDOT", "SEV", "SEVL", "SHA1C", "SHA1H",
"SHA1M", "SHA1P", "SHA1SU0", "SHA1SU1", "SHA256H", "SHA256H2", "SHA256SU0", "SHA256SU1",
"SHA512H", "SHA512H2", "SHA512SU0", "SHA512SU1", "SHADD", "SHL", "SHLL", "SHLL2",
"SHRN", "SHRN2", "SHSUB", "SLI", "SM3PARTW1", "SM3PARTW2", "SM3SS1", "SM3TT1A",
"SM3TT1B", "SM3TT2A", "SM3TT2B", "SM4E", "SM4EKEY", "SMADDL", "SMAX", "SMAXP",
"SMAXV", "SMC", "SMIN", "SMINP", "SMINV", "SMLAL", "SMLAL2", "SMLSL",
"SMLSL2", "SMNEGL", "SMOV", "SMSUBL", "SMULH", "SMULL", "SMULL2", "SQABS",
"SQADD", "SQDMLAL", "SQDMLAL2", "SQDMLSL", "SQDMLSL2", "SQDMULH", "SQDMULL", "SQDMULL2",
"SQNEG", "SQRDMLAH", "SQRDMLSH", "SQRDMULH", "SQRSHL", "SQRSHRN", "SQRSHRN2", "SQRSHRUN",
"SQRSHRUN2", "SQSHL", "SQSHLU", "SQSHRN", "SQSHRN2", "SQSHRUN", "SQSHRUN2", "SQSUB",
"SQXTN", "SQXTN2", "SQXTUN", "SQXTUN2", "SRHADD", "SRI", "SRSHL", "SRSHR",
"SRSRA", "SSHL", "SSHLL", "SSHLL2", "SSHR", "SSRA", "SSUBL", "SSUBL2",
"SSUBW", "SSUBW2", "ST1", "ST2", "ST3", "ST4", "STADD", "STADDB",
"STADDH", "STADDL", "STADDLB", "STADDLH", "STCLR", "STCLRB", "STCLRH", "STCLRL",
"STCLRLB", "STCLRLH", "STEOR", "STEORB", "STEORH", "STEORL", "STEORLB", "STEORLH",
"STLLR", "STLLRB", "STLLRH", "STLR", "STLRB", "STLRH", "STLXP", "STLXR",
"STLXRB", "STLXRH", "STNP", "STP", "STR", "STRB", "STRH", "STSET",
"STSETB", "STSETH", "STSETL", "STSETLB", "STSETLH", "STSMAX", "STSMAXB", "STSMAXH",
"STSMAXL", "STSMAXLB", "STSMAXLH", "STSMIN", "STSMINB", "STSMINH", "STSMINL", "STSMINLB",
"STSMINLH", "STTR", "STTRB", "STTRH", "STUMAX", "STUMAXB", "STUMAXH", "STUMAXL",
"STUMAXLB", "STUMAXLH", "STUMIN", "STUMINB", "STUMINH", "STUMINL", "STUMINLB", "STUMINLH",
"STUR", "STURB", "STURH", "STXP", "STXR", "STXRB", "STXRH", "SUB",
"SUBHN", "SUBHN2", "SUBS", "SUQADD", "SVC", "SWP", "SWPA", "SWPAB",
"SWPAH", "SWPAL", "SWPALB", "SWPALH", "SWPB", "SWPH", "SWPL", "SWPLB",
"SWPLH", "SXTB", "SXTH", "SXTL", "SXTL2", "SXTW", "SYS", "SYSL",
"TBL", "TBNZ", "TBX", "TBZ", "TLBI", "TRN1", "TRN2", "TST",
"UABA", "UABAL", "UABAL2", "UABD", "UABDL", "UABDL2", "UADALP", "UADDL",
"UADDL2", "UADDLP", "UADDLV", "UADDW", "UADDW2", "UBFIZ", "UBFM", "UBFX",
"UCVTF", "UDIV", "UDOT", "UHADD", "UHSUB", "UMADDL", "UMAX", "UMAXP",
"UMAXV", "UMIN", "UMINP", "UMINV", "UMLAL", "UMLAL2", "UMLSL", "UMLSL2",
"UMNEGL", "UMOV", "UMSUBL", "UMULH", "UMULL", "UMULL2", "UQADD", "UQRSHL",
"UQRSHRN", "UQRSHRN2", "UQSHL", "UQSHRN", "UQSHRN2", "UQSUB", "UQXTN", "UQXTN2",
"URECPE", "URHADD", "URSHL", "URSHR", "URSQRTE", "URSRA", "USHL", "USHLL",
"USHLL2", "USHR", "USQADD", "USRA", "USUBL", "USUBL2", "USUBW", "USUBW2",
"UXTB", "UXTH", "UXTL", "UXTL2", "UXTW", "UZP1", "UZP2", "WFE", "WFI",
"XAR", "XPACD", "XPACI", "XPACLRI", "XTN", "XTN2", "YIELD", "ZIP1",
"ZIP2"
} ;

// Lowercase r in STr constant because STR is declared elsewhere
enum mnemonics {
ABS, ADC, ADCS, ADD, ADDHN, ADDHN2, ADDP, ADDS,
ADDV, ADR, ADRP, AESD, AESE, AESIMC, AESMC, ALIGN,
AND, ANDS, ASR, ASRV, AT, AUTDA, AUTDB, AUTDZA,
AUTDZB, AUTIA, AUTIA1716, AUTIASP, AUTIAZ, AUTIB, AUTIB1716, AUTIBSP,
AUTIBZ, AUTIZA, AUTIZB, B, BCAX, BFC, BFI,
BFM, BFXIL, BIC, BICS, BIF, BIT, BL, BLR,
BLRAA, BLRAAZ, BLRAB, BLRABZ, BR, BRAA, BRAAZ, BRAB,
BRABZ, BRK, BSL, CAS, CASA, CASAB, CASAH, CASAL,
CASALB, CASALH, CASB, CASH, CASL, CASLB, CASLH, CASP,
CASPA, CASPAL, CASPL, CBNZ, CBZ, CCMN, CCMP, CINC,
CINV, CLREX, CLS, CLZ, CMEQ, CMGE, CMGT, CMHI,
CMHS, CMLE, CMLT, CMN, CMP, CMTST, CNEG, CNT,
CRC32B, CRC32CB, CRC32CH, CRC32CW, CRC32CX, CRC32H, CRC32W, CRC32X,
CSEL, CSET, CSETM, CSINC, CSINV, CSNEG, DB, DC,
DCB, DCD, DCPS1, DCPS2, DCPS3, DCS, DCW, DMB,
DRPS, DSB, DUP, EON, EOR, EOR3, EQUB, EQUD,
EQUQ, EQUS, EQUW, ERET, ERETAA, ERETAB, ESB, EXT,
EXTR, FABD, FABS, FACGE, FACGT, FADD, FADDP, FCADD,
FCCMP, FCCMPE, FCMEQ, FCMGE, FCMGT, FCMLA, FCMLE, FCMLT,
FCMP, FCMPE, FCSEL, FCVT, FCVTAS, FCVTAU, FCVTL, FCVTL2,
FCVTMS, FCVTMU, FCVTN, FCVTN2, FCVTNS, FCVTNU, FCVTPS, FCVTPU,
FCVTXN, FCVTXN2, FCVTZS, FCVTZU, FDIV, FJCVTZS, FMADD, FMAX,
FMAXNM, FMAXNMP, FMAXNMV, FMAXP, FMAXV, FMIN, FMINNM, FMINNMP,
FMINNMV, FMINP, FMINV, FMLA, FMLAL, FMLAL2, FMLS, FMLSL,
FMLSL2, FMOV, FMSUB, FMUL, FMULX, FNEG, FNMADD, FNMSUB,
FNMUL, FRECPE, FRECPS, FRECPX, FRINTA, FRINTI, FRINTM, FRINTN,
FRINTP, FRINTX, FRINTZ, FRSQRTE, FRSQRTS, FSQRT, FSUB, HINT,
HLT, HVC, IC, INS, ISB, LD1, LD1R, LD2,
LD2R, LD3, LD3R, LD4, LD4R, LDADD, LDADDA, LDADDAB,
LDADDAH, LDADDAL, LDADDALB, LDADDALH, LDADDB, LDADDH, LDADDL, LDADDLB,
LDADDLH, LDAPR, LDAPRB, LDAPRH, LDAR, LDARB, LDARH, LDAXP,
LDAXR, LDAXRB, LDAXRH, LDCLR, LDCLRA, LDCLRAB, LDCLRAH, LDCLRAL,
LDCLRALB, LDCLRALH, LDCLRB, LDCLRH, LDCLRL, LDCLRLB, LDCLRLH, LDEOR,
LDEORA, LDEORAB, LDEORAH, LDEORAL, LDEORALB, LDEORALH, LDEORB, LDEORH,
LDEORL, LDEORLB, LDEORLH, LDLAR, LDLARB, LDLARH, LDNP, LDP,
LDPSW, LDR, LDRAA, LDRAB, LDRB, LDRH, LDRSB, LDRSH,
LDRSW, LDSET, LDSETA, LDSETAB, LDSETAH, LDSETAL, LDSETALB, LDSETALH,
LDSETB, LDSETH, LDSETL, LDSETLB, LDSETLH, LDSMAX, LDSMAXA, LDSMAXAB,
LDSMAXAH, LDSMAXAL, LDSMAXALB, LDSMAXALH, LDSMAXB, LDSMAXH, LDSMAXL, LDSMAXLB,
LDSMAXLH, LDSMIN, LDSMINA, LDSMINAB, LDSMINAH, LDSMINAL, LDSMINALB, LDSMINALH,
LDSMINB, LDSMINH, LDSMINL, LDSMINLB, LDSMINLH, LDTR, LDTRB, LDTRH,
LDTRSB, LDTRSH, LDTRSW, LDUMAX, LDUMAXA, LDUMAXAB, LDUMAXAH, LDUMAXAL,
LDUMAXALB, LDUMAXALH, LDUMAXB, LDUMAXH, LDUMAXL, LDUMAXLB, LDUMAXLH, LDUMIN,
LDUMINA, LDUMINAB, LDUMINAH, LDUMINAL, LDUMINALB, LDUMINALH, LDUMINB, LDUMINH,
LDUMINL, LDUMINLB, LDUMINLH, LDUR, LDURB, LDURH, LDURSB, LDURSH,
LDURSW, LDXP, LDXR, LDXRB, LDXRH, LSL, LSLV, LSR,
LSRV, MADD, MLA, MLS, MNEG, MOV, MOVI, MOVK,
MOVN, MOVZ, MRS, MSR, MSUB, MUL, MVN, MVNI,
NEG, NEGS, NGC, NGCS, NOP, NOT, OPT, ORN,
ORR, PACDA, PACDB, PACDZA, PACDZB, PACGA, PACIA, PACIA1716,
PACIASP, PACIAZ, PACIB, PACIB1716, PACIBSP, PACIBZ, PACIZA, PACIZB,
PMUL, PMULL, PMULL2, PRFM, PSB, PSBCSYNC, RADDHN, RADDHN2,
RAX1, RBIT, RET, RETAA, RETAB, REV, REV16, REV32,
REV64, ROR, RORV, RSHRN, RSHRN2, RSUBHN, RSUBHN2, SABA,
SABAL, SABAL2, SABD, SABDL, SABDL2, SADALP, SADDL, SADDL2,
SADDLP, SADDLV, SADDW, SADDW2, SBC, SBCS, SBFIZ, SBFM,
SBFX, SCVTF, SDIV, SDOT, SEV, SEVL, SHA1C, SHA1H,
SHA1M, SHA1P, SHA1SU0, SHA1SU1, SHA256H, SHA256H2, SHA256SU0, SHA256SU1,
SHA512H, SHA512H2, SHA512SU0, SHA512SU1, SHADD, SHL, SHLL, SHLL2,
SHRN, SHRN2, SHSUB, SLI, SM3PARTW1, SM3PARTW2, SM3SS1, SM3TT1A,
SM3TT1B, SM3TT2A, SM3TT2B, SM4E, SM4EKEY, SMADDL, SMAX, SMAXP,
SMAXV, SMC, SMIN, SMINP, SMINV, SMLAL, SMLAL2, SMLSL,
SMLSL2, SMNEGL, SMOV, SMSUBL, SMULH, SMULL, SMULL2, SQABS,
SQADD, SQDMLAL, SQDMLAL2, SQDMLSL, SQDMLSL2, SQDMULH, SQDMULL, SQDMULL2,
SQNEG, SQRDMLAH, SQRDMLSH, SQRDMULH, SQRSHL, SQRSHRN, SQRSHRN2, SQRSHRUN,
SQRSHRUN2, SQSHL, SQSHLU, SQSHRN, SQSHRN2, SQSHRUN, SQSHRUN2, SQSUB,
SQXTN, SQXTN2, SQXTUN, SQXTUN2, SRHADD, SRI, SRSHL, SRSHR,
SRSRA, SSHL, SSHLL, SSHLL2, SSHR, SSRA, SSUBL, SSUBL2,
SSUBW, SSUBW2, ST1, ST2, ST3, ST4, STADD, STADDB,
STADDH, STADDL, STADDLB, STADDLH, STCLR, STCLRB, STCLRH, STCLRL,
STCLRLB, STCLRLH, STEOR, STEORB, STEORH, STEORL, STEORLB, STEORLH,
STLLR, STLLRB, STLLRH, STLR, STLRB, STLRH, STLXP, STLXR,
STLXRB, STLXRH, STNP, STP, STr, STRB, STRH, STSET,
STSETB, STSETH, STSETL, STSETLB, STSETLH, STSMAX, STSMAXB, STSMAXH,
STSMAXL, STSMAXLB, STSMAXLH, STSMIN, STSMINB, STSMINH, STSMINL, STSMINLB,
STSMINLH, STTR, STTRB, STTRH, STUMAX, STUMAXB, STUMAXH, STUMAXL,
STUMAXLB, STUMAXLH, STUMIN, STUMINB, STUMINH, STUMINL, STUMINLB, STUMINLH,
STUR, STURB, STURH, STXP, STXR, STXRB, STXRH, SUB,
SUBHN, SUBHN2, SUBS, SUQADD, SVC, SWP, SWPA, SWPAB,
SWPAH, SWPAL, SWPALB, SWPALH, SWPB, SWPH, SWPL, SWPLB,
SWPLH, SXTB, SXTH, SXTL, SXTL2, SXTW, SYS, SYSL,
TBL, TBNZ, TBX, TBZ, TLBI, TRN1, TRN2, TST,
UABA, UABAL, UABAL2, UABD, UABDL, UABDL2, UADALP, UADDL,
UADDL2, UADDLP, UADDLV, UADDW, UADDW2, UBFIZ, UBFM, UBFX,
UCVTF, UDIV, UDOT, UHADD, UHSUB, UMADDL, UMAX, UMAXP,
UMAXV, UMIN, UMINP, UMINV, UMLAL, UMLAL2, UMLSL, UMLSL2,
UMNEGL, UMOV, UMSUBL, UMULH, UMULL, UMULL2, UQADD, UQRSHL,
UQRSHRN, UQRSHRN2, UQSHL, UQSHRN, UQSHRN2, UQSUB, UQXTN, UQXTN2,
URECPE, URHADD, URSHL, URSHR, URSQRTE, URSRA, USHL, USHLL,
USHLL2, USHR, USQADD, USRA, USUBL, USUBL2, USUBW, USUBW2,
UXTB, UXTH, UXTL, UXTL2, UXTW, UZP1, UZP2, WFE, WFI,
XAR, XPACD, XPACI, XPACLRI, XTN, XTN2, YIELD, ZIP1,
ZIP2
} ;

static char *oslist[] = {
        "osrdch", "oswrch", "oskey", "osline", "oscli", "osopen", "osbyte", "osword",
        "osshut", "osbget", "osbput", "getptr", "setptr", "getext" } ;

static void *osfunc[] = {
        osrdch, oswrch, oskey, osline, oscli, osopen, osbyte, osword, 
        osshut, osbget, osbput, getptr, setptr, getext } ;

static int lookup (char **arr, int num)
{
    int i, n ;

    nxt() ;

    const char *code = (const char *)esi ;

    i = num ;
    while (--i >= 0)
        {
        n = strlen (*(arr + i)) ;
        if (strnicmp (code, *(arr + i), n) == 0)
            {
            esi += n ;
            break;
            }
        }

    return i ;
}

static enum mnemonics lookup_mnemonic(void)
{
    const char *code = (const char *)esi;
    int n ;

    if (code[0] == (char) 0x84 && (code[1] == 'r' || code[1] == 'R')) // Tokenised OR
        {
        esi += 2;
        return ORR;
        }

    if (code[0] == (char) 0x80)
        { // Tokenised AND
        if (code[1] == 's' || code[1] == 'S')
            {
            esi += 2;
            return ANDS;
            }
        else
            {
            esi += 1;
            return AND;
            }
        }

    // Simple binary chop.
    // Tested for all valid values, invalid values before ADC, after YIELD, and in between.

    int top = sizeof( mnemonics ) / sizeof( mnemonics[0] ) ;
    int bottom = 0;
    int i = top / 2;
    int best = -1;

    while (bottom < top)
        {
        n = strlen (mnemonics[i]) ;
        int cmp = strnicmp (mnemonics[i], code, n) ;

        if (cmp <= 0)
            {    
            if (cmp == 0)
                best = i; // There may be a better match later in the alphabetically sorted array

            bottom = i + 1;
            }
        else
            {
            top = i;
            }

        i = (top + bottom)/2;
        }

    if (best != -1)
        {
        esi += strlen (mnemonics[best]) ;
        }

    return best;
}

static char *shift_types[] = { "LSL", "LSR", "ASR", "ROR" };

static int shift_type(void)
{
    int found = lookup( shift_types, 4 ) ;
    if (found < 0)
        {
        error( 16, "Shift types: LSL, LSR, ASR, or sometimes ROR" ) ;
        }
    return found;
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

void *align (void)
{
    while (stavar[16] & 3)
        {
        stavar[16]++ ;
        if (liston & BIT6)
                stavar[15]++ ;
        } ;
    return PC ;
}

/*
 * Table C1-6 A64 Load/Store addressing modes:
 * [n, #imm] includes [n] and [n, #0]
 * [n, #imm]!
 * [n], #imm
 * [n, Xm, LSL#imm] includes [n, Xm] (zero shift)
 * [n, Wm, UXTW#imm] [n, Wm, SXTW#imm]
 * [n], Xm (SIMD only)
 *
 * Doesn't quite match up with C6.2.113 LDR (register), which allows [n, Xm, SXTX#imm]
 * It's also not obvious that the values of imm are limited to 0, 2, or 3 (depending on the WXm register)
 * [n, Xm, LSL#(0|3)] includes [n, Xm] (zero shift) [n, Xm, SXTX#(0|3)]
 * [n, Wm, UXTW#(0|2)] [n, Wm, SXTW#(0|2)]
 * Finally, [n, #imm] can also be an alias for LDUR, if the offset is -ve or not a multiple of 4 or 8.
 */

enum addressing_mode { REG_UXTW, REG_SXTW, REG_SXTX, REG_LSL, REG_UNMODIFIED,
                       LITERAL, PRE_INDEXED, POST_INDEXED, NO_OFFSET, IMMEDIATE,
                       NOTFOUND = -1 };

static char *extends[] = { "UXTW", "SXTW", "SXTX", "LSL" };

static const unsigned address_extend_op[5] = { 0x4000, 0xc000, 0xe000, 0x6000, 0x6000 };

struct addressing {
    enum addressing_mode mode;
    unsigned n;
    unsigned m;
    unsigned amount_present;
    long long imm;
};

static struct addressing read_addressing(void)
{
    struct addressing result = { .mode = NO_OFFSET };

    if (nxt () != '[')
        { // Offset from PC
        result.imm = (void *) (size_t) expri () - PC ;
        result.mode = LITERAL;
        return result;
        }

    open_square() ;

    result.n = reg() ;

    // n must be 64-bit, and not XZR
    if (is_zero( result.n )
     || 64 != reg_size( result.n ))
        {
        error (16, NULL) ; // 'Syntax error'
        }

    if (',' == nxt())
        {
        comma();
        if ('#' == nxt())
            {
            hash();
            result.imm = expri();
            result.mode = IMMEDIATE;
            }
        else
            {
            result.m = reg();
            if (',' == nxt())
                {
                comma();
                result.mode = lookup( extends, sizeof( extends )/sizeof( extends[0] ) ) ;
                if (result.mode == -1)
                    error( 16, NULL );
                result.amount_present = ('#' == nxt ());
                if (result.amount_present)
                    {
                    hash () ;
                    result.imm = expri () ;
                    }
                }
            else
                result.mode = REG_UNMODIFIED;
            }
        }

    close_square();

    if (result.mode == IMMEDIATE && '!' == nxt())
        {
	esi++;
        result.mode = PRE_INDEXED;
        }

    if (result.mode == NO_OFFSET && ',' == nxt())
        {
        comma();
        hash();
        result.imm = expri();
        result.mode = POST_INDEXED;
        }

    return result;
}

static inline unsigned rotl32( unsigned n )
{
    return (n & 0x80000000) ? (n << 1) | 1 : (n << 1) ;
}

static inline unsigned long long rotl64( unsigned long long n )
{
    return (n & 0x8000000000000000ull) ? (n << 1) | 1 : (n << 1) ;
}

static int validated_condition(void)
{
    static char *conditions[] = { "EQ", "NE", "CS", "HS", "CC", "LO", "MI", "PL", "VS", "VC", "HI", "LS", "GE", "LT", "GT", "LE", "AL", "NV" };
    static int code[] = { 0, 1, 2, 2, 3, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 };

    int index = lookup( conditions, sizeof( conditions )/sizeof( conditions[0] ) ) ;
    if (index == -1)
        {
        error( 16, NULL ) ;
        return 0;
        }

    return code[index];
}

static inline unsigned validated_literal_offset ( int imm )
{
    if ((((imm >> 21) != 0 && (imm >> 21) != -1) || (0 != (imm & 3))) && (liston & BIT5))
        error (8, "Label is not within 1MB of the instruction, or is not word aliigned" ) ;
    return (imm >> 2) & ((1 << 19) - 1);
}

static int validated_N_immr_imms( long long imm, unsigned word_data )
{
    if (imm == 0 || imm == -1 || (word_data && imm == 0xffffffff))
        {
        assembler_error() ;
        }

    // Here's the plan:
    // Rotate left until the lsb is 1 and the msb is 0
    // Count the number of 1 bits at the least significant end
    // Count the number of 0 bits above those
    // The sum of those two counts is the pattern size
    // The pattern size must be a multiple of 2
    // Check the immediate value against the pattern generated
    if (word_data)
        {
        int i;
        // 32-bit value
        unsigned n = imm & 0xffffffff;
        unsigned steps = 0;
        while ((n & 0x80000001) != 1)
            {
            steps++;
            n = rotl32( n ) ;
            }

        unsigned bits1 = 0;
        unsigned bits0 = 0;
        unsigned test = 1;
        while ((n & test) != 0)
            {
            bits1++;
            test = test << 1;
            }
        while (test != 0 && (n & test) == 0)
            {
            bits0++;
            test = test << 1;
            }

        unsigned pattern_size = (bits0 + bits1) ;
        if (32 % pattern_size != 0)
            error( 8, "Immediate value cannot be encoded" ) ;

        unsigned pattern = 0xffffffff >> (32 - pattern_size + bits0) ;
        unsigned check = pattern;

        for (i = pattern_size; i < 32; i += pattern_size) {
            check = (check << pattern_size) | pattern;
        }

        if (check != n)
            error( 8, "Immediate value cannot be encoded" ) ;

        unsigned pattern_code = 0b111111;
        while (pattern_size != 0)
            {
            pattern_size = pattern_size >> 1;
            pattern_code = pattern_code << 1;
            }

        return (steps << 6) | (pattern_code & 0b111111) | (bits1 - 1) ;
        }
    else
        {
        int i;
        // 64-bit value
        unsigned long long n = imm;
        unsigned steps = 0;
        while ((n & 0x8000000000000001ull) != 1)
            {
            steps++;
            n = rotl64( n ) ;
            }

        unsigned bits1 = 0;
        unsigned bits0 = 0;
        unsigned long long test = 1;
        while ((n & test) != 0)
            {
            bits1++;
            test = test << 1;
            }
        while (test != 0 && (n & test) == 0)
            {
            bits0++;
            test = test << 1;
            }

        unsigned pattern_size = (bits0 + bits1) ;
        if (64 % pattern_size != 0)
            error( 8, "Immediate value cannot be encoded" ) ;

        unsigned long long pattern = 0xffffffffffffffffull >> (64 - pattern_size + bits0) ;
        unsigned long long check = pattern;

        for (i = pattern_size; i < 64; i += pattern_size) {
            check = (check << pattern_size) | pattern;
        }

        if (check != n)
            error( 8, "Immediate value cannot be encoded" ) ;

        unsigned result = (steps << 6) | (bits1 - 1) ;

        if (pattern_size == 64)
            {
            // Set N
            result |= (1 << 12) ;
            }
        else
            {
            // Clear N, 
            result |= (64 - (2 * pattern_size)) ;
            }

        return result;
        }

    return 0x1fff; // FIXME bigtime!
}

static int validated_imm9( int imm )
{
    // -256 <= offset < 256, imm9, byte offset
    if (imm < -256 || imm >= 256)
        error( 8, "Offset must be between -256 and 255" ) ;
    return (imm & 0x1ff) ;
}

// Alignment 0 = byte, 1 = half-word, 2 = word, 3 = dword
static int validated_imm12( int imm, int alignment )
{
    if (imm < 0 || 0 != (imm & ((1 << alignment) - 1)) || imm >= (4096 << alignment))
        {
        switch (alignment)
            {
            case 0: error( 8, "Constant must be positive and less than 4096" ) ; break;
            case 1: error( 8, "Constant must be positive, a multiple of 2, and less than 8192" ) ; break;
            case 2: error( 8, "Constant must be positive, a multiple of 4, and less than 16384" ) ; break;
            case 3: error( 8, "Constant must be positive, a multiple of 8, and less than 32768" ) ; break;
            }
        }
    return ((imm >> alignment) & 0xfff) ;
}

static unsigned validated_number_0_to_15(void)
{
    int result;
    char al;

    al = *esi++;
    result = al - '0';
    if (result == 1)
        {
        al = *esi;
        result = al - '0';
        if (result >= 0 && result < 6)
            {
            result += 10;
            esi++;
            }
        else
            {
            result = 1;
            }
        }

    if (result < 0 || result > 15)
        error( 16, NULL );

    return result;
}

static unsigned validated_system_register(void)
{
    unsigned result = 0;
    char al ;

    // No named system registers, yet.
    // Code: e.g. s3_1_c11_c0_2

    nxt ();

    al = *esi++;
    if ('S' != al && 's' != al)
        error( 16, NULL );

    al = *esi++;
    switch (al)
        {
        case '2': break;
        case '3': result |= (1 << 19); break;
        default: error( 16, NULL );
        }

    al = *esi++;
    if (al != '_')
        error( 16, NULL );

    al = *esi++;
    int op1 = al - '0';
    if (op1 < 0 || op1 > 7)
        error( 16, NULL );

    result |= op1 << 16;

    al = *esi++;
    if (al != '_')
        error( 16, NULL );

    al = *esi++;
    if (al != 'c' && al != 'C')
        error( 16, NULL );

    result |= validated_number_0_to_15() << 12;

    al = *esi++;
    if (al != '_')
        error( 16, NULL );

    al = *esi++;
    if (al != 'c' && al != 'C')
        error( 16, NULL );

    result |= validated_number_0_to_15() << 8;

    al = *esi++;
    if (al != '_')
        error( 16, NULL );

    al = *esi++;
    int op2 = al - '0';
    if (op2 < 0 || op2 > 7)
        error( 16, NULL );

    result |= op2 << 5;

    return result;
}

static unsigned validated_FB_SIMD_instruction( enum mnemonics mnemnonic )
{
    error( 16, "Unsupported FP/SIMD operation" );
    return 0;
}

void assemble (void)
{
    signed char al ;
    signed char *oldesi = esi ;
    int init = 1 ;
    void *oldpc = PC ;

    unsigned old_liston_bit2 = liston & BIT2;

    liston |= BIT2;

    while (1)
        {
        enum mnemonics mnemonic;
        int instruction = 0 ;

        if (liston & BIT7)
            {
            int tmp ;
            if (liston & BIT6)
                tmp = stavar[15] ;      // O%   physical memory
            else
                tmp = stavar[16] ;      // P%   nominal memory
            if (tmp >= stavar[12])      // L%   limit
                error (8, NULL) ; // 'Address out of range'
            }

        al = nxt () ;
        esi++ ;

        switch (al) 
            {
            case 0:
                esi-- ;
                liston = (liston & 0x0F) | 0x30 ;
                // Restore bit 2, set to get 64-bit constants
                liston &= ~BIT2;
                liston |= old_liston_bit2;
                return ;

            case ']':
                liston = (liston & 0x0F) | 0x30 ;
                // Restore bit 2, set to get 64-bit constants
                liston &= ~BIT2;
                liston |= old_liston_bit2;
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
#ifdef _WIN32
                        sprintf (accs, "%016I64X ", (long long) (size_t) oldpc) ;
#else
                        sprintf (accs, "%016llX ", (long long) (size_t) oldpc) ;
#endif
                        switch (n)
                            {
                            case 0: break ;
                            case 1:    i &= 0xFF ;
                            case 2: i &= 0xFFFF ;
                            case 3: i &= 0xFFFFFF ;
                            case 4: sprintf (accs + 17, "%0*X ", n*2, i) ;
                                break ;
                            default: sprintf (accs + 17, "%08X ", i) ;
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
                            tabit (26) ;
                            do    
                                {
                                token (*oldesi++ ) ;
                                }
                            while (range0(*oldesi)) ;
                            while (*oldesi == ' ') oldesi++ ;
                            }
                        tabit (38) ;
                        while ((*oldesi != ':') && (*oldesi != 0x0D)) 
                            token (*oldesi++) ;
                        crlf () ;
                        }
                    while (n) ;
                    }
                nxt () ;
#if defined(__arm__) || defined(__aarch64__)
                if ((liston & BIT6) == 0)
	#ifdef __APPLE__
		        sys_icache_invalidate (oldpc, PC - oldpc) ;
	#else
                        __builtin___clear_cache (oldpc, PC) ;
	#endif
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
                esi-- ; // Switch was probably on the first character of an assembler mnemonic, include it in the search
                mnemonic = lookup_mnemonic () ;

                if (mnemonic != OPT)
                    init = 0 ;

                // Assember controls stay the same as 32-bit ARM
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

                    case ADC:
                    case ADCS:
                    case ASRV:
                    case LSLV:
                    case LSRV:
                    case MNEG:
                    case MUL:
                    case RORV:
                    case SBC:
                    case SBCS:
                    case SDIV:
                    case UDIV:
                        {
                        if ((mnemonic == MUL) && ('D' == nxt() || 'd' == nxt()))
                            {
                            instruction = validated_FB_SIMD_instruction( mnemonic );
                            break;
                            }

                        // Three same-sized registers
                        switch (mnemonic)
                            {
                            case ADC:  instruction = 0x1a000000; break; // C6.2.1
                            case ADCS: instruction = 0x3a000000; break; // C6.2.2
                            case ASRV: instruction = 0x1ac02800; break; // C6.2.17
                            case LSLV: instruction = 0x1ac02000; break; // C6.2.159
                            case LSRV: instruction = 0x1ac02400; break; // C6.2.162
                            case MNEG: instruction = 0x1b00fc00; break; // C6.2.164
                            case MUL:  instruction = 0x1b007c00; break; // C6.2.177
                            case RORV: instruction = 0x1ac02c00; break; // C6.2.206
                            case SBC:  instruction = 0x5a000000; break; // C6.2.207
                            case SBCS: instruction = 0x7a000000; break; // C6.2.208
                            case SDIV: instruction = 0x1ac00c00; break; // C6.2.212
                            case UDIV: instruction = 0x1ac00800; break; // C6.2.296
                            default: assembler_error() ;
                            }

                        unsigned d = reg () ;
                        comma () ;
                        unsigned n = reg () ;
                        comma () ;
                        unsigned m = reg () ;

                        if (reg_size( d ) != reg_size( n )
                         || reg_size( d ) != reg_size( m ))
                            error( 16, NULL ) ;

                        unsigned word_data = 32 == reg_size( d )  ;
                        unsigned size_bit = (word_data ? 0 : (1 << 31)) ;

                        instruction |= size_bit;
                        instruction |= (d & 0x1f) << 0;
                        instruction |= (n & 0x1f) << 5;
                        instruction |= (m & 0x1f) << 16;
                        }
                        break;

                    case ADD:
                    case ADDS:
                    case SUB:
                    case SUBS:
                    case CMN:
                    case CMP:
                        {
                        if ((mnemonic == ADD || mnemonic == SUB) && ('D' == nxt() || 'd' == nxt()))
                            {
                            instruction = validated_FB_SIMD_instruction( mnemonic );
                            break;
                            }

                        // Three forms: Immediate, Shifted or Extended register

                        unsigned d;

                        if (mnemonic != CMN && mnemonic != CMP)
                            {
                            d = reg () ;
                            comma () ;
                            }

                        unsigned n = reg () ;
                        comma () ;

                        if (mnemonic == CMN || mnemonic == CMP)
                            {
                            // CMN m, n == ADDS z, m, n
                            d = 31 | REGISTER_IS_ZERO | reg_size( n ) ;
                            }

                        unsigned word_data = 32 == reg_size( d )  ;
                        unsigned size_bit = (word_data ? 0 : (1 << 31)) ;

                        if (nxt() == '#')
                            {
                            switch (mnemonic)
                                {
                                case ADD:       instruction = 0x11000000; break; // C6.2.4
                                case CMN:       // C6.2.53
                                case ADDS:      instruction = 0x31000000; break; // C6.2.7
                                case SUB:       instruction = 0x51000000; break; // C6.2.274
                                case CMP:       // C6.2.56
                                case SUBS:      instruction = 0x71000000; break; // C6.2.277
                                default: assembler_error() ;
                                }
                            hash () ;
                            long long imm = expri () ;
                            if (nxt() == ',')
                                {
                                comma () ;
                                nxt () ;
                                if (strnicmp ((const char *) esi, "LSL", 3) == 0)
                                    {
                                    esi += 3;
                                    hash () ;
                                    int shift = expri () ;
                                    if (shift != 12 && shift != 0)
                                        error( 16, NULL ) ;
                                    if (shift == 12)
                                        instruction |= 1 << 22;
                                    }
                                else
                                    {
                                    error( 16, NULL ) ;
                                    }
                                }
                            else if (imm >= 4096 && (imm & 0xfff) == 0 && (imm >> 12) < 4096)
                                {
                                // Automatically perform the shift for suitable larger numbers
                                imm = imm >> 12;
                                instruction |= 1 << 22;
                                }

                            if (imm < 0 || imm >= 4096)
                                error( 16, NULL ) ;

                            instruction |= imm << 10;
                            }
                        else
                            {
                            switch (mnemonic)
                                {
                                case ADD:       instruction = 0x0b000000; break; // C6.2.3 C6.2.5
                                case CMN:       // C6.2.53
                                case ADDS:      instruction = 0x2b000000; break; // C6.2.7 C6.2.8
                                case SUB:       instruction = 0x4b000000; break; // C6.2.273, C6.2.275
                                case CMP:       // C6.2.56
                                case SUBS:      instruction = 0x6b000000; break; // C6.2.276, C6.2.278
                                default: assembler_error() ;
                                }

                            unsigned m ;

                            m = reg ();

                            enum modifier { EXT_UXTB, EXT_UXTH, EXT_UXTW, EXT_UXTX,
                                            EXT_SXTB, EXT_SXTH, EXT_SXTW, EXT_SXTX,
                                            EXT_LSL,  EXT_LSR,  EXT_ASR,  EXT_ROR, EXT_NO_MODIFIER,
                                            NOTFOUND = -1 };

                            static char *modifiers[] = { "UXTB", "UXTH", "UXTW", "UXTX",
                                                         "SXTB", "SXTH", "SXTW", "SXTX",
                                                         "LSL",  "LSR",  "ASR",  "ROR" };

                            int imm = 0;
                            int amount_present = 0;
                            enum modifier mod = EXT_NO_MODIFIER ;

                            if (',' == nxt ())
                                {
                                comma () ;

                                mod = lookup( modifiers, sizeof( modifiers )/sizeof( modifiers[0] ) ) ;

                                if (mod == -1)
                                    error( 16, NULL );

                                amount_present = ('#' == nxt ());
                                if (amount_present)
                                    {
                                    hash () ;
                                    imm = expri () ;
                                    }

                                }

                            // The encoding can be extended register or shifted register
                            // Register n being number 31 is SP in extended, ZR in shifted
                            // Register d being number 31 is SP in extended, ZR in shifted
                            // The amount may only be 0-4 in extended, 0 to reg_size( m ) - 1 in shifted.
                            // Either mode can use LSL, with two different encodings within extended mode.

                            if (is_sp( m )) error( 16, "Third register cannot be SP" );

                            if (is_sp( n ) || is_sp( d ) || (mod < EXT_LSL)) //  || (mod == EXT_LSL && 0 <= imm && 4 >= imm)))
                                {
                                // Extended
                                instruction |= (1 << 21) ;

                                if (mod == EXT_LSL || mod == EXT_NO_MODIFIER)
                                    {
                                    if (32 == reg_size( d ))
                                        mod = EXT_UXTW;
                                    else
                                        mod = EXT_UXTX;
                                    }

                                if (mod >= EXT_LSL)
                                    error( 16, NULL );

                                instruction |= (mod & 7) << 13;

                                if (imm < 0 || imm > 4)
                                    error( 8, "Extension amount must be 0..4" ) ;
                                }
                            else
                                {
                                // Shifted
                                if (mod != EXT_NO_MODIFIER)
                                    instruction |= (mod - EXT_LSL) << 22;

                                if (imm < 0 || imm >= reg_size( d ) )
                                    error( 8, NULL ) ;
                                }

                            instruction |= imm << 10;

                            if (reg_size( d ) != reg_size( n )
                             || reg_size( d ) < reg_size( m ))
                                error( 16, NULL ) ;

                            instruction |= (m & 0x1f) << 16;
                            }

                        instruction |= size_bit;
                        instruction |= (d & 0x1f) << 0;
                        instruction |= (n & 0x1f) << 5;
                        }
                        break;

                    case ADR: // Within 1MB of the PC, to the byte.
                        {
                        int offpc ;
                        unsigned d = reg () ;

                        comma () ;
                        offpc = (void *) (size_t) expri () - PC ;

                        if (((d & 0x1f) == 0x1f && 0 == (d & REGISTER_IS_ZERO)) // SP is not allowed, but XZR is!
                         || 32 == reg_size( d )
                         || offpc < -0x100000 || offpc >= 0x100000)
                            error (16, NULL) ; // 'Syntax error'

                        instruction = 0x10000000;   // C6.2.9
                        instruction |= (d & 0x1f) ;
                        // Words offset goes in bits [32:5], byte offset in [30:29]
                        instruction |= ((offpc & 0x1ffffc) << 3) | ((offpc & 3) << 29) ;
                        }
                        break ;

                    case ADRP: // Within 4GB of the current page, to 4k boundary
                        {
                        long long offpc ;
                        unsigned d = reg () ;

                        if (((d & 0x1f) == 0x1f && 0 == (d & REGISTER_IS_ZERO)) // SP is not allowed, but XZR is!
                         || 32 == reg_size( d ))
                            error (16, NULL) ;

                        comma () ;
                        offpc = (void *) (size_t) expri () - PC ;
                        offpc = (offpc >> 12) & ((1 << 21)-1);
                        unsigned immlo = offpc & 3;
                        unsigned immhi = (offpc >> 2) & ((1 << 19)-1);
                        instruction = 0x90000000;   // C6.2.10
                        instruction |= (d & 0x1f) ;
                        instruction |= (immhi << 5) | (immlo << 29) ;
                        }
                        break ;

                    case AT:
                        {
                        char *ops[] = {
                                "S1E1R", "S1E1W", "S1E0R", "S1E0W", "S1E2R", "S1E2W", "S12E1R",
                                "S12E1W", "S12E0R", "S12E0W", "S1E3R", "S1E3W", "S1E1RP", "S1E1WP" };
                        unsigned bits[] = {
                                0x00000000, 0x00000020, 0x00000040, 0x00000060, 0x00040000, 0x00040020, 0x00040080,
                                0x000400a0, 0x000400c0, 0x000400e0, 0x00060000, 0x00060020, 0x00000100, 0x00000120 };

                        nxt () ;
                        int op = lookup( ops, sizeof( ops ) / sizeof( ops[0] ) ) ;
                        if (op == -1)
                            error( 16, NULL ) ;
                        instruction = 0xd5087800;       // C6.2.18
                        instruction |= bits[op];
                        comma () ;
                        unsigned d = reg () ;
                        if (32 == reg_size( d ) )
                            error( 16, NULL ) ;
                        instruction |= (d & 0x1f) ;
                        }
                        break ;

                    case AUTDA:
                    case AUTDB:
                        {
                        unsigned d = reg () ;
                        comma () ;
                        unsigned n = reg () ;
                        if (32 == reg_size( d )  || 0 == reg_size( n ))
                            error( 16, NULL ) ;
                        instruction = 0xdac11800;       // C6.2.19
                        if (mnemonic == AUTDB)
                            instruction |= 0x00000400;  // C6.2.20
                        instruction |= (d & 0x1f) << 0;
                        instruction |= (n & 0x1f) << 5;
                        }
                        break ;

                    case AUTDZA:
                    case AUTDZB:
                        {
                        unsigned d = reg () ;
                        if (32 == reg_size( d ) )
                            error( 16, NULL ) ;
                        instruction = 0xdac13be0;       // C6.2.19
                        if (mnemonic == AUTDZB)
                            instruction |= 0x00000400;  // C6.2.20
                        instruction |= (d & 0x1f) << 0;
                        }
                        break ;


                    case AUTIA:
                    case AUTIB:
                        {
                        unsigned d = reg () ;
                        comma () ;
                        unsigned n = reg () ;
                        if (32 == reg_size( d )  || 0 == reg_size( n ))
                            error( 16, NULL ) ;
                        instruction = 0xdac11000;       // C6.2.21
                        if (mnemonic == AUTIB)
                            instruction |= 0x00000400;  // C6.2.22
                        instruction |= (d & 0x1f) << 0;
                        instruction |= (n & 0x1f) << 5;
                        }
                        break ;

                    case AUTIZA:
                    case AUTIZB:
                        {
                        unsigned d = reg () ;
                        if (32 == reg_size( d ) )
                            error( 16, NULL ) ;
                        instruction = 0xdac133e0;       // C6.2.21
                        if (mnemonic == AUTIZB)
                            instruction |= 0x00000400;  // C6.2.22
                        instruction |= (d & 0x1f) << 0;
                        }
                        break ;

                    case AUTIA1716:
                        {
                        instruction = 0xd503219f;       // C6.2.21
                        }
                        break;

                    case AUTIB1716:
                        {
                        instruction = 0xd50321df;       // C6.2.22
                        }
                        break;

                    case AUTIASP:
                        {
                        instruction = 0xd50323bf;       // C6.2.21
                        }
                        break;

                    case AUTIBSP:
                        {
                        instruction = 0xd50323ff;       // C6.2.22
                        }
                        break;

                    case AUTIAZ:
                        {
                        instruction = 0xd503239f;       // C6.2.21
                        }
                        break;

                    case AUTIBZ:
                        {
                        instruction = 0xd50323df;       // C6.2.22
                        }
                        break;

                    case B:
                    case BL:
                        {
                        if (mnemonic == B && '.' == *esi)
                            {
                            esi++;
                            instruction = 0x54000000;       // C6.2.23
                            instruction |= validated_condition() ;
                            int offpc = (void *) (size_t) expri () - PC ;
                            instruction |= validated_literal_offset( offpc ) << 5;
                            }
                        else
                            {
                            if (!isspace( *esi ))
                                error( 16, NULL );

                            switch (mnemonic)
                                {
                                case B:  instruction = 0x14000000; break;       // C6.2.24
                                case BL: instruction = 0x94000000; break;       // C6.2.31
                                default: assembler_error() ;
                                }
                            int imm = (void *) (size_t) expri () - PC ;
                            if ((((imm >> 28) != 0 && (imm >> 28) != -1) || (0 != (imm & 3))) && (liston & BIT5))
                                error (1, "Label is not within 128MB of the instruction, or is not word aliigned" ) ;
                            instruction |= (imm & 0xffffffc) >> 2;
                            }
                        }
                        break;

                    case CBNZ:
                    case CBZ:
                        {
                        unsigned t = reg () ;
                        comma () ;

                        unsigned word_data = 32 == reg_size( t )  ;
                        unsigned size_bit = (word_data ? 0 : (1 << 31)) ;

                        switch (mnemonic)
                            {
                            case CBNZ:      instruction = 0x35000000; break;       // C6.2.41
                            case CBZ:       instruction = 0x34000000; break;       // C6.2.42
                            default: assembler_error() ;
                            }

                        int offpc = (void *) (size_t) expri () - PC ;

                        instruction |= size_bit;
                        instruction |= validated_literal_offset( offpc ) << 5;
                        instruction |= (t & 0x1f) << 0;
                        }
                        break;

                    case TBNZ:
                    case TBZ:
                        {
                        unsigned t = reg () ;
                        comma () ;
                        hash () ;
                        int bit = expri () ;
                        comma () ;

                        if (bit < 0 || bit >= reg_size( t ))
                            error( 16, "Bit numbers must be 0-31 for W registers, 0-63 for X registers" ) ;

                        switch (mnemonic)
                            {
                            case TBNZ:      instruction = 0x37000000; break;       // C6.2.288
                            case TBZ:       instruction = 0x36000000; break;       // C6.2.289
                            default: assembler_error() ;
                            }

                        int offpc = (void *) (size_t) expri () - PC ;

                        if ((offpc & 3) != 0
                         || offpc >= (1 << 16)
                         || offpc < -(1 << 16))
                            error( 8, NULL );

                        if (bit >= 32)
                            instruction |= (1 << 31) ;

                        instruction |= (bit & 0x1f) << 19;
                        instruction |= ((offpc >> 2) & 0x3fff) << 5;
                        instruction |= (t & 0x1f) << 0;
                        }
                        break;

                    case BLR:
                    case BR:
                        {
                        switch (mnemonic)
                            {
                            case BLR: instruction = 0xd63f0000; break; // C6.2.32
                            case BR:  instruction = 0xd61f0000; break; // C6.2.34
                            default: assembler_error() ;
                            }

                        unsigned n = reg () ;
                        if (32 == reg_size( n ) )
                            error( 16, NULL ) ;

                        instruction |= (n & 0x1f) << 5;
                        }
                        break;

                    case BLRAA:
                    case BLRAB:
                    case BRAA:
                    case BRAB:
                        {
                        switch (mnemonic)
                            {
                            case BLRAA: instruction = 0xd73f0800; break; // C6.2.32
                            case BLRAB: instruction = 0xd73f0c00; break; // C6.2.32
                            case BRAA:  instruction = 0xd71f0800; break; // C6.2.35
                            case BRAB:  instruction = 0xd71f0c00; break; // C6.2.35
                            default: assembler_error() ;
                            }

                        unsigned n = reg () ;
                        comma () ;
                        unsigned m = reg () ;

                        if (32 == reg_size( n ) 
                         || 32 == reg_size( n ) )
                            error( 16, NULL ) ;

                        instruction |= (n & 0x1f) << 5;
                        instruction |= (m & 0x1f) << 0;
                        }
                        break;

                    case BLRAAZ:
                    case BLRABZ:
                    case BRAAZ:
                    case BRABZ:
                        {
                        switch (mnemonic)
                            {
                            case BLRAAZ: instruction = 0xd63f081f; break; // C6.2.32
                            case BLRABZ: instruction = 0xd63f0c1f; break; // C6.2.32
                            case BRAAZ:  instruction = 0xd61f081f; break; // C6.2.35
                            case BRABZ:  instruction = 0xd61f0c1f; break; // C6.2.35
                            default: assembler_error() ;
                            }

                        unsigned n = reg () ;
                        if (32 == reg_size( n ) )
                            error( 16, NULL ) ;

                        instruction |= (n & 0x1f) << 5;
                        }
                        break;

                    case BRK:
                    case HVC:
                    case HLT:
                    case SMC:
                    case SVC:
                        {
                        switch (mnemonic)
                            {
                            case BRK:   instruction = 0xd4200000; break; // C6.2.36
                            case HLT:   instruction = 0xd4400000; break; // C6.2.82
                            case HVC:   instruction = 0xd4000002; break; // C6.2.83
                            case SMC:   instruction = 0xd4000003; break; // C6.2.216
                            case SVC:   instruction = 0xd4000001; break; // C6.2.279
                            default: assembler_error() ;
                            }

                        hash () ;
                        int imm = expri () ;
                        if (imm < 0 || imm > 65535)
                            error( 16, NULL ) ;
                        instruction |= imm << 5;
                        }
                        break;

                    case DCPS1:
                    case DCPS2:
                    case DCPS3:
                        {
                        switch (mnemonic)
                            {
                            case DCPS1: instruction = 0xd4a00001; break; // C6.2.68
                            case DCPS2: instruction = 0xd4a00002; break; // C6.2.69
                            case DCPS3: instruction = 0xd4a00003; break; // C6.2.70
                            default: assembler_error() ;
                            }

                        int imm = 0;
                        if ('#' == nxt ())
                            {
                            esi++;
                            imm = expri () ;
                            if (imm < 0 || imm > 65535)
                                error( 16, NULL ) ;
                            }
                        instruction |= imm << 5;
                        }
                        break;

                    case BFM:
                    // Aliases that use a different bit identification scheme:
                    case BFC:   // C6.2.25
                    case BFI:   // C6.2.26
                    case BFXIL: // C6.2.28
                        {
                        unsigned d = reg () ;
                        comma () ;

                        unsigned n;
                        if (mnemonic == BFC) // Second register is WZR/XZR
                            {
                            n = 31 | REGISTER_IS_ZERO | reg_size( d ) ;
                            }
                        else
                            {
                            n = reg () ;
                            comma () ;
                            }

                        hash () ;
                        int immr = expri () ;
                        comma () ;
                        hash () ;
                        int imms = expri () ;

                        unsigned word_data = 32 == reg_size( d )  ;
                        unsigned size_bits = (word_data ? 0 : 0x80400000) ;

                        if (mnemonic == BFC || mnemonic == BFI)
                            {
                            imms = imms - 1; // width is 1...bits
                            }
                        else if (mnemonic == BFXIL)
                            {
                            imms = immr + imms - 1;
                            }

                        if (reg_size( d ) != reg_size( n )
                         || immr < 0 || immr >= reg_size( d ) || imms < 0 || imms >= reg_size( d ))
                            error( 16, NULL ) ;

                        if (mnemonic == BFC || mnemonic == BFI)
                            {
                            immr = (-immr) & (reg_size( d ) - 1);
                            }

                        instruction = 0x33000000;       // C6.2.27
                        instruction |= size_bits;
                        instruction |= (d & 0x1f) << 0;
                        instruction |= (n & 0x1f) << 5;
                        instruction |= (immr & 0x3f) << 16;
                        instruction |= (imms & 0x3f) << 10;
                        }
                        break;

                    case BIC: // C6.2.29
                    case BICS: // C6.2.30
                        {
                        if ((mnemonic == BIC) && ('D' == nxt() || 'd' == nxt()))
                            {
                            instruction = validated_FB_SIMD_instruction( mnemonic );
                            break;
                            }

                        unsigned d = reg () ;
                        comma () ;
                        unsigned n = reg () ;
                        comma () ;
                        unsigned m = reg () ;
                        int shift = 0;
                        int amount = 0;

                        unsigned word_data = 32 == reg_size( d )  ;
                        unsigned size_bit = (word_data ? 0 : (1 << 31)) ;

                        if (',' == nxt())
                            {
                            comma () ;
                            shift = shift_type() ;
                            hash () ;
                            amount = expri() ;
                            if (amount < 0 || amount >= reg_size( d ))
                                error( 16, NULL ) ;
                            }

                        if (reg_size( d ) != reg_size( n )
                         || amount < 0 || amount >= reg_size( d ))
                            error( 16, NULL ) ;

                        switch (mnemonic)
                            {
                            case BIC:  instruction = 0x0a200000; break; // C6.2.29
                            case BICS: instruction = 0x6a200000; break; // C6.2.30
                            default: assembler_error() ;
                            }
                        instruction |= size_bit;
                        instruction |= shift << 22;
                        instruction |= amount << 10;
                        instruction |= (d & 0x1f) << 0;
                        instruction |= (n & 0x1f) << 5;
                        instruction |= (m & 0x1f) << 16;
                        }
                        break;

                    /*************************************************************************
                     * Byte/Halfword in memory instructions
                     *************************************************************************/
                    case CASB:
                    case CASAB:
                    case CASALB:
                    case CASLB:
                    case LDADDB:
                    case LDADDAB:
                    case LDADDALB:
                    case LDADDLB:
                    case LDCLRB:
                    case LDCLRAB:
                    case LDCLRALB:
                    case LDCLRLB:
                    case LDEORB:
                    case LDEORAB:
                    case LDEORALB:
                    case LDEORLB:
                    case LDSETB:
                    case LDSETAB:
                    case LDSETALB:
                    case LDSETLB:
                    case LDSMAXB:
                    case LDSMAXAB:
                    case LDSMAXALB:
                    case LDSMAXLB:
                    case LDSMINB:
                    case LDSMINAB:
                    case LDSMINALB:
                    case LDSMINLB:
                    case LDUMAXB:
                    case LDUMAXAB:
                    case LDUMAXALB:
                    case LDUMAXLB:
                    case LDUMINB:
                    case LDUMINAB:
                    case LDUMINALB:
                    case LDUMINLB:

                    case CASH:
                    case CASAH:
                    case CASALH:
                    case CASLH:
                    case LDADDH:
                    case LDADDAH:
                    case LDADDALH:
                    case LDADDLH:
                    case LDCLRH:
                    case LDCLRAH:
                    case LDCLRALH:
                    case LDCLRLH:
                    case LDEORH:
                    case LDEORAH:
                    case LDEORALH:
                    case LDEORLH:
                    case LDSETH:
                    case LDSETAH:
                    case LDSETALH:
                    case LDSETLH:
                    case LDSMAXH:
                    case LDSMAXAH:
                    case LDSMAXALH:
                    case LDSMAXLH:
                    case LDSMINH:
                    case LDSMINAH:
                    case LDSMINALH:
                    case LDSMINLH:
                    case LDUMAXH:
                    case LDUMAXAH:
                    case LDUMAXALH:
                    case LDUMAXLH:
                    case LDUMINH:
                    case LDUMINAH:
                    case LDUMINALH:
                    case LDUMINLH:
                        {
                        // <Ws>, <Wt>, [<Xn|SP>] (documentation suggests an optional , #0 in the brackets.)
                        unsigned s = reg () ;
                        comma() ;
                        unsigned t = reg () ;
                        comma() ;
                        open_square() ;
                        unsigned n = reg () ;
                        close_square() ;

                        switch (mnemonic)
                            {
                            case CASB:          instruction = 0x08a07c00 | 0x00000000; break; // C6.2.37
                            case CASAB:         instruction = 0x08a07c00 | 0x00400000; break; // C6.2.37
                            case CASALB:        instruction = 0x08a07c00 | 0x00408000; break; // C6.2.37
                            case CASLB:         instruction = 0x08a07c00 | 0x00008000; break; // C6.2.37

                            case LDADDB:        instruction = 0x38200000 | 0x00000000; break; // C6.2.86
                            case LDADDAB:       instruction = 0x38200000 | 0x00800000; break; // C6.2.86
                            case LDADDALB:      instruction = 0x38200000 | 0x00c00000; break; // C6.2.86
                            case LDADDLB:       instruction = 0x38200000 | 0x00400000; break; // C6.2.86

                            case LDCLRB:        instruction = 0x38201000 | 0x00000000; break; // C6.2.99
                            case LDCLRAB:       instruction = 0x38201000 | 0x00800000; break; // C6.2.99
                            case LDCLRALB:      instruction = 0x38201000 | 0x00c00000; break; // C6.2.99
                            case LDCLRLB:       instruction = 0x38201000 | 0x00400000; break; // C6.2.99

                            case LDEORB:        instruction = 0x38202000 | 0x00000000; break; // C6.2.102
                            case LDEORAB:       instruction = 0x38202000 | 0x00800000; break; // C6.2.102
                            case LDEORALB:      instruction = 0x38202000 | 0x00c00000; break; // C6.2.102
                            case LDEORLB:       instruction = 0x38202000 | 0x00400000; break; // C6.2.102

                            case LDSETB:        instruction = 0x38203000 | 0x00000000; break; // C6.2.126
                            case LDSETAB:       instruction = 0x38203000 | 0x00800000; break; // C6.2.126
                            case LDSETALB:      instruction = 0x38203000 | 0x00c00000; break; // C6.2.126
                            case LDSETLB:       instruction = 0x38203000 | 0x00400000; break; // C6.2.126

                            case LDSMAXB:       instruction = 0x38204000 | 0x00000000; break; // C6.2.129
                            case LDSMAXAB:      instruction = 0x38204000 | 0x00800000; break; // C6.2.129
                            case LDSMAXALB:     instruction = 0x38204000 | 0x00c00000; break; // C6.2.129
                            case LDSMAXLB:      instruction = 0x38204000 | 0x00400000; break; // C6.2.129

                            case LDSMINB:       instruction = 0x38205000 | 0x00000000; break; // C6.2.132
                            case LDSMINAB:      instruction = 0x38205000 | 0x00800000; break; // C6.2.132
                            case LDSMINALB:     instruction = 0x38205000 | 0x00c00000; break; // C6.2.132
                            case LDSMINLB:      instruction = 0x38205000 | 0x00400000; break; // C6.2.132

                            case LDUMAXB:       instruction = 0x38206000 | 0x00000000; break; // C6.2.141
                            case LDUMAXAB:      instruction = 0x38206000 | 0x00800000; break; // C6.2.141
                            case LDUMAXALB:     instruction = 0x38206000 | 0x00c00000; break; // C6.2.141
                            case LDUMAXLB:      instruction = 0x38206000 | 0x00400000; break; // C6.2.141

                            case LDUMINB:       instruction = 0x38207000 | 0x00000000; break; // C6.2.144
                            case LDUMINAB:      instruction = 0x38207000 | 0x00800000; break; // C6.2.144
                            case LDUMINALB:     instruction = 0x38207000 | 0x00c00000; break; // C6.2.144
                            case LDUMINLB:      instruction = 0x38207000 | 0x00400000; break; // C6.2.144

                            case CASH:          instruction = 0x48a07c00 | 0x00000000; break; // C6.2.38
                            case CASAH:         instruction = 0x48a07c00 | 0x00400000; break; // C6.2.38
                            case CASALH:        instruction = 0x48a07c00 | 0x00408000; break; // C6.2.38
                            case CASLH:         instruction = 0x48a07c00 | 0x00008000; break; // C6.2.38

                            case LDADDH:        instruction = 0x78200000 | 0x00000000; break; // C6.2.87
                            case LDADDAH:       instruction = 0x78200000 | 0x00800000; break; // C6.2.87
                            case LDADDALH:      instruction = 0x78200000 | 0x00c00000; break; // C6.2.87
                            case LDADDLH:       instruction = 0x78200000 | 0x00400000; break; // C6.2.87

                            case LDCLRH:        instruction = 0x78201000 | 0x00000000; break; // C6.2.100
                            case LDCLRAH:       instruction = 0x78201000 | 0x00800000; break; // C6.2.100
                            case LDCLRALH:      instruction = 0x78201000 | 0x00c00000; break; // C6.2.100
                            case LDCLRLH:       instruction = 0x78201000 | 0x00400000; break; // C6.2.100

                            case LDEORH:        instruction = 0x78202000 | 0x00000000; break; // C6.2.103
                            case LDEORAH:       instruction = 0x78202000 | 0x00800000; break; // C6.2.103
                            case LDEORALH:      instruction = 0x78202000 | 0x00c00000; break; // C6.2.103
                            case LDEORLH:       instruction = 0x78202000 | 0x00400000; break; // C6.2.103

                            case LDSETH:        instruction = 0x78203000 | 0x00000000; break; // C6.2.127
                            case LDSETAH:       instruction = 0x78203000 | 0x00800000; break; // C6.2.127
                            case LDSETALH:      instruction = 0x78203000 | 0x00c00000; break; // C6.2.127
                            case LDSETLH:       instruction = 0x78203000 | 0x00400000; break; // C6.2.127

                            case LDSMAXH:       instruction = 0x78204000 | 0x00000000; break; // C6.2.130
                            case LDSMAXAH:      instruction = 0x78204000 | 0x00800000; break; // C6.2.130
                            case LDSMAXALH:     instruction = 0x78204000 | 0x00c00000; break; // C6.2.130
                            case LDSMAXLH:      instruction = 0x78204000 | 0x00400000; break; // C6.2.130

                            case LDSMINH:       instruction = 0x78205000 | 0x00000000; break; // C6.2.133
                            case LDSMINAH:      instruction = 0x78205000 | 0x00800000; break; // C6.2.133
                            case LDSMINALH:     instruction = 0x78205000 | 0x00c00000; break; // C6.2.133
                            case LDSMINLH:      instruction = 0x78205000 | 0x00400000; break; // C6.2.133

                            case LDUMAXH:       instruction = 0x78206000 | 0x00000000; break; // C6.2.142
                            case LDUMAXAH:      instruction = 0x78206000 | 0x00800000; break; // C6.2.142
                            case LDUMAXALH:     instruction = 0x78206000 | 0x00c00000; break; // C6.2.142
                            case LDUMAXLH:      instruction = 0x78206000 | 0x00400000; break; // C6.2.142

                            case LDUMINH:       instruction = 0x78207000 | 0x00000000; break; // C6.2.145
                            case LDUMINAH:      instruction = 0x78207000 | 0x00800000; break; // C6.2.145
                            case LDUMINALH:     instruction = 0x78207000 | 0x00c00000; break; // C6.2.145
                            case LDUMINLH:      instruction = 0x78207000 | 0x00400000; break; // C6.2.145

                            default: assembler_error() ;
                            }

                        if (64 == reg_size( s ) 
                         || 64 == reg_size( t ) 
                         || 32 == reg_size( n ) )
                            error( 16, NULL ) ;

                        instruction |= (s & 0x1f) << 16;
                        instruction |= (t & 0x1f) << 0;
                        instruction |= (n & 0x1f) << 5;
                        }
                        break;

                    case LDAPR:
                    case LDAPRB:
                    case LDAPRH:
                    case LDAXR:
                    case LDAXRB:
                    case LDAXRH:
                    case LDAR:
                    case LDARB:
                    case LDARH:
                    case LDLAR:
                    case LDLARB:
                    case LDLARH:
                    case LDXR:
                    case STLLRB:
                    case STLLRH:
                    case STLLR:
                    case STLR:
                    case STLRB:
                    case STLRH:
                        {
                        unsigned only_32bit = 0;
                        switch (mnemonic)
                            {
                            case LDAPR: instruction = 0xb8bfc000; only_32bit = 0; break; // C6.2.89
                            case LDAPRB:instruction = 0x38bfc000; only_32bit = 1; break; // C6.2.90
                            case LDAPRH:instruction = 0x78bfc000; only_32bit = 1; break; // C6.2.91
                            case LDAXR: instruction = 0x885ffc00; only_32bit = 0; break; // C6.2.96
                            case LDAXRB:instruction = 0x085ffc00; only_32bit = 1; break; // C6.2.97
                            case LDAXRH:instruction = 0x485ffc00; only_32bit = 1; break; // C6.2.98
                            case LDAR:  instruction = 0x88dffc00; only_32bit = 0; break; // C6.2.92
                            case LDARB: instruction = 0x08dffc00; only_32bit = 1; break; // C6.2.93
                            case LDARH: instruction = 0x48dffc00; only_32bit = 1; break; // C6.2.94
                            case LDLAR: instruction = 0x88df7c00; only_32bit = 0; break; // C6.2.107
                            case LDLARB:instruction = 0x08df7c00; only_32bit = 1; break; // C6.2.105
                            case LDLARH:instruction = 0x48df7c00; only_32bit = 1; break; // C6.2.106
                            case LDXR:  instruction = 0x885f7c00; only_32bit = 0; break; // C6.2.154

                            case STLLRB:instruction = 0x089f7c00; only_32bit = 1; break; // C6.2.230
                            case STLLRH:instruction = 0x489f7c00; only_32bit = 1; break; // C6.2.231
                            case STLLR: instruction = 0x889f7c00; only_32bit = 0; break; // C6.2.232
                            case STLR:  instruction = 0x889ffc00; only_32bit = 0; break; // C6.2.233
                            case STLRB: instruction = 0x089ffc00; only_32bit = 1; break; // C6.2.234
                            case STLRH: instruction = 0x489ffc00; only_32bit = 1; break; // C6.2.235
                            default: assembler_error() ;
                            }

                        // <Ws>, [<Xn|SP> {, #0}]
                        unsigned t = reg () ;
                        comma() ;
                        open_square() ;

                        unsigned n = reg () ;
                        optional_zero_offset() ;

                        close_square() ;

                        unsigned word_data = 32 == reg_size( t )  ;
                        unsigned size_bit = word_data ? 0 : (1 << 30) ;

                        if (32 == reg_size( n ) 
                         || (only_32bit && !word_data))
                            error( 16, NULL ) ;

                        instruction |= size_bit;
                        instruction |= (t & 0x1f) << 0;
                        instruction |= (n & 0x1f) << 5;
                        }
                        break;

                    case SWP:
                    case SWPA:
                    case SWPAL:
                    case SWPL:
                    case SWPB:
                    case SWPAB:
                    case SWPALB:
                    case SWPLB:
                    case SWPH:
                    case SWPAH:
                    case SWPALH:
                    case SWPLH:
                        {
                        unsigned only_32bit = 0;
                        switch (mnemonic)
                            {
                            case SWPB:   instruction = 0x38208000; only_32bit = 1; break; // C6.2.280
                            case SWPAB:  instruction = 0x38a08000; only_32bit = 1; break; // C6.2.280
                            case SWPALB: instruction = 0x38e08000; only_32bit = 1; break; // C6.2.280
                            case SWPLB:  instruction = 0x38608000; only_32bit = 1; break; // C6.2.280
                            case SWPH:   instruction = 0x78208000; only_32bit = 1; break; // C6.2.281
                            case SWPAH:  instruction = 0x78a08000; only_32bit = 1; break; // C6.2.281
                            case SWPALH: instruction = 0x78e08000; only_32bit = 1; break; // C6.2.281
                            case SWPLH:  instruction = 0x78608000; only_32bit = 1; break; // C6.2.281

                            case SWP:    instruction = 0xb8208000; only_32bit = 0; break; // C6.2.282
                            case SWPA:   instruction = 0xb8a08000; only_32bit = 0; break; // C6.2.282
                            case SWPAL:  instruction = 0xb8e08000; only_32bit = 0; break; // C6.2.282
                            case SWPL:   instruction = 0xb8608000; only_32bit = 0; break; // C6.2.282

                            default: assembler_error() ;
                            }

                        // <Ws>, <Xt>|<Wt>, [<Xn|SP> {, #0}]
                        unsigned s = reg () ;
                        comma() ;
                        unsigned t = reg () ;
                        comma() ;
                        open_square() ;

                        unsigned n = reg () ;
                        optional_zero_offset() ;

                        close_square() ;

                        if (reg_size( t ) != reg_size( s ) 
                         || (only_32bit && 64 == reg_size( t ) )
                         || 32 == reg_size( n ) )
                            error( 16, NULL ) ;

                        if (!only_32bit && 64 == reg_size( t ) )
                            {
                            instruction |= (1 << 30) ;
                            }

                        instruction |= (t & 0x1f) << 0;
                        instruction |= (n & 0x1f) << 5;
                        instruction |= (s & 0x1f) << 16;
                        }
                        break;

                    case SYSL:
                    case SYS:
                        {
                        int op1;
                        int cn = 0;
                        int cm = 0;
                        int op2;
                        unsigned t = 31 | 64;

                        if (mnemonic == SYSL)
                            {
                            instruction = 0xd5280000;       // C6.2.286
                            t = reg () ;
                            comma() ;
                            }
                        else
                            {
                            instruction = 0xd5080000;       // C6.2.286
                            }

                        hash() ;
                        op1 = expri () ;
                        comma () ;
                        if ('C' == nxt() || 'c' == nxt())
                            {
                            esi++;
                            cn = expri () ;
                            }
                        else
                            error( 16, 0 ) ;

                        comma () ;
                        if ('C' == nxt() || 'c' == nxt())
                            {
                            esi++;
                            cm = expri () ;
                            }
                        else
                            error( 16, 0 ) ;
                        comma () ;
                        hash() ;
                        op2 = expri () ;

                        if (mnemonic == SYS && ',' == nxt())
                            {
                                comma() ;
                                t = reg () ;
                            }

                        if (op1 < 0 || op1 > 7
                         || cm < 0  || cm > 15
                         || cn < 0  || cn > 15
                         || op2 < 0 || op2 > 7
                         || 32 == reg_size( t ) )
                            error( 16, NULL ) ;

                        instruction |= op1 << 16;
                        instruction |= cn << 12;
                        instruction |= cm << 8;
                        instruction |= op2 << 5;
                        instruction |= (t & 0x1f) << 0;
                        }
                        break;

                    case TLBI:
                        {
                        char *ops[] = { "ALLE1", "ALLE1IS", "ALLE2", "ALLE2IS", "ALLE3", "ALLE3IS", "ASIDE1", "ASIDE1IS", "IPAS2E1", "IPAS2E1IS", "IPAS2LE1", "IPAS2LE1IS", "VAAE1", "VAAE1IS", "VAALE1", "VAALE1IS", "VAE1", "VAE1IS", "VAE2", "VAE2IS", "VAE3", "VAE3IS", "VALE1", "VALE1IS", "VALE2", "VALE2IS", "VALE3", "VALE3IS", "VMALLE1", "VMALLE1IS", "VMALLS12E1", "VMALLS12E1IS", };

                        unsigned code[] = { 0x47080 , 0x43080 , 0x47000 , 0x43000 , 0x67000 , 0x63000 , 0x00740 , 0x00340 , 0x44020 , 0x40020 , 0x440a0 , 0x400a0 , 0x00760 , 0x00360 , 0x007e0 , 0x003e0 , 0x00720 , 0x00320 , 0x47020 , 0x43020 , 0x67020 , 0x63020 , 0x007a0 , 0x003a0 , 0x470a0 , 0x430a0 , 0x670a0 , 0x630a0 , 0x07000 , 0x03000 , 0x470c0 , 0x430c0 };

                        unsigned t = 31;
                        int op = lookup( ops, sizeof( ops ) / sizeof( ops[0] ) ) ;

                        if (op == -1 || !isspace( *esi ))
                            error( 16, NULL ) ;

                        instruction = 0xd5088000;
                        instruction |= code[op];
                        if (',' == nxt())
                            {
                            comma() ;
                            t = reg () ;
                            }
                        instruction |= (t & 0x1f) << 0;
                        }
                        break;

                    case STLXP:
                    case STXP:
                        {
                        switch (mnemonic)
                            {
                            case STLXP: instruction = 0x88208000; break; // C6.2.236
                            case STXP:  instruction = 0x88200000; break; // C6.2.269
                            default: assembler_error() ;
                            }

                        unsigned s = reg () ;
                        comma() ;

                        unsigned t1 = reg () ;
                        comma() ;
                        unsigned t2 = reg () ;
                        comma() ;

                        open_square() ;

                        unsigned n = reg () ;
                        optional_zero_offset() ;

                        close_square() ;

                        if (64 == reg_size( s ) 
                         || reg_size( t1 ) != reg_size( t2 )
                         || (32 == reg_size( n ) ))
                            error( 16, NULL ) ;

                        unsigned word_data = 32 == reg_size( t1 )  ;
                        unsigned size_bit = word_data ? 0 : (1 << 30) ;

                        instruction |= size_bit;
                        instruction |= (t1 & 0x1f) << 0;
                        instruction |= (t2 & 0x1f) << 10;
                        instruction |= (s & 0x1f) << 16;
                        instruction |= (n & 0x1f) << 5;
                        }
                        break;

                    case STLXR:
                    case STLXRB:
                    case STLXRH:
                        {
                        switch (mnemonic)
                            {
                            case STLXR:  instruction = 0x8800fc00; break; // C6.2.237
                            case STLXRB: instruction = 0x0800fc00; break; // C6.2.238
                            case STLXRH: instruction = 0x4800fc00; break; // C6.2.239

                            default: assembler_error() ;
                            }

                        unsigned s = reg () ;
                        comma() ;

                        unsigned t = reg () ;
                        comma() ;

                        open_square() ;

                        unsigned n = reg () ;
                        optional_zero_offset() ;

                        close_square() ;

                        unsigned word_data = 32 == reg_size( t )  ;
                        unsigned size_bit = word_data ? 0 : (1 << 30) ;

                        if (64 == reg_size( s ) 
                         || (32 == reg_size( n ) )
                         || (!word_data && mnemonic != STLXR))
                            error( 16, NULL ) ;

                        instruction |= size_bit;
                        instruction |= (t & 0x1f) << 0;
                        instruction |= (s & 0x1f) << 16;
                        instruction |= (n & 0x1f) << 5;
                        }
                        break;

                    case LDRAA:
                    case LDRAB:
                        {
                        switch (mnemonic)
                            {
                            case LDRAA: instruction = 0xf8200400; break; // C6.2.114
                            case LDRAB: instruction = 0xf8a00400; break; // C6.2.114
                            default: assembler_error() ;
                            }

                        unsigned t = reg () ;
                        comma() ;
                        open_square() ;

                        unsigned n = reg () ;
                        int offset = 0;
                        if (',' == nxt())
                            {
                            comma () ;
                            hash();
                            offset = expri () ;
                            }

                        close_square() ;

                        if ('!' == nxt())
                            {
                            esi++;
                            instruction |= (1 << 11) ; // W
                            }

                        if (32 == reg_size( t ) 
                         || 32 == reg_size( n ) 
                         || 0 != (offset & 7)
                         || offset < -4096 || offset > 4088)
                            error( 16, NULL ) ;

                        instruction |= (offset & 0x0ff8) << (12 - 3) ;
                        instruction |= (offset & 0x1000) << (22 - 12) ; // sign bit
                        instruction |= (t & 0x1f) << 0;
                        instruction |= (n & 0x1f) << 5;
                        }
                        break;

                    case CAS:
                    case CASA:
                    case CASAL:
                    case CASL:

                    case LDADD:
                    case LDADDA:
                    case LDADDAL:
                    case LDADDL:

                    case LDCLR:
                    case LDCLRA:
                    case LDCLRAL:
                    case LDCLRL:

                    case LDEOR:
                    case LDEORA:
                    case LDEORAL:
                    case LDEORL:

                    case LDSET:
                    case LDSETA:
                    case LDSETAL:
                    case LDSETL:

                    case LDSMAX:
                    case LDSMAXA:
                    case LDSMAXAL:
                    case LDSMAXL:

                    case LDSMIN:
                    case LDSMINA:
                    case LDSMINAL:
                    case LDSMINL:

                    case LDUMAX:
                    case LDUMAXA:
                    case LDUMAXAL:
                    case LDUMAXL:

                    case LDUMIN:
                    case LDUMINA:
                    case LDUMINAL:
                    case LDUMINL:

                        {
                        // <Ws>, <Wt>, [<Xn|SP>]
                        unsigned s = reg () ;
                        comma() ;
                        unsigned t = reg () ;
                        comma() ;
                        open_square() ;
                        unsigned n = reg () ;
                        close_square() ;

                        switch (mnemonic)
                            {
                            case CAS:           instruction = 0x88a07c00 | 0x00000000; break; // C6.2.37
                            case CASA:          instruction = 0x88a07c00 | 0x00400000; break; // C6.2.37
                            case CASAL:         instruction = 0x88a07c00 | 0x00408000; break; // C6.2.37
                            case CASL:          instruction = 0x88a07c00 | 0x00008000; break; // C6.2.37

                            case LDADD:         instruction = 0xb8200000 | 0x00000000; break; // C6.2.88
                            case LDADDA:        instruction = 0xb8200000 | 0x00800000; break; // C6.2.88
                            case LDADDAL:       instruction = 0xb8200000 | 0x00c00000; break; // C6.2.88
                            case LDADDL:        instruction = 0xb8200000 | 0x00400000; break; // C6.2.88

                            case LDCLR:         instruction = 0xb8201000 | 0x00000000; break; // C6.2.101
                            case LDCLRA:        instruction = 0xb8201000 | 0x00800000; break; // C6.2.101
                            case LDCLRAL:       instruction = 0xb8201000 | 0x00c00000; break; // C6.2.101
                            case LDCLRL:        instruction = 0xb8201000 | 0x00400000; break; // C6.2.101

                            case LDEOR:         instruction = 0xb8202000 | 0x00000000; break; // C6.2.104
                            case LDEORA:        instruction = 0xb8202000 | 0x00800000; break; // C6.2.104
                            case LDEORAL:       instruction = 0xb8202000 | 0x00c00000; break; // C6.2.104
                            case LDEORL:        instruction = 0xb8202000 | 0x00400000; break; // C6.2.104

                            case LDSET:         instruction = 0xb8203000 | 0x00000000; break; // C6.2.128
                            case LDSETA:        instruction = 0xb8203000 | 0x00800000; break; // C6.2.128
                            case LDSETAL:       instruction = 0xb8203000 | 0x00c00000; break; // C6.2.128
                            case LDSETL:        instruction = 0xb8203000 | 0x00400000; break; // C6.2.128

                            case LDSMAX:        instruction = 0xb8204000 | 0x00000000; break; // C6.2.131
                            case LDSMAXA:       instruction = 0xb8204000 | 0x00800000; break; // C6.2.131
                            case LDSMAXAL:      instruction = 0xb8204000 | 0x00c00000; break; // C6.2.131
                            case LDSMAXL:       instruction = 0xb8204000 | 0x00400000; break; // C6.2.131

                            case LDSMIN:        instruction = 0xb8205000 | 0x00000000; break; // C6.2.131
                            case LDSMINA:       instruction = 0xb8205000 | 0x00800000; break; // C6.2.131
                            case LDSMINAL:      instruction = 0xb8205000 | 0x00c00000; break; // C6.2.131
                            case LDSMINL:       instruction = 0xb8205000 | 0x00400000; break; // C6.2.131

                            case LDUMAX:        instruction = 0xb8206000 | 0x00000000; break; // C6.2.131
                            case LDUMAXA:       instruction = 0xb8206000 | 0x00800000; break; // C6.2.131
                            case LDUMAXAL:      instruction = 0xb8206000 | 0x00c00000; break; // C6.2.131
                            case LDUMAXL:       instruction = 0xb8206000 | 0x00400000; break; // C6.2.131

                            case LDUMIN:        instruction = 0xb8207000 | 0x00000000; break; // C6.2.131
                            case LDUMINA:       instruction = 0xb8207000 | 0x00800000; break; // C6.2.131
                            case LDUMINAL:      instruction = 0xb8207000 | 0x00c00000; break; // C6.2.131
                            case LDUMINL:       instruction = 0xb8207000 | 0x00400000; break; // C6.2.131

                            default: assembler_error() ;
                            }

                        unsigned word_data = 32 == reg_size( s )  ;
                        unsigned size_bit = (word_data ? 0 : (1 << 30)) ;

                        if (reg_size( s ) != reg_size( t )
                         || 32 == reg_size( n ) )
                            error( 16, NULL ) ;

                        instruction |= size_bit;
                        instruction |= (s & 0x1f) << 16;
                        instruction |= (t & 0x1f) << 0;
                        instruction |= (n & 0x1f) << 5;
                        }
                        break;

                    case STADDB:        // C6.2.221
                    case STADDLB:       // C6.2.221
                    case STADDH:        // C6.2.222
                    case STADDLH:       // C6.2.222

                    case STCLRB:        // C6.2.224
                    case STCLRLB:       // C6.2.224
                    case STCLRH:        // C6.2.225
                    case STCLRLH:       // C6.2.225

                    case STEORB:        // C6.2.227
                    case STEORLB:       // C6.2.227
                    case STEORH:        // C6.2.228
                    case STEORLH:       // C6.2.228

                    case STSETB:        // C6.2.248
                    case STSETLB:       // C6.2.248
                    case STSETH:        // C6.2.249
                    case STSETLH:       // C6.2.249

                    case STSMAXB:       // C6.2.251
                    case STSMAXLB:      // C6.2.251
                    case STSMAXH:       // C6.2.252
                    case STSMAXLH:      // C6.2.252

                    case STSMINB:       // C6.2.254
                    case STSMINLB:      // C6.2.254
                    case STSMINH:       // C6.2.255
                    case STSMINLH:      // C6.2.255

                    case STUMAXB:       // C6.2.260
                    case STUMAXLB:      // C6.2.260
                    case STUMAXH:       // C6.2.261
                    case STUMAXLH:      // C6.2.261

                    case STUMINB:       // C6.2.263
                    case STUMINLB:      // C6.2.263
                    case STUMINH:       // C6.2.264
                    case STUMINLH:      // C6.2.264

                        {
                        // <Ws>, <Wt>, [<Xn|SP>] (documentation suggests an optional , #0 in the brackets.)
                        unsigned s = reg () ;
                        comma() ;
                        open_square() ;
                        unsigned n = reg () ;
                        close_square() ;

                        switch (mnemonic)
                            {
                            case STADDB:        instruction = 0x38200000 | 0x00000000; break;
                            case STADDLB:       instruction = 0x38200000 | 0x00400000; break;
                            case STADDH:        instruction = 0x78200000 | 0x00000000; break;
                            case STADDLH:       instruction = 0x78200000 | 0x00400000; break;

                            case STCLRB:        instruction = 0x38201000 | 0x00000000; break;
                            case STCLRLB:       instruction = 0x38201000 | 0x00400000; break;
                            case STCLRH:        instruction = 0x78201000 | 0x00000000; break;
                            case STCLRLH:       instruction = 0x78201000 | 0x00400000; break;

                            case STEORB:        instruction = 0x38202000 | 0x00000000; break;
                            case STEORLB:       instruction = 0x38202000 | 0x00400000; break;
                            case STEORH:        instruction = 0x78202000 | 0x00000000; break;
                            case STEORLH:       instruction = 0x78202000 | 0x00400000; break;

                            case STSETB:        instruction = 0x38203000 | 0x00000000; break;
                            case STSETLB:       instruction = 0x38203000 | 0x00400000; break;
                            case STSETH:        instruction = 0x78203000 | 0x00000000; break;
                            case STSETLH:       instruction = 0x78203000 | 0x00400000; break;

                            case STSMAXB:       instruction = 0x38204000 | 0x00000000; break;
                            case STSMAXLB:      instruction = 0x38204000 | 0x00400000; break;
                            case STSMAXH:       instruction = 0x78204000 | 0x00000000; break;
                            case STSMAXLH:      instruction = 0x78204000 | 0x00400000; break;

                            case STSMINB:       instruction = 0x38205000 | 0x00000000; break;
                            case STSMINLB:      instruction = 0x38205000 | 0x00400000; break;
                            case STSMINH:       instruction = 0x78205000 | 0x00000000; break;
                            case STSMINLH:      instruction = 0x78205000 | 0x00400000; break;

                            case STUMAXB:       instruction = 0x38206000 | 0x00000000; break;
                            case STUMAXLB:      instruction = 0x38206000 | 0x00400000; break;
                            case STUMAXH:       instruction = 0x78206000 | 0x00000000; break;
                            case STUMAXLH:      instruction = 0x78206000 | 0x00400000; break;

                            case STUMINB:       instruction = 0x38207000 | 0x00000000; break;
                            case STUMINLB:      instruction = 0x38207000 | 0x00400000; break;
                            case STUMINH:       instruction = 0x78207000 | 0x00000000; break;
                            case STUMINLH:      instruction = 0x78207000 | 0x00400000; break;

                            default: assembler_error() ;
                            }

                        if (64 == reg_size( s ) 
                         || 32 == reg_size( n ) )
                            error( 16, NULL ) ;

                        instruction |= (s & 0x1f) << 16;
                        instruction |= 31;
                        instruction |= (n & 0x1f) << 5;
                        }
                        break;

                    case STADD:         // C6.2.223
                    case STADDL:        // C6.2.223
                    case STCLR:         // C6.2.226
                    case STCLRL:        // C6.2.226
                    case STEOR:         // C6.2.229
                    case STEORL:        // C6.2.229
                    case STSET:         // C6.2.250
                    case STSETL:        // C6.2.250
                    case STSMAX:        // C6.2.253
                    case STSMAXL:       // C6.2.253
                    case STSMIN:        // C6.2.256
                    case STSMINL:       // C6.2.256
                    case STUMAX:        // C6.2.262
                    case STUMAXL:       // C6.2.262
                    case STUMIN:        // C6.2.265
                    case STUMINL:       // C6.2.265

                        {
                        // <Xs|Ws>, <Xt|Wt>, [<Xn|SP>]
                        // Equivalent to LD* <Xs|Ws>, <XZR|WZR>, [<Xn|SP>]
                        unsigned s = reg () ;
                        comma() ;
                        open_square() ;
                        unsigned n = reg () ;
                        close_square() ;

                        switch (mnemonic)
                            {
                            case STADD:         instruction = 0xb8200000 | 0x0000001f; break;
                            case STADDL:        instruction = 0xb8200000 | 0x0040001f; break;

                            case STCLR:         instruction = 0xb8201000 | 0x0000001f; break;
                            case STCLRL:        instruction = 0xb8201000 | 0x0040001f; break;

                            case STEOR:         instruction = 0xb8202000 | 0x0000001f; break;
                            case STEORL:        instruction = 0xb8202000 | 0x0040001f; break;

                            case STSET:         instruction = 0xb8203000 | 0x0000001f; break;
                            case STSETL:        instruction = 0xb8203000 | 0x0040001f; break;

                            case STSMAX:        instruction = 0xb8204000 | 0x0000001f; break;
                            case STSMAXL:       instruction = 0xb8204000 | 0x0040001f; break;

                            case STSMIN:        instruction = 0xb8205000 | 0x0000001f; break;
                            case STSMINL:       instruction = 0xb8205000 | 0x0040001f; break;

                            case STUMAX:        instruction = 0xb8206000 | 0x0000001f; break;
                            case STUMAXL:       instruction = 0xb8206000 | 0x0040001f; break;

                            case STUMIN:        instruction = 0xb8207000 | 0x0000001f; break;
                            case STUMINL:       instruction = 0xb8207000 | 0x0040001f; break;

                            default: assembler_error() ;
                            }

                        unsigned word_data = 32 == reg_size( s )  ;
                        unsigned size_bit = (word_data ? 0 : (1 << 30)) ;

                        if (32 == reg_size( n ) )
                            error( 16, NULL ) ;

                        instruction |= size_bit;
                        instruction |= (s & 0x1f) << 16;
                        instruction |= (n & 0x1f) << 5;
                        }
                        break;

                    case CASP:
                    case CASPA:
                    case CASPAL:
                    case CASPL:
                        {
                        // <Xs|Ws>, <Xt|Wt>, [<Xn|SP>]
                        // Equivalent to LD* <Xs|Ws>, <XZR|WZR>, [<Xn|SP>]
                        unsigned s = reg () ;
                        comma() ;
                        unsigned s1 = reg () ;
                        comma() ;
                        unsigned t = reg () ;
                        comma() ;
                        unsigned t1 = reg () ;
                        comma() ;
                        open_square() ;
                        unsigned n = reg () ;
                        close_square() ;

                        switch (mnemonic)       // C6.2.39
                            {
                            case CASP:  instruction = 0x08207c00 | 0x00000000; break; 
                            case CASPA: instruction = 0x08207c00 | 0x00400000; break; 
                            case CASPAL:instruction = 0x08207c00 | 0x00408000; break; 
                            case CASPL: instruction = 0x08207c00 | 0x00008000; break; 

                            default: assembler_error() ;
                            }

                        unsigned word_data = 32 == reg_size( s )  ;
                        unsigned size_bit = (word_data ? 0 : (1 << 30)) ;

                        if (reg_size( s ) != reg_size( t )
                         || ((s1 & 0x7f) != s + 1) // X30, XZR or W30, WZR is allowed
                         || ((t1 & 0x7f) != t + 1) // X30, XZR or W30, WZR is allowed
                         || 32 == reg_size( n ) )
                            error( 16, "Pairs of registers must have sequential numbers and all be the same width" ) ;

                        instruction |= size_bit;
                        instruction |= (s & 0x1f) << 16;
                        instruction |= (t & 0x1f) << 0;
                        instruction |= (n & 0x1f) << 5;
                        }
                        break;

                    case CCMN:
                    case CCMP:
                        {
                        switch (mnemonic)
                            {
                            case CCMN: instruction = 0x3a400000; break; // C6.2.43, C6.2.44
                            case CCMP: instruction = 0x7a400000; break; // C6.2.45, C6.2.46

                            default: assembler_error() ;
                            }

                        unsigned n = reg () ;
                        comma () ;

                        unsigned word_data = 32 == reg_size( n )  ;
                        unsigned size_bit = (word_data ? 0 : (1 << 31)) ;

                        if ('#' == nxt())
                            {
                            hash () ;
                            instruction |= 0x800; // Immediate form C6.2.43, C6.2.45
                            int imm5 = expri () ;
                            if (imm5 < 0 || imm5 > 31)
                                error( 8, NULL ) ;
                            instruction |= imm5 << 16;
                            }
                        else
                            {
                            unsigned m = reg() ;

                            if (reg_size( n ) != reg_size( m ))
                                error( 16, NULL ) ;

                            instruction |= (m & 0x1f) << 16;
                            }

                        comma () ;
                        hash () ;
                        int new_cond = expri () ;
                        if (new_cond < 0 || new_cond > 15)
                            error( 8, NULL ) ;
                        instruction |= new_cond;

                        comma () ;
                        instruction |= validated_condition() << 12;

                        instruction |= size_bit;
                        instruction |= (n & 0x1f) << 5;
                        }
                        break;

                    case CLREX:
                        {
                        int CRm = 15;
                        if ('#' == nxt())
                            {
                            hash() ;
                            CRm = expri() ;
                            if (CRm < 0 || CRm >15)
                                error( 8, NULL ) ;
                            }

                        instruction = 0xd503305f; // C6.2.49
                        instruction |= CRm << 8;
                        }
                        break;

                    case CLS:
                    case CLZ:
                        {
                        if ('D' == nxt() || 'd' == nxt())
                            {
                            instruction = validated_FB_SIMD_instruction( mnemonic );
                            break;
                            }

                        unsigned d = reg () ;
                        comma () ;
                        unsigned n = reg () ;
                        if (reg_size( d ) != reg_size( n ))
                            {
                            error( 16, NULL ) ;
                            }

                        unsigned word_data = 32 == reg_size( d )  ;
                        unsigned size_bit = (word_data ? 0 : (1 << 31)) ;

                        switch (mnemonic)
                            {
                            case CLS: instruction = 0x5ac01400; break; // C6.2.50
                            case CLZ: instruction = 0x5ac01000; break; // C6.2.51

                            default: assembler_error() ;
                            }
                        instruction |= size_bit;
                        instruction |= (d & 0x1f) << 0;
                        instruction |= (n & 0x1f) << 5;
                        }
                        break;

                    case CSEL:
                    case CSINC:
                    case CSINV:
                    case CSNEG:
                        {
                        unsigned d = reg () ;
                        comma () ;
                        unsigned n = reg () ;
                        comma () ;
                        unsigned m = reg () ;
                        comma () ;

                        if (reg_size( d ) != reg_size( n )
                         || reg_size( d ) != reg_size( m ))
                            error( 16, NULL ) ;

                        unsigned word_data = 32 == reg_size( d )  ;
                        unsigned size_bit = (word_data ? 0 : (1 << 31)) ;

                        switch (mnemonic)
                            {
                            case CSEL:  instruction = 0x1a800000; break; // C6.2.61
                            case CSINC: instruction = 0x1a800400; break; // C6.2.64
                            case CSINV: instruction = 0x5a800000; break; // C6.2.65
                            case CSNEG: instruction = 0x5a800400; break; // C6.2.66

                            default: assembler_error() ;
                            }

                        instruction |= size_bit;
                        instruction |= (d & 0x1f) << 0;
                        instruction |= (n & 0x1f) << 5;
                        instruction |= (m & 0x1f) << 16;
                        instruction |= validated_condition() << 12;
                        }
                        break;


                    case CINC:
                    case CINV:
                    case CNEG:
                        {
                        unsigned d = reg () ;
                        comma () ;
                        unsigned n = reg () ;
                        comma () ;

                        if (reg_size( d ) != reg_size( n ))
                            error( 16, NULL ) ;

                        unsigned word_data = 32 == reg_size( d )  ;
                        unsigned size_bit = (word_data ? 0 : (1 << 31)) ;

                        switch (mnemonic)
                            {
                            case CINC: instruction = 0x1a800400; break; // C6.2.47
                            case CINV: instruction = 0x5a800000; break; // C6.2.48
                            case CNEG: instruction = 0x5a800400; break; // C6.2.58

                            default: assembler_error() ;
                            }

                        instruction |= size_bit;
                        instruction |= (d & 0x1f) << 0;
                        instruction |= (n & 0x1f) << 5;
                        instruction |= (n & 0x1f) << 16; // Same register twice
                        instruction |= validated_condition() << 12;
                        instruction ^= (1 << 12) ; // Invert the condition
                        }
                        break;

                    case CSET:
                    case CSETM:
                        {
                        unsigned d = reg () ;
                        comma () ;

                        unsigned word_data = 32 == reg_size( d )  ;
                        unsigned size_bit = (word_data ? 0 : (1 << 31)) ;

                        switch (mnemonic)
                            {
                            case CSET:  instruction = 0x1a9f07e0; break; // C6.2.62
                            case CSETM: instruction = 0x5a9f03e0; break; // C6.2.63

                            default: assembler_error() ;
                            }

                        instruction |= size_bit;
                        instruction |= (d & 0x1f) << 0;
                        int cond = validated_condition() ;
                        if (cond < 0 || cond > 13)
                            error( 16, NULL ) ;
                        instruction |= cond << 12;
                        instruction ^= (1 << 12) ; // Invert the condition
                        }
                        break;

                    case CRC32B:
                    case CRC32H:
                    case CRC32W:
                    case CRC32X:
                    case CRC32CB:
                    case CRC32CH:
                    case CRC32CW:
                    case CRC32CX:
                        {
                        unsigned d = reg () ;
                        comma () ;
                        unsigned n = reg () ;
                        comma () ;
                        unsigned m = reg () ;

                        if (64 == reg_size( d ) || 64 == reg_size( n )
                         || ((64 == reg_size( m )) != (mnemonic == CRC32X || mnemonic == CRC32CX)))
                            error( 16, NULL ) ;

                        switch (mnemonic)
                            {
                            case CRC32B: instruction = 0x1ac04000; break; // C6.2.59
                            case CRC32H: instruction = 0x1ac04400; break; // C6.2.59
                            case CRC32W: instruction = 0x1ac04800; break; // C6.2.59
                            case CRC32X: instruction = 0x9ac04c00; break; // C6.2.59

                            case CRC32CB: instruction = 0x1ac05000; break; // C6.2.59
                            case CRC32CH: instruction = 0x1ac05400; break; // C6.2.59
                            case CRC32CW: instruction = 0x1ac05800; break; // C6.2.59
                            case CRC32CX: instruction = 0x9ac05c00; break; // C6.2.59

                            default: assembler_error() ;
                            }

                        instruction |= (m & 0x1f) << 16;
                        instruction |= (d & 0x1f) << 0;
                        instruction |= (n & 0x1f) << 5;
                        }
                        break;

                    case DC:
                        {
                        char *ops[] = { "IVAC", "ISW", "CSW", "CISW", "ZVA", "CVAC", "CVAU", "CIVAC", "CVAP" };
                        unsigned code[] = { 0x00000620, 0x00000640, 0x00000a40, 0x00000e40,
                                            0x00030420, 0x00030a20, 0x00030b20, 0x00030c20 };
                        int op = lookup( ops, sizeof( ops ) / sizeof( ops[0] ) ) ;
                        comma () ;
                        unsigned d = reg () ;

                        if (op < 0 || (32 == reg_size( d ) ) || !isspace( *esi ))
                            error( 16, NULL ) ;

                        instruction = 0xd5087000;       // C6.2.67
                        instruction |= code[op];
                        instruction |= (d & 0x1f) ;
                        }
                        break;

                    case IC:
                        {
                        char *ops[] = { "IALLU", "IALLUIS", "IVAU" };
                        unsigned code[] = { 0x00000500, 0x00000100, 0x00030520 };

                        int op = lookup( ops, sizeof( ops ) / sizeof( ops[0] ) ) ;

                        unsigned d = 64 | 0b11111;

                        if (',' == nxt())
                            {
                            comma () ;
                            d = reg () ;
                            }

                        if (op < 0 || (32 == reg_size( d ) ))
                            error( 16, NULL ) ;

                        instruction = 0xd5087000;       // C6.2.84
                        instruction |= code[op];
                        instruction |= (d & 0x1f) ;
                        }
                        break;

                    case DMB:
                    case DSB:
                    case ISB:
                        {
                        char *ops[] = { "SY", "ST", "LD",
                                        "ISH", "ISHST", "ISHLD",
                                        "NSH", "NSHST", "NSHLD",
                                        "OSH", "OSHST", "OSHLD" };
                        unsigned code[] = { 0b1111, 0b1110, 0b1101,
                                            0b1011, 0b1010, 0b1001,
                                            0b0111, 0b0110, 0b0101,
                                            0b0011, 0b0010, 0b0001 };

                        switch (mnemonic)
                            {
                            case DMB: instruction = 0xd50330bf; break; // C6.2.71
                            case DSB: instruction = 0xd503309f; break; // C6.2.73
                            case ISB: instruction = 0xd50330df; break; // C6.2.73

                            default: assembler_error() ;
                            }

                        int CRm = 0;

                        if ('#' == nxt() )
                            {
                            hash() ;
                            CRm = expri() ;
                            if (CRm < 0 || CRm > 15)
                                error( 8, NULL ) ;
                            }
                        else if (mnemonic == ISB)
                            {
                            // Optional SY, encoded as 15
                            if (0 == strnicmp ((const char *)esi, "SY", 2))
                                {
                                esi += 2;
                                }
                            CRm = 15;
                            }
                        else
                            {
                            int op = lookup( ops, sizeof( ops ) / sizeof( ops[0] ) ) ;

                            if (op < 0)
                                error( 16, NULL ) ;
                            CRm = code[op];
                            }

                        instruction |= CRm << 8;
                        }
                        break;

                    case LDAXP:
                        {
                        unsigned t1 = reg () ;
                        comma() ;
                        unsigned t2 = reg () ;
                        comma() ;

                        unsigned word_data = 32 == reg_size( t1 )  ;
                        unsigned size_bit = (word_data ? 0 : (1 << 30)) ;

                        open_square() ;
                        unsigned n = reg () ;
                        optional_zero_offset() ;
                        close_square () ;

                        if (32 == reg_size( n ) 
                         || reg_size( t1 ) != reg_size( t2 ))
                            error( 16, NULL ) ;

                        instruction = 0x887f8000;       // C6.2.95
                        instruction |= size_bit;
                        instruction |= (t1 & 0x1f) << 0;
                        instruction |= (t2 & 0x1f) << 10;
                        instruction |= (n & 0x1f) << 5;
                        }
                        break;


                    case LDNP:
                    case STNP:
                        {
                        switch (mnemonic)
                            {
                            case LDNP:  instruction = 0x28400000; break; // C6.2.108
                            case STNP:  instruction = 0x28000000; break; // C6.2.240

                            default: assembler_error() ;
                            }

                        unsigned t1 = reg () ;
                        comma() ;
                        unsigned t2 = reg () ;
                        comma() ;

                        unsigned word_data = 32 == reg_size( t1 ) ;
                        unsigned size_bit = (word_data ? 0 : (1 << 31)) ;

                        struct addressing addressing = read_addressing();
                        if ((addressing.mode != NO_OFFSET && addressing.mode != IMMEDIATE)
                         || reg_size( t1 ) != reg_size( t2 )
                         || (addressing.imm & 3) != 0 || (!word_data && (addressing.imm & 7) != 0)
                         || addressing.imm < -512 || addressing.imm > 504
                         || (word_data && (addressing.imm < -256 || addressing.imm > 252)))
                            error( 16, NULL ) ;

                        instruction |= size_bit;
                        instruction |= (t1 & 0x1f) << 0;
                        instruction |= (t2 & 0x1f) << 10;
                        instruction |= (addressing.n & 0x1f) << 5;
                        instruction |= ((addressing.imm >> (word_data ? 2 : 3)) & 0x7f) << 15;
                        }
                        break;

                    case LDP:
                    case LDPSW:
                    case STP:
                        {
                        switch (mnemonic)
                            {
                            case LDP:   instruction = 0x28400000; break; // C6.2.109
                            case LDPSW: instruction = 0x68400000; break; // C6.2.110
                            case STP:   instruction = 0x28000000; break; // C6.2.241

                            default: assembler_error() ;
                            }

                        unsigned t1 = reg () ;
                        comma() ;
                        unsigned t2 = reg () ;
                        comma() ;

                        // The whole point of LDPSW is to read words and extend them, hence:
                        unsigned word_data = (32 == reg_size( t1 )) || (mnemonic == LDPSW) ;
                        unsigned size_bit = (word_data ? 0 : (1 << 31)) ;

                        struct addressing addressing = read_addressing();

                        if (reg_size( t1 ) != reg_size( t2 )
                         || (addressing.imm & 3) != 0 || (!word_data && (addressing.imm & 7) != 0)
                         || addressing.imm < -512 || addressing.imm > 504
                         || (word_data && (addressing.imm < -256 || addressing.imm > 252)))
                            error( 16, NULL ) ;

                        // Post-indexed: 0x00800000
                        // Pre-indexed:  0x01800000
                        // Signed offset:0x01000000

                        switch (addressing.mode)
                            {
                            case NO_OFFSET:
                            case IMMEDIATE:     instruction |= 0x01000000; break; // Signed offset
                            case PRE_INDEXED:   instruction |= 0x01800000; break;
                            case POST_INDEXED:  instruction |= 0x00800000; break;
                            default: error( 16, NULL );
                            }

                        instruction |= size_bit;
                        instruction |= (t1 & 0x1f) << 0;
                        instruction |= (t2 & 0x1f) << 10;
                        instruction |= (addressing.n & 0x1f) << 5;
                        instruction |= ((addressing.imm >> (word_data ? 2 : 3)) & 0x7f) << 15;
                        }
                        break;

                    case LDR:
                    case STr:
                        {
                        if ('B' == nxt() || 'b' == nxt()
                         || 'H' == nxt() || 'h' == nxt()
                         || 'S' == nxt() || 's' == nxt()
                         || 'D' == nxt() || 'd' == nxt()
                         || 'Q' == nxt() || 'q' == nxt())
                            {
                            instruction = validated_FB_SIMD_instruction( mnemonic );
                            break;
                            }

                        unsigned load_store_bit = (mnemonic == STr) ? 0 : (1 << 22) ;
                        unsigned t = reg () ;
                        comma () ;

                        struct addressing addressing = read_addressing();

                        unsigned word_data = 32 == reg_size( t ) ;
                        unsigned size_bit = (word_data ? 0 : (1 << 30)) ;

                        unsigned allowed_imm = word_data ? 2 : 3;

                        switch (addressing.mode)
                            {
                            case NO_OFFSET:
                            case IMMEDIATE:
                                {
                                if (addressing.imm < 0 || (0 != (addressing.imm & 0x3))
                                 || (!word_data && (0 != (addressing.imm & 7))))
                                    {
                                    // Use STUR/LDUR
                                    instruction = 0xb8000000;
                                    instruction |= (addressing.n & 0x1f) << 5;
                                    instruction |= validated_imm9( addressing.imm ) << 12;
                                    }
                                else
                                    {
                                    instruction = 0xb9000000;
                                    instruction |= (addressing.n & 0x1f) << 5;
                                    // Only positive offsets, imm12 * size of destination register
                                    instruction |= validated_imm12( addressing.imm, word_data ? 2 : 3 ) << 10;
                                    }
                                }
                                break;
                            case PRE_INDEXED:
                                {
                                instruction = 0xb8000c00;
                                instruction |= (addressing.n & 0x1f) << 5;
                                instruction |= validated_imm9( addressing.imm ) << 12;
                                }
                                break;
                            case POST_INDEXED:
                                {
                                instruction = 0xb8000400;
                                instruction |= (addressing.n & 0x1f) << 5;
                                instruction |= validated_imm9( addressing.imm ) << 12;
                                }
                                break;

                            case REG_LSL:
                            case REG_UXTW:
                            case REG_SXTW:
                            case REG_SXTX:
                            case REG_UNMODIFIED:
                                {
                                instruction = 0xb8200800;    // C6.2.113
                                instruction |= address_extend_op[addressing.mode];
                                instruction |= (addressing.n & 0x1f) << 5;
                                instruction |= (addressing.m & 0x1f) << 16;
                                if (addressing.imm == allowed_imm)
                                    instruction |= addressing.amount_present ? (1 << 12) : 0 ; // S bit
                                else if (addressing.imm != 0)
                                    error( 8, NULL ) ; // address out of range
                                }
                                break;

                            case LITERAL:
                                { // Offset from PC
                                if (!load_store_bit)
                                    error( 16, NULL ) ; // There is no STR literal

                                load_store_bit = 0;
                                instruction = 0x18000000;
                                instruction |= validated_literal_offset( addressing.imm ) << 5;
                                break ;
                                }

                            default:
                                {
                                error (16, NULL) ;
                                }
                                break;
                            }

                        instruction |= size_bit;
                        instruction |= (t & 0x1f) << 0;
                        instruction |= load_store_bit;
                        }
                        break;

                    case LDRB:
                    case STRB:
                        {
                        unsigned load_store_bit = (mnemonic == STRB) ? 0 : (1 << 22) ;
                        unsigned t = reg () ;
                        comma () ;

                        struct addressing addressing = read_addressing();

                        if (64 == reg_size( t ))
                            error( 16, NULL ) ;

                        switch (addressing.mode)
                            {
                            case NO_OFFSET:
                            case IMMEDIATE:
                                {
                                instruction = 0x39000000;
                                instruction |= (addressing.n & 0x1f) << 5;
                                instruction |= validated_imm12( addressing.imm, 0 ) << 10;
                                }
                                break;
                            case PRE_INDEXED:
                                {
                                instruction = 0x38000c00;
                                instruction |= (addressing.n & 0x1f) << 5;
                                instruction |= validated_imm9( addressing.imm ) << 12;
                                }
                                break;
                            case POST_INDEXED:
                                {
                                instruction = 0x38000400;
                                instruction |= (addressing.n & 0x1f) << 5;
                                instruction |= validated_imm9( addressing.imm ) << 12;
                                }
                                break;

                            case REG_LSL:
                            case REG_UXTW:
                            case REG_SXTW:
                            case REG_SXTX:
                            case REG_UNMODIFIED:
                                {
                                instruction = 0x38200800;    // C6.2.116
                                instruction |= address_extend_op[addressing.mode];
                                instruction |= (addressing.n & 0x1f) << 5;
                                instruction |= (addressing.m & 0x1f) << 16;
                                if (addressing.imm == 0)
                                    instruction |= addressing.amount_present ? (1 << 12) : 0 ; // S bit
                                else if (addressing.imm != 0)
                                    error( 8, NULL ) ; // address out of range
                                }
                                break;

                            default:
                                {
                                error (16, NULL) ;
                                }
                                break;
                            }
                        instruction |= load_store_bit;
                        instruction |= (t & 0x1f) << 0;
                        }
                        break;

                    case LDRH:
                    case STRH:
                        {
                        unsigned load_store_bit = (mnemonic == STRH) ? 0 : (1 << 22) ;
                        unsigned t = reg () ;
                        comma () ;

                        struct addressing addressing = read_addressing();

                        if (64 == reg_size( t ))
                            error( 16, NULL ) ;

                        switch (addressing.mode)
                            {
                            case NO_OFFSET:
                            case IMMEDIATE:
                                {
                                instruction = 0x79000000;
                                instruction |= (addressing.n & 0x1f) << 5;
                                instruction |= validated_imm12( addressing.imm, 1 ) << 10;
                                }
                                break;
                            case PRE_INDEXED:
                                {
                                instruction = 0x78000c00;
                                instruction |= (addressing.n & 0x1f) << 5;
                                instruction |= validated_imm9( addressing.imm ) << 12;
                                }
                                break;
                            case POST_INDEXED:
                                {
                                instruction = 0x78000400;
                                instruction |= (addressing.n & 0x1f) << 5;
                                instruction |= validated_imm9( addressing.imm ) << 12;
                                }
                                break;

                            case REG_LSL:
                            case REG_UXTW:
                            case REG_SXTW:
                            case REG_SXTX:
                            case REG_UNMODIFIED:
                                {
                                instruction = 0x78200800;    // C6.2.116
                                instruction |= address_extend_op[addressing.mode];
                                instruction |= (addressing.n & 0x1f) << 5;
                                instruction |= (addressing.m & 0x1f) << 16;
                                if (addressing.imm == 2)
                                    instruction |= (1 << 12) ; // S bit
                                else if (addressing.imm != 0)
                                    error( 8, NULL ) ; // address out of range
                                }
                                break;

                            default:
                                {
                                error (16, NULL) ;
                                }
                                break;
                            }
                        instruction |= (t & 0x1f) << 0;
                        instruction |= load_store_bit;
                        }
                        break;

                    case LDRSB:
                    case LDRSH:
                    case LDRSW:
                        {
                        unsigned t = reg () ;
                        comma () ;

                        struct addressing addressing = read_addressing();

                        unsigned word_data = 32 == reg_size( t ) ;
                        unsigned allowed_imm = 0;

                        // Size of destination register
                        unsigned size_bits = (word_data ? (1 << 22) : 0) ;

                        switch (mnemonic) // Size of data read
                            {
                            case LDRSB: size_bits |= 0x00000000; allowed_imm = 0; break;
                            case LDRSH: size_bits |= 0x40000000; allowed_imm = 1; break;
                            case LDRSW: size_bits |= 0x80000000; allowed_imm = 2; break;
                            default: assembler_error();
                        };

                        switch (addressing.mode)
                            {
                            case NO_OFFSET:
                            case IMMEDIATE:
                                {
                                instruction = 0x39800000;    // C6.2.119, C6.2.121, C6.2.123
                                instruction |= (addressing.n & 0x1f) << 5;
                                instruction |= validated_imm12( addressing.imm, allowed_imm ) << 10;
                                }
                                break;
                            case PRE_INDEXED:
                                {
                                instruction = 0x38800c00;    // C6.2.119, C6.2.121, C6.2.123
                                instruction |= (addressing.n & 0x1f) << 5;
                                instruction |= validated_imm9( addressing.imm ) << 12;
                                }
                                break;
                            case POST_INDEXED:
                                {
                                instruction = 0x38800400;    // C6.2.119, C6.2.121, C6.2.123
                                instruction |= (addressing.n & 0x1f) << 5;
                                instruction |= validated_imm9( addressing.imm ) << 12;
                                }
                                break;

                            case REG_LSL:
                            case REG_SXTW:
                            // case REG_UXTW: Not a valid option for LDRSB?
                            case REG_SXTX:
                            case REG_UNMODIFIED:
                                {
                                instruction = 0x38a00800;    // C6.2.120, C6.2.122, C6.2.125
                                instruction |= address_extend_op[addressing.mode];
                                instruction |= (addressing.n & 0x1f) << 5;
                                instruction |= (addressing.m & 0x1f) << 16;
                                if (addressing.imm == allowed_imm)
                                    instruction |= addressing.amount_present ? (1 << 12) : 0 ; // S bit
                                else if (addressing.imm != 0)
                                    error( 8, NULL ) ; // address out of range
                                }
                                break;

                            case LITERAL:
                                {
                                if (mnemonic != LDRSW || word_data)
                                    error( 16, NULL ) ; // No literal option for others
                                instruction = 0x98000000;       // C6.2.124
                                size_bits = 0; // All known, in this case
                                instruction |= validated_literal_offset( addressing.imm ) << 5;
                                break ;
                                }

                            default:
                                {
                                error (16, NULL) ;
                                }
                                break;
                            }

                            instruction |= size_bits;
                            instruction |= (t & 0x1f) << 0;
                        }
                        break;

                    case LDTR:
                    case LDTRB:
                    case LDTRH:
                        {
                        unsigned t = reg () ;
                        comma () ;

                        unsigned word_data = 32 == reg_size( t ) ;

                        struct addressing addressing = read_addressing();

                        if ((addressing.mode != NO_OFFSET && addressing.mode != IMMEDIATE)
                         || (!word_data && mnemonic != LDTR))
                            error (16, NULL) ;

                        switch (mnemonic)
                            {
                            case LDTR:
                                instruction = 0xb8400800;
                                if (!word_data)
                                    instruction |= (1 << 30);
                                break; // C6.2.135
                            case LDTRB:
                                instruction = 0x38400800;
                                break; // C6.2.136
                            case LDTRH:
                                instruction = 0x78400800;
                                break; // C6.2.137
                            default: assembler_error();
                            };
                        
                        instruction |= (t & 0x1f) << 0;
                        instruction |= (addressing.n & 0x1f) << 5;
                        // Not as wide a range of addresses as LDR
                        instruction |= validated_imm9( addressing.imm ) << 12;
                        }
                        break;

                    case LDTRSB:
                    case LDTRSH:
                    case LDTRSW:
                        {
                        unsigned t = reg () ;
                        comma () ;

                        unsigned word_data = 32 == reg_size( t ) ;
                        unsigned size_bit = (word_data ? (1 << 22) : 0) ;

                        struct addressing addressing = read_addressing();

                        if ((addressing.mode != NO_OFFSET && addressing.mode != IMMEDIATE)
                         || (word_data && mnemonic == LDTRSW))
                            error (16, NULL) ;

                        switch (mnemonic)
                            {
                            case LDTRSB:
                                instruction = 0x38800800;
                                break; // C6.2.138
                            case LDTRSH:
                                instruction = 0x78800800;
                                break; // C6.2.139
                            case LDTRSW:
                                instruction = 0xb8800800;
                                size_bit = 0;
                                break; // C6.2.140
                            default: assembler_error();
                            };
                        
                        instruction |= size_bit;
                        instruction |= (t & 0x1f) << 0;
                        instruction |= (addressing.n & 0x1f) << 5;
                        instruction |= validated_imm9( addressing.imm ) << 12;
                        }
                        break;

                    case LDURB:
                    case STURB:
                    case LDURH:
                    case STURH:
                    case LDUR:
                    case STUR:
                    case LDURSB:
                    case LDURSH:
                    case LDURSW:
                    case STTR:
                    case STTRB:
                    case STTRH:
                        {
                        switch (mnemonic)
                            {
                            case LDURB: instruction = 0x38400000; break; // C6.2.148
                            case STURB: instruction = 0x38000000; break; // C6.2.267
                            case LDURH: instruction = 0x78400000; break; // C6.2.149
                            case LDURSB:instruction = 0x38800000; break; // C6.2.150
                            case LDURSH:instruction = 0x78800000; break; // C6.2.151
                            case LDURSW:instruction = 0xb8800000; break; // C6.2.152
                            case STURH: instruction = 0x78000000; break; // C6.2.268
                            case LDUR:  instruction = 0xb8400000; break; // C6.2.147
                            case STUR:  instruction = 0xb8000000; break; // C6.2.265
                            case STTR:  instruction = 0xb8000800; break; // C6.2.257
                            case STTRB: instruction = 0x38000800; break; // C6.2.258
                            case STTRH: instruction = 0x78000800; break; // C6.2.259

                            default: assembler_error() ;
                            }

                        unsigned t = reg () ;
                        comma () ;

                        struct addressing addressing = read_addressing();

                        unsigned word_data = 32 == reg_size( t )  ;
                        if (!word_data)
                            {
                            if (mnemonic != LDUR
                             && mnemonic != LDURSB
                             && mnemonic != LDURSH
                             && mnemonic != LDURSW
                             && mnemonic != STUR
                             && mnemonic != STTR)
                                {
                                error (16, NULL) ;
                                }

                            if (mnemonic == LDUR || mnemonic == STUR || mnemonic == STTR)
                                {
                                instruction |= (1 << 30) ; // Transfer 64 bits, default 32
                                }
                            }
                        else
                            {
                            if (mnemonic == LDURSW)    // Always expands to 64 bits
                                error( 16, NULL ) ;

                            if (mnemonic == LDURSB || mnemonic == LDURSH)
                                instruction |= (1 << 22) ; // Expand to 32 bits, default 64
                            }

                        if (addressing.mode != NO_OFFSET && addressing.mode != IMMEDIATE)
                            error( 16, NULL ) ;

                        instruction |= t & 0x1f;
                        instruction |= (addressing.n & 0x1f) << 5;
                        instruction |= validated_imm9( addressing.imm ) << 12;
                        }
                        break;

                    case LDXP:
                        {
                        unsigned t1 = reg () ;
                        comma () ;
                        unsigned t2 = reg () ;
                        comma () ;

                        struct addressing addressing = read_addressing();

                        unsigned word_data = 32 == reg_size( t1 )  ;
                        unsigned size_bit = (word_data ? 0 : (1 << 30)) ;

                        if ((addressing.mode != NO_OFFSET && addressing.mode != IMMEDIATE)
                         || reg_size( t1 ) != reg_size( t2 )
                         || 32 == reg_size( addressing.n )  || addressing.imm != 0)
                            error( 16, NULL ) ;

                        instruction = 0x887f0000 | size_bit;    // C6.2.153
                        instruction |= (t1 & 0x1f) << 0;
                        instruction |= (t2 & 0x1f) << 10;
                        instruction |= (addressing.n & 0x1f) << 5;
                        instruction |= validated_imm9( addressing.imm ) << 12;
                        }
                        break;

                    case STXR:
                    case STXRB:
                    case STXRH:
                        {
                        switch (mnemonic)
                            {
                            case STXR:  instruction = 0x88007c00; break;        // C6.2.270
                            case STXRB: instruction = 0x08007c00; break;        // C6.2.271
                            case STXRH: instruction = 0x48007c00; break;        // C6.2.272
                            default: assembler_error() ;
                            }

                        unsigned s = reg () ;
                        comma () ;
                        unsigned t = reg () ;
                        comma () ;

                        open_square() ;

                        unsigned n = reg () ;
                        optional_zero_offset() ;

                        close_square() ;

                        unsigned word_data = 32 == reg_size( t )  ;
                        unsigned size_bit = word_data ? 0 : (1 << 30) ;

                        if (32 == reg_size( n ) 
                         || 64 == reg_size( s ) )
                            error( 16, NULL ) ;

                        instruction |= size_bit;
                        instruction |= (t & 0x1f) << 0;
                        instruction |= (n & 0x1f) << 5;
                        instruction |= (s & 0x1f) << 16;
                        }
                        break;

                    case LDXRB:
                    case LDXRH:
                        {
                        switch (mnemonic)
                            {
                            case LDXRB: instruction = 0x085f7c00; break; // C6.2.155
                            case LDXRH: instruction = 0x485f7c00; break; // C6.2.156
                            default: assembler_error() ;
                            }

                        unsigned t = reg () ;
                        comma () ;

                        struct addressing addressing = read_addressing();

                        if ((addressing.mode != NO_OFFSET && addressing.mode != IMMEDIATE)
                         || 32 == reg_size( addressing.n ) || addressing.imm != 0)
                            error( 16, NULL ) ;

                        instruction |= (t & 0x1f) << 0;
                        instruction |= (addressing.n & 0x1f) << 5;
                        }
                        break;

                    case ASR:
                    case LSL:
                    case ROR:
                    case LSR:
                        {
                        // Alias for UBFM/SBFM or V variant
                        unsigned d = reg () ;
                        comma () ;
                        unsigned word_data = 32 == reg_size( d )  ;
                        unsigned n = reg () ;
                        comma () ;

                        if ('#' == nxt())       // Alias for UBFM/SBFM/EXTR
                            {
                            switch (mnemonic)
                                {
                                case ASR: instruction = 0x13007c00; break; // C6.2.16   (SBFM)
                                case LSL: instruction = 0x53000000; break; // C6.2.158  (UBFM)
                                case LSR: instruction = 0x53007c00; break; // C6.2.161  (UBFM)
                                case ROR: instruction = 0x13800000; break; // C6.2.204  (EXTR)
                                default: assembler_error() ;
                                }

                            hash () ;
                            unsigned shift = expri () ;

                            unsigned bits = reg_size( d ) ;

                            if (reg_size( d ) != reg_size( n )
                             || (shift >= bits))
                                error( 16, NULL ) ;

                            unsigned immr = 0, imms = 0;

                            switch (mnemonic)
                                {
                                case ASR: immr = shift;           imms = bits - 1;       break; // C6.2.16
                                case LSL: immr = (-shift) % bits; imms = (bits-1)-shift; break; // C6.2.158
                                case LSR: immr = shift;           imms = bits - 1;       break; // C6.2.161
                                case ROR: immr = (n & 0x1f) ;     imms = shift;          break; // C6.2.204
                                default: assembler_error() ;
                                }

                            // sf == N
                            unsigned size_bits = (word_data ? 0 : 0x80400000) ;
                            instruction |= size_bits;
                            instruction |= (imms & 0x3f) << 10;
                            instruction |= (immr & 0x3f) << 16;
                            }
                        else                    // Alias for V form
                            {
                            switch (mnemonic)
                                {
                                case ASR: instruction = 0x1ac02800; break; // C6.2.17
                                case LSL: instruction = 0x1ac02000; break; // C6.2.159
                                case LSR: instruction = 0x1ac02400; break; // C6.2.162
                                case ROR: instruction = 0x1ac02c00; break; // C6.2.206
                                default: assembler_error() ;
                                }

                            unsigned m = reg () ;

                            if (reg_size( d ) != reg_size( n )
                             || reg_size( n ) != reg_size( m ))
                                error( 16, NULL ) ;

                            unsigned size_bit = (word_data ? 0 : (1 << 31)) ;
                            instruction |= size_bit;
                            instruction |= (m & 0x1f) << 16;
                            }

                        instruction |= (d & 0x1f) << 0;
                        instruction |= (n & 0x1f) << 5;
                        }
                        break;

                    case EXTR:
                        {
                        unsigned d = reg () ;
                        comma () ;
                        unsigned n = reg () ;
                        comma () ;
                        unsigned m = reg () ;
                        comma () ;
                        hash () ;
                        int lsb = expri () ;

                        unsigned word_data = 32 == reg_size( d )  ;

                        if (reg_size( d ) != reg_size( n )
                         || reg_size( n ) != reg_size( m )
                         || lsb < 0 || lsb >= reg_size( d ))
                            error( 16, NULL ) ;

                        instruction = 0x13800000;

                        // sf == N
                        unsigned size_bits = (word_data ? 0 : 0x80400000) ;
                        instruction |= lsb << 10;
                        instruction |= size_bits;
                        instruction |= (m & 0x1f) << 16;
                        instruction |= (d & 0x1f) << 0;
                        instruction |= (n & 0x1f) << 5;
                        }
                        break;

                    case MADD:
                    case MSUB:
                        {
                        unsigned d = reg () ;
                        comma () ;
                        unsigned n = reg () ;
                        comma () ;
                        unsigned m = reg () ;
                        comma () ;
                        unsigned a = reg () ;

                        if (reg_size( d ) != reg_size( n )
                         || reg_size( d ) != reg_size( m )
                         || reg_size( d ) != reg_size( a ))
                            error( 16, NULL ) ;

                        unsigned word_data = 32 == reg_size( d )  ;
                        unsigned size_bit = (word_data ? 0 : (1 << 31)) ;

                        switch (mnemonic)
                            {
                            case MADD: instruction = 0x1b000000; break;    // C6.2.163
                            case MSUB: instruction = 0x1b008000; break;    // C6.2.176
                            default: assembler_error() ;
                            }
                        instruction |= size_bit;
                        instruction |= (d & 0x1f) << 0;
                        instruction |= (n & 0x1f) << 5;
                        instruction |= (m & 0x1f) << 16;
                        instruction |= (a & 0x1f) << 10;
                        }
                        break;

                    case SMADDL:
                    case SMSUBL:
                    case UMADDL:
                    case UMSUBL:
                        {
                        unsigned d = reg () ;
                        comma () ;
                        unsigned n = reg () ;
                        comma () ;
                        unsigned m = reg () ;
                        comma () ;
                        unsigned a = reg () ;

                        if (32 == reg_size( d ) 
                         || 64 == reg_size( n ) 
                         || 64 == reg_size( m ) 
                         || 32 == reg_size( a ) )
                            error( 16, "Registers must be: Xd, Wn, Wm, Xa" ) ;

                        switch (mnemonic)
                            {
                            case SMADDL: instruction = 0x9b200000; break;    // C6.2.215
                            case SMSUBL: instruction = 0x9b208000; break;    // C6.2.218
                            case UMADDL: instruction = 0x9ba00000; break;    // C6.2.297
                            case UMSUBL: instruction = 0x9ba08000; break;    // C6.2.299
                            default: assembler_error() ;
                            }
                        instruction |= (d & 0x1f) << 0;
                        instruction |= (n & 0x1f) << 5;
                        instruction |= (m & 0x1f) << 16;
                        instruction |= (a & 0x1f) << 10;
                        }
                        break;

                    case SMNEGL:
                    case SMULL:
                    case UMNEGL:
                    case UMULL:
                        {
                        if ((mnemonic == SMULL || mnemonic == UMULL) && ('D' == nxt() || 'd' == nxt()))
                            {
                            instruction = validated_FB_SIMD_instruction( mnemonic );
                            break;
                            }

                        unsigned d = reg () ;
                        comma () ;
                        unsigned n = reg () ;
                        comma () ;
                        unsigned m = reg () ;

                        if (32 == reg_size( d ) 
                         || 64 == reg_size( n ) 
                         || 64 == reg_size( m ) )
                            error( 16, "Registers must be: Xd, Wn, Wm" ) ;

                        switch (mnemonic)
                            {
                            case SMNEGL:instruction = 0x9b20fc00; break;    // C6.2.217
                            case SMULL: instruction = 0x9b207c00; break;    // C6.2.220
                            case UMULL: instruction = 0x9ba07c00; break;    // C6.2.301
                            case UMNEGL:instruction = 0x9ba0fc00; break;    // C6.2.298
                            default: assembler_error() ;
                            }
                        instruction |= (d & 0x1f) << 0;
                        instruction |= (n & 0x1f) << 5;
                        instruction |= (m & 0x1f) << 16;
                        }
                        break;

                    case MOV:
                        {
                        // Variations:
                        // C6.2.165 MOV (to/from SP)
                        // C6.2.169 MOV (register)

                        // C6.2.166 MOV (inverted wide immediate)
                        // C6.2.167 MOV (wide immediate)
                        // C6.2.168 MOV (bitmask immediate)

                        if ('D' == nxt() || 'd' == nxt())
                            {
                            instruction = validated_FB_SIMD_instruction( mnemonic );
                            break;
                            }

                        unsigned d = reg () ;
                        comma () ;

                        unsigned word_data = 32 == reg_size( d )  ;

                        if (nxt() == '#')
                            {
                            esi++;
                            long long imm = expri () ;

                            if (is_sp( d ))
                                {
                                // Alias of ORR, needed to write to SP. Why you can MOV to the zero register, IDK.
                                instruction = 0x320003e0;       // C6.2.168
                                instruction |= validated_N_immr_imms( imm, word_data ) << 10 ;
                                }
                            else
                                {
                                unsigned long long all_ones = word_data ? 0xffffffff : 0xffffffffffffffffull;

                                unsigned long long eor = 0; // Don't invert

                                instruction = 0;
                                // Are all the relevant bits within a 16-bit area?

                                // First, all other bits are zeros? (Use MOVZ)
                                if (imm == (imm & 0xffffull))
                                    {
                                    instruction = 0x52800000; // No shift
                                    }
                                else if (imm == (imm & 0xffff0000ull))
                                    {
                                    imm = imm >> 16;
                                    instruction = 0x52a00000; // 16 bit left shift
                                    }
                                else if (!word_data && imm == (imm & 0xffff00000000ull))
                                    {
                                    imm = imm >> 32;
                                    instruction = 0x52c00000; // 32 bit left shift
                                    }
                                else if (!word_data && imm == (imm & 0xffff000000000000ull))
                                    {
                                    imm = imm >> 48;
                                    instruction = 0x52e00000; // 48 bit left shift
                                    }
                                // Or all other bits are ones (use MOVN)
                                else if ((imm | 0xffffull) == all_ones)
                                    {
                                    instruction = 0x12800000; // No shift
                                    eor = all_ones;
                                    }
                                else if ((imm | 0xffff0000ull) == all_ones)
                                    {
                                    imm = imm >> 16;
                                    eor = all_ones;
                                    instruction = 0x12a00000; // 16 bit left shift
                                    }
                                else if (!word_data && (imm | 0xffff00000000ull) == all_ones)
                                    {
                                    imm = imm >> 32;
                                    eor = all_ones;
                                    instruction = 0x12c00000; // 32 bit left shift
                                    }
                                else if (!word_data && (imm | 0xffff000000000000ull) == all_ones)
                                    {
                                    imm = imm >> 48;
                                    eor = all_ones;
                                    instruction = 0x12e00000; // 48 bit left shift
                                    }

                                if (instruction != 0)
                                    {
                                    imm = imm ^ eor;
                                    instruction |= ((imm & 0xffff) << 5) ;
                                    }
                                else
                                    {
                                    // Alias of ORR, needed to write to SP. Why you can MOV to the zero register, IDK.
                                    instruction = 0x320003e0;       // C6.2.168
                                    instruction |= validated_N_immr_imms( imm, word_data ) << 10 ;
                                    }
                                }
                            }
                        else
                            {
                            unsigned s = reg () ;

                            if (reg_size( d ) != reg_size( s ))
                                error( 16, NULL ) ;

                            if (is_sp( d ) || is_sp( s ))
                                { // To or from SP
                                instruction = 0x11000000;  // C6.2.165 MOV (to/from SP)
                                instruction |= (s & 0x1f) << 5;         // Rn
                                }
                            else
                                { // Other register
                                instruction = 0x2a0003e0;  // C6.2.169 MOV (register)
                                instruction |= (s & 0x1f) << 16;        // Rm
                                }
                            }

                        unsigned size_bit = (word_data ? 0 : (1 << 31)) ;

                        instruction |= size_bit;
                        instruction |= (d & 0x1f) << 0;
                        }
                        break;

                    case MOVK:
                    case MOVN:
                    case MOVZ:
                        {
                        unsigned d = reg () ;
                        comma () ;

                        unsigned word_data = 32 == reg_size( d )  ;

                        switch (mnemonic)
                            {
                            case MOVK: instruction = 0x72800000; break;       // C6.2.170
                            case MOVN: instruction = 0x12800000; break;       // C6.2.171
                            case MOVZ: instruction = 0x52800000; break;       // C6.2.172
                            default: assembler_error() ;
                            }

                        unsigned size_bit = (word_data ? 0 : (1 << 31)) ;

                        instruction |= size_bit;
                        instruction |= (d & 0x1f) << 0;

                        hash() ;

                        long long imm = expri () ;
                        int shift = 0;

                        if (imm < 0 || imm > 65535)
                            error( 8, NULL ) ;

                        instruction |= imm << 5;

                        if (nxt() == ',')
                            {
                            comma() ;
                            nxt() ;

                            if (strnicmp ((const char *)esi, "LSL", 3) == 0)
                                {
                                esi += 3;

                                hash() ;

                                shift = expri () ;
                                if (shift != 0 && shift != 16 && shift != 32 && shift != 48)
                                    error ( 16, "Shift must be a multiple of 16, up to 48" ) ;
                                shift = (shift / 16) & 3;
                                instruction |= shift << 21;
                                }
                            else
                                {
                                error( 16, NULL ) ;
                                }
                            }
                        }
                        break;

                    case MRS:
                        {
                        unsigned t = reg () ;
                        comma () ;

                        if (32 == reg_size( t ) )
                            error( 16, NULL );

                        instruction = 0xd5300000;
                        instruction |= validated_system_register();
                        instruction |= (t & 0x1f) << 0;
                        }
                        break;

                    case MSR:
                        {
                        char *immediate_targets[] = { "SPSel", "DAIFSet", "DAIFClr", "UAO", "PAN" };
                        int entry = lookup( immediate_targets, sizeof( immediate_targets ) / sizeof( immediate_targets[0] ) );
                        unsigned code[] = { 0b000 << 16 | 0b101 << 5, // SPSel
                                            0b011 << 16 | 0b110 << 5, // DAIFSet
                                            0b011 << 16 | 0b111 << 5, // DAIFClr
                                            0b000 << 16 | 0b011 << 5, // UAO
                                            0b000 << 16 | 0b100 << 5  // PAN
                                            };

                        if (entry >= 0 && !isspace( *esi))
                            error( 16, "Unnown tlbi_op" );

                        if (entry >= 0)
                            {
                            instruction = 0xd5300000;   // C6.2.174
                            instruction |= code[entry];
                            }
                        else
                            {
                            instruction = 0xd5100000;   // C6.2.175
                            instruction |= validated_system_register();
                            }

                        comma () ;
                        unsigned t = reg () ;
                        instruction |= (t & 0x1f) << 0;

                        if (32 == reg_size( t ) )
                            error( 16, NULL );
                        }
                        break;

                    case MVN:
                    case NEG:
                    case NEGS:
                        {
                        unsigned d = reg () ;

                        unsigned word_data = 32 == reg_size( d )  ;

                        comma () ;
                        unsigned m = reg () ;

                        int shift = 0;
                        int amount = 0;

                        if (',' == nxt())
                            {
                            comma () ;
                            shift = shift_type() ;
                            hash () ;
                            amount = expri () ;
                            if (amount < 0 || amount > 63 || (amount > 31 && word_data))
                                {
                                error ( 16, NULL ) ;
                                }
                            // Only MVN of these three allows ROR shifts
                            if (shift == 3 && mnemonic != MVN)
                                {
                                error ( 16, NULL ) ;
                                }
                            }

                        switch (mnemonic)
                            {
                            case MVN : instruction = 0x2a2003e0; break;       // C6.2.178
                            case NEG : instruction = 0x4b0003e0; break;       // C6.2.179
                            case NEGS: instruction = 0x6b0003e0; break;       // C6.2.180
                            default: assembler_error() ;
                            }

                        unsigned size_bit = (word_data ? 0 : (1 << 31)) ;

                        instruction |= size_bit;
                        instruction |= (d & 0x1f) << 0;
                        instruction |= (m & 0x1f) << 16;
                        instruction |= shift << 22;
                        instruction |= amount << 10;
                        }
                        break;


                    case NGC:
                    case NGCS:
                        {
                        unsigned d = reg () ;

                        unsigned word_data = 32 == reg_size( d )  ;

                        comma () ;
                        unsigned m = reg () ;

                        switch (mnemonic)
                            {
                            case NGC : instruction = 0x5a0003e0; break;       // C6.2.181
                            case NGCS: instruction = 0x7a0003e0; break;       // C6.2.182
                            default: assembler_error() ;
                            }

                        unsigned size_bit = (word_data ? 0 : (1 << 31)) ;

                        instruction |= size_bit;
                        instruction |= (d & 0x1f) << 0;
                        instruction |= (m & 0x1f) << 16;
                        }
                        break;

                    case ORN:
                        {
                        unsigned d = reg () ;
                        comma () ;
                        unsigned n = reg () ;
                        comma () ;
                        unsigned m = reg () ;

                        unsigned word_data = 32 == reg_size( d )  ;

                        int shift = 0;
                        int amount = 0;

                        if (',' == nxt())
                            {
                            comma () ;
                            shift = shift_type() ;
                            hash () ;
                            amount = expri () ;
                            if (amount < 0 || amount > 63 || (amount > 31 && word_data))
                                {
                                error ( 16, NULL ) ;
                                }
                            }

                        instruction = 0x2a200000;       // C6.2.184

                        if (reg_size( d ) != reg_size( n )
                         || reg_size( d ) != reg_size( m ))
                            error( 16, NULL ) ;

                        unsigned size_bit = (word_data ? 0 : (1 << 31)) ;

                        instruction |= size_bit;
                        instruction |= (d & 0x1f) << 0;
                        instruction |= (n & 0x1f) << 5;
                        instruction |= (m & 0x1f) << 16;
                        instruction |= shift << 22;
                        instruction |= amount << 10;
                        }
                        break;

                    case AND:
                    case ANDS:
                    case EOR:
                    case ORR:
                    case TST:
                    case EON:
                        {
                        if ((mnemonic == ORR || mnemonic == AND || mnemonic == EOR)
                         && ('D' == nxt() || 'd' == nxt()))
                            {
                            instruction = validated_FB_SIMD_instruction( mnemonic );
                            break;
                            }

                        unsigned d;
                        if (mnemonic != TST)
                            {
                            d = reg () ;
                            comma () ;
                            }

                        unsigned n = reg () ;
                        comma () ;

                        if (mnemonic == TST)
                            {
                            d = 0x1f | reg_size( n ) | REGISTER_IS_ZERO; 
                            }

                        if (reg_size( d ) != reg_size( n ))
                            error( 16, NULL ) ;

                        unsigned word_data = 32 == reg_size( d )  ;
                        unsigned size_bit = (word_data ? 0 : (1 << 31)) ;

                        if (mnemonic != EON && '#' == nxt())
                            {
                            switch (mnemonic)
                                {
                                case AND:       instruction = 0x12000000; break; // C6.2.11
                                case TST:
                                case ANDS:      instruction = 0x72000000; break; // C6.2.13
                                case EOR:       instruction = 0x52000000; break; // C6.2.75
                                case ORR:       instruction = 0x32000000; break; // C6.2.185
                                default: assembler_error() ;
                                }
                            
                            esi++;
                            long long imm = expri () ;
                            instruction |= validated_N_immr_imms( imm, word_data ) << 10;
                            }
                        else
                            {
                            switch (mnemonic)
                                {
                                case AND:       instruction = 0x0a000000; break; // C6.2.12
                                case TST:
                                case ANDS:      instruction = 0x6a000000; break; // C6.2.14
                                case EON:       instruction = 0x4a200000; break; // C6.2.74
                                case EOR:       instruction = 0x4a000000; break; // C6.2.76
                                case ORR:       instruction = 0x2a000000; break; // C6.2.186
                                default: assembler_error() ;
                                }
                            
                            unsigned m = reg () ;

                            int shift = 0;
                            int amount = 0;

                            if (',' == nxt())
                                {
                                comma () ;
                                shift = shift_type() ;
                                hash () ;
                                amount = expri () ;
                                if (amount < 0 || amount > 63 || (amount > 31 && word_data))
                                    {
                                    error ( 16, NULL ) ;
                                    }
                                }

                            if (reg_size( d ) != reg_size( n )
                             || reg_size( d ) != reg_size( m ))
                                error( 16, NULL ) ;

                            instruction |= (m & 0x1f) << 16;
                            instruction |= shift << 22;
                            instruction |= amount << 10;
                            }

                        instruction |= size_bit;
                        instruction |= (d & 0x1f) << 0;
                        instruction |= (n & 0x1f) << 5;
                        }
                        break;

                    case PACDA:
                    case PACDB:
                    case PACIA:
                    case PACIB:
                        {
                        unsigned d = reg () ;
                        comma () ;
                        unsigned n = reg () ;
                        switch (mnemonic)
                            {
                            case PACDA: instruction = 0xdac10800; break;       // C6.2.187
                            case PACDB: instruction = 0xdac10c00; break;       // C6.2.188
                            case PACIA: instruction = 0xdac10000; break;       // C6.2.190
                            case PACIB: instruction = 0xdac10400; break;       // C6.2.191
                            default: assembler_error() ;
                            }

                        if (32 == reg_size( d ) 
                         || 32 == reg_size( n ) 
                         || (n == 31 && 0 != (n & REGISTER_IS_ZERO))) // XZR not allowed
                            {
                            error( 16, NULL ) ;
                            }

                        instruction |= (d & 0x1f) << 0;
                        instruction |= (n & 0x1f) << 5;
                        }
                        break;

                    case PACDZA:
                    case PACDZB:
                    case PACIZA:
                    case PACIZB:
                        {
                        unsigned d = reg () ;
                        switch (mnemonic)
                            {
                            case PACDZA: instruction = 0xdac12be0; break;       // C6.2.187
                            case PACDZB: instruction = 0xdac12fe0; break;       // C6.2.188
                            case PACIZA: instruction = 0xdac123e0; break;       // C6.2.190
                            case PACIZB: instruction = 0xdac127e0; break;       // C6.2.191
                            default: assembler_error() ;
                            }
                        
                        instruction |= (d & 0x1f) << 0;
                        if (32 == reg_size( d ) )
                            {
                            error( 16, NULL ) ;
                            }
                        }
                        break;

                    case PACGA:
                        {
                        unsigned d = reg () ;
                        comma () ;
                        unsigned n = reg () ;
                        comma () ;
                        unsigned m = reg () ;

                        switch (mnemonic)
                            {
                            case PACGA: instruction = 0x9ac03000; break;       // C6.2.189
                            default: assembler_error() ;
                            }

                        if (32 == reg_size( d ) 
                         || 32 == reg_size( n ) 
                         || 32 == reg_size( m ) 
                         || (m == 31 && 0 != (m & REGISTER_IS_ZERO))) // XZR not allowed
                            {
                            error( 16, NULL ) ;
                            }

                        instruction |= (d & 0x1f) << 0;
                        instruction |= (n & 0x1f) << 5;
                        instruction |= (m & 0x1f) << 16;
                        }
                        break;

                    case PRFM:
                        {
                        char *prfops_type[] = { "PLD", "PLI", "PST" };
                        char *prfops_target[] = { "L1", "L2", "L3" };
                        char *prfops_policy[] = { "KEEP", "STRM" };

                        int prfop = 0;

                        if ('#' == nxt() )
                            {
                            hash();
                            prfop = expri () ;
                            }
                        else
                            {
                            prfop = (lookup( prfops_type, 3 ) << 3)
                                  | (lookup( prfops_target, 3 ) << 1)
                                  | (lookup( prfops_policy, 2 ) << 0) ;
                            }

                        if (prfop < 0 || prfop > 31)
                            error( 16, NULL ) ;

                        comma () ;

                        struct addressing addressing = read_addressing();

                        if (addressing.mode == LITERAL)
                            {
                            // Literal
                            instruction = 0xd8000000;           // C6.2.193
                            instruction |= validated_literal_offset( addressing.imm ) << 5;
                            }
                        else if (addressing.mode == NO_OFFSET || addressing.mode == IMMEDIATE)
                            {
                            if (addressing.imm >= 0 && addressing.imm <= 32760 && (addressing.imm & 7) == 0)
                                {
                                instruction = 0xf9800000;       // C6.2.192
                                instruction |= validated_imm12( addressing.imm, 3 ) << 10;
                                }
                            else
                                {
                                instruction = 0xf8800000;       // C6.2.195
                                instruction |= validated_imm9( addressing.imm ) << 12;
                                }
                            }
                        else if (addressing.mode < LITERAL)
                            { // register
                            instruction = 0xf8a00800;           // C6.2.194
                            instruction |= address_extend_op[addressing.mode];
                            instruction |= (addressing.m & 0x1f) << 16;
                            if (addressing.imm == 3)
                                instruction |= (1 << 12) ;
                            else if (addressing.imm != 0
                                  || (!addressing.amount_present && addressing.mode == REG_LSL))
                                error( 8, "Only shifts #0 and #3 allowed, must be present with LSL" ) ;
                            }
                        else
                            error( 16, NULL ) ;

                        instruction |= (addressing.n & 0x1f) << 5;
                        instruction |= prfop << 0;
                        }
                        break;

                    case REV:
                    case REV16:
                    case REV32:
                    case REV64:
                    case RBIT:
                        {
                        unsigned d = reg () ;
                        comma () ;
                        unsigned n = reg () ;
                        switch (mnemonic)
                            {
                            case REV:   instruction = 0x5ac00800; break; // C6.2.200
                            case REV16: instruction = 0x5ac00400; break; // C6.2.201
                            case REV32: instruction = 0xdac00800; break; // C6.2.202
                            case REV64: instruction = 0xdac00c00; break; // C6.2.203
                            case RBIT:  instruction = 0x5ac00000; break; // C6.2.197

                            default: assembler_error() ;
                            }

                        switch (mnemonic)
                            {
                            case REV:   if (64 == reg_size( n ) ) instruction |= (1 << 31) | (1 << 10) ; break;
                            case RBIT:
                            case REV16: if (64 == reg_size( n ) ) instruction |= (1 << 31) ; break;

                            case REV32: 
                            case REV64: if (32 == reg_size( n ) ) error( 16, NULL ) ; break;

                            default: assembler_error() ;
                            }

                        if (reg_size( d ) != reg_size( n ))
                            error( 16, NULL ) ;

                        instruction |= (d & 0x1f) << 0;
                        instruction |= (n & 0x1f) << 5;
                        }
                        break;

                    case SMULH:
                    case UMULH:
                        {
                        unsigned d = reg () ;
                        comma () ;
                        unsigned n = reg () ;
                        comma () ;
                        unsigned m = reg () ;
                        switch (mnemonic)
                            {
                            case SMULH: instruction = 0x9b407c00; break; // C6.2.200
                            case UMULH: instruction = 0x9bc07c00; break; // C6.2.300

                            default: assembler_error() ;
                            }

                        if (32 == reg_size( d ) 
                         || 32 == reg_size( n ) 
                         || 32 == reg_size( m ) )
                            error( 16, NULL ) ;

                        instruction |= (d & 0x1f) << 0;
                        instruction |= (n & 0x1f) << 5;
                        instruction |= (m & 0x1f) << 16;
                        }
                        break;


                    case SBFM:  // C6.2.210
                    case UBFM:  // C6.2.294

                    // Alias...
                    case SBFIZ: // C6.2.209,
                    case UBFIZ: // C6.2.293
                    case SBFX:  // C6.2.211
                    case UBFX:  // C6.2.295
                        {
                        // Two registers, two constants
                        instruction = 0x13000000;
                        if (mnemonic == UBFIZ || mnemonic == UBFM || mnemonic == UBFX) // Unsigned
                            instruction |= 0x40000000;

                        unsigned d = reg () ;
                        comma () ;
                        unsigned n = reg () ;
                        comma () ;
                        hash () ;
                        int c1 = expri () ;
                        comma() ;
                        hash () ;
                        int c2 = expri () ;

                        unsigned word_data = 32 == reg_size( d )  ;
                        unsigned size_bits = word_data ? 0 : 0x80400000; // sf != N is a reserved value

                        switch (mnemonic)
                            {
                            case UBFIZ:
                            case SBFIZ: c1 = ((unsigned) -c1) % reg_size( d ); c2 --; break;

                            case UBFX:
                            case SBFX: c2 += c1 - 1; break; 

                            case SBFM: break;
                            case UBFM: break;
                            default: assembler_error() ;
                            }

                        if (reg_size( d ) != reg_size( n )
                         || c1 < 0 || c1 > 63 || (word_data && c1 > 31)
                         || c2 < 0 || c2 > 63 || (word_data && c2 > 31))
                            {
                            error( 16, NULL ) ;
                            }

                        instruction |= size_bits;
                        instruction |= (d & 0x1f) << 0;
                        instruction |= (n & 0x1f) << 5;
                        instruction |= c1 << 16;
                        instruction |= c2 << 10;
                        }
                        break;

                    case SXTB:
                    case SXTH:
                    case SXTW:
                    case UXTB:
                    case UXTH:
                    case UXTW: // This is an 'unofficial' mnemonic
                        {
                        // Two registers, implicit constants

                        switch (mnemonic)
                            {
                            case SXTB: instruction = 0x13001c00; break; // C6.2.283
                            case SXTH: instruction = 0x13003c00; break; // C6.2.284
                            case SXTW: instruction = 0x93407c00; break; // C6.2.285
                            case UXTB: instruction = 0x53001c00; break; // C6.2.302
                            case UXTH: instruction = 0x53003c00; break; // C6.2.303
                            case UXTW: instruction = 0x53000000; break;
                            default: assembler_error() ;
                            }

                        unsigned d = reg () ;
                        comma () ;
                        unsigned n = reg () ;
                        int c1 = 0;
                        int c2 = 0; // Initial value to avoid compiler warning

                        unsigned word_data = 32 == reg_size( d )  ;
                        unsigned size_bits = word_data ? 0 : ((1 << 31) | (1 << 22)) ;

                        switch (mnemonic)
                            {
                            case UXTB:
                            case SXTB: c2 = 7; break;
                            case UXTH:
                            case SXTH: c2 = 15; break;
                            case SXTW:
                            case UXTW: c2 = 31; break;
                            default: assembler_error() ;
                            }

                        if (reg_size( d ) < reg_size( n ) 
                         || c1 < 0 || c1 >= reg_size( d )
                         || c2 < 0 || c2 >= reg_size( d )
                         || ( word_data && mnemonic == SXTW)
                         || ( word_data && mnemonic == UXTW)
                         || (!word_data && mnemonic == UXTB)
                         || (!word_data && mnemonic == UXTH))
                            {
                            error( 16, NULL ) ;
                            }

                        instruction |= size_bits;
                        instruction |= (d & 0x1f) << 0;
                        instruction |= (n & 0x1f) << 5;
                        instruction |= c1 << 16;
                        instruction |= c2 << 10;
                        }
                        break;

                    case DRPS:
                        {
                        instruction = 0xd6bf03e0;
                        }
                        break;

                    case PACIA1716:
                        {
                        instruction = 0xd503211f;
                        }
                        break;

                    case PACIASP:
                        {
                        instruction = 0xd503233f;
                        }
                        break;

                    case PACIAZ:
                        {
                        instruction = 0xd503231f;
                        }
                        break;

                    case PACIB1716:
                        {
                        instruction = 0xd503215f;
                        }
                        break;

                    case PACIBSP:
                        {
                        instruction = 0xd503237f;
                        }
                        break;

                    case PACIBZ:
                        {
                        instruction = 0xd503235f;
                        }
                        break;

                    case RET:
                        {
                        instruction = 0xd65f0000;       // C6.2.198
                        int link = 30;
                        if ('X' == nxt() || 'x' == nxt ())
                            link = reg () ;
                        instruction |= (link & 0x1f) << 5;
                        }
                        break;

                    case ERET:
                        {
                        instruction = 0xd69f03e0;       // C6.2.77
                        }
                        break;

                    case RETAA:
                        {
                        instruction = 0xd65f0bff;       // C6.2.199
                        }
                        break;

                    case RETAB:
                        {
                        instruction = 0xd65f0fff;       // C6.2.199
                        }
                        break;

                    case ERETAA:
                        {
                        instruction = 0xd69f0bff;       // C6.2.78
                        }
                        break;

                    case ERETAB:
                        {
                        instruction = 0xd69f0fff;       // C6.2.78
                        }
                        break;

                    case HINT:
                        {
                        instruction = 0xd503201f;       // C6.2.81
                        hash () ;
                        int imm = expri () ;
                        if (imm < 0 || imm > 127)
                            error( 8, NULL ) ;
                        instruction |= imm << 5;
                        }
                        break;

                    case NOP:
                        {
                        instruction = 0xd503201f;       // C6.2.81
                        instruction |= (0 << 5) ;        // C6.2.183
                        }
                        break;

                    case YIELD:
                        {
                        instruction = 0xd503201f;       // C6.2.81
                        instruction |= (1 << 5) ;        // C6.2.307
                        }
                        break;

                    case WFE:
                        {
                        instruction = 0xd503201f;       // C6.2.81
                        instruction |= (2 << 5) ;        // C6.2.304
                        }
                        break;

                    case WFI:
                        {
                        instruction = 0xd503201f;       // C6.2.81
                        instruction |= (3 << 5) ;        // C6.2.305
                        }
                        break;

                    case SEV:
                        {
                        instruction = 0xd503201f;       // C6.2.81
                        instruction |= (4 << 5) ;        // C6.2.213
                        }
                        break;

                    case SEVL:
                        {
                        instruction = 0xd503201f;       // C6.2.81
                        instruction |= (5 << 5) ;        // C6.2.214
                        }
                        break;

                    case ESB:
                        {
                        instruction = 0xd503201f;       // C6.2.81
                        instruction |= (16 << 5) ;       // C6.2.79
                        }
                        break;

                    case PSB:
                        {
                        nxt () ;
                        if (strnicmp ((const char *)esi, "CSYNC", 5) == 0)
                            esi += 5;
                        else
                            error( 16, "" ) ;
                        instruction = 0xd503201f;       // C6.2.81
                        instruction |= (17 << 5) ;       // C6.2.196
                        }
                        break;

                    case XPACD:
                        {
                        unsigned d = reg () ;
                        if (32 == reg_size( d ) )
                            error( 16, NULL ) ;
                        instruction = 0xdac147e0;       // C6.2.306
                        instruction |= (d & 0x1f) << 0;
                        }
                        break;

                    case XPACI:
                        {
                        unsigned d = reg () ;
                        if (32 == reg_size( d ) )
                            error( 16, NULL ) ;
                        instruction = 0xdac143e0;       // C6.2.306
                        instruction |= (d & 0x1f) << 0;
                        }
                        break;

                    case XPACLRI:
                        {
                        instruction = 0xd50320ff;       // C6.2.306
                        }
                        break;

                    // Floating point and SIMD operations
                    case ABS:
                        {
                        instruction = 0x5ee0b800;
                        }
                        break;

                    case ADDHN:
                    case ADDHN2:
                    case ADDP:
                    case ADDV:
                    case AESD:
                    case AESE:
                    case AESIMC:
                    case AESMC:
                    case BCAX:
                    case BIF:
                    case BIT:
                    case BSL:
                    case CMEQ:
                    case CMGE:
                    case CMGT:
                    case CMHI:
                    case CMHS:
                    case CMLE:
                    case CMLT:
                    case CMTST:
                    case CNT:
                    case DUP:
                    case EOR3:
                    case EXT:
                    case FABD:
                    case FABS:
                    case FACGE:
                    case FACGT:
                    case FADD:
                    case FADDP:
                    case FCADD:
                    case FCCMP:
                    case FCCMPE:
                    case FCMEQ:
                    case FCMGE:
                    case FCMGT:
                    case FCMLA:
                    case FCMLE:
                    case FCMLT:
                    case FCMP:
                    case FCMPE:
                    case FCSEL:
                    case FCVT:
                    case FCVTAS:
                    case FCVTAU:
                    case FCVTL:
                    case FCVTL2:
                    case FCVTMS:
                    case FCVTMU:
                    case FCVTN:
                    case FCVTN2:
                    case FCVTNS:
                    case FCVTNU:
                    case FCVTPS:
                    case FCVTPU:
                    case FCVTXN:
                    case FCVTXN2:
                    case FCVTZS:
                    case FCVTZU:
                    case FDIV:
                    case FJCVTZS:
                    case FMADD:
                    case FMAX:
                    case FMAXNM:
                    case FMAXNMP:
                    case FMAXNMV:
                    case FMAXP:
                    case FMAXV:
                    case FMIN:
                    case FMINNM:
                    case FMINNMP:
                    case FMINNMV:
                    case FMINP:
                    case FMINV:
                    case FMLA:
                    case FMLAL:
                    case FMLAL2:
                    case FMLS:
                    case FMLSL:
                    case FMLSL2:
                    case FMOV:
                    case FMSUB:
                    case FMUL:
                    case FMULX:
                    case FNEG:
                    case FNMADD:
                    case FNMSUB:
                    case FNMUL:
                    case FRECPE:
                    case FRECPS:
                    case FRECPX:
                    case FRINTA:
                    case FRINTI:
                    case FRINTM:
                    case FRINTN:
                    case FRINTP:
                    case FRINTX:
                    case FRINTZ:
                    case FRSQRTE:
                    case FRSQRTS:
                    case FSQRT:
                    case FSUB:
                    case INS:
                    case LD1:
                    case LD1R:
                    case LD2:
                    case LD2R:
                    case LD3:
                    case LD3R:
                    case LD4:
                    case LD4R:
                    case MLA:
                    case MLS:
                    case MOVI:
                    case MVNI:
                    case NOT:
                    case PMUL:
                    case PMULL:
                    case PMULL2:
                    case PSBCSYNC:
                    case RADDHN:
                    case RADDHN2:
                    case RAX1:
                    case RSHRN:
                    case RSHRN2:
                    case RSUBHN:
                    case RSUBHN2:
                    case SABA:
                    case SABAL:
                    case SABAL2:
                    case SABD:
                    case SABDL:
                    case SABDL2:
                    case SADALP:
                    case SADDL:
                    case SADDL2:
                    case SADDLP:
                    case SADDLV:
                    case SADDW:
                    case SADDW2:
                    case SCVTF:
                    case SDOT:
                    case SHA1C:
                    case SHA1H:
                    case SHA1M:
                    case SHA1P:
                    case SHA1SU0:
                    case SHA1SU1:
                    case SHA256H:
                    case SHA256H2:
                    case SHA256SU0:
                    case SHA256SU1:
                    case SHA512H:
                    case SHA512H2:
                    case SHA512SU0:
                    case SHA512SU1:
                    case SHADD:
                    case SHL:
                    case SHLL:
                    case SHLL2:
                    case SHRN:
                    case SHRN2:
                    case SHSUB:
                    case SLI:
                    case SM3PARTW1:
                    case SM3PARTW2:
                    case SM3SS1:
                    case SM3TT1A:
                    case SM3TT1B:
                    case SM3TT2A:
                    case SM3TT2B:
                    case SM4E:
                    case SM4EKEY:
                    case SMAX:
                    case SMAXP:
                    case SMAXV:
                    case SMIN:
                    case SMINP:
                    case SMINV:
                    case SMLAL:
                    case SMLAL2:
                    case SMLSL:
                    case SMLSL2:
                    case SMOV:
                    case SMULL2:
                    case SQABS:
                    case SQADD:
                    case SQDMLAL:
                    case SQDMLAL2:
                    case SQDMLSL:
                    case SQDMLSL2:
                    case SQDMULH:
                    case SQDMULL:
                    case SQDMULL2:
                    case SQNEG:
                    case SQRDMLAH:
                    case SQRDMLSH:
                    case SQRDMULH:
                    case SQRSHL:
                    case SQRSHRN:
                    case SQRSHRN2:
                    case SQRSHRUN:
                    case SQRSHRUN2:
                    case SQSHL:
                    case SQSHLU:
                    case SQSHRN:
                    case SQSHRN2:
                    case SQSHRUN:
                    case SQSHRUN2:
                    case SQSUB:
                    case SQXTN:
                    case SQXTN2:
                    case SQXTUN:
                    case SQXTUN2:
                    case SRHADD:
                    case SRI:
                    case SRSHL:
                    case SRSHR:
                    case SRSRA:
                    case SSHL:
                    case SSHLL:
                    case SSHLL2:
                    case SSHR:
                    case SSRA:
                    case SSUBL:
                    case SSUBL2:
                    case SSUBW:
                    case SSUBW2:
                    case ST1:
                    case ST2:
                    case ST3:
                    case ST4:
                    case SUBHN:
                    case SUBHN2:
                    case SUQADD:
                    case SXTL:
                    case SXTL2:
                    case TBL:
                    case TBX:
                    case TRN1:
                    case TRN2:
                    case UABA:
                    case UABAL:
                    case UABAL2:
                    case UABD:
                    case UABDL:
                    case UABDL2:
                    case UADALP:
                    case UADDL:
                    case UADDL2:
                    case UADDLP:
                    case UADDLV:
                    case UADDW:
                    case UADDW2:
                    case UCVTF:
                    case UDOT:
                    case UHADD:
                    case UHSUB:
                    case UMAX:
                    case UMAXP:
                    case UMAXV:
                    case UMIN:
                    case UMINP:
                    case UMINV:
                    case UMLAL:
                    case UMLAL2:
                    case UMLSL:
                    case UMLSL2:
                    case UMOV:
                    case UMULL2:
                    case UQADD:
                    case UQRSHL:
                    case UQRSHRN:
                    case UQRSHRN2:
                    case UQSHL:
                    case UQSHRN:
                    case UQSHRN2:
                    case UQSUB:
                    case UQXTN:
                    case UQXTN2:
                    case URECPE:
                    case URHADD:
                    case URSHL:
                    case URSHR:
                    case URSQRTE:
                    case URSRA:
                    case USHL:
                    case USHLL:
                    case USHLL2:
                    case USHR:
                    case USQADD:
                    case USRA:
                    case USUBL:
                    case USUBL2:
                    case USUBW:
                    case USUBW2:
                    case UXTL:
                    case UXTL2:
                    case UZP1:
                    case UZP2:
                    case XAR:
                    case XTN:
                    case XTN2:
                    case ZIP1:
                    case ZIP2:
                        instruction = validated_FB_SIMD_instruction( mnemonic );
                        break;

                    default:
                        error (16, "Unknown instruction" );
                    }

                oldpc = align () ;

                poke (&instruction, 4) ;
            }
        } ;
}
