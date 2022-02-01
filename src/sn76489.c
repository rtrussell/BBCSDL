/* sn76489.c - Emulation of the SN76489 chip for sound generation on MCUs

In the BBC micro the SN76489 chip is driven at 4MHz.
This clock is initially divided by 16 to give a 250KHz chip clock
For reasonable audio output, require samples at appox 44.1KHz
Now 250KHz / 44.1KHz = 5.669
This suggests one sample every 6 chip clocks
 */

#ifdef PICO
#include "pico/stdlib.h"
#else
#define __time_critical_func(x)     x
#endif
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include "bbccon.h"

#define DEBUG   0x00

#define NCHAN   4
#define CLKSTP  6

void snd_init (void);
void snd_start (void);
void snd_stop (void);

static struct st_tone
    {
    int     reload;
    int     dcount;
    int     amp;
    int     val;
    } snd_tone[NCHAN];

static int  noise_dcount = 0;
static uint8_t noise_mode = 0;
static uint16_t noise_sr = 0x8000;
static int  noise_amp = 0;
static int  noise_val = 0;
static int  snd_val = 0;
#if DEBUG & 0x01
static int  snd_cnt = 0;
static int  snd_min = 0;
static int  snd_max = 0;
#endif

static bool bInitSnd = false;

#define NOCTAVE 48  // Number of tones per octave
static const double ftone[NOCTAVE] =
    {                   // = 440Hz * 2 ^ ( ( i - 136 ) / 48 )
    61.73541266,        // 0
    62.63337491,        // 1
    63.54439833,        // 2
    64.46867289,        // 3
    65.40639133,        // 4
    66.35774919,        // 5
    67.32294488,        // 6
    68.30217966,        // 7
    69.29565774,        // 8
    70.30358630,        // 9
    71.32617551,        // 10
    72.36363862,        // 11
    73.41619198,        // 12
    74.48405508,        // 13
    75.56745061,        // 14
    76.66660449,        // 15
    77.78174593,        // 16
    78.91310748,        // 17
    80.06092506,        // 18
    81.22543803,        // 19
    82.40688923,        // 20
    83.60552503,        // 21
    84.82159540,        // 22
    86.05535391,        // 23
    87.30705786,        // 24
    88.57696826,        // 25
    89.86534993,        // 26
    91.17247154,        // 27
    92.49860568,        // 28
    93.84402888,        // 29
    95.20902171,        // 30
    96.59386882,        // 31
    97.99885900,        // 32
    99.42428522,        // 33
    100.87044475,       // 34
    102.33763916,       // 35
    103.82617439,       // 36
    105.33636088,       // 37
    106.86851353,       // 38
    108.42295185,       // 39
    110.00000000,       // 40
    111.59998684,       // 41
    113.22324603,       // 42
    114.87011607,       // 43
    116.54094038,       // 44
    118.23606739,       // 45
    119.95585059,       // 46
    121.70064862        // 47
    };

