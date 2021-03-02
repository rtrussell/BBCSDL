/*****************************************************************\
*       32-bit or 64-bit BBC BASIC for SDL 2.0                    *
*       (C) 2017-2021  R.T.Russell  http://www.rtrussell.co.uk/   *
*                                                                 *
*       The name 'BBC BASIC' is the property of the British       *
*       Broadcasting Corporation and used with their permission   *
*                                                                 *
*       bbcmos.c  Machine Operating System emulation              *
*       This module runs in the context of the interpreter thread *
*       Version 1.20a, 02-Mar-2021                                *
\*****************************************************************/

#define _GNU_SOURCE
#define __USE_GNU
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include "SDL2/SDL.h"
#include "SDL_ttf.h"
#include "bbcsdl.h"

#if defined __WINDOWS__ || defined __EMSCRIPTEN__
void *dlsym (void *, const char *) ;
#define RTLD_DEFAULT (void *)(-1)
#else
#include "dlfcn.h"
#endif

#if defined __EMSCRIPTEN__
// WASM has no official SIMD support yet
#elif defined __arm__ || defined __aarch64__
#include <arm_neon.h>
#else
#include <emmintrin.h>
#endif

// Delared in bbmain.c:
void error (int, const char *) ;
void text (const char *) ;	// Output NUL-terminated string
void crlf (void) ;		// Output a newline

// Declared in bbeval.c:
unsigned int rnd (void) ;	// Return a pseudo-random number

// Declared in bbccli.c:
char *setup (char *, char *, char *, char, int *) ;
void oscli (char *) ;		// Execute an emulated OS command

// Interpreter entry point:
int basic (void *, void *, void *) ;

// Forward references:
unsigned char osbget (void*, int*) ;
void osbput (void*, unsigned char) ;

// Array of VDU command lengths:
static int vdulen[] = {
	0,
	1,		// print next
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	1,		// COLOUR
	2,		// GCOL
	5,		// set palette
	0,
	0,
	1,		// MODE
	9,		// reprogram character (etc.)
	8,		// set graphics viewport
	5,		// PLOT
	0,
	1,		// ESCape
	4,		// set text viewport
	4,		// set graphics origin
	0,
	2} ;		// move text cursor (caret)

// Translation table for negative INKEY:
static unsigned char xkey[] = {
	225,		// -1 (SHIFT)
	224,		// -2 (CTRL)
	226,		// -3 (ALT)
	225,		// -4 (left SHIFT)
	224,		// -5 (left CTRL)
	226,		// -6 (left ALT)
	229,		// -7 (right SHIFT)
	228,		// -8 (right CTRL)
	230,		// -9 (right ALT)
	0,		// -10 (left button)
	0,		// -11 (middle button)
	0,		// -12 (right button)
	155,		// -13 (CANCEL)
	0,		// -14
	0,		// -15
	0,		// -16
	0x14,		// -17 (Q)
	0x20,		// -18 (3)
	0x21,		// -19 (4)
	0x22,		// -20 (5)
	0x3D,		// -21 (f4)
	0x25,		// -22 (8)
	0x40,		// -23 (f7)
	0x2D,		// -24 (-)
	0,		// -25
	0x50,		// -26 (LEFT)
	0x5E,		// -27 (keypad 6)
	0x5F,		// -28 (keypad 7)
	0x44,		// -29 (f11)
	0x45,		// -30 (f12)
	0x43,		// -31 (f10)
	0x47,		// -32 (SCROLL LOCK)
	0x46,		// -33 (PRINT SCREEN)
	0x1A,		// -34 (W)
	0x08,		// -35 (E)
	0x17,		// -36 (T)
	0x24,		// -37 (7)
	0x0C,		// -38 (I)
	0x26,		// -39 (9)
	0x27,		// -40 (0)
	0,		// -41
	0x51,		// -42 (DOWN)
	0x60,		// -43 (keypad 8)
	0x61,		// -44 (keypad 9)
	0x48,		// -45 (BREAK/PAUSE)
	0x35,		// -46 (`)
	0x31,		// -47 (#)
	0x2A,		// -48 (BACKSPACE)
	0x1E,		// -49 (1)
	0x1F,		// -50 (2)
	0x07,		// -51 (D)
	0x15,		// -52 (R)
	0x23,		// -53 (6)
	0x18,		// -54 (U)
	0x12,		// -55 (O)
	0x13,		// -56 (P)
	0x2F,		// -57 ([)
	0x52,		// -58 (UP)
	0x57,		// -59 (keypad +)
	0x56,		// -60 (keypad -)
	0x58,		// -61 (keypad ENTER)
	0x49,		// -62 (INSERT)
	0x4A,		// -63 (HOME)
	0x4B,		// -64 (PAGE UP)
	0x39,		// -65 (CAPS LOCK)
	0x04,		// -66 (A)
	0x1B,		// -67 (X)
	0x09,		// -68 (F)
	0x1C,		// -69 (Y)
	0x0D,		// -70 (J)
	0x0E,		// -71 (K)
	0,		// -72
	0x77,		// -73 (SELECT)
	0x28,		// -74 (RETURN)
	0x54,		// -75 (keypad /)
	0,		// -76
	0x63,		// -77 (keypad .)
	0x53,		// -78 (NUM LOCK)
	0x4E,		// -79 (PAGE DOWN)
	0x34,		// -80 (')
	0,		// -81 (was SHIFT LOCK)
	0x16,		// -82 (S)
	0x06,		// -83 (C)
	0x0A,		// -84 (G)
	0x0B,		// -85 (H)
	0x11,		// -86 (N)
	0x0F,		// -87 (L)
	0x33,		// -88 (;)
	0x30,		// -89 (])
	0x4C,		// -90 (DELETE)
	0x31,		// -91 (#)
	0x55,		// -92 (keypad *)
	117,		// -93 (HELP)
	0x2E,		// -94 (=)
	0,		// -95
	0,		// -96
	0x2B,		// -97 (TAB)
	0x1D,		// -98 (Z)
	0x2C,		// -99 (SPACE)
	0x19,		// -100 (V)
	0x05,		// -101 (B)
	0x10,		// -102 (M)
	0x36,		// -103 (,)
	0x37,		// -104 (.)
	0x38,		// -105 (/)
	0x4D,		// -106 (END) (COPY)
	0x62,		// -107 (keypad 0)
	0x59,		// -108 (keypad 1)
	0x5B,		// -109 (keypad 3)
	227,		// -110 (left Windows key)
	231,		// -111 (right Windows key)
	101,		// -112 (context menu key)
	0x29,		// -113 (ESCAPE)
	0x3A,		// -114 (f1)
	0x3B,		// -115 (f2)
	0x3C,		// -116 (f3)
	0x3E,		// -117 (f5)
	0x3F,		// -118 (f6)
	0x41,		// -119 (f8)
	0x42,		// -120 (f9)
	0x64,		// -121 (\)
	0x4F,		// -122 (RIGHT)
	0x5C,		// -123 (keypad 4)
	0x5D,		// -124 (keypad 5)
	0x5A,		// -125 (keypad 2)
	0x68,		// -126 (f13)
	0x69,		// -127 (f14)
	0x6A		// -128 (f15)
} ;

static unsigned short noises[8] = {
	0x8050,
	0x8300,
	0x8180,
	0x8300,
	0x8018,
	0x8018,
	0x8018,
	0x8018} ;

