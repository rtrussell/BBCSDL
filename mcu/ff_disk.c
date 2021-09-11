/* ff_disk.c - Media functions required by FatFS */

#include "pico.h"
#include "pico/stdlib.h"
#include "ff.h"
#include "diskio.h"

#define USE_SPI     // Never managed to make SD card mode from Pico SDK work
static uint32_t lba_base = 0;

// #define DEBUG
#ifdef DEBUG
#include <stdio.h>

void hexline (BYTE *ptr, int n)
    {
    for (int i = 0; i < n; ++i)
        {
        printf (" %02X", ptr[i]);
        }
    printf ("  ");
    for (int i = 0; i < n; ++i)
        {
        if ((ptr[i] >= 0x20) && (ptr[i] < 0x7F)) printf ("%c", ptr[i]);
        else printf (".");
        }
    printf ("\n");
    }

void hexdump (BYTE *ptr, int n)
    {
    int addr = 0;
    while ( n > 0 )
        {
        int nn = n;
        if ( nn > 16 ) nn = 16;
        printf ("%03X:", addr);
        hexline (ptr, ( n > 16 ) ? 16 : n);
        addr += 16;
        ptr += 16;
        n -= 16;
        }
    }
#endif

#ifdef USE_SPI
#include "sd_spi.h"

static int iStat = STA_NOINIT;

DSTATUS disk_status (BYTE pdrv)
    {
#ifdef DEBUG
    printf ("disk_status (%d) = 0x%02X\n", pdrv, iStat);
#endif
    return iStat;
    }

DRESULT disk_read (BYTE pdrv, BYTE* buff, LBA_t sector, UINT count)
    {
#ifdef DEBUG
    printf ("disk_read (%d, %p, 0x%04X, %d)\n", pdrv, buff, sector, count);
#endif
    if ( iStat & STA_NOINIT )
        {
#ifdef DEBUG
        printf ("Media not ready\n");
#endif
        return RES_NOTRDY;
        }
    if (( buff == NULL ) || ( count == 0 ))
        {
#ifdef DEBUG
        printf ("Parameter error\n");
#endif
        return RES_PARERR;
        }
    sector += lba_base;
    for (int i = 0; i < count; ++i)
        {
#ifdef DEBUG
        printf ("Read sector 0x%04X\n", sector);
#endif
        if ( ! sd_spi_read (sector, buff) )
            {
#ifdef DEBUG
            printf ("Read error\n");
#endif
            return RES_ERROR;
            }
#ifdef DEBUG
        printf ("Sector 0x%04X: ");
        hexline (buff, 16);
        // hexdump (buff, 512);
#endif
        ++sector;
        buff += 512;
        }
    return RES_OK;
    }

DRESULT disk_write (BYTE pdrv, const BYTE* buff, LBA_t sector, UINT count)
    {
#ifdef DEBUG
    printf ("disk_write (%d, %p, 0x%04X, %d)\n", pdrv, buff, sector, count);
#endif
    if ( iStat & STA_NOINIT )
        {
#ifdef DEBUG
        printf ("Media not ready\n");
#endif
        return RES_NOTRDY;
        }
    if (( buff == NULL ) || ( count == 0 ))
        {
#ifdef DEBUG
        printf ("Parameter error\n");
#endif
        return RES_PARERR;
        }
    sector += lba_base;
    for (int i = 0; i < count; ++i)
        {
#ifdef DEBUG
        printf ("Write sector 0x%04X\n", sector);
#endif
        if ( ! sd_spi_write (sector, buff) )
            {
#ifdef DEBUG
            printf ("Write error\n");
#endif
            return RES_ERROR;
            }
        ++sector;
        buff += 512;
        }
    return RES_OK;
    }

DSTATUS disk_initialize (BYTE pdrv)
    {
#ifdef DEBUG
    printf ("disk_initialize (%d)\n", pdrv);
#endif
    if ( sd_spi_init () )
        {
        iStat = 0;
        uint8_t mbr[512];
#ifdef DEBUG
        printf ("Reading first sector\n");
#endif
        lba_base = 0;
        if ( disk_read (0, mbr, 0u, 1) == RES_OK )
            {
            if (( mbr[510] == 0x55 ) && ( mbr[511] == 0xAA ))
                {
                lba_base = ( mbr[457] << 24 ) | ( mbr[456] << 16 ) | ( mbr[455] << 8 ) | mbr[454];
#ifdef DEBUG
                printf ("Found partition table: First partition at %d\n", lba_base);
                }
            else
                {
                printf ("No partition table - Assuming super-floppy\n");
#endif
                }
            }
        else
            {
#ifdef DEBUG
            printf ("Error reading first sector\n");
#endif
            iStat = STA_NOINIT;
            }
        }
    else iStat = STA_NOINIT;
#ifdef DEBUG
    printf ("disk_initialize: iStat = 0x%02X\n", iStat);
#endif
    return iStat;
    }