static const int logamp[128] =
    {           // = 0x1FFFFFFF * 10 ^ ( - ( 127 - i ) / 4 / 20 )
    0x00000000,	// 0
    0x00D9F772,	// 1
    0x00E054D2,	// 2
    0x00E6E1C6,	// 3
    0x00ED9FB1,	// 4
    0x00F49002,	// 5
    0x00FBB432,	// 6
    0x01030DC4,	// 7
    0x010A9E48,	// 8
    0x01126758,	// 9
    0x011A6A9A,	// 10
    0x0122A9C2,	// 11
    0x012B2690,	// 12
    0x0133E2D0,	// 13
    0x013CE05E,	// 14
    0x0146211F,	// 15
    0x014FA70D,	// 16
    0x0159742A,	// 17
    0x01638A8C,	// 18
    0x016DEC56,	// 19
    0x01789BBC,	// 20
    0x01839B02,	// 21
    0x018EEC7D,	// 22
    0x019A9294,	// 23
    0x01A68FBF,	// 24
    0x01B2E689,	// 25
    0x01BF9990,	// 26
    0x01CCAB85,	// 27
    0x01DA1F2F,	// 28
    0x01E7F767,	// 29
    0x01F6371E,	// 30
    0x0204E158,	// 31
    0x0213F932,	// 32
    0x022381E0,	// 33
    0x02337EAD,	// 34
    0x0243F2FD,	// 35
    0x0254E24E,	// 36
    0x02665036,	// 37
    0x02784069,	// 38
    0x028AB6B4,	// 39
    0x029DB701,	// 40
    0x02B14559,	// 41
    0x02C565E1,	// 42
    0x02DA1CDE,	// 43
    0x02EF6EB4,	// 44
    0x03055FEA,	// 45
    0x031BF526,	// 46
    0x03333333,	// 47
    0x034B1EFE,	// 48
    0x0363BD9B,	// 49
    0x037D1442,	// 50
    0x03972852,	// 51
    0x03B1FF55,	// 52
    0x03CD9EFB,	// 53
    0x03EA0D20,	// 54
    0x04074FCB,	// 55
    0x04256D31,	// 56
    0x04446BB6,	// 57
    0x046451EC,	// 58
    0x04852697,	// 59
    0x04A6F0AE,	// 60
    0x04C9B75B,	// 61
    0x04ED81FF,	// 62
    0x05125831,	// 63
    0x053841C0,	// 64
    0x055F46B8,	// 65
    0x05876F5E,	// 66
    0x05B0C438,	// 67
    0x05DB4E09,	// 68
    0x060715D8,	// 69
    0x063424EC,	// 70
    0x066284D5,	// 71
    0x06923F69,	// 72
    0x06C35EC6,	// 73
    0x06F5ED59,	// 74
    0x0729F5D9,	// 75
    0x075F8351,	// 76
    0x0796A11C,	// 77
    0x07CF5AEA,	// 78
    0x0809BCC3,	// 79
    0x0845D30A,	// 80
    0x0883AA7C,	// 81
    0x08C35038,	// 82
    0x0904D1BD,	// 83
    0x09483CEF,	// 84
    0x098DA01C,	// 85
    0x09D509FB,	// 86
    0x0A1E89B0,	// 87
    0x0A6A2ED4,	// 88
    0x0AB80970,	// 89
    0x0B082A08,	// 90
    0x0B5AA19B,	// 91
    0x0BAF81A6,	// 92
    0x0C06DC29,	// 93
    0x0C60C3AC,	// 94
    0x0CBD4B3F,	// 95
    0x0D1C8683,	// 96
    0x0D7E89A9,	// 97
    0x0DE3697D,	// 98
    0x0E4B3B63,	// 99
    0x0EB6155F,	// 100
    0x0F240E1B,	// 101
    0x0F953CEA,	// 102
    0x1009B9CE,	// 103
    0x10819D7B,	// 104
    0x10FD015F,	// 105
    0x117BFFA4,	// 106
    0x11FEB33B,	// 107
    0x128537DB,	// 108
    0x130FAA0D,	// 109
    0x139E272D,	// 110
    0x1430CD74,	// 111
    0x14C7BBFC,	// 112
    0x156312C7,	// 113
    0x1602F2C9,	// 114
    0x16A77DEA,	// 115
    0x1750D70F,	// 116
    0x17FF2223,	// 117
    0x18B2841E,	// 118
    0x196B230B,	// 119
    0x1A292612,	// 120
    0x1AECB580,	// 121
    0x1BB5FACF,	// 122
    0x1C8520AF,	// 123
    0x1D5A530F,	// 124
    0x1E35BF26,	// 125
    0x1F17937F,	// 126
    0x1FFFFFFF	// 127
    };

static uint16_t tonediv[256];

static void snd_cfg (int iChan, int iTone, int iAmp)
    {
    if (( iChan == 0 ) && ( (tempo & 0x80) == 0 ))
        {
        noise_mode = iTone & 0x07;
        noise_amp = logamp[iAmp & 0x7F];
#if DEBUG & 0x04
        // printf ("snd_cfd: noise_mode = 0x%02X, noise_amp = 0x%08X\n", noise_mode, noise_amp);
#endif
        }
    else
        {
        snd_tone[iChan].reload = tonediv[iTone];
        snd_tone[iChan].amp = logamp[iAmp & 0x7F];
#if DEBUG & 0x04
        if ( iChan == 1 ) printf ("snd_cfd: iTone = %d, iAmp = %d, amp = 0x%08X, reload = 0x%03X\n",
            iTone, iAmp, snd_tone[iChan].amp, snd_tone[iChan].reload);
#endif
        }
    }

static bool __time_critical_func(tone_step) (struct st_tone *ptone)
    {
    ptone->dcount -= CLKSTP;
    if ( ptone->dcount < 0 )
        {
        ptone->dcount = ptone->reload;
        if ( ptone->val > 0 ) ptone->val = - ptone->amp;
        else ptone->val = ptone->amp;
        return true;
        }
    return false;
    }

