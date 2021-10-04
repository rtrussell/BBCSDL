/* pico_snd.c - Sound routines for BBC Basic on Pico

In the BBC micro the SN76489 chip is driven at 4MHz.
This clock is initially divided by 16 to give a 250KHz chip clock
For reasonable audio output, require samples at appox 44.1KHz
Now 250KHz / 44.1KHz = 5.669
This suggests one sample every 6 chip clocks
The PIO requires 64 clocks per audio sample, requiring a PIO clock of 2.8224Mhz
The Pico is clocked at 150MHz for generating 640x480 video, giving a PIO clock ratio of 53.14.
Taking the nearest integer value of 53, the PIO clock becomes 2..830Mhz
This gives an audio sample rate of 44.222KHz and an effective chip clock frequency of 265.33KHz
 */

#include "pico/stdlib.h"
#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "hardware/dma.h"
#include "hardware/irq.h"
#include "sound.pio.h"
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include "bbccon.h"

#define DEBUG   1

#define NCHAN   4
#define CLKSTP  6

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
#if DEBUG > 0
static int  snd_cnt = 0;
static int  snd_min = 0;
static int  snd_max = 0;
#endif

static PIO  pio_snd = pio1;
static int  sm_snd_out = -1;
static uint offset_out = 0;

static bool bInitSnd = false;
static int  nFill = 0;
static int  nFillMax;

#define NOCTAVE 48  // Number of tones per octave
static const double ftone[NOCTAVE] =
    {
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
    {
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
#if DEBUG > 1
        printf ("noise_mode = 0x%02X, noise_amp = 0x%08X\n", noise_mode, noise_amp);
#endif
        }
    else
        {
        snd_tone[iChan].reload = tonediv[iTone];
        snd_tone[iChan].amp = logamp[iAmp & 0x7F];
#if DEBUG > 1
        printf ("amp = 0x%08X, reload = 0x%03X\n", snd_tone[iChan - 1].amp, snd_tone[iChan - 1].reload);
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

static int16_t __time_critical_func(snd_step) (void)
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
#if DEBUG > 0
    ++snd_cnt;
    if ( snd_val > snd_max ) snd_max = snd_val;
    else if ( snd_val < snd_min ) snd_min = snd_val;
#endif
    return (int16_t) (snd_val >> 16);
    }

static void snd_process (void);

static void __time_critical_func(snd_fill) (void)
    {
    while ( ! pio_sm_is_tx_fifo_full (pio_snd, sm_snd_out) )
        {
        io_rw_16 *txfifo = (io_rw_16 *) &pio_snd->txf[sm_snd_out];
        *txfifo = snd_step ();
        ++nFill;
        }
    if ( nFill >= nFillMax )
        {
        snd_process ();
        nFill -= nFillMax;
        }
    }

static void __time_critical_func(snd_isr) (void)
    {
    if ( pio_snd->ints0 & ( PIO_INTR_SM0_TXNFULL_BITS << sm_snd_out ) )
        {
        // pio_snd->ints0 = ( PIO_INTR_SM0_TXNFULL_BITS << sm_snd_out );
        snd_fill ();
        }
    }

static void snd_freq (double fchip)
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
    nFillMax = (int)(fchip / 100.0 + 0.5);
    }

#define LEN_SNDQUE  5
#define NENVL       16

typedef struct
    {
    uint8_t ntick;
    int8_t  ip[3];
    uint8_t np[3];
    int8_t  ia[4];
    uint8_t la;
    uint8_t ld;
    }
    ENVEL;

typedef struct
    {
    uint8_t sync;
    uint8_t durn;
    uint8_t pitch;
    int8_t  amp;
    }
    SNDDEF;

typedef struct
    {
    ENVEL   *envel;
    enum {evmAttack, evmDecay, evmSustain, evmRelease} evmode;
    uint8_t durn;
    uint8_t pitch;
    int8_t  amp;
    uint8_t itick;
    uint8_t nstep;
    uint8_t nstage;
    uint8_t nrd;
    uint8_t nwr;
    SNDDEF  sd[LEN_SNDQUE];
    }
    SNDQUE;

static SNDQUE sndque[NCHAN];
static ENVEL senv[NENVL];
static int nsync[NCHAN];