// Table of SOUND pitches for 44.100 kHz sampling frequency):
static unsigned short freqs[256] = {
	0,	// 0 = silence
	745,	// 1 = 62.63 Hz
	755,	// 2 = 63.54 Hz
	766,	// 3 = 64.47 Hz
	778,	// 4 = 65.41 Hz
	789,	// 5 = 66.36 Hz
	800,	// 6 = 67.32 Hz
	812,	// 7 = 68.30 Hz
	824,	// 8 = 69.30 Hz
	836,	// 9 = 70.30 Hz
	848,	// 10 = 71.33 Hz
	860,	// 11 = 72.36 Hz
	873,	// 12 = 73.42 Hz
	886,	// 13 = 74.48 Hz
	898,	// 14 = 75.57 Hz
	911,	// 15 = 76.67 Hz
	925,	// 16 = 77.78 Hz
	938,	// 17 = 78.91 Hz
	952,	// 18 = 80.06 Hz
	966,	// 19 = 81.23 Hz
	980,	// 20 = 82.41 Hz
	994,	// 21 = 83.61 Hz
	1008,	// 22 = 84.82 Hz
	1023,	// 23 = 86.06 Hz
	1038,	// 24 = 87.31 Hz
	1053,	// 25 = 88.58 Hz
	1068,	// 26 = 89.87 Hz
	1084,	// 27 = 91.17 Hz
	1100,	// 28 = 92.50 Hz
	1116,	// 29 = 93.84 Hz
	1132,	// 30 = 95.21 Hz
	1148,	// 31 = 96.59 Hz
	1165,	// 32 = 98.00 Hz
	1182,	// 33 = 99.42 Hz
	1199,	// 34 = 100.87 Hz
	1217,	// 35 = 102.34 Hz
	1234,	// 36 = 103.83 Hz
	1252,	// 37 = 105.34 Hz
	1271,	// 38 = 106.87 Hz
	1289,	// 39 = 108.42 Hz
	1308,	// 40 = 110.00 Hz
	1327,	// 41 = 111.60 Hz
	1346,	// 42 = 113.22 Hz
	1366,	// 43 = 114.87 Hz
	1386,	// 44 = 116.54 Hz
	1406,	// 45 = 118.24 Hz
	1426,	// 46 = 119.96 Hz
	1447,	// 47 = 121.70 Hz
	1468,	// 48 = 123.47 Hz
	1489,	// 49 = 125.27 Hz
	1511,	// 50 = 127.09 Hz
	1533,	// 51 = 128.94 Hz
	1555,	// 52 = 130.81 Hz
	1578,	// 53 = 132.72 Hz
	1601,	// 54 = 134.65 Hz
	1624,	// 55 = 136.60 Hz
	1648,	// 56 = 138.59 Hz
	1672,	// 57 = 140.61 Hz
	1696,	// 58 = 142.65 Hz
	1721,	// 59 = 144.73 Hz
	1746,	// 60 = 146.83 Hz
	1771,	// 61 = 148.97 Hz
	1797,	// 62 = 151.13 Hz
	1823,	// 63 = 153.33 Hz
	1849,	// 64 = 155.56 Hz
	1876,	// 65 = 157.83 Hz
	1904,	// 66 = 160.12 Hz
	1931,	// 67 = 162.45 Hz
	1959,	// 68 = 164.81 Hz
	1988,	// 69 = 167.21 Hz
	2017,	// 70 = 169.64 Hz
	2046,	// 71 = 172.11 Hz
	2076,	// 72 = 174.61 Hz
	2106,	// 73 = 177.15 Hz
	2137,	// 74 = 179.73 Hz
	2168,	// 75 = 182.34 Hz
	2199,	// 76 = 185.00 Hz
	2231,	// 77 = 187.69 Hz
	2264,	// 78 = 190.42 Hz
	2297,	// 79 = 193.19 Hz
	2330,	// 80 = 196.00 Hz
	2364,	// 81 = 198.85 Hz
	2398,	// 82 = 201.74 Hz
	2433,	// 83 = 204.68 Hz
	2469,	// 84 = 207.65 Hz
	2505,	// 85 = 210.67 Hz
	2541,	// 86 = 213.74 Hz
	2578,	// 87 = 216.85 Hz
	2615,	// 88 = 220.00 Hz
	2654,	// 89 = 223.20 Hz
	2692,	// 90 = 226.45 Hz
	2731,	// 91 = 229.74 Hz
	2771,	// 92 = 233.08 Hz
	2811,	// 93 = 236.47 Hz
	2852,	// 94 = 239.91 Hz
	2894,	// 95 = 243.40 Hz
	2936,	// 96 = 246.94 Hz
	2978,	// 97 = 250.53 Hz
	3022,	// 98 = 254.18 Hz
	3066,	// 99 = 257.87 Hz
	3110,	// 100 = 261.63 Hz
	3156,	// 101 = 265.43 Hz
	3202,	// 102 = 269.29 Hz
	3248,	// 103 = 273.21 Hz
	3295,	// 104 = 277.18 Hz
	3343,	// 105 = 281.21 Hz
	3392,	// 106 = 285.30 Hz
	3441,	// 107 = 289.45 Hz
	3491,	// 108 = 293.66 Hz
	3542,	// 109 = 297.94 Hz
	3594,	// 110 = 302.27 Hz
	3646,	// 111 = 306.67 Hz
	3699,	// 112 = 311.13 Hz
	3753,	// 113 = 315.65 Hz
	3807,	// 114 = 320.24 Hz
	3863,	// 115 = 324.90 Hz
	3919,	// 116 = 329.63 Hz
	3976,	// 117 = 334.42 Hz
	4034,	// 118 = 339.29 Hz
	4092,	// 119 = 344.22 Hz
	4152,	// 120 = 349.23 Hz
	4212,	// 121 = 354.31 Hz
	4273,	// 122 = 359.46 Hz
	4336,	// 123 = 364.69 Hz
	4399,	// 124 = 369.99 Hz
	4463,	// 125 = 375.38 Hz
	4528,	// 126 = 380.84 Hz
	4593,	// 127 = 386.38 Hz
	4660,	// 128 = 392.00 Hz
	4728,	// 129 = 397.70 Hz
	4797,	// 130 = 403.48 Hz
	4867,	// 131 = 409.35 Hz
	4937,	// 132 = 415.30 Hz
	5009,	// 133 = 421.35 Hz
	5082,	// 134 = 427.47 Hz
	5156,	// 135 = 433.69 Hz
	5231,	// 136 = 440.00 Hz
	5307,	// 137 = 446.40 Hz
	5384,	// 138 = 452.89 Hz
	5463,	// 139 = 459.48 Hz
	5542,	// 140 = 466.16 Hz
	5623,	// 141 = 472.94 Hz
	5704,	// 142 = 479.82 Hz
	5787,	// 143 = 486.80 Hz
	5872,	// 144 = 493.88 Hz
	5957,	// 145 = 501.07 Hz
	6044,	// 146 = 508.36 Hz
	6132,	// 147 = 515.75 Hz
	6221,	// 148 = 523.25 Hz
	6311,	// 149 = 530.86 Hz
	6403,	// 150 = 538.58 Hz
	6496,	// 151 = 546.42 Hz
	6591,	// 152 = 554.37 Hz
	6686,	// 153 = 562.43 Hz
	6784,	// 154 = 570.61 Hz
	6882,	// 155 = 578.91 Hz
	6983,	// 156 = 587.33 Hz
	7084,	// 157 = 595.87 Hz
	7187,	// 158 = 604.54 Hz
	7292,	// 159 = 613.33 Hz
	7398,	// 160 = 622.25 Hz
	7505,	// 161 = 631.30 Hz
	7615,	// 162 = 640.49 Hz
	7725,	// 163 = 649.80 Hz
	7838,	// 164 = 659.26 Hz
	7952,	// 165 = 668.84 Hz
	8067,	// 166 = 678.57 Hz
	8185,	// 167 = 688.44 Hz
	8304,	// 168 = 698.46 Hz
	8424,	// 169 = 708.62 Hz
	8547,	// 170 = 718.92 Hz
	8671,	// 171 = 729.38 Hz
	8797,	// 172 = 739.99 Hz
	8925,	// 173 = 750.75 Hz
	9055,	// 174 = 761.67 Hz
	9187,	// 175 = 772.75 Hz
	9321,	// 176 = 783.99 Hz
	9456,	// 177 = 795.39 Hz
	9594,	// 178 = 806.96 Hz
	9733,	// 179 = 818.70 Hz
	9875,	// 180 = 830.61 Hz
	10018,	// 181 = 842.69 Hz
	10164,	// 182 = 854.95 Hz
	10312,	// 183 = 867.38 Hz
	10462,	// 184 = 880.00 Hz
	10614,	// 185 = 892.80 Hz
	10769,	// 186 = 905.79 Hz
	10925,	// 187 = 918.96 Hz
	11084,	// 188 = 932.33 Hz
	11245,	// 189 = 945.89 Hz
	11409,	// 190 = 959.65 Hz
	11575,	// 191 = 973.61 Hz
	11743,	// 192 = 987.77 Hz
	11914,	// 193 = 1002.13 Hz
	12087,	// 194 = 1016.71 Hz
	12263,	// 195 = 1031.50 Hz
	12441,	// 196 = 1046.50 Hz
	12622,	// 197 = 1061.72 Hz
	12806,	// 198 = 1077.17 Hz
	12992,	// 199 = 1092.83 Hz
	13181,	// 200 = 1108.73 Hz
	13373,	// 201 = 1124.86 Hz
	13568,	// 202 = 1141.22 Hz
	13765,	// 203 = 1157.82 Hz
	13965,	// 204 = 1174.66 Hz
	14168,	// 205 = 1191.74 Hz
	14374,	// 206 = 1209.08 Hz
	14583,	// 207 = 1226.67 Hz
	14795,	// 208 = 1244.51 Hz
	15011,	// 209 = 1262.61 Hz
	15229,	// 210 = 1280.97 Hz
	15451,	// 211 = 1299.61 Hz
	15675,	// 212 = 1318.51 Hz
	15903,	// 213 = 1337.69 Hz
	16135,	// 214 = 1357.15 Hz
	16369,	// 215 = 1376.89 Hz
	16607,	// 216 = 1396.91 Hz
	16849,	// 217 = 1417.23 Hz
	17094,	// 218 = 1437.85 Hz
	17343,	// 219 = 1458.76 Hz
	17595,	// 220 = 1479.98 Hz
	17851,	// 221 = 1501.50 Hz
	18110,	// 222 = 1523.34 Hz
	18374,	// 223 = 1545.50 Hz
	18641,	// 224 = 1567.98 Hz
	18912,	// 225 = 1590.79 Hz
	19187,	// 226 = 1613.93 Hz
	19466,	// 227 = 1637.40 Hz
	19750,	// 228 = 1661.22 Hz
	20037,	// 229 = 1685.38 Hz
	20328,	// 230 = 1709.90 Hz
	20624,	// 231 = 1734.77 Hz
	20924,	// 232 = 1760.00 Hz
	21228,	// 233 = 1785.60 Hz
	21537,	// 234 = 1811.57 Hz
	21850,	// 235 = 1837.92 Hz
	22168,	// 236 = 1864.66 Hz
	22491,	// 237 = 1891.78 Hz
	22818,	// 238 = 1919.29 Hz
	23150,	// 239 = 1947.21 Hz
	23486,	// 240 = 1975.53 Hz
	23828,	// 241 = 2004.27 Hz
	24175,	// 242 = 2033.42 Hz
	24526,	// 243 = 2063.00 Hz
	24883,	// 244 = 2093.00 Hz
	25245,	// 245 = 2123.45 Hz
	25612,	// 246 = 2154.33 Hz
	25985,	// 247 = 2185.67 Hz
	26363,	// 248 = 2217.46 Hz
	26746,	// 249 = 2249.71 Hz
	27135,	// 250 = 2282.44 Hz
	27530,	// 251 = 2315.64 Hz
	27930,	// 252 = 2349.32 Hz
	28336,	// 253 = 2383.49 Hz
	28749,	// 254 = 2418.16 Hz
	29167	// 255 = 2453.33 Hz
} ;