#else
#include "pico/sd_card.h"

// Work around stuck dma channel 11 rem 00000002 1 @ 12
#define MAX_BLOCKS  1

static int iStat = SD_ERR_STUCK;

DSTATUS disk_status (BYTE pdrv)
    {
    if ( iStat == SD_OK )
        {
#ifdef DEBUG
        printf ("disk_status: OK\n");
#endif
        return 0;
        }
#ifdef DEBUG
    printf ("disk_status: Not initialised\n");
#endif
    return STA_NOINIT;
    }

DSTATUS disk_initialize (BYTE pdrv)
    {
#ifdef SD_4PINS
#ifdef DEBUG
    printf ("disk_initialize: 4 pins\n");
    sleep_ms(500);
#endif
    iStat = sd_init_4pins();
#else
#ifdef DEBUG
    printf ("disk_initialize: 1 pin\n");
    sleep_ms(500);
#endif
    iStat = sd_init_1pin();
#endif
    setup_default_uart();
#ifdef DEBUG
    printf ("disk_initialise: iStat = %d\n", iStat);
#endif
    uint8_t mbr[512];
#ifdef DEBUG
    printf ("Reading first sector\n");
#endif
    int iRes = sd_readblocks_sync((uint32_t *)mbr, 0u, 1u);
    if (( mbr[510] == 0x55 ) && ( mbr[511] == 0xAA ))
        {
        lba_base = ( mbr[457] << 24 ) | ( mbr[456] << 16 ) | ( mbr[455] << 8 ) | mbr[454];
#ifdef DEBUG
        printf ("Found partition table: First partition at %d\n", lba_base);
#endif
        }
    return disk_status (pdrv);
    }

DRESULT disk_read (BYTE pdrv, BYTE* buff, LBA_t sector, UINT count)
    {
#ifdef DEBUG
    printf ("disk_read: sector = %d, count = %d\n", sector, count);
#endif
    int iRes = SD_OK;
    sector += lba_base;
    while (( count > 0 ) && ( iRes == SD_OK ))
        {
        sleep_ms(1);
        UINT nBlk = count;
        if ( nBlk > MAX_BLOCKS ) nBlk = MAX_BLOCKS;
        iRes = sd_readblocks_sync((uint32_t *)buff, (uint32_t) sector, nBlk);
#ifdef DEBUG
        printf ("Sector %d:\n", sector);
        hexdump (buff, 512);
        // hexline (buff, 16);
#endif
        buff += nBlk * SD_SECTOR_SIZE;
        sector += nBlk;
        count -= nBlk;
        }
    if ( iRes == SD_OK )
        {
#ifdef DEBUG
        printf ("Status OK\n");
#endif
        return RES_OK;
        }
#ifdef DEBUG
    printf ("Status = %d\n", iRes);
#endif
    return RES_ERROR;
    }

DRESULT disk_write (BYTE pdrv, const BYTE* buff, LBA_t sector, UINT count)
    {
#ifdef DEBUG
    printf ("disk_write: sector = %d, count = %d\n", sector, count);
#endif
    sector += lba_base;
    int iRes = sd_writeblocks_async((const uint32_t *)buff, (uint32_t) sector, count);
    if ( iRes == SD_OK )
        {
#ifdef DEBUG
        printf ("Waiting for completion\n");
#endif
        while ( ! sd_write_complete (&iRes) )
            {
            tight_loop_contents();
            }
#ifdef DEBUG
        printf ("Completed\n");
#endif
        return RES_OK;
        }
#ifdef DEBUG
    printf ("Status = %d\n", iRes);
#endif
    return RES_ERROR;
    }

#endif

DRESULT disk_ioctl (BYTE pdrv, BYTE cmd, void* buff)
    {
    if ( cmd == CTRL_SYNC ) return RES_OK;
    return RES_PARERR;
    }