void snd_init (void)
    {
#if DEBUG > 0
    printf ("snd_init (%d, %f)\n");
#endif
    if ( bInitSnd ) return;
    memset (snd_tone, 0, sizeof (snd_tone));
    memset (sndque, 0, sizeof (sndque));
    memset (senv, 0, sizeof (senv));
    memset (nsync, 0, sizeof (nsync));
    noise_dcount = 0;
    noise_mode = 0;
    noise_sr = 0x8000;
    noise_amp = 0;
    noise_val = 0;
    snd_val = 0;
    nFill = 0;
    tempo = 0x45;
    
    pio_gpio_init (pio_snd, PICO_AUDIO_I2S_DATA_PIN);
    pio_gpio_init (pio_snd, PICO_AUDIO_I2S_CLOCK_PIN_BASE);
    pio_gpio_init (pio_snd, PICO_AUDIO_I2S_CLOCK_PIN_BASE + 1);
    
    sm_snd_out = pio_claim_unused_sm (pio_snd, true);
    pio_sm_set_pindirs_with_mask (pio_snd, sm_snd_out,
        ( 1u << PICO_AUDIO_I2S_DATA_PIN ) | ( 3u << PICO_AUDIO_I2S_CLOCK_PIN_BASE ),
        ( 1u << PICO_AUDIO_I2S_DATA_PIN ) | ( 3u << PICO_AUDIO_I2S_CLOCK_PIN_BASE ));
    offset_out = pio_add_program (pio_snd, &sound_out_program);
#if DEBUG > 1
    printf ("sm_snd_out = %d, offset_out = %d\n", sm_snd_out, offset_out);
#endif
    pio_sm_config c_out = sound_out_program_get_default_config (offset_out);
    sm_config_set_out_pins (&c_out, PICO_AUDIO_I2S_DATA_PIN, 1);
    sm_config_set_sideset_pins (&c_out, PICO_AUDIO_I2S_CLOCK_PIN_BASE);
    int fsys = clock_get_hz (clk_sys);
    int div = fsys / ( 64 * 44100 );
    snd_freq (((double) CLKSTP) * fsys / ( 64 * div ));
#if DEBUG > 1
    printf ("div = %f\n", div);
#endif
    sm_config_set_clkdiv (&c_out, div);
    sm_config_set_out_shift (&c_out, false, true, 32);
    pio_sm_init (pio_snd, sm_snd_out, offset_out, &c_out);
    snd_fill ();
    irq_set_exclusive_handler (PIO1_IRQ_0, snd_isr);
    irq_set_enabled (PIO1_IRQ_0, true);
    pio_snd->inte0 = ( PIO_INTR_SM0_TXNFULL_BITS << sm_snd_out );
    pio_sm_set_enabled (pio_snd, sm_snd_out, true);
    bInitSnd = true;
    }

static void snd_pop (SNDQUE *pque)
    {
    if ( pque->nrd != pque->nwr )
        {
        SNDDEF *pdef = &pque->sd[pque->nrd];
        if ( pdef->sync > 0 )
            {
            nsync[0] = 1;
            ++nsync[pdef->sync];
            }
        else
            {
#if DEBUG > 0
            printf ("Pop queue: durn = %d, amp = %d, pitch = %d\n",
                pdef->durn, pdef->amp, pdef->pitch);
#endif
            pque->durn = ( tempo & 0x3F ) * pdef->durn;
            if ( pdef->amp <= -16 )
                {
                if ( pque->envel ) pque->evmode = evmRelease;
                }
            else if ( pdef->amp <= 0 )
                {
                pque->envel = NULL;
                pque->amp = -8 * pdef->amp;
                pque->pitch = pdef->pitch;
                }
            else
                {
                pque->envel = &senv[pque->amp];
                pque->evmode = evmAttack;
                pque->amp = 0;
                pque->pitch = pdef->pitch;
                }
            if ( ++pque->nrd >= LEN_SNDQUE ) pque->nrd = 0;
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
                    SNDQUE *pque = &sndque[ch];
                    SNDDEF *pdef = &pque->sd[pque->nrd];
                    if ( pdef->sync == isync ) pdef->sync = 0;
                    }
                }
            nsync[isync] = 0;
            }
        }
    nsync[0] = 0;
    }