// Logarithmic amplitude table:
static short ampl[128] = {
	0x0000, 0x0368, 0x0381, 0x039B, 0x03B6, 0x03D2, 0x03EF, 0x040C,
	0x042A, 0x044A, 0x046A, 0x048B, 0x04AD, 0x04D0, 0x04F3, 0x0518,
	0x053F, 0x0566, 0x058E, 0x05B8, 0x05E2, 0x060E, 0x063C, 0x066A,
	0x069A, 0x06CC, 0x06FE, 0x0733, 0x0768, 0x07A0, 0x07D9, 0x0813,
	0x0850, 0x088E, 0x08CE, 0x0910, 0x0953, 0x0999, 0x09E1, 0x0A2B,
	0x0A77, 0x0AC5, 0x0B16, 0x0B68, 0x0BBE, 0x0C15, 0x0C70, 0x0CCD,
	0x0D2C, 0x0D8F, 0x0DF4, 0x0E5D, 0x0EC8, 0x0F36, 0x0FA8, 0x101D,
	0x1096, 0x1112, 0x1191, 0x1214, 0x129C, 0x1327, 0x13B6, 0x1449,
	0x14E1, 0x157D, 0x161E, 0x16C3, 0x176D, 0x181C, 0x18D0, 0x198A,
	0x1A49, 0x1B0D, 0x1BD7, 0x1CA8, 0x1D7E, 0x1E5A, 0x1F3D, 0x2027,
	0x2117, 0x220E, 0x230D, 0x2413, 0x2521, 0x2636, 0x2754, 0x287A,
	0x29A8, 0x2AE0, 0x2C20, 0x2D6A, 0x2EBE, 0x301B, 0x3183, 0x32F5,
	0x3472, 0x35FA, 0x378D, 0x392C, 0x3AD8, 0x3C90, 0x3E54, 0x4026,
	0x4206, 0x43F3, 0x45EF, 0x47FA, 0x4A14, 0x4C3E, 0x4E78, 0x50C3,
	0x531E, 0x558C, 0x580B, 0x5A9D, 0x5D43, 0x5FFC, 0x62C9, 0x65AC,
	0x68A4, 0x6BB2, 0x6ED7, 0x7214, 0x7568, 0x78D6, 0x7C5D, 0x7FFF} ;

// Table of mask bytes for channel synchronisation:
static unsigned char syncs[256] = {
	0b11111111,	// 00000000B
	0b11111111,	// 00000001B
	0b11111111,	// 00000010B
	0b11111111,	// 00000011B
	0b11111111,	// 00000100B
	0b11111010,	// 00000101B
	0b11111111,	// 00000110B
	0b11111111,	// 00000111B
	0b11111111,	// 00001000B
	0b11111111,	// 00001001B
	0b11111111,	// 00001010B
	0b11111111,	// 00001011B
	0b11111111,	// 00001100B
	0b11111111,	// 00001101B
	0b11111111,	// 00001110B
	0b11111111,	// 00001111B
	0b11111111,	// 00010000B
	0b11101110,	// 00010001B
	0b11111111,	// 00010010B
	0b11111111,	// 00010011B
	0b11101011,	// 00010100B
	0b11101010,	// 00010101B
	0b11101011,	// 00010110B
	0b11101011,	// 00010111B
	0b11111111,	// 00011000B
	0b11101110,	// 00011001B
	0b11111111,	// 00011010B
	0b11111111,	// 00011011B
	0b11111111,	// 00011100B
	0b11101110,	// 00011101B
	0b11111111,	// 00011110B
	0b11111111,	// 00011111B
	0b11111111,	// 00100000B
	0b11111111,	// 00100001B
	0b11111111,	// 00100010B
	0b11111111,	// 00100011B
	0b11111111,	// 00100100B
	0b11111010,	// 00100101B
	0b11111111,	// 00100110B
	0b11111111,	// 00100111B
	0b11111111,	// 00101000B
	0b11111111,	// 00101001B
	0b11101010,	// 00101010B
	0b11111111,	// 00101011B
	0b11111111,	// 00101100B
	0b11111111,	// 00101101B
	0b11111111,	// 00101110B
	0b11111111,	// 00101111B
	0b11111111,	// 00110000B
	0b11111111,	// 00110001B
	0b11111111,	// 00110010B
	0b11111111,	// 00110011B
	0b11111111,	// 00110100B
	0b11111010,	// 00110101B
	0b11111111,	// 00110110B
	0b11111111,	// 00110111B
	0b11111111,	// 00111000B
	0b11111111,	// 00111001B
	0b11111111,	// 00111010B
	0b11111111,	// 00111011B
	0b11111111,	// 00111100B
	0b11111111,	// 00111101B
	0b11111111,	// 00111110B
	0b11111111,	// 00111111B
	0b11111111,	// 01000000B
	0b10111110,	// 01000001B
	0b11111111,	// 01000010B
	0b11111111,	// 01000011B
	0b10111011,	// 01000100B
	0b10111010,	// 01000101B
	0b10111011,	// 01000110B
	0b10111011,	// 01000111B
	0b11111111,	// 01001000B
	0b10111110,	// 01001001B
	0b11111111,	// 01001010B
	0b11111111,	// 01001011B
	0b11111111,	// 01001100B
	0b10111110,	// 01001101B
	0b11111111,	// 01001110B
	0b11111111,	// 01001111B
	0b10101111,	// 01010000B
	0b10101110,	// 01010001B
	0b10101111,	// 01010010B
	0b10101111,	// 01010011B
	0b10101011,	// 01010100B
	0b10101010,	// 01010101B
	0b10101011,	// 01010110B
	0b10101011,	// 01010111B
	0b10101111,	// 01011000B
	0b10101110,	// 01011001B
	0b10101111,	// 01011010B
	0b10101111,	// 01011011B
	0b10101111,	// 01011100B
	0b10101110,	// 01011101B
	0b10101111,	// 01011110B
	0b10101111,	// 01011111B
	0b11111111,	// 01100000B
	0b10111110,	// 01100001B
	0b11111111,	// 01100010B
	0b11111111,	// 01100011B
	0b10111011,	// 01100100B
	0b10111010,	// 01100101B
	0b10111011,	// 01100110B
	0b10111011,	// 01100111B
	0b11111111,	// 01101000B
	0b10111110,	// 01101001B
	0b11101010,	// 01101010B
	0b11111111,	// 01101011B
	0b11111111,	// 01101100B
	0b10111110,	// 01101101B
	0b11111111,	// 01101110B
	0b11111111,	// 01101111B
	0b11111111,	// 01110000B
	0b10111110,	// 01110001B
	0b11111111,	// 01110010B
	0b11111111,	// 01110011B
	0b10111011,	// 01110100B
	0b10111010,	// 01110101B
	0b10111011,	// 01110110B
	0b10111011,	// 01110111B
	0b11111111,	// 01111000B
	0b10111110,	// 01111001B
	0b11111111,	// 01111010B
	0b11111111,	// 01111011B
	0b11111111,	// 01111100B
	0b10111110,	// 01111101B
	0b11111111,	// 01111110B
	0b11111111,	// 01111111B
	0b11111111,	// 10000000B
	0b11111111,	// 10000001B
	0b11111111,	// 10000010B
	0b11111111,	// 10000011B
	0b11111111,	// 10000100B
	0b11111010,	// 10000101B
	0b11111111,	// 10000110B
	0b11111111,	// 10000111B
	0b11111111,	// 10001000B
	0b11111111,	// 10001001B
	0b10111010,	// 10001010B
	0b11111111,	// 10001011B
	0b11111111,	// 10001100B
	0b11111111,	// 10001101B
	0b11111111,	// 10001110B
	0b11111111,	// 10001111B
	0b11111111,	// 10010000B
	0b11101110,	// 10010001B
	0b11111111,	// 10010010B
	0b11111111,	// 10010011B
	0b11101011,	// 10010100B
	0b11101010,	// 10010101B
	0b11101011,	// 10010110B
	0b11101011,	// 10010111B
	0b11111111,	// 10011000B
	0b11101110,	// 10011001B
	0b10111010,	// 10011010B
	0b11111111,	// 10011011B
	0b11111111,	// 10011100B
	0b11101110,	// 10011101B
	0b11111111,	// 10011110B
	0b11111111,	// 10011111B
	0b11111111,	// 10100000B
	0b11111111,	// 10100001B
	0b10101110,	// 10100010B
	0b11111111,	// 10100011B
	0b11111111,	// 10100100B
	0b11111010,	// 10100101B
	0b10101110,	// 10100110B
	0b11111111,	// 10100111B
	0b10101011,	// 10101000B
	0b10101011,	// 10101001B
	0b10101010,	// 10101010B
	0b10101011,	// 10101011B
	0b11111111,	// 10101100B
	0b11111111,	// 10101101B
	0b10101110,	// 10101110B
	0b11111111,	// 10101111B
	0b11111111,	// 10110000B
	0b11111111,	// 10110001B
	0b11111111,	// 10110010B
	0b11111111,	// 10110011B
	0b11111111,	// 10110100B
	0b11111010,	// 10110101B
	0b11111111,	// 10110110B
	0b11111111,	// 10110111B
	0b11111111,	// 10111000B
	0b11111111,	// 10111001B
	0b10111010,	// 10111010B
	0b11111111,	// 10111011B
	0b11111111,	// 10111100B
	0b11111111,	// 10111101B
	0b11111111,	// 10111110B
	0b11111111,	// 10111111B
	0b11111111,	// 11000000B
	0b11111111,	// 11000001B
	0b11111111,	// 11000010B
	0b11111111,	// 11000011B
	0b11111111,	// 11000100B
	0b11111010,	// 11000101B
	0b11111111,	// 11000110B
	0b11111111,	// 11000111B
	0b11111111,	// 11001000B
	0b11111111,	// 11001001B
	0b11111111,	// 11001010B
	0b11111111,	// 11001011B
	0b11111111,	// 11001100B
	0b11111111,	// 11001101B
	0b11111111,	// 11001110B
	0b11111111,	// 11001111B
	0b11111111,	// 11010000B
	0b11101110,	// 11010001B
	0b11111111,	// 11010010B
	0b11111111,	// 11010011B
	0b11101011,	// 11010100B
	0b11101010,	// 11010101B
	0b11101011,	// 11010110B
	0b11101011,	// 11010111B
	0b11111111,	// 11011000B
	0b11101110,	// 11011001B
	0b11111111,	// 11011010B
	0b11111111,	// 11011011B
	0b11111111,	// 11011100B
	0b11101110,	// 11011101B
	0b11111111,	// 11011110B
	0b11111111,	// 11011111B
	0b11111111,	// 11100000B
	0b11111111,	// 11100001B
	0b11111111,	// 11100010B
	0b11111111,	// 11100011B
	0b11111111,	// 11100100B
	0b11111010,	// 11100101B
	0b11111111,	// 11100110B
	0b11111111,	// 11100111B
	0b11111111,	// 11101000B
	0b11111111,	// 11101001B
	0b11101010,	// 11101010B
	0b11111111,	// 11101011B
	0b11111111,	// 11101100B
	0b11111111,	// 11101101B
	0b11111111,	// 11101110B
	0b11111111,	// 11101111B
	0b11111111,	// 11110000B
	0b11111111,	// 11110001B
	0b11111111,	// 11110010B
	0b11111111,	// 11110011B
	0b11111111,	// 11110100B
	0b11111010,	// 11110101B
	0b11111111,	// 11110110B
	0b11111111,	// 11110111B
	0b11111111,	// 11111000B
	0b11111111,	// 11111001B
	0b11111111,	// 11111010B
	0b11111111,	// 11111011B
	0b11111111,	// 11111100B
	0b11111111,	// 11111101B
	0b11111111,	// 11111110B
	0b10101010	// 11111111B
} ;

