/*  sd_spi.c - Routines for accessing SD card using SPI routines */

#include "pico/stdlib.h"
#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "hardware/dma.h"
#include "sd_spi.pio.h"
#include "sd_spi.h"
#include "boards/vgaboard.h"

// #define DEBUG
#ifdef DEBUG
#include <stdio.h>
#endif

#define SD_CS_PIN       ( PICO_SD_DAT0_PIN + 3 )
#define SD_CLK_PIN      PICO_SD_CLK_PIN
#define SD_MOSI_PIN     PICO_SD_CMD_PIN
// #define SD_MOSI_PIN     PICO_SD_DAT0_PIN
#define SD_MISO_PIN     PICO_SD_DAT0_PIN

static PIO pio_sd = pio1;
static int sd_sm = -1;
static int dma_tx = -1;
static int dma_rx = -1;
SD_TYPE sd_type = sdtpUnk;

bool sd_spi_load (void)
    {
    dma_tx = dma_claim_unused_channel (true);
    dma_rx = dma_claim_unused_channel (true);
    if (( dma_tx < 0 ) || ( dma_rx < 0 )) return false;
    gpio_init (SD_CS_PIN);
    gpio_set_dir (SD_CS_PIN, GPIO_OUT);
    gpio_pull_up (SD_MISO_PIN);
    gpio_put (SD_CS_PIN, 1);
    uint offset = pio_add_program (pio_sd, &sd_spi_program);
    sd_sm = pio_claim_unused_sm (pio_sd, true);
    pio_sm_config c = sd_spi_program_get_default_config (offset);
    sm_config_set_out_pins (&c, SD_MOSI_PIN, 1);
    sm_config_set_in_pins (&c, SD_MISO_PIN);
    sm_config_set_sideset_pins (&c, SD_CLK_PIN);
    sm_config_set_out_shift (&c, false, true, 8);
    sm_config_set_in_shift (&c, false, true, 8);
    pio_sm_set_pins_with_mask(pio_sd, sd_sm, 0, (1 << SD_CLK_PIN) | (1 << SD_MOSI_PIN));
    pio_sm_set_pindirs_with_mask(pio_sd, sd_sm,  (1 << SD_CLK_PIN) | (1 << SD_MOSI_PIN),
        (1 << SD_CLK_PIN) | (1 << SD_MOSI_PIN) | (1 << SD_MISO_PIN));
    pio_gpio_init (pio_sd, SD_CLK_PIN);
    pio_gpio_init (pio_sd, SD_MOSI_PIN);
    pio_gpio_init (pio_sd, SD_MISO_PIN);
    pio_sm_init (pio_sd, sd_sm, offset, &c);
    pio_sm_set_enabled (pio_sd, sd_sm, true);
    return true;
    }

void sd_spi_unload (void)
    {
    pio_sm_set_enabled (pio_sd, sd_sm, false);
    pio_sm_unclaim (pio_sd, sd_sm);
    dma_channel_unclaim (dma_tx);
    dma_channel_unclaim (dma_rx);
    sd_sm = -1;
    dma_tx = -1;
    dma_rx = -1;
    }

void sd_spi_freq (float freq)
    {
    float div = clock_get_hz (clk_sys) / ( 4000.0 * freq );
    pio_sm_set_clkdiv (pio_sd, sd_sm, div);
    }

void sd_spi_chpsel (bool sel)
    {
    gpio_put (SD_CS_PIN, ! sel);
    }