static void __time_critical_func(noise_shift) (void)
    {
    if ( noise_sr == 0 ) noise_sr = 0x8000;
    uint16_t hibit = noise_sr << 15;
    if ( noise_mode & 0x04 ) hibit ^= ( noise_sr & 0x0008 ) << 12;
    noise_sr = hibit | ( noise_sr >> 1 );
    if ( noise_sr & 1 ) noise_val = noise_amp;
    else noise_val = - noise_amp;
    }

static void __time_critical_func(noise_step) (bool bOut3)
    {
    if ( ( noise_mode & 0x03 ) == 0x03 )
        {
        if ( bOut3 ) noise_shift ();
        }
    else
        {
        noise_dcount -= CLKSTP;
        if ( noise_dcount < 0 )
            {
            noise_shift ();
            noise_dcount = 2 << ( noise_mode & 0x03 );
            }
        }
    }

int16_t __time_critical_func(snd_step) (void)
    {
    bool bOut3;
    tone_step (&snd_tone[1]);
    tone_step (&snd_tone[2]);
    bOut3 = tone_step (&snd_tone[3]);
    int val = snd_tone[1].val + snd_tone[2].val + snd_tone[3].val;
    if ( tempo & 0x80 )
        {
        tone_step (&snd_tone[0]);
        val += snd_tone[0].val;
        }
    else
        {
        noise_step (bOut3);
        val += noise_val;
        }
    snd_val += ( val - snd_val ) >> 3;
#if DEBUG & 0x01
    ++snd_cnt;
    if ( snd_val > snd_max ) snd_max = snd_val;
    else if ( snd_val < snd_min ) snd_min = snd_val;
#endif
    return (int16_t) (snd_val >> 16);
    }

void snd_freq (double fchip)
    {
    double scale = 2.0;
    int iTone = 0;
    for (int i = 0; i < 256; ++i)
        {
        if ( iTone >= NOCTAVE )
            {
            scale *= 2.0;
            iTone = 0;
            }
        tonediv[i] = (uint16_t) (fchip / ftone[iTone] / scale + 0.5);
        ++iTone;
        }
#if DEBUG & 0x01
    printf ("fchip = %3.1f Hz\n", fchip);
#endif
    }

#define LEN_SNDQUE  5
#define NENVL       16

typedef struct
    {
    uint8_t ntick;      // Time between envelope updates (multiples of 10ms)
    int8_t  ip[3];      // Pitch increments for each stage
    uint8_t np[3];      // Number of steps in each pitch stage
    int8_t  ia[4];      // Amplitude increments for each stage
    uint8_t la[4];      // Limit values for each amplitude stage
    }
    ENVEL;

static ENVEL    senv[NENVL];
static ENVEL   *penv[NCHAN];
static uint32_t durn[NCHAN];

typedef struct
    {
    uint8_t durn;       // Sound duration
    uint8_t pitch;      // Sound pitch
    int8_t  amp;        // Sound amplitude
    uint8_t sync;       // Channel syncronisation
    }
    SNDDEF;

#define AS_ATTACK   0
#define AS_DECAY    1
#define AS_SUSTAIN  2
#define AS_RELEASE  3

// The following structure overlays the global variables of the same names
// Hopefully it gives the optimiser a few hints to optimise data access
// The entries must exactly match those in bbdata_arm_32.s in size and order

typedef struct
    {
    uint8_t sndqw[NCHAN];   /* Sound queue write pointers */
    uint8_t sndqr[NCHAN];   /* Sound queue read pointers */
    uint8_t eenvel[NCHAN];  /* Envelope number */
    uint8_t escale[NCHAN];  /* Envelope scaler */
    uint8_t epsect[NCHAN];  /* Envelope pitch section */
    uint8_t easect[NCHAN];  /* Envelope amplitude section */
    uint8_t epitch[NCHAN];  /* Envelope pitch (frequency) */
    uint8_t ecount[NCHAN];  /* Envelope count */
    SNDDEF  soundq[NCHAN][SOUNDQL / SOUNDQE];   /* Sound queue (four channels) */
    }
    SOUND_DATA;

// A constant pointer to variable data
static SOUND_DATA * const psd = (SOUND_DATA *) sndqw;

static int nsync[NCHAN];