// Sound waveform harmonics:
static float harms[8][9] = {
// Waveform 0 (square):
{
	0.5,
	0.0,
	0.166667,	// 1/3
	0.0,
	0.100000,	// 1/5
	0.0,
	0.071429,	// 1/7
	0.0,
	0.055556	// 1/9
},
// Waveform 1 (triangular):
{
	0.8,
	0.0,
	-0.0888889,	// 1/9
	0.0,
	0.0320000,	// 1/25
	0.0,
	-0.0163265,	// 1/49
	0.0,
	0.0098765	// 1/81
},
// Waveform 2:
{
	0.6,
	0.1,		// 1/5
	0.0,
	0.1,		// 1/5
	0.2,		// 2/5
	0.0,
	0.0,
	0.0,
	0.0
},
// Waveform 3:
{
	0.5,
	-0.5,
	0.0,
	0.0,
	0.0,
	0.0,
	0.0,
	0.0,
	0.0
},
// Waveform 4 (sine):
{
	1.0,
	0.0,
	0.0,
	0.0,
	0.0,
	0.0,
	0.0,
	0.0,
	0.0
},
// Waveform 5:
{
	0.500,
	0.250,		// 1/2
	0.0,
	0.125,		// 1/4
	0.0,
	0.0,
	0.0,
	0.0,
	0.0
},
// Waveform 6:
{
	0.270,
	0.225,		// 5/6
	0.180,		// 4/6
	0.135,		// 3/6
	0.090,		// 2/6
	0.045,		// 1/6
	0.0,
	0.0,
	0.0
},
// Waveform 7:
{
	0.520,
	0.260,		// 1/2
	0.0,
	0.0,
	0.0,
	0.0,
	0.0,
	0.0,
	0.0
}} ;

// Forward reference:
void quiet (void) ;

// Prepare for outputting an error message:
void reset (void)
{
	vduq[10] = 0 ;	// Flush VDU queue
	keyptr = NULL ;	// Cancel *KEY expansion
	optval = 0 ;	// Cancel I/O redirection
	reflag = 0 ;	// *REFRESH ON
 }

static int BBC_PushEvent(SDL_Event* event)
{
	int ret ;
	SDL_LockMutex (Mutex) ;
	ret = SDL_PushEvent (event) ;
	SDL_UnlockMutex (Mutex) ;
	return ret ;
}

// Push event onto queue:
void pushev (int code, void *data1, void *data2)
{
	SDL_Event event ;

	event.type = SDL_USEREVENT ;
	event.user.code = code ;
	event.user.data1 = data1 ;
	event.user.data2 = data2 ;

	while (SDL_SemValue (Sema4))
		SDL_SemWait (Sema4) ;

	SDL_AtomicIncRef ((SDL_atomic_t*) &nUserEv) ;
	while (BBC_PushEvent (&event) <= 0)
		SDL_Delay (1) ;
	while (nUserEv >= MAX_EVENTS)
		SDL_Delay (1) ;
}

static int BBC_RWclose (struct SDL_RWops* context)
{
	int ret = SDL_RWclose (context) ;
	pushev (EVT_FSSYNC, NULL, NULL) ;
	return ret ;
}

// Wait for an event to be acknowledged:
size_t waitev (void)
{
	SDL_SemWait (Sema4) ;
	return iResult ;
}

// Put event into event queue, unless full:
int putevt (heapptr handler, int msg, int wparam, int lparam)
{
	unsigned char bl = evtqw ;
	unsigned char al = bl + 8 ;
	int index = bl >> 2 ;
	if ((al == evtqr) || (eventq == NULL))
		return 0 ;
	eventq[index] = lparam ;
	eventq[index + 1] = msg ;
	eventq[index + 64] = wparam ;
	eventq[index + 65] = handler ;
	evtqw = al ;
	return 1 ;
}

// Get event from event queue, unless empty:
static heapptr getevt (void)
{
	heapptr handler ;
	unsigned char al = evtqr ;
	int index = al >> 2 ;
	flags &= ~ALERT ;
	if ((al == evtqw) || (eventq == NULL))
		return 0 ;
	lParam = eventq[index] ;
	iMsg = eventq[index + 1] ;
	wParam = eventq[index + 64] ;
	handler = eventq[index + 65] ;
	al += 8 ;
	evtqr = al ;
	if (al != evtqw)
		flags |= ALERT ;
	return handler ;
}

// Put keycode to keyboard queue:
int putkey (char key)
{
	unsigned char bl = kbdqw ;
	unsigned char al = bl + 1 ;
	if (al != kbdqr)
	{
		keybdq[(int) bl] = key ;
		kbdqw = al ;
		return 1 ;
	}
	return 0 ;
}

// Get keycode (if any) from keyboard queue:
static int getkey (unsigned char *pkey)
{
	unsigned char bl = kbdqr ;
	OSKtime = 6 ;
	if (bl != kbdqw)
	{
		*pkey = keybdq[(int) bl] ;
		kbdqr = bl + 1 ;
		return 1 ;
	}
	return 0 ;
}

// Get text cursor (caret) coordinates:
void getcsr(int *px, int *py)
{
	unsigned int xy ;
	pushev (EVT_CARET, NULL, NULL) ;
	xy = waitev() ;
	if (px != NULL)
		*px = xy & 0xFFFF ;
	if (py != NULL)
		*py = xy >> 16 ;
}

// Get pixel RGB colour:
int vtint (int x, int y)
{
	pushev (EVT_TINT, (void *)(intptr_t)x, (void *)(intptr_t)y) ;
	return waitev () ;
}

// Get nearest palette index:
int vpoint (int x, int y)
{
	unsigned char rgb[3] ;
	unsigned int best = 0x7FFFFFFF ;
	int i, n = -1 ;
	pushev (EVT_TINT, (void *)(intptr_t)x, (void *)(intptr_t)y) ;
	i = waitev () ;
	if (i < 0)
		return i ;
	rgb[0] = i & 0xFF ;
	rgb[1] = i >> 8 ;
	rgb[2] = i >> 16 ;
	for (i = 0; i <= colmsk; i++)
	    {
		unsigned int sqr ;
		int dif ;
		dif = rgb[0] - (palette[i] & 0xFF) ;
		sqr = dif * dif ;
		dif = rgb[1] - ((palette[i] >> 8) & 0xFF) ;
		sqr += dif * dif ;
		dif = rgb[2] - ((palette[i] >> 16) & 0xFF) ;
		sqr += dif * dif ;
		if (sqr < best)
		    {
			best = sqr ;
			n = i ;
		    }
	    }
	return n ;
}

int vgetc (int x, int y)
{
	int eax, ebx, ecx ;
	pushev (EVT_CHAR, (void *)(intptr_t)x, (void *)(intptr_t)y) ;
	eax = waitev () ;
	if (eax < 0)
		return -1 ;
	eax &= 0xFFFF ;
	if ((eax < 0x80) || ((vflags & UTF8) == 0))
		return eax ;
	ecx = -64 ;
	ebx = 0 ;
	do
	    {
		ebx = (ebx & 0xFFFF00) | (eax & 0x3F) | 0x80 ;
		ecx = ecx >> 1 ;
		eax = eax >> 6 ;
		ebx = ebx << 8 ;
	    }
	while (eax & ecx) ;
	eax |= (ecx << 1) & 0xFF ;
	return (eax | ebx) ;
}

// Enable on-screen keyboard (Android):
static void oskon (void)
{
#if defined(__ANDROID__) || defined(__IPHONEOS__)
	pushev (EVT_OSK, (void *) 1, NULL) ;
	OSKtime = 6 ;
#endif
}

// Read (possibly on-screen) keyboard:
static int rdkey (unsigned char *pkey)
{
	if (keyptr)
	{
		*pkey = *keyptr++ ;
		if (*keyptr == 0)
			keyptr = NULL ;
		return 1 ;
	}

	if (getkey (pkey))
	{
		int keyno = 0 ;
		if ((*pkey >= 128) && (*pkey < 156))
			keyno = *pkey ^ 144 ;
		if (keyno >= 24)
			keyno -= 12 ;
		if (*pkey == 8)
			keyno = 24 ;
		if (keyno)
		{
			keyptr = *((unsigned char **)keystr + keyno) ;
			if (keyptr)
			{
				*pkey = *keyptr++ ;
				if (*keyptr == 0)
					keyptr = NULL ;
			}
		}
		return 1 ;
	}
	return 0 ;
}

// Check for Escape (if enabled) and kill:
void trap (void)
{
		while (bBackground)
			SDL_Delay (5) ;
		if (flags & KILL)
			error (-1, NULL) ; // Quit
		if (flags & ESCFLG)
		    {
			flags &= ~ESCFLG ;
			kbdqr = kbdqw ;
			quiet () ;
			if (exchan)
			    {
				SDL_RWclose (exchan) ;
 				exchan = NULL ;
			    }
			error (17, NULL) ; // 'Escape'
		    }
}

// Test for escape, kill, pause, single-step, flash and alert:
heapptr xtrap (void)
{
		if (flags & KILL)
			error (-1, NULL) ; // Quit
		if (flags & ESCFLG)
		    {
			flags &= ~ESCFLG ;
			kbdqr = kbdqw ;
			quiet () ;
			if (exchan)
			    {
				SDL_RWclose (exchan) ;
 				exchan = NULL ;
			    }
			error (17, NULL) ; // 'Escape'
		    }
		if (flags & ALERT)
			return getevt () ;
		if ((flags & PAUSE) && (curlin < breakhi) && (curlin >= breakpt))
		    {
			flags |= SSTEP ; 
			while (flags & SSTEP)
			    {
				SDL_Delay (1) ;
				trap () ;
			    }
		    }
		return 0 ;
}