// Do 8 bit accesses on FIFO, so that write data is byte-replicated. This
// gets us the left-justification for free (for MSB-first shift-out)
void sd_spi_xfer (bool bWrite, const uint8_t *src, uint8_t *dst, size_t len)
    {
    io_rw_8 *txfifo = (io_rw_8 *) &pio_sd->txf[sd_sm];
    io_rw_8 *rxfifo = (io_rw_8 *) &pio_sd->rxf[sd_sm];
    dma_hw->sniff_data = 0;
    dma_channel_config c = dma_channel_get_default_config (dma_rx);
    channel_config_set_transfer_data_size (&c, DMA_SIZE_8);
    channel_config_set_enable (&c, true);
    channel_config_set_read_increment (&c, false);
    channel_config_set_write_increment (&c, !bWrite);
    channel_config_set_dreq (&c, DREQ_PIO1_RX0 + sd_sm);
    if ( ! bWrite )
        {
        channel_config_set_sniff_enable (&c, true);
        dma_sniffer_enable (dma_rx, DMA_SNIFF_CTRL_CALC_VALUE_CRC16, true);
        }
    dma_channel_configure (dma_rx, &c, dst, rxfifo, len, true);
    c = dma_channel_get_default_config (dma_tx);
    channel_config_set_transfer_data_size (&c, DMA_SIZE_8);
    channel_config_set_enable (&c, true);
    channel_config_set_read_increment (&c, bWrite);
    channel_config_set_write_increment (&c, false);
    channel_config_set_dreq (&c, DREQ_PIO1_TX0 + sd_sm);
    if ( bWrite )
        {
        channel_config_set_sniff_enable (&c, true);
        dma_sniffer_enable (dma_tx, DMA_SNIFF_CTRL_CALC_VALUE_CRC16, true);
        }
    dma_channel_configure (dma_tx, &c, txfifo, src, len, true);
    dma_channel_wait_for_finish_blocking (dma_rx);
    }

uint8_t sd_spi_put (const uint8_t *src, size_t len)
    {
    uint8_t resp;
    sd_spi_xfer (true, src, &resp, len);
    return resp;
    }

void sd_spi_get (uint8_t *dst, size_t len)
    {
    uint8_t fill = 0xFF;
    sd_spi_xfer (false, &fill, dst, len);
    }

uint8_t sd_spi_clk (size_t len)
    {
    size_t tx_remain = len;
    size_t rx_remain = len;
    uint8_t resp;
    io_rw_8 *txfifo = (io_rw_8 *) &pio_sd->txf[sd_sm];
    io_rw_8 *rxfifo = (io_rw_8 *) &pio_sd->rxf[sd_sm];
    while (tx_remain || rx_remain)
        {
        if (tx_remain && !pio_sm_is_tx_fifo_full (pio_sd, sd_sm))
            {
            *txfifo = 0xFF;
            --tx_remain;
            }
        if (rx_remain && !pio_sm_is_rx_fifo_empty (pio_sd, sd_sm))
            {
            resp = *rxfifo;
            --rx_remain;
            }
        }
    return resp;
    }

#define SD_R1_OK        0x00
#define SD_R1_IDLE      0x01
#define SD_R1_ILLEGAL   0x04

#define SDBT_START	    0xFE	// Start of data token
#define SDBT_ERRMSK	    0xF0	// Mask to select zero bits in error token
#define SDBT_ERANGE	    0x08	// Out of range error flag
#define SDBT_EECC	    0x04	// Card ECC failed
#define SDBT_ECC	    0x02	// CC error
#define SDBT_ERROR	    0x01	// Error
#define SDBT_ECLIP	    0x10	// Value above all error bits

static uint8_t cmd0[]   = { 0xFF, 0x40 |  0, 0x00, 0x00, 0x00, 0x00, 0x95 }; // Go Idle
static uint8_t cmd8[]   = { 0xFF, 0x40 |  8, 0x00, 0x00, 0x01, 0xAA, 0x87 }; // Set interface condition
static uint8_t cmd17[]  = { 0xFF, 0x40 | 17, 0x00, 0x00, 0x00, 0x00, 0x00 }; // Read single block
static uint8_t cmd24[]  = { 0xFF, 0x40 | 24, 0x00, 0x00, 0x00, 0x00, 0x00 }; // Write single block
static uint8_t cmd55[]  = { 0xFF, 0x40 | 55, 0x00, 0x00, 0x01, 0xAA, 0x65 }; // Application command follows
static uint8_t cmd58[]  = { 0xFF, 0x40 | 58, 0x00, 0x00, 0x00, 0x00, 0xFD }; // Read Operating Condition Reg.
static uint8_t acmd41[] = { 0xFF, 0x40 | 41, 0x40, 0x00, 0x00, 0x00, 0x77 }; // Set operation condition

uint8_t sd_spi_cmd (uint8_t *src)
    {
    uint8_t resp = sd_spi_put (src, 7);
    for (int i = 0; i < 100; ++i)
	{
	if ( !( resp & 0x80 ) ) break;
        resp = sd_spi_clk (1);
	}
    return resp;
    }