static void snd_process (void)
    {
    for (int ch = 0; ch < NCHAN; ++ch)
        {
        SNDQUE *pque = &sndque[ch];
        if ( pque->durn == 0 )
            {
            if ( ! pque->envel ) pque->amp = 0;
            else pque->evmode = evmRelease;
            snd_pop (pque);
            }
        else if ( pque->durn != 0xFF )
            {
            --pque->durn;
#if DEBUG > 0
            if ( pque->durn == 0 )
                {
                printf ("End of sound on channel %d: cnt = %d, min = %d, max = %d\n",
                    ch, snd_cnt, snd_min, snd_max);
                snd_cnt = 0;
                snd_min = 0;
                snd_max = 0;
                }
#endif
            }
        if ( pque->envel )
            {
            if ( pque->itick == 0 )
                {
                pque->itick = pque->envel->ntick & 0x7F;
                if ( pque->nstage < 3 )
                    {
                    pque->pitch += pque->envel->ip[pque->nstage];
                    if ( --pque->nstep == 0 )
                        {
                        ++pque->nstage;
                        if ( pque->nstage < 3 )
                            {
                            if ( tempo & 0x40 )
                                {
                                pque->pitch -= pque->envel->np[0] * pque->envel->ip[0]
                                    + pque->envel->np[1] * pque->envel->ip[1]
                                    + pque->envel->np[2] * pque->envel->ip[2];
                                }
                            pque->nstep = pque->envel->np[pque->nstage];
                            }
                        else if ( (pque->envel->ntick & 0x80) == 0 )
                            {
                            pque->nstage = 0;
                            pque->nstep = pque->envel->np[0];
                            }
                        }
                    }
                pque->amp += pque->envel->ia[pque->evmode];
                if ( pque->evmode == evmAttack)
                    {
                    if ( pque->amp >= pque->envel->la ) pque->evmode = evmDecay;
                    }
                else if ( pque->evmode == evmDecay)
                    {
                    if ( pque->amp <= pque->envel->ld ) pque->evmode = evmSustain;
                    }
                if ( pque->amp <= 0 )
                    {
                    pque->amp = 0;
                    pque->envel = NULL;
                    }
                }
            --pque->itick;
            }
        snd_cfg (ch, pque->pitch, pque->amp);
        }
    snd_sync ();
    }

// ENVELOPE N,T,PI1,PI2,PI3,PN1,PN2,PN3,AA,AD,AS,AR,ALA,ALD
void envel (signed char *env)
    {
	int n = (*env) & 0x15;
    memcpy (&senv[n], ++env, sizeof (ENVEL));
    }

void sound (short chan, signed char ampl, unsigned char pitch, unsigned char duration)
    {
#if DEBUG > 0
    printf ("sound (0x%04X, %d, %d, %d)\n", chan, ampl, pitch, duration);
#endif
    if ( ! bInitSnd ) snd_init ();
    int ch = chan & 0x0F;
    SNDQUE  *pque = &sndque[ch];
    uint8_t sync = ( chan >> 8 ) & 0x03;
    if (( chan & 0x10 ) > 0)
        {
#if DEBUG > 0
        printf ("Flush queue\n");
#endif
        pque->nwr = pque->nrd;
        pque->durn = 0;
        }
    uint8_t nxwr = pque->nwr + 1;
    if ( nxwr >= LEN_SNDQUE ) nxwr = 0;
#if DEBUG > 0
    printf ("nrd = %d, nwr = %d, nxwr = %d\n", pque->nrd, pque->nwr, nxwr);
#endif
    while ( pque->nrd == nxwr ) sleep_ms (100);
    SNDDEF *pdef = &(pque->sd[pque->nwr]);
    pdef->sync = sync;
    pdef->pitch = pitch;
    pdef->amp = ampl;
    pdef->durn = duration;
    if ( chan & 0x1000 ) pdef->amp = -16;
    pque->nwr = nxwr;
    }

int snd_free (int ch)
    {
    SNDQUE *pque = &sndque[ch];
    int nfree = pque->nrd - pque->nwr - 1;
    if ( nfree < 0 ) nfree += LEN_SNDQUE;
    return nfree;
    }

void snd_term (void)
    {
    if ( ! bInitSnd ) return;
#if DEBUG > 1
    printf ("snd_term\n");
#endif
    pio_sm_set_enabled (pio_snd, sm_snd_out, false);
    pio_snd->inte0 = 0;
    irq_remove_handler (PIO1_IRQ_0, snd_isr);
    pio_remove_program (pio_snd, &sound_out_program, offset_out);
    pio_sm_unclaim (pio_snd, sm_snd_out);
    bInitSnd = false;
    }

// Disable sound generation:
void quiet (void)
    {
    snd_term ();
    }