// Report a 'fatal' error:
void faterr (const char *msg)
{
	SDL_ShowSimpleMessageBox (SDL_MESSAGEBOX_ERROR, szVersion, msg, hwndProg) ;
}

// Wait a maximum period for a keypress, or test key asynchronously:
int oskey (int wait)
{
	if (wait >= 0)
	    {
		unsigned int start = SDL_GetTicks () ;
		while (1)
		    {
			unsigned char key ;
			if (rdkey (&key))
				return (int) key ;
			if ((unsigned int)(SDL_GetTicks () - start) >= wait * 10)
				return -1 ;
			SDL_Delay (5) ;
			trap () ;
		    }
	    }

	if ((wait <= -10) && (wait >= -12))
	    {
		int state = SDL_GetMouseState (NULL, NULL) ;
		return -((state & (0x40 >> (wait & 7))) != 0) ;
	    }

	if (wait >= -128)
	    {
		const unsigned char *keys = SDL_GetKeyboardState (NULL) ;
		int index = xkey[~wait] ;
		int state = keys[index] ;
		if (wait >= -3)
			state |= keys[index + 4 ] ; // Shift, Ctrl, Alt
		return -state ;
	    }

	return 's' ;
}

// Wait for keypress:
unsigned char osrdch (void)
{
	unsigned char key ;
	if (exchan)
	{
		if (SDL_RWread (exchan, &key, 1, 1))
			return key ;
		SDL_RWclose (exchan) ;
 		exchan = NULL ;
	}

	if (optval >> 4)
		return osbget ((void *)(size_t)(optval >> 4), NULL) ;

	oskon () ;
	while (!rdkey (&key))
	{
		SDL_Delay (5) ;
		trap () ;
	}
	return key ;
}

// Output byte (ANSI or UTF-8) to VDU stream:
void oswrch (unsigned char vdu)
{
	unsigned char *pqueue = vduq ;

	if (optval & 0x0F)
	    {
		osbput ((void *)(size_t)(optval & 0x0F), vdu) ;
		return ;
	    }

	if (spchan)
	{
		if (0 == SDL_RWwrite (spchan, &vdu, 1, 1))
		    {
			BBC_RWclose (spchan) ;
			spchan = NULL ;
		    }
	}

	if (vduq[10] != 0)		// Filling queue ?
	{
		vduq[9 - vduq[10]] = vdu ;
		vduq[10] -- ;
		if (vduq[10] != 0)
			return ;
		vdu = vduq[9] ;
		if (vdu >= 0x80)
		{
			int eax = vdu ;
			int ecx = (vdu >> 4) & 3 ;
			if (ecx == 0) ecx++ ;
			pqueue -= ecx - 9 ;
			eax &= 0x1F ;
			for ( ; ecx > 0 ; ecx--)
				eax = (eax << 6) | (0x3F & *pqueue++) ;
			pushev (EVT_VDU, (void *)(intptr_t)eax, NULL) ;
			return ;
		}
	}
	else if (vdu >= 0x20)
	{
		if ((vdu >= 0x80) && (vflags & UTF8))
		{
			char ah = (vdu >> 4) & 3 ;
			if (ah == 0) ah++ ;
			vduq[10] = ah ;	// No. of bytes to follow
			vduq[9] = vdu ;
			return ;
		}
		pushev (EVT_VDU, (void *)(size_t) vdu, NULL) ;
		return ;
	}
	else
	{
		vduq[10] = vdulen[(int) vdu] ;
		vduq[9] = vdu ;
		if (vduq[10] != 0)
			return ;
	}

// Mapping of VDU queue to UserEvent parameters,
// VDU 23 (defchr) has the most parameter bytes:
//
//          0  ^
// status-> 0  | ev.user.code
//          V  | (eax)
//          h  v
//          g  ^
//          f  | ev.user.data1
//          e  | (ecx)
//          d  v
//          c  ^
//          b  | ev.user.data2
//          a  | (edx)
//  vduq->  n  v

	pushev (*(int*)(pqueue + 8) & 0xFFFF, (void *)(intptr_t)*(int*)(pqueue + 4), 
					      (void *)(intptr_t)*(int*)pqueue) ;
	if ((vduq[9] == 23) && (vduq[0] == 22))
		getcsr(NULL, NULL) ; // thread sync after possible UTF8 change
}

static unsigned char copykey (int *cc, int key)
{
	pushev (EVT_COPYKEY, cc, (void *)(intptr_t) key) ;
	return (unsigned char) waitev () ;
}

// Input and edit a string :
void osline (char *buffer)
{
	char *eol = buffer ;
	char *p = buffer ;
	*buffer = 0x0D ;
	int cc = -1 ; // Copy key
	int n ;

	while (1)
	    {
		unsigned char key ;

		copykey (&cc, 0) ;
		do
			key = osrdch () ;
		while ((key = copykey (&cc, key)) == 0) ;

		switch (key)
		    {
			case 0x0D:
				return ;

			case 8:
			case 127:
				if (p > buffer)
				    {
					char *q = p ;
					do p-- ; while ((vflags & UTF8) && (*(signed char*)p < -64)) ;
					oswrch (8) ;
					memmove (p, q, buffer + 256 - q) ;
				    }
				break ;

			case 21:
				while (p > buffer)
				    {
					char *q = p ;
					do p-- ; while ((vflags & UTF8) && (*(signed char*)p < -64)) ;
					oswrch (8) ;
					memmove (p, q, buffer + 256 - q) ;
				    }
				break ;

			case 130:
				while (p > buffer)
				    {
					oswrch (8) ;
					do p-- ; while ((vflags & UTF8) && (*(signed char*)p < -64)) ;
				    }
				break ;

			case 131:
				while (*p != 0x0D)
				    {
					oswrch (9) ;
					do p++ ; while ((vflags & UTF8) && (*(signed char*)p < -64)) ;
				    }
				break ;

			case 135:
				if (*p != 0x0D)
				    {
					char *q = p ;
					do q++ ; while ((vflags & UTF8) && (*(signed char*)q < -64)) ;
					memmove (p, q, buffer + 256 - q) ;
				    }
				break ;

			case 136:
				if (p > buffer)
				    {
					oswrch (8) ;
					do p-- ; while ((vflags & UTF8) && (*(signed char*)p < -64)) ;
				    }
				break ;

			case 137:
				if (*p != 0x0D)
				    {
					oswrch (9) ;
					do p++ ; while ((vflags & UTF8) && (*(signed char*)p < -64)) ;
				    }
				break ;

			case 138:
			case 139:
				copykey (&cc, 9) ;
				copykey (&cc, 0) ;
				copykey (&cc, key) ;
				copykey (&cc, 32) ;
				if (*p == 0x0D)
				    {
					oswrch (32) ;
					oswrch (8) ;
				    }
				break ;

			case 155:
				copykey (&cc, 155) ;
				break ;

			case 22:
				if (SDL_HasClipboardText ())
				    {
					char *t = SDL_GetClipboardText () ;
					char *s = t ;
					while ((*s >= ' ') && (p < (buffer + 255)))
					    {
						oswrch (*s) ;
						memmove (p + 1, p, buffer + 255 - p) ;
						*p++ = *s++ ;
					    }
					*(buffer + 255) = 0x0D ;
					SDL_free (t) ;
				    }
				break ;

			case 140:
			case 141:
				break ;

			default:
				if (p < (buffer + 255))
				    {
					if (key != 0x0A)
					    {
						sclflg = -1 ;
						oswrch (key) ;
						if (cc != -1)
						    {
							getcsr (NULL, NULL) ; // thread sync
							switch (sclflg)
							    {
								case 0:	cc += charx ;
									break ;
								case 1:	cc += chary << 16 ;
									break ;
								case 2:	cc -= charx ;
									break ;
								case 3:	cc -= chary << 16 ;
							    }
						    } 
					    }
					if (key >= 32)
					    {
						memmove (p + 1, p, buffer + 255 - p) ;
						*p++ = key ;
						*(buffer + 255) = 0x0D ;
						if ((*p != 0x0D) && (vflags & IOFLAG) && (queue == 0))
						    {
							char *q = p ;
							do q++ ; while ((vflags & UTF8) &&
								(*(signed char*)q < -64)) ;
							memmove (p, q, buffer + 256 - q) ;
						    }
					    }
				    }
		    }

		if (queue == 0)
		    {
			int i ;
			oswrch (23) ;
			oswrch (1) ;
			for (n = 8 ; n != 0 ; n--)
				oswrch (0) ;
			while (*p != 0x0D)
			    {
				oswrch (*p++) ;
				n++ ;
			    }
			for (i = 0; i < (eol - p); i++)
				oswrch (32) ;
			for (i = 0; i < (eol - p); i++)
				oswrch (8) ;
			eol = p ;
			while (n)
			    {
				if (!(vflags & UTF8) || (*(signed char*)p >= -64))
					oswrch (8) ;
				p-- ;
				n-- ;
			    }
			oswrch (23) ;
			oswrch (1) ;
			oswrch (1) ;
			for (n = 7 ; n != 0 ; n--)
				oswrch (0) ;
		    }
	    }
}

// Get TIME
int getime (void)
{
	unsigned int n = SDL_GetTicks () ;
	if (n < lastick)
		timoff += 0x19999999 ;
	lastick = n ;
	return n / 10 + timoff ;
}

int getims (void)
{
	char *at ;
	time_t tt ;

	time (&tt) ;
	at = ctime (&tt) ;
	*(int *)(accs + 0) = *(int *)(at + 0) ; // Day-of-week
	*(int *)(accs + 4) = *(int *)(at + 8) ; // Day-of-month
	*(int *)(accs + 7) = *(int *)(at + 4) ; // Month
	*(int *)(accs + 11) = *(int *)(at + 20) ; // Year
	if (*(accs + 4) == ' ') *(accs + 4) = '0' ;
	memcpy (accs + 16, at + 11, 8) ; // Time
	*(accs + 3) = '.' ;
	*(accs + 15) = ',' ;
	return 24 ;
}

// Put TIME
void putime (int n)
{
	lastick = SDL_GetTicks () ;
	timoff = n - lastick / 10 ;
}

// Wait for a specified number of centiseconds:
// On some platforms specifying a negative value causes a task switch
void oswait (int cs)
{
	unsigned int start = SDL_GetTicks () ;
	if (cs < 0)
	    {
		SDL_Delay (0) ;
		return ;
	    }
	cs *= 10 ;
	do
	    {
		trap () ;
		SDL_Delay (1) ;
	    }
	while ((unsigned int)(SDL_GetTicks () - start) < cs) ;
}