bool sd_spi_init (void)
    {
    uint8_t chk[4];
    uint8_t resp;
#ifdef DEBUG
    printf ("sd_spi_init\n");
#endif
    if ( sd_sm < 0 ) sd_spi_load ();
    sd_type = sdtpUnk;
    sd_spi_freq (200);
    sd_spi_chpsel (false);
    sd_spi_clk (10);
    for (int i = 0; i < 256; ++i)
        {
        sd_spi_chpsel (true);
#ifdef DEBUG
        printf ("Go idle\n");
#endif
        resp = sd_spi_cmd (cmd0);
#ifdef DEBUG
        printf ("   Response 0x%02X\n", resp);
#endif
        if ( resp == SD_R1_IDLE ) break;
        sd_spi_chpsel (false);
        sleep_ms (1);
        }
    if ( resp != SD_R1_IDLE )
        {
#ifdef DEBUG
        printf ("Failed @1\n");
#endif
        return false;
        }
    for (int i = 0; i < 10; ++i)
        {
#ifdef DEBUG
        printf ("Set interface condition\n");
#endif
        resp = sd_spi_cmd (cmd8);
#ifdef DEBUG
        printf ("   Response 0x%02X\n", resp);
#endif
        if ( resp == SD_R1_IDLE )
            {
            sd_spi_get (chk, 4);
#ifdef DEBUG
            printf ("   Data 0x%02X 0x%02X 0x%02X 0x%02X\n", chk[0], chk[1], chk[2], chk[3]);
#endif
            if ( chk[3] == cmd8[4] ) break;
            }
        else if ( resp & SD_R1_ILLEGAL )
            {
            break;
            }
        }
    if ( resp & SD_R1_ILLEGAL )
        {
#ifdef DEBUG
        printf ("Version 1 SD Card\n");
#endif
        sd_type = sdtpVer1;
        }
    else if ( resp != SD_R1_IDLE )
        {
#ifdef DEBUG
        printf ("Failed @2\n");
#endif
        sd_spi_chpsel (false);
        return false;
        }
    for (int i = 0; i < 256; ++i)
        {
#ifdef DEBUG
        printf ("Set operation condition\n");
#endif
        resp = sd_spi_cmd (cmd55);
        resp = sd_spi_cmd (acmd41);
#ifdef DEBUG
        printf ("   Response 0x%02X\n", resp);
#endif
        if ( resp == SD_R1_OK ) break;
        }
    if ( resp != SD_R1_OK )
        {
#ifdef DEBUG
        printf ("Failed @3\n");
#endif
        sd_spi_chpsel (false);
        return false;
        }
    if ( sd_type == sdtpUnk )
        {
#ifdef DEBUG
        printf ("Read Operating Condition Register\n");
#endif
        resp = sd_spi_cmd (cmd58);
#ifdef DEBUG
        printf ("   Response 0x%02X\n", resp);
#endif
        if ( resp != SD_R1_OK )
            {
#ifdef DEBUG
            printf ("Failed @3\n");
#endif
            sd_spi_chpsel (false);
            return false;
            }
        sd_spi_get (chk, 4);
#ifdef DEBUG
        printf ("   Data 0x%02X 0x%02X 0x%02X 0x%02X\n", chk[0], chk[1], chk[2], chk[3]);
#endif
        if ( chk[0] & 0x40 )
            {
#ifdef DEBUG
            printf ("High capacity SD card\n");
#endif
            sd_type = sdtpHigh;
            }
        else
            {
#ifdef DEBUG
            printf ("Version 2 SD card\n");
#endif
            sd_type = sdtpVer2;
            }
        }
#ifdef DEBUG
    printf ("SD Card initialised\n");
#endif
    sd_spi_freq (25000);
    return true;
    }

void sd_spi_term (void)
    {
#ifdef DEBUG
    printf ("SD Card terminate\n");
#endif
    sd_type = sdtpUnk;
    sd_spi_chpsel (false);
    sd_spi_freq (200);
    }

void sd_spi_set_crc7 (uint8_t *pcmd)
    {
    uint8_t crc = 0;
    for (int i = 0; i < 5; ++i)
        {
        uint8_t v = *pcmd;
        for (int j = 0; j < 8; ++j)
            {
            if ( ( v ^ crc ) & 0x80 ) crc ^= 0x09;
            crc <<= 1;
            v <<= 1;
            }
        ++pcmd;
        }
    *pcmd = crc + 1;
    }