static void snd_pop (int ch)
    {
    if ( psd->sndqr[ch] != psd->sndqw[ch] )
        {
        SNDDEF *pdef = &psd->soundq[ch][psd->sndqr[ch]];
        if ( pdef->sync > 0 )
            {
            nsync[0] = 1;
            ++nsync[pdef->sync];
            }
        else
            {
#if DEBUG & 0x01
            if ( ch == 1 ) printf ("Pop queue: durn = %d, amp = %d, pitch = %d\n",
                pdef->durn, pdef->amp, pdef->pitch);
#endif
            if ( pdef->durn == 0xFF ) durn[ch] = -1;
            else durn[ch] = ( tempo & 0x3F ) * pdef->durn;
            if ( pdef->amp == (int8_t) 0x80 )
                {
#if DEBUG & 0x01
                if ( ch == 1 ) printf ("Hold previous note\n");
#endif
                if ( penv[ch] ) psd->easect[ch] = AS_RELEASE;
                }
            else if ( pdef->amp <= 0 )
                {
#if DEBUG & 0x01
                if ( ch == 1 ) printf ("Fixed amplitude and pitch\n");
#endif
                penv[ch] = NULL;
                psd->eenvel[ch] = -8 * pdef->amp;
                psd->epitch[ch] = pdef->pitch;
                }
            else
                {
                penv[ch] = &senv[(pdef->amp - 1) & 0x0F];
                psd->escale[ch] = 1;
                psd->ecount[ch] = 0;
                psd->epsect[ch] = 0;
                psd->easect[ch] = AS_ATTACK;
                psd->eenvel[ch] = 0;
                psd->epitch[ch] = pdef->pitch;
#if DEBUG & 0x01
                if ( ch == 1 ) printf ("Envelope %d: ntick = %d, ip = %d %d %d, np = %d %d %d, ia = %d %d %d %d, la = %d %d %d %d\n",
                    (pdef->amp - 1) & 0x0F, penv[ch]->ntick,
                    penv[ch]->ip[0], penv[ch]->ip[1], penv[ch]->ip[2],
                    penv[ch]->np[0], penv[ch]->np[1], penv[ch]->np[2],
                    penv[ch]->ia[0], penv[ch]->ia[1], penv[ch]->ia[2], penv[ch]->ia[3],
                    penv[ch]->la[0], penv[ch]->la[1], penv[ch]->la[2], penv[ch]->la[3]);
#endif
                }
            if ( ++psd->sndqr[ch] >= LEN_SNDQUE ) psd->sndqr[ch] = 0;
            }
        }
    }

static void snd_sync (void)
    {
    if ( nsync[0] > 0 )
        {
        for (int isync = 1; isync < NCHAN; ++isync)
            {
            if ( nsync[isync] > isync )
                {
                for (int ch = 0; ch < NCHAN; ++ch)
                    {
                    SNDDEF *pdef = &psd->soundq[ch][psd->sndqr[ch]];
                    if ( pdef->sync == isync ) pdef->sync = 0;
                    }
                }
            nsync[isync] = 0;
            }
        }
    nsync[0] = 0;
    }

void snd_process (void)
    {
    for (int ch = 0; ch < NCHAN; ++ch)
        {
#if DEBUG & 0x01
        if ( ch == 1 ) printf ("snd_process: ch = %d, durn[ch] = %d, escale = %d\n", ch, durn[ch], psd->escale[ch]);
#endif
        if ( durn[ch] == 0 )
            {
            if ( ! penv[ch] ) psd->eenvel[ch] = 0;
            else psd->easect[ch] = AS_RELEASE;
            snd_pop (ch);
            }
        else if ( durn[ch] > 0 )
            {
            --durn[ch];
#if DEBUG & 0x01
            if ( durn[ch] == 0 )
                {
                if ( ch == 1 ) printf ("End of sound on channel %d: cnt = %d, min = %d, max = %d\n",
                    ch, snd_cnt, snd_min, snd_max);
                snd_cnt = 0;
                snd_min = 0;
                snd_max = 0;
                }
#endif
            }
        if ( penv[ch] )
            {
            if ( --psd->escale[ch] == 0 )
                {
                psd->escale[ch] = penv[ch]->ntick & 0x7F;
                int stage = psd->epsect[ch];
                if (( stage == 3 ) && ( (penv[ch]->ntick & 0x80) == 0 ))
                    {
                    if ( tempo & 0x40 )
                        {
                        psd->epitch[ch] -= penv[ch]->np[0] * penv[ch]->ip[0]
                            + penv[ch]->np[1] * penv[ch]->ip[1]
                            + penv[ch]->np[2] * penv[ch]->ip[2];
                        }
                    stage = 0;
                    psd->epsect[ch] = 0;
                    psd->ecount[ch] = 0;
                    }
                if ( stage < 3 )
                    {
#if DEBUG & 0x02
                    if ( ch == 1 ) printf ("Pitch %d, step %d\n", psd->epitch[ch], penv[ch]->ip[stage]);
#endif
                    psd->epitch[ch] += penv[ch]->ip[stage];
                    if ( ++psd->ecount[ch] >= penv[ch]->np[stage] )
                        {
#if DEBUG & 0x01
                        if ( ch == 1 ) printf ("End of pitch stage %d\n", psd->epsect[ch]);
#endif
                        ++psd->epsect[ch];
                        psd->ecount[ch] = 0;
                        }
                    }
                stage = psd->easect[ch];
                int amp = psd->eenvel[ch];
                int step = penv[ch]->ia[stage];
                int target = penv[ch]->la[stage];
#if DEBUG & 0x02
                if ( ch == 1 ) printf ("Stage = %d, amplitude %d, step %d, target = %d\n", stage, amp, step, target);
#endif
                amp += step;
                if ( amp < 0 )
                    {
                    if ( step > 0 ) amp = 127;
                    else amp = 0;
                    }
                if ( stage < 3 )
                    {
                    if ((( step > 0 ) && ( amp >= target ))
                        || (( step < 0 ) && ( amp <= target )))
                        {
#if DEBUG & 0x01
                        if ( ch == 1 ) printf ("End of amplitude stage %d\n", psd->easect[ch]);
#endif
                        ++psd->easect[ch];
                        amp = target;
                        }
                    }
                else if ( amp <= 0 )
                    {
                    penv[ch] = NULL;
                    }
                psd->eenvel[ch] = amp;
                }
            }
        snd_cfg (ch, psd->epitch[ch], psd->eenvel[ch]);
        }
    snd_sync ();
    }

