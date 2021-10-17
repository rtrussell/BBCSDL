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
#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "hardware/irq.h"
#include "sound.pio.h"
#include <string.h>
#include <stdio.h>
#include <stdint.h>

#define CLKSTP  6       // Must match the value in sn76489.c

uint16_t snd_step (void);
void snd_freq (double fchip);
void snd_process (void);

static PIO  pio_snd = pio1;
static int  sm_snd_out = -1;
static uint offset_out = 0;

static int  nFill = 0;
static int  nFillMax;

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