// MOUSE x%, y%, b%
void mouse (int *px, int *py, int *pb)
{
	pushev (EVT_MOUSE, px, py) ;
	*pb = waitev () ;
}

// MOUSE ON [type]
void mouseon (int type)
{
	SDL_Cursor* cursor ;
	int x, y;

	if (type >= 135) type-- ;
	if (type >= 130) type -= 125 ;
	cursor = SDL_CreateSystemCursor (type) ;
	SDL_FreeCursor (SDL_GetCursor ()) ;
	SDL_SetCursor (cursor) ;
	SDL_ShowCursor (SDL_ENABLE) ;
	SDL_GetMouseState (&x, &y) ;
	SDL_WarpMouseInWindow (0, x, y) ;
}

// MOUSE OFF
void mouseoff (void)
{
	int x, y ;
	SDL_ShowCursor (SDL_DISABLE) ;
	SDL_GetMouseState (&x, &y) ;
	SDL_WarpMouseInWindow (0, x, y) ;
}

// MOUSE TO x%, y%
void mouseto (int x, int y)
{
	pushev (EVT_MOUSETO, (void *)(intptr_t)x, (void *)(intptr_t)y) ;
	waitev () ;
}

// Get address of an API function:
void *sysadr (char *name)
{
// On Android it's important to use 'SDL_GL_GetProcAddress' in preference
// because 'dlsym' may return the wrong address for OpenGLES functions.
	void *addr = NULL ;
	if (useGPA)
		addr = SDL_GL_GetProcAddress (name) ;
	if (addr != NULL)
		return addr ; 
	return dlsym (RTLD_DEFAULT, name) ;
}

// Call a function in the context of the GUI thread:
size_t guicall (void *func, PARM *parm)
{
	pushev (EVT_SYSCALL, func, parm) ;
	return waitev () ;
}

int osbyte (int al, int xy)
{
	if (al == 129)
		return (oskey (xy) << 8) + 129 ;
	return ((vgetc (0x80000000, 0x80000000) << 8) + 135) ;
}

void osword (int al, void *xy)
{
	if (al == 139)
		memcpy (xy + 4, &ttxtfont[*(unsigned char*)(xy + 2) * 20], 40) ;
	else if (al == 140)
	    {
		pushev (EVT_OSWORD, (void *)140, xy) ;
		waitev () ;
	    }
	else
		memcpy (xy + 1, &bbcfont[*(unsigned char*)(xy) << 3], 8) ;
}

// Call an emulated OS subroutine (if CALL or USR to an address < 0x10000)
int oscall (int addr)
{
	int al = stavar[1] ;
	void *xy = (void *) ((size_t)stavar[24] | ((size_t)stavar[25] << 8)) ;
	switch (addr)
	    {
		case 0xFFE0: // OSRDCH
			return (int) osrdch () ;

		case 0xFFE3: // OSASCI
			if (al != 0x0D)
			    {
				oswrch (al) ;
				return 0 ;
			    }

		case 0xFFE7: // OSNEWL
			crlf () ;
			return 0 ;

		case 0xFFEE: // OSWRCH
			oswrch (al) ;
			return 0 ;

		case 0xFFF1: // OSWORD
			osword (al, xy) ;
			return 0 ;

		case 0xFFF4: // OSBYTE
			return osbyte (al, stavar[24] | stavar[25] << 8) ;

		case 0xFFF7: // OSCLI
			oscli (xy) ;
			return 0 ; 

		default:
			error (8, NULL) ; // 'Address out of range'
	    }
	return 0 ;
}

// Get string width in graphics units:
int widths (unsigned char *s, int l)
{
	int i ;
	char *t = malloc (l + 1) ;
	char *p = t ;
	for (i = 0; i < l; i++)
	    {
		unsigned char al ;
		if ((al = *s++) >= ' ')
			*p++ = al ;
		else
		    {
			int n = vdulen[(int) al];
			s += n ;
			i += n ;
		    }
	    }
	*p = 0 ; // NUL termination
	pushev (EVT_WIDTH, (void *)(p - t), t) ;
	i = waitev () ;
	free (t) ;
	return i ;
}

// Request memory allocation above HIMEM:
heapptr oshwm (void *addr, int settop)
{
	pushev (WMU_REALLOC, addr, (void *)(size_t) settop) ;
	return waitev () ;
}

// ADVAL(n)
int adval (int n)
{
	if (n <= -5)
	    {
		n = (3 - n) & 3 ; // 0,1,2,3
		n = sndqr[n] - sndqw[n] ;
		if (n <= 0)
			n += SOUNDQL ;
		return n - SOUNDQE ;
	    }
	if (n <= -1)
	    	return (kbdqr - kbdqw - 1) & 0xFF ;

	if (Joystick == NULL)
	    {
		Joystick = SDL_JoystickOpen (0) ;
		if (Joystick == NULL)
			error (245, "Device unavailable") ;
	    }
	if (n == 0)
	    {
		unsigned char buttons = 0 ;
		for (n = 0; n < 8; n++)
		    {
			buttons |= (SDL_JoystickGetButton (Joystick, n) << n) ;
			buttons |= (SDL_JoystickGetButton (Joystick, n + 20) << n) ;
		    }
		return buttons ;
	    }
 	return (SDL_JoystickGetAxis (Joystick, n - 1) + 0x8000) ;
}

// ENVELOPE N,T,PI1,PI2,PI3,PN1,PN2,PN3,AA,AD,AS,AR,ALA,ALD
void envel (signed char *env)
{
	int n ;
	int chan = *env++ & 15 ;
	for (n = 0; n < 13; n++)
		*(envels + 16 * chan + n) = *env++ ;
	*(envels + 16 * chan + n) = 0 ; // Target for sustain & release
}

// SOUND Channel,Amplitude,Pitch,Duration
void sound (short chan, signed char ampl, unsigned char pitch, unsigned char duration)
{
	unsigned char al ;
	int index ;
	int ch = chan & 3 ;
	if (chan & 0xE000) return ;
	if (hwo == 0)
	    {
		pushev (WMU_WAVEOPEN, NULL, NULL) ;
		waitev () ;
	    }
	if (chan & 0x00F0) // Flush ?
	    {
		do
			sndqw[ch] = sndqr[ch] ; // might be interrupted
		while (sndqw[ch] != sndqr[ch]) ;
	    }
	al = sndqw[ch] ;
	index = ch * SOUNDQL + al ;
	soundq[index + 0] = duration ;
	soundq[index + 1] = pitch ;
	soundq[index + 2] = ampl ;
	soundq[index + 3] = chan >> 8 ;
	al += SOUNDQE ;
	if (al >= SOUNDQL)
		al = 0 ;
	while (al == sndqr[ch])
	    {
		SDL_Delay (1) ;
		trap () ;
	    }
	sndqw[ch] = al ;
}

// Disable sound generation:
void quiet (void)
{
	int i ;
	for (i = 0; i < 4; i++)
	    {
		sndqw[i] = 0 ;
		sndqr[i] = 0 ;
		eenvel[i] = 0 ;
		sacc[i] = 0 ;
	    }
	pushev (WMU_WAVECLOSE, NULL, NULL) ;
}

// Process note(s) from sound queue:
static unsigned char note (unsigned char mask)
{
	unsigned char al ;
	unsigned char ch ;
	for (ch = 0; ch < 4; ch++)
	    {
		while (((mask & BIT0) == 0) && ((al = sndqr[ch]) != sndqw[ch]))
		    {
			int index = ch * SOUNDQL + al ;
 			unsigned char duration = soundq[index + 0] ;
			unsigned char pitch = soundq[index + 1] ;
			signed char ampl = soundq[index + 2] ;
			unsigned char hs = soundq[index + 3] ;
			if (((mask & BIT1) == 0) && ((hs & 3) != 0))
			    {
				mask |= hs & 3 ; // waiting for sync
				break ;
			    }
			else
			    {
				soundq[index + 3] = 0x10 ;
				if ((hs & 0xF0) == 0)
				    {
					eenvel[ch] = ampl ;
					epitch[ch] = pitch ;
					easect[ch] = 0 ;
					epsect[ch] = 0 ;
					ecount[ch] = 0 ;
					escale[ch] = 1 ;
				    }
				if (duration == 0)
				    {
					easect[ch] = 3 ; // release phase
					if (ampl <= 0)
					    {
						epitch[ch] = 0 ; // silence
						if (ch == 0) eenvel[ch] = 0 ;
					    }
					al += SOUNDQE ;
					if (al >= SOUNDQL)
						al = 0 ;
					sndqr[ch] = al ;
					mask &= 0xFD ;
				    }
				else if (duration != 255)
				    {
					soundq[index + 0] = duration - 1 ;
					break ;
				    }
				else
					break ;
			    }
		    } ;
		mask = (mask >> 2) | (mask << 6) ;
	    }
	return mask ;
}