void sd_spi_set_lba (uint lba, uint8_t *pcmd)
    {
    if ( sd_type != sdtpHigh ) lba <<= 9;
    pcmd += 5;
    for (int i = 0; i < 4; ++i)
        {
        *pcmd = lba & 0xFF;
        lba >>= 8;
        --pcmd;
        }
    sd_spi_set_crc7 (pcmd);
    }

bool sd_spi_read (uint lba, uint8_t *buff)
    {
    uint8_t chk[2];
    sd_spi_set_lba (lba, cmd17);
#ifdef DEBUG
    printf ("Read command 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X\n",
        cmd17[1], cmd17[2], cmd17[3], cmd17[4], cmd17[5], cmd17[6]);
#endif
    uint8_t resp = sd_spi_cmd (cmd17);
#ifdef DEBUG
    printf ("   Resp 0x%02X", resp);
#endif
    if ( resp != SD_R1_OK )
        {
#ifdef DEBUG
        printf ("\nFailed\n");
#endif
        return false;
        }
    while (true)
        {
        resp = sd_spi_clk (1);
#ifdef DEBUG
        printf (" 0x%02X", resp);
#endif
        if ( resp == SDBT_START ) break;
        if ( resp < SDBT_ECLIP )
            {
#ifdef DEBUG
            printf ("\nError\n");
#endif
            return false;
            }
        }
#ifdef DEBUG
    printf ("\n");
#endif
    sd_spi_get (buff, 512);
    uint16_t crc = dma_hw->sniff_data;
    sd_spi_get (chk, 2);
#ifdef DEBUG
    printf ("Check bytes 0x%02X 0x%02X, checksum 0x%04X\n", chk[0], chk[1], crc);
#endif
    if (( chk[0] != ( crc >> 8 )) || (chk[1] != ( crc & 0xFF )))
        {
#ifdef DEBUG
        printf ("CRC mismatch\n");
#endif
        return false;
        }
    return true;
    }

bool sd_spi_write (uint lba, const uint8_t *buff)
    {
    uint8_t chk[2];
#ifdef DEBUG
    printf ("Write block\n");
#endif
    sd_spi_set_lba (lba, cmd24);
#ifdef DEBUG
    printf ("Write command 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X\n",
        cmd24[1], cmd24[2], cmd24[3], cmd24[4], cmd24[5], cmd24[6]);
#endif
    uint8_t resp = sd_spi_cmd (cmd24);
#ifdef DEBUG
    printf ("   Resp 0x%02X\n", resp);
#endif
    if ( resp != SD_R1_OK )
        {
#ifdef DEBUG
        printf ("\nFailed\n");
#endif
        return false;
        }
#ifdef DEBUG
    printf ("Write data\n");
#endif
    resp = SDBT_START;
    resp = sd_spi_put (&resp, 1);
#ifdef DEBUG
    printf ("   Resp 0x%02X\n", resp);
#endif
    resp = sd_spi_put (buff, 512);
    uint16_t crc = dma_hw->sniff_data;
#ifdef DEBUG
    printf ("   Resp 0x%02X, crc = 0x%04X\n", resp, crc);
#endif
    chk[0] = crc >> 8;
    chk[1] = crc & 0xFF;
    sd_spi_put (chk, 2);
#ifdef DEBUG
    printf ("   Resp");
#endif
    bool bResp = false;
    while (true)
        {
        resp = sd_spi_clk (1);
#ifdef DEBUG
        printf (" 0x%02X", resp);
#endif
        switch (resp & 0x0E)
            {
            case 0x04:
#ifdef DEBUG
                printf (" Data accepted\n");
#endif
                bResp = true;
                break;
            case 0xA0:
#ifdef DEBUG
                printf (" CRC error\n");
#endif
                bResp = false;
                break;
            case 0xC0:
#ifdef DEBUG
                printf (" Write error\n");
#endif
                bResp = false;
                break;
            default:
                break;
            }
        if ( resp == 0xFF ) break;
        }
#ifdef DEBUG
    printf ("\n");
#endif
    return bResp;
    }