// ENVELOPE N,T,PI1,PI2,PI3,PN1,PN2,PN3,AA,AD,AS,AR,ALA,ALD
void envel (signed char *env)
    {
	ENVEL *penv = &senv[(*env - 1) & 0x15];
    memcpy (penv, ++env, 13);
    penv->la[2] = 0;
    penv->la[3] = 0;
    if ( (penv->ntick & 0x7F) == 0 ) ++penv->ntick;
    }

void sound (short chan, signed char ampl, unsigned char pitch, unsigned char duration)
    {
#if DEBUG & 0x01
    printf ("sound (0x%04X, %d, %d, %d)\n", chan, ampl, pitch, duration);
#endif
    if ( ! bInitSnd ) snd_init ();
    int ch = chan & 0x0F;
    uint8_t sync = ( chan >> 8 ) & 0x03;
    if (( chan & 0x10 ) > 0)
        {
#if DEBUG & 0x01
        printf ("Flush queue\n");
#endif
        psd->sndqw[ch] = psd->sndqr[ch];
        durn[ch] = 0;
        }
    uint8_t nxwr = psd->sndqw[ch] + 1;
    if ( nxwr >= LEN_SNDQUE ) nxwr = 0;
#if DEBUG & 0x01
    printf ("nrd = %d, nwr = %d, nxwr = %d\n", psd->sndqr[ch], psd->sndqw[ch], nxwr);
#endif
    while ( psd->sndqr[ch] == nxwr ) sleep_ms (100);
    SNDDEF *pdef = &(psd->soundq[ch][psd->sndqw[ch]]);
    if ( ampl < 0 ) ampl |= 0xF0;
    pdef->sync = sync;
    pdef->pitch = pitch;
    pdef->amp = ampl;
    pdef->durn = duration;
    if ( chan & 0x1000 ) pdef->amp = 0x80;
    psd->sndqw[ch] = nxwr;
    }

void snd_init (void)
    {
#if DEBUG & 0x01
    printf ("snd_init (%d, %f)\n");
#endif
    if ( bInitSnd ) return;
    memset (snd_tone, 0, sizeof (snd_tone));
    memset (psd, 0, sizeof (SOUND_DATA));
    memset (penv, 0, sizeof (penv));
    memset (durn, 0, sizeof (durn));
    memset (nsync, 0, sizeof (nsync));
    noise_dcount = 0;
    noise_mode = 0;
    noise_sr = 0x8000;
    noise_amp = 0;
    noise_val = 0;
    snd_val = 0;
    tempo = 0x45;
    snd_start ();
    bInitSnd = true;
    }

void snd_term (void)
    {
    if ( ! bInitSnd ) return;
#if DEBUG & 0x04
    printf ("snd_term\n");
#endif
    snd_stop ();
    bInitSnd = false;
    }

// Disable sound generation:
void quiet (void)
    {
#if DEBUG & 0x04
    printf ("quiet\n");
#endif
    snd_term ();
    }

int snd_free (int ch)
    {
    int nfree = psd->sndqr[ch] - psd->sndqw[ch] - 1;
    if ( nfree < 0 ) nfree += LEN_SNDQUE;
    return nfree;
    }