// Synthesise sound waveform:
static void tone (short **pbuffer)
{
	int i, ch ;
	short *buffer ;
	unsigned char taps ;
	static unsigned short noise ;
	static unsigned int __attribute__((aligned(16))) tempi[4] ;
	static short __attribute__((aligned(16))) temps[8] ;

	for (ch = 0; ch < 4; ch++)
	    {
		signed char bl = eenvel[ch] ;
		if (bl < 0) // fixed amplitude
			elevel[ch] = -bl << 3 ;
		else if (bl == 0) // silence
		    {
			if (ch == 0)
				elevel[ch] = 0 ; // noise channel
			else
				epitch[ch] = 0 ;
		    }
		else // envelope
		    {
			escale[ch]-- ;
			if (escale[ch] == 0)
			    {
				signed char *ebx = envels + ((bl & 15) << 4) ;
				char cl = *ebx & 0x7F ; // strip repeat bit
				if (cl == 0) cl++ ;
				escale[ch] = cl ;
				unsigned char al = easect[ch] ; // amplitude section
				if (al < 4)
				    {
					signed char step = *(ebx + al + 7) ; // level change
					signed char target = *(ebx + al + 11) ; // target level
					signed char level = elevel[ch] ; // current level
					level += step ; // adjust level
					if ((level < 0) && (step > 0)) level = 127 ;
					if ((level < 0) && (step < 0)) level = 0 ;
					elevel[ch] = level ; // update level
					if (((step < 0) && (level <= target)) ||
					    ((step > 0) && (level >= target)))
					    {
						al++ ; // move to next section
						if (al < 3)
						    {
							elevel[ch] = target ;
							easect[ch] = al ;
						    }
					    }
				    }
				al = epsect[ch] ; // pitch section
				if ((al >= 3) && ((*ebx & BIT7) == 0)) // repeat ?
				    {
					al = 0 ;
					epsect[ch] = 0 ;
					if (tempo & 0x40)
						epitch[ch] -= *(ebx+1) * *(ebx+4) +
							      *(ebx+2) * *(ebx+5) +
							      *(ebx+3) * *(ebx+6) ;
				    }
				if (al < 3)
				    {
					signed char step = *(ebx + al + 1) ; // pitch change
					unsigned char num = *(ebx + al + 4) ; // no.of steps
					ecount[ch]++ ;
					if (ecount[ch] >= num)
					    {
						ecount[ch] = 0 ;
						al++ ; // move to next section
						epsect[ch] = al ;
					    }
					if (num)
						epitch[ch] += step ;
				    }
			    }
		    }
	    }

	tempi[0] = voices[0] ;
	tempi[1] = voices[1] ;
	tempi[2] = voices[2] ;
	tempi[3] = voices[3] ;
#if defined __EMSCRIPTEN__
	int wavep[4] = { tempi[0] << 13, tempi[1] << 13, tempi[2] << 13, tempi[3] << 13 } ;
#elif defined __arm__ || defined __aarch64__
	int32x4_t wavep = vshlq_n_s32 (vld1q_s32 ((int32_t*) tempi), 13) ;
#else
	__m128i wavep = _mm_slli_epi32 (_mm_load_si128 ((__m128i*)tempi), 13) ;
#endif
	tempi[0] = freqs[epitch[0]] ;
	tempi[1] = freqs[epitch[1]] ;
	tempi[2] = freqs[epitch[2]] ;
	tempi[3] = freqs[epitch[3]] ;
#if defined __EMSCRIPTEN__
	unsigned int inctp[4] = { tempi[0] << 13, tempi[1] << 13, tempi[2] << 13, tempi[3] << 13 } ;
#elif defined __arm__ || defined __aarch64__
	uint32x4_t inctp = vshlq_n_u32 (vld1q_u32 ((uint32_t*)tempi), 13) ;
#else
	__m128i inctp = _mm_slli_epi32 (_mm_load_si128 ((__m128i*)tempi), 13) ;
#endif
	temps[0] = ampl[elevel[0]] ;
	temps[1] = ampl[elevel[1]] ;
	temps[2] = ampl[elevel[2]] ;
	temps[3] = ampl[elevel[3]] ;
	temps[4] = temps[0] ;
	temps[5] = temps[1] ;
	temps[6] = temps[2] ;
	temps[7] = temps[3] ;
#if defined __EMSCRIPTEN__
	short smixp[8] = { (smix[0] * temps[0]) >> 16, (smix[1] * temps[1]) >> 16,
			   (smix[2] * temps[2]) >> 16, (smix[3] * temps[3]) >> 16, 
			   (smix[4] * temps[4]) >> 16, (smix[5] * temps[5]) >> 16, 
			   (smix[6] * temps[6]) >> 16, (smix[7] * temps[7]) >> 16 } ; 
#elif defined __arm__ || defined __aarch64__
	int16x8_t smixp = vqdmulhq_s16 (vld1q_s16 ((int16_t*)smix),
					   vld1q_s16 ((int16_t*)temps)) ;
	uint32x4_t saccp = vld1q_u32 ((uint32_t*) sacc) ;
#else
	__m128i smixp = _mm_mulhi_epi16 (_mm_load_si128 ((__m128i*)smix),
					 _mm_load_si128 ((__m128i*)temps)) ;
	__m128i saccp = _mm_loadu_si128 ((__m128i*) sacc) ;
#endif

	taps = noises[epitch[0] & 7] >> 3 ;
	if (noise == 0xFFFF) noise = 0 ;
	buffer = *pbuffer ;
	for (i = 0; i < 441; i++) // samples in 0.01 seconds
	    {
#if defined __EMSCRIPTEN__
		sacc[0] += inctp[0] ; sacc[1] += inctp[1] ; sacc[2] += inctp[2] ; sacc[3] += inctp[3] ;
		tempi[0] = (sacc[0] >> 19) + wavep[0] ; tempi[1] = (sacc[1] >> 19) + wavep[1] ;
		tempi[2] = (sacc[2] >> 19) + wavep[2] ; tempi[3] = (sacc[3] >> 19) + wavep[3] ;
#elif defined __arm__ || defined __aarch64__
		saccp = vaddq_u32 (saccp, inctp) ; // DDS accumulator
		vst1q_u32 ((uint32_t*) tempi, vorrq_u32 (vshrq_n_u32 (saccp, 19), (uint32x4_t)wavep)) ; 
#else
		saccp = _mm_add_epi32 (saccp, inctp) ; // DDS accumulator
		_mm_store_si128 ((__m128i*) tempi, _mm_or_si128 (_mm_srli_epi32 (saccp, 19), wavep)) ;
#endif

		if (tempo < 128)
		    {
			noise = noise >> 1 ;
			noise |= (((noise & taps) == taps) || ((noise & taps) == 0)) << 15 ;
			tempi[0] = noise ;
		    }
		temps[0] = *(waves + tempi[0]) ;
		temps[1] = *(waves + tempi[1]) ;
		temps[2] = *(waves + tempi[2]) ;
		temps[3] = *(waves + tempi[3]) ;
 		temps[4] = temps[0] ;
 		temps[5] = temps[1] ;
 		temps[6] = temps[2] ;
 		temps[7] = temps[3] ;

#if defined __EMSCRIPTEN__
		temps[0] = (temps[0] * smixp[0]) >> 16 ; temps[1] = (temps[1] * smixp[1]) >> 16 ; 
		temps[2] = (temps[2] * smixp[2]) >> 16 ; temps[3] = (temps[3] * smixp[3]) >> 16 ; 
		temps[4] = (temps[4] * smixp[4]) >> 16 ; temps[5] = (temps[5] * smixp[5]) >> 16 ; 
		temps[6] = (temps[6] * smixp[6]) >> 16 ; temps[7] = (temps[7] * smixp[7]) >> 16 ; 
		*buffer++ = ((int)temps[0] + (int)temps[1] + (int)temps[2] + (int)temps[3]) << 1 ; // left
		*buffer++ = ((int)temps[4] + (int)temps[5] + (int)temps[6] + (int)temps[7]) << 1 ; // right
#elif defined __arm__ || defined __aarch64__
		vst1q_s16 ((int16_t*) temps,
				vqdmulhq_s16 (vld1q_s16 ((int16_t*) temps), smixp)) ;
		*buffer++ = ((int)temps[0] + (int)temps[1] + (int)temps[2] + (int)temps[3]) >> 1 ; // left
		*buffer++ = ((int)temps[4] + (int)temps[5] + (int)temps[6] + (int)temps[7]) >> 1 ; // right
#else
		_mm_store_si128 ((__m128i*) temps,
				_mm_mulhi_epi16 (_mm_load_si128 ((__m128i*) temps), smixp)) ;
		*buffer++ = (temps[0] + temps[1] + temps[2] + temps[3]) << 1 ; // left
		*buffer++ = (temps[4] + temps[5] + temps[6] + temps[7]) << 1 ; // right
#endif
	    }

#if defined __EMSCRIPTEN__
//
#elif defined __arm__ || defined __aarch64__
	vst1q_u32 ((uint32_t*) sacc, saccp) ;
#else
	_mm_storeu_si128 ((__m128i*) sacc, saccp) ;
#endif

	*pbuffer = buffer ;
}

// Refill sound buffer (called at approx 100/tempo Hz):
void stick (short* buffer)
{
	unsigned char al = note (0) ;
	unsigned char ah = tempo & 0x3F ;

	if (al)
		note (syncs [al]) ;
	while (ah--)
		tone (&buffer) ;
}

// Get a file context from a channel number:
static SDL_RWops *lookup (void *chan)
{
	SDL_RWops *file = NULL ;

	if ((chan >= (void *)1) && (chan <= (void *)(MAX_PORTS + MAX_FILES)))
		file = (SDL_RWops*) filbuf[(size_t)chan] ;
	else
		file = (SDL_RWops*) chan ;

	if (file == NULL)
		error (222, "Invalid channel") ;
	return file ;
}

// Load a file into memory:
void osload (char *p, void *addr, int max)
{
	int n ;
	SDL_RWops *file ;
	if (NULL == setup (path, p, ".bbc", '\0', NULL))
		error (253, "Bad string") ;
	file = SDL_RWFromFile (path, "rb") ;
	if (file == NULL)
		error (214, "File or path not found") ;
	n = SDL_RWread(file, addr, 1, max) ;
	SDL_RWclose (file) ;
	if (n == 0)
		error (189, SDL_GetError()) ;
}

// Save a file from memory:
void ossave (char *p, void *addr, int len)
{
	int n ;
	SDL_RWops *file ;
	if (NULL == setup (path, p, ".bbc", '\0', NULL))
		error (253, "Bad string") ;
	file = SDL_RWFromFile (path, "w+b") ;
	if (file == NULL)
		error (214, "Couldn't create file") ;
	n = SDL_RWwrite(file, addr, 1, len) ;
	BBC_RWclose (file) ;
	if (n < len)
		error (189, SDL_GetError ()) ;
}

// Open a file:
void *osopen (int type, char *p)
{
	int chan, first, last ;
	SDL_RWops *file ;
	if (setup (path, p, ".bbc", '\0', NULL) == NULL)
		return 0 ;
	if (type == 0)
		file = SDL_RWFromFile (path, "rb") ;
	else if (type == 1)
		file = SDL_RWFromFile (path, "w+b") ;
	else
		file = SDL_RWFromFile (path, "r+b") ;
	if (file == NULL)
		return NULL ;

	if (0 == memcmp (path, "/dev", 4))
	    {
		first = 1 ;
		last = MAX_PORTS ;
	    }
	else
	    {
		first = MAX_PORTS + 1 ;
		last = MAX_PORTS + MAX_FILES ;
	    }

	for (chan = first; chan <= last; chan++)
	    {
		if (filbuf[chan] == 0)
		    {
			filbuf[chan] = file ;
			if (chan > MAX_PORTS)
				*(int *)&fcbtab[chan - MAX_PORTS - 1] = 0 ;
			return (void *)(size_t)chan ;
		    }
	    }
	SDL_RWclose (file) ;
	error (192, "Too many open files") ;
	return NULL ; // never happens
}

