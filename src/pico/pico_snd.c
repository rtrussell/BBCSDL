/* pico_snd.c - Sound routines for BBC Basic on Pico

In the BBC micro the SN76489 chip is driven at 4MHz.
This clock is initially divided by 16 to give a 250KHz chip clock
For reasonable audio output, require samples at appox 44.1KHz
Now 250KHz / 44.1KHz = 5.669
This suggests one sample every 6 chip clocks
The PIO requires 64 clocks per audio sample, requiring a PIO clock of 2.8224Mhz
The Pico is clocked at 150MHz for generating 640x480 video, giving a PIO clock ratio of 53.14.
Taking the nearest integer value of 53, the PIO clock becomes 2.830Mhz
This gives an audio sample rate of 44.222KHz and an effective chip clock frequency of 265.33KHz
 */

#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include <string.h>
#include <stdio.h>
#include <stdint.h>

#define CLKSTP  6       // Must match the value in sn76489.c

uint16_t snd_step (void);
void snd_freq (double fchip);
void snd_process (void);

static int  nFill = 0;
static int  nFillMax;

#if PICO_SOUND == 1

#include "hardware/clocks.h"
#include "hardware/irq.h"
#include "sound.pio.h"

static PIO  pio_snd = pio1;
static int  sm_snd_out = -1;
static uint offset_out = 0;

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

void snd_setup (void)
    {
    }

void snd_start (void)
    {
    pio_gpio_init (pio_snd, PICO_AUDIO_I2S_DATA_PIN);
    pio_gpio_init (pio_snd, PICO_AUDIO_I2S_CLOCK_PIN_BASE);
    pio_gpio_init (pio_snd, PICO_AUDIO_I2S_CLOCK_PIN_BASE + 1);
    
    sm_snd_out = pio_claim_unused_sm (pio_snd, true);
    pio_sm_set_pindirs_with_mask (pio_snd, sm_snd_out,
        ( 1u << PICO_AUDIO_I2S_DATA_PIN ) | ( 3u << PICO_AUDIO_I2S_CLOCK_PIN_BASE ),
        ( 1u << PICO_AUDIO_I2S_DATA_PIN ) | ( 3u << PICO_AUDIO_I2S_CLOCK_PIN_BASE ));
    offset_out = pio_add_program (pio_snd, &sound_out_program);
#if DEBUG & 0x04
    printf ("sm_snd_out = %d, offset_out = %d\n", sm_snd_out, offset_out);
#endif
    pio_sm_config c_out = sound_out_program_get_default_config (offset_out);
    sm_config_set_out_pins (&c_out, PICO_AUDIO_I2S_DATA_PIN, 1);
    sm_config_set_sideset_pins (&c_out, PICO_AUDIO_I2S_CLOCK_PIN_BASE);
    int fsys = clock_get_hz (clk_sys);
    int div = fsys / ( 64 * 44100 );
#if DEBUG & 0x04
    printf ("div = %f\n", div);
#endif
    double fchip = ((double) CLKSTP) * fsys / ( 64 * div );
    snd_freq (fchip);
    nFillMax = (int)(fchip / ( 100.0 * CLKSTP ) + 0.5);
    nFill = 0;
    sm_config_set_clkdiv (&c_out, div);
    sm_config_set_out_shift (&c_out, false, true, 32);
    pio_sm_init (pio_snd, sm_snd_out, offset_out, &c_out);
    snd_fill ();
    irq_set_exclusive_handler (PIO1_IRQ_0, snd_isr);
    irq_set_enabled (PIO1_IRQ_0, true);
    pio_snd->inte0 = ( PIO_INTR_SM0_TXNFULL_BITS << sm_snd_out );
    pio_sm_set_enabled (pio_snd, sm_snd_out, true);
    }

void snd_stop (void)
    {
#if DEBUG & 0x04
    printf ("snd_term\n");
#endif
    pio_sm_set_enabled (pio_snd, sm_snd_out, false);
    pio_snd->inte0 = 0;
    irq_remove_handler (PIO1_IRQ_0, snd_isr);
    pio_remove_program (pio_snd, &sound_out_program, offset_out);
    pio_sm_unclaim (pio_snd, sm_snd_out);
    }

#elif PICO_SOUND == 2

#include "hardware/pwm.h"
#include "pico/time.h"

#ifndef PICO_AUDIO_PWM_L_PIN
#define PICO_AUDIO_PWM_L_PIN    2
#endif
#define SND_PERIOD      23      // microseconds = 43.478 kHz
#define SND_BUFF_LEN    16      // Must be a power of 2

static uint snd_buff[SND_BUFF_LEN];
static uint snd_rd = 0;
static uint snd_wr = 0;
static bool bSndRun = false;
static uint iSliceL;
static uint iChanL;
#ifdef PICO_AUDIO_PWM_R_PIN
static uint iSliceR;
static uint iChanR;
#endif
static alarm_pool_t *apFast;    // An Alarm Pool with higher priority than default
static alarm_id_t idaTick;      // Alarm for updating PWM ratio (in fast pool)
static alarm_id_t idaFill;      // Alarm for filling snd_buff

#if DEBUG & 0x01
static int n1 = 0;
static int n2 = 0;
static int n3 = 0;
static int n4 = 0;
#endif