// Read file to 256-byte buffer:
static void readb (SDL_RWops *context, unsigned char *buffer, FCB *pfcb)
{
	int amount ;
	if (context == NULL)
		error (222, "Invalid channel") ;
	SDL_RWseek (context, (pfcb->p - pfcb->o) & 0xFF, RW_SEEK_CUR) ;
	amount = SDL_RWread (context, buffer, 1, 256) ;
	pfcb->p = 0 ;
	pfcb->o = amount & 0xFF ;
	pfcb->w = 0 ;
	pfcb->f = (amount != 0) ;
	while (amount < 256)
		buffer[amount++] = 0 ;
	return ;
}

// Write 256-byte buffer to file:
static int writeb (SDL_RWops *context, unsigned char *buffer, FCB *pfcb)
{
	int amount ;
	if (pfcb->f >= 0)
	    {
		pfcb->f = 0 ;
		return 0 ;
	    }
	if (context == NULL)
		error (222, "Invalid channel") ;
	if (pfcb->f & 1)
		SDL_RWseek (context, pfcb->o ? -pfcb->o : -256, RW_SEEK_CUR) ;
	amount = SDL_RWwrite (context, buffer, 1, pfcb->w ? pfcb->w : 256) ;
	pfcb->o = amount & 0xFF ;
	pfcb->w = 0 ;
	pfcb->f = 1 ;
	return (amount == 0) ;
}

// Close a single file:
static int closeb (void *chan)
{
	int result ;
	if ((chan > (void *)MAX_PORTS) && (chan <= (void *)(MAX_PORTS+MAX_FILES)))
	    {
		int index = (size_t) chan - MAX_PORTS - 1 ;
		unsigned char *buffer = (unsigned char *) filbuf[0] + index * 0x100 ;
		FCB *pfcb = &fcbtab[index] ;
		SDL_RWops *handle = (SDL_RWops *) filbuf[(size_t) chan] ;
		if (writeb (handle, buffer, pfcb))
			return 1 ;
	    }
	result = BBC_RWclose (lookup (chan)) ;
	if ((chan >= (void *)1) && (chan <= (void *)(MAX_PORTS + MAX_FILES)))
		filbuf[(size_t)chan] = 0 ;
	return result ;
}

// Read a byte:
unsigned char osbget (void *chan, int *peof)
{
	unsigned char byte = 0 ;
	if (peof != NULL)
		*peof = 0 ;
	if ((chan > (void *)MAX_PORTS) && (chan <= (void *)(MAX_PORTS+MAX_FILES)))
	    {
		int index = (size_t) chan - MAX_PORTS - 1 ;
		unsigned char *buffer = (unsigned char *) filbuf[0] + index * 0x100 ;
		FCB *pfcb = &fcbtab[index] ;
		if (pfcb->p == pfcb->o)
		    {
			SDL_RWops *handle = (SDL_RWops *) filbuf[(size_t) chan] ;
			if (writeb (handle, buffer, pfcb))
				error (189, SDL_GetError ()) ;
			readb (handle, buffer, pfcb) ;
			if ((pfcb->f & 1) == 0)
			    {
				if (peof != NULL)
					*peof = 1 ;
				return 0 ;
			    } 
		    }
		return buffer[pfcb->p++] ;
	    }
	if ((0 == SDL_RWread (lookup (chan), &byte, 1, 1)) && (peof != NULL))
		*peof = 1 ;
	return byte ;
}

// Write a byte:
void osbput (void *chan, unsigned char byte)
{
	if ((chan > (void *)MAX_PORTS) && (chan <= (void *)(MAX_PORTS+MAX_FILES)))
	    {
		int index = (size_t) chan - MAX_PORTS - 1 ;
		unsigned char *buffer = (unsigned char *) filbuf[0] + index * 0x100 ;
		FCB *pfcb = &fcbtab[index] ;
		if (pfcb->p == pfcb->o)
		    {
			SDL_RWops *handle = (SDL_RWops *) filbuf[(size_t) chan] ;
			if (writeb (handle, buffer, pfcb))
				error (189, SDL_GetError ()) ;
			readb (handle, buffer, pfcb) ;
		    }
		buffer[pfcb->p++] = byte ;
		pfcb->w = pfcb->p ;
		pfcb->f |= 0x80 ;
		return ;
	    }
	if (0 == SDL_RWwrite (lookup (chan), &byte, 1, 1))
		error (189, SDL_GetError ()) ;
}

// Get file pointer:
long long getptr (void *chan)
{
	long long ptr = SDL_RWseek (lookup (chan), 0, RW_SEEK_CUR) ;
	if (ptr == -1)
		error (189, SDL_GetError ()) ;
	if ((chan > (void *)MAX_PORTS) && (chan <= (void *)(MAX_PORTS+MAX_FILES)))
	    {
		FCB *pfcb = &fcbtab[(size_t) chan - MAX_PORTS - 1] ;
		if (pfcb->o)
			ptr -= pfcb->o ;
		else if (pfcb->f & 1)
			ptr -= 256 ;
		if (pfcb->p)
			ptr += pfcb->p ;
		else if (pfcb->f & 0x81)
			ptr += 256 ;
	    }
	return ptr ;
}

// Set file pointer:
void setptr (void *chan, long long ptr)
{
	if ((chan > (void *)MAX_PORTS) && (chan <= (void *)(MAX_PORTS+MAX_FILES)))
	    {
		int index = (size_t) chan - MAX_PORTS - 1 ;
		unsigned char *buffer = (unsigned char *) filbuf[0] + index * 0x100 ;
		FCB *pfcb = &fcbtab[index] ;
		SDL_RWops *handle = (SDL_RWops *) filbuf[(size_t) chan] ;
		if (writeb (handle, buffer, pfcb))
			error (189, SDL_GetError ()) ;
		*(int *)pfcb = 0 ;
	    }
	if (-1 == SDL_RWseek (lookup (chan), ptr, RW_SEEK_SET))
		error (189, SDL_GetError ()) ;
}

// Get file size:
long long getext (void *chan)
{
	long long newptr = getptr (chan) ;
	SDL_RWops *file = lookup (chan) ;
	long long ptr = SDL_RWseek (file, 0, RW_SEEK_CUR) ;
	long long size = SDL_RWseek (file, 0, RW_SEEK_END) ;
	if ((ptr == -1) || (size == -1))
		error (189, SDL_GetError ()) ;
	SDL_RWseek (file, ptr, RW_SEEK_SET) ;
	if (newptr > size)
		return newptr ;
	return size ;
}

// Get EOF status:
long long geteof (void *chan)
{
	if ((chan > (void *)MAX_PORTS) && (chan <= (void *)(MAX_PORTS+MAX_FILES)))
	    {
		FCB *pfcb = &fcbtab[(size_t) chan - MAX_PORTS - 1] ;
		if ((pfcb->p != 0) && (pfcb->o == 0) && ((pfcb->f) & 1))
			return 0 ;
	    }
	return -(getptr (chan) >= getext (chan)) ;
}

// Close file (if chan = 0 all open files closed and errors ignored):
void osshut (void *chan)
{
	if (chan == NULL)
	    {
		int chan ;
		for (chan = MAX_PORTS + MAX_FILES; chan > 0; chan--)
		    {
			if (filbuf[chan])
				closeb ((void *)(size_t)chan) ; // ignore errors
		    }
		return ;
	    }
	if (closeb (chan))
		error (189, SDL_GetError()) ;
}

// Start interpreter:
int entry (void *immediate)
{
	int i ;
	short *table ;

	memset (&stavar[1], 0, (char *)datend - (char *)&stavar[1]) ;

	accs = (char*) userRAM ;		// String accumulator
	buff = (char*) accs + 0x10000 ;		// Temporary string buffer
	path = (char*) buff + 0x100 ;		// File path
	keystr = (char**) (path + 0x100) ;	// *KEY strings
	keybdq = (char*) keystr + 0x100 ;	// Keyboard queue
	eventq = (void*) keybdq + 0x100 ;	// Event queue
	envels = (signed char*) eventq + 0x200 ;// Envelopes
	waves = (short*) (envels + 0x100) ;	// Waveforms n.b. &20000 bytes long
	filbuf[0] = (waves + 0x10000) ;		// File buffers n.b. pointer arithmetic!!
	usrchr = (char*) (filbuf[0] + 0x800) ;	// User-defined characters
	tempo = 0x45 ;				// Default SOUND tempo
	farray = 1 ;				// @hfile%() number of dimensions
	fasize = MAX_PORTS + MAX_FILES + 4 ;	// @hfile%() number of elements

	prand = (unsigned int) SDL_GetPerformanceCounter() ;	// Seed PRNG
	*(unsigned char*)(&prand + 1) = (prand == 0) ;
	rnd () ;				// Randomise !

	table = waves ;
	for (i = 0; i < 8; i++)
	    {
		int sample ;
		for (sample = 0; sample < 8192; sample++)
		    {
			int harmonic ;
			float sum = 0 ;
			for (harmonic = 0; harmonic < 9; harmonic++)
			    {
				sum += harms[i][harmonic] *
				    sinf ((float)(harmonic + 1) * (float)sample * 0.00076699) ;
			    }
			*table++ = (short) 32767.0 * sum ;
		    }
		smix[i] = 0x4000 ;
	    }

	memset (keystr, 0, 256) ;
	pushev (0x171F, NULL, (void *) 0x171F) ;	// initialise VDU drivers
	spchan = NULL ;
	exchan = NULL ;

	if (immediate)
	    {
		text (szVersion) ;
		crlf () ;
		text (szNotice) ;
		crlf () ;
	    }

	pushev (EVT_QUIT, NULL, (void *)(intptr_t) basic (progRAM, userTOP, immediate)) ;
	return 0 ;
}