void snd_setup (void)
    {
    gpio_set_function (PICO_AUDIO_PWM_L_PIN, GPIO_FUNC_PWM);
    iSliceL = pwm_gpio_to_slice_num (PICO_AUDIO_PWM_L_PIN);
    iChanL = pwm_gpio_to_channel (PICO_AUDIO_PWM_L_PIN);
    pwm_config cfg = pwm_get_default_config ();
    pwm_config_set_clkdiv_mode (&cfg, PWM_DIV_FREE_RUNNING);
    pwm_config_set_clkdiv_int (&cfg, 1);
    pwm_config_set_wrap (&cfg, 0xFF);
#ifdef PICO_AUDIO_PWM_R_PIN
    gpio_set_function (PICO_AUDIO_PWM_R_PIN, GPIO_FUNC_PWM);
    iSliceR = pwm_gpio_to_slice_num (PICO_AUDIO_PWM_R_PIN);
    iChanR = pwm_gpio_to_channel (PICO_AUDIO_PWM_R_PIN);
    if ( iSliceR == iSliceL )
        {
        pwm_set_both_levels (iSliceL, 0x80, 0x80);
        pwm_init (iSliceL, &cfg, true);
        }
    else
        {
        pwm_set_chan_level (iSliceL, iChanL, 0x80);
        pwm_set_chan_level (iSliceR, iChanR, 0x80);
        pwm_init (iSliceL, &cfg, false);
        pwm_init (iSliceR, &cfg, false);
        pwm_set_mask_enabled (( 1 << iSliceL ) | ( 1 << iSliceR ));
        }
#else
    pwm_set_chan_level (iSliceL, iChanL, 0x80);
    pwm_init (iSliceL, &cfg, true);
#endif
    double fchip = ((double) CLKSTP) * 1E6 / SND_PERIOD;
    snd_freq (fchip);
    nFillMax = (int)(fchip / ( 100.0 * CLKSTP ) + 0.5);
    apFast = alarm_pool_create (2, 2);
#if DEBUG & 0x04
    printf ("snd_setup: fchip = %f, nFillMax = %d\n", fchip, nFillMax);
    printf ("apFast = %p\n", apFast);
#endif
    }

static void __time_critical_func(snd_out) (uint uLevel)
    {
#ifdef PICO_AUDIO_PWM_R_PIN
    if ( iSliceR == iSliceL )
        {
        pwm_set_both_levels (iSliceL, uLevel, uLevel);
        }
    else
        {
        pwm_set_chan_level (iSliceL, iChanL, uLevel);
        pwm_set_chan_level (iSliceR, iChanR, uLevel);
        }
#else
    pwm_set_chan_level (iSliceL, iChanL, uLevel);
#endif
    }

int64_t __time_critical_func(snd_tick) (alarm_id_t id, void *user_data)
    {
    uint uLevel = 0x80;
#if DEBUG & 0x01
    ++n1;
#endif
    if ( bSndRun )
        {
        uLevel = snd_buff[snd_rd];
        snd_rd = ( ++snd_rd ) & ( SND_BUFF_LEN - 1 );
        }

    snd_out (uLevel);
    return bSndRun ? - SND_PERIOD : 0;
    }

int64_t __time_critical_func(snd_fill) (alarm_id_t id, void *user_data)
    {
#if DEBUG & 0x01
    ++n2;
    n3 = 0;
#endif
    while ( snd_wr != snd_rd )
        {
        snd_buff[snd_wr] = ( ( snd_step () >> 8 ) + 0x80 ) & 0xFF;
        snd_wr = ( ++snd_wr ) & ( SND_BUFF_LEN - 1 );
        ++nFill;
#if DEBUG & 0x01
        ++n3;
#endif
        }
#if DEBUG & 0x01
    if ( n3 > n4 ) n4 = n3;
#endif
    if ( nFill >= nFillMax )
        {
        // The following may take more than one tick, hence the snd_buff fifo.
        snd_process ();
        nFill -= nFillMax;
        }
    return bSndRun ? SND_PERIOD : 0;
    }


void snd_start (void)
    {
#if DEBUG & 0x04
    printf ("snd_start: bSndRun = %d\n", bSndRun);
#endif
    if ( ! bSndRun )
        {
        nFill = 0;
        snd_rd = 0;
        snd_wr = 1;
        snd_buff[0] = 0x80;
        snd_fill (0, NULL);
#if DEBUG & 0x01
        n4 = 0;
#endif
        bSndRun = true;
        idaTick = alarm_pool_add_alarm_in_us (apFast, SND_PERIOD, snd_tick, NULL, true);
        idaFill = add_alarm_in_us (SND_PERIOD, snd_fill, NULL, true);
#if DEBUG & 0x04
        printf ("Sound started: idaTick = %d, idaFill = %d\n", idaTick, idaFill);
#endif
        }
    }

void snd_stop (void)
    {
#if DEBUG & 0x04
    printf ("snd_stop: nFill = %d, snd_rd = %d, snd_wr = %d\n", nFill, snd_rd, snd_wr);
#endif
#if DEBUG & 0x01
    printf ("n1 = %d, n2 = %d, n3 = %d, n4 = %d\n", n1, n2, n3, n4);
#endif
    if ( bSndRun )
        {
        bSndRun = false;
        alarm_pool_cancel_alarm (apFast, idaTick);
        cancel_alarm (idaFill);
        snd_out (0x80);
        }
    }
#endif
