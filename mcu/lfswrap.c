/*  fswrap.c -- Wrappers for LittleFS and other glue.
    Written 2021 by Eric Olson
    FatFS & Device support added by Memotech-Bill

    This is free software released under the exact same terms as
    stated in license.txt for the Basic interpreter.  */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lfswrap.h"

#ifdef HAVE_LFS
#include "lfsmcu.h"
lfs_t lfs_root;
lfs_bbc_t lfs_root_context;
#endif

#if defined(HAVE_FAT) && defined(HAVE_LFS)
#define	SDMOUNT	"sdcard"	// SD Card mount point
#define SDMLEN	( strlen (SDMOUNT) + 1 )
#endif
#if defined(HAVE_DEV)
#define DEVMOUNT "dev"      // Device mount point
#define DEVMLEN ( strlen (DEVMOUNT) + 1 )

typedef enum {
    didStdio,
    didCount
    } DEVID;

static const char *psDevName[] = {
    "serial",       // didStdio
    };
#endif

typedef enum {
#ifdef HAVE_LFS
    fstLFS,
#endif
#ifdef HAVE_FAT
    fstFAT,
#endif
#ifdef HAVE_DEV
    fstDEV,
#endif
    } FSTYPE;

#define flgRoot     0x01
#ifdef HAVE_DEV
#define flgDev      0x02
#endif
#if defined (HAVE_LFS) && defined (HAVE_FAT)
#define flgFat      0x04
#endif

typedef struct
    {
    FSTYPE fstype;
    union
        {
#ifdef HAVE_LFS
        lfs_file_t lfs_ft;
#endif
#ifdef HAVE_FAT
        FIL	   fat_ft;
#endif
#ifdef HAVE_DEV
        DEVID   did;
#endif
        };
    } multi_file;

static inline FSTYPE get_filetype (const FILE *fp)
    {
    return ((multi_file *)fp)->fstype;
    }

static inline void set_filetype (FILE *fp, FSTYPE fst)
    {
    ((multi_file *)fp)->fstype = fst;
    }

typedef struct
    {
    FSTYPE fstype;
    int flags;
    struct dirent   de;
    union
        {
#ifdef HAVE_LFS
        lfs_dir_t   lfs_dt;
#endif
#ifdef HAVE_FAT
        DIR	    fat_dt;
#endif
#ifdef HAVE_DEV
        DEVID   did;
#endif
        };
    } dir_info;

static inline FSTYPE get_dirtype (const DIR *dirp)
    {
    return ((dir_info *)dirp)->fstype;
    }

static inline void set_dirtype (DIR *dirp, FSTYPE fst)
    {
    ((dir_info *)dirp)->fstype = fst;
    }

static inline struct dirent * deptr (DIR *dirp)
    {
    return &((dir_info *)dirp)->de;
    }

#ifdef HAVE_LFS
static inline lfs_file_t *lfsptr (FILE *fp)
    {
    return &((multi_file *)fp)->lfs_ft;
    }

static inline lfs_dir_t * lfsdir (DIR *dirp)
    {
    return &((dir_info *)dirp)->lfs_dt;
    }
#endif

#ifdef HAVE_FAT
static inline char *fat_path (char *path)
    {
#ifdef HAVE_LFS
    return path + SDMLEN;
#else
    return path;
#endif
    }

static inline FIL *fatptr (FILE *fp)
    {
    return &((multi_file *)fp)->fat_ft;
    }

static inline DIR * fatdir (DIR *dirp)
    {
    return &((dir_info *)dirp)->fat_dt;
    }
#endif

#ifdef HAVE_DEV
static inline const char *dev_path (const char *path)
    {
    return path + DEVMLEN;
    }

static inline void set_devid (FILE *fp, DEVID did)
    {
    ((multi_file *)fp)->did = did;
    }

static inline DEVID get_devid (const FILE *fp)
    {
    return ((multi_file *)fp)->did;
    }
#endif

static FSTYPE pathtype (const char *path)
    {
#ifdef HAVE_DEV
    if (( ! strncmp (path, "/"DEVMOUNT, DEVMLEN) ) &&
        ( ( path[DEVMLEN] == '/' ) || ( path[DEVMLEN] == '\0' ) ) ) return fstDEV;
#endif
#ifdef HAVE_LFS
#ifdef HAVE_FAT
    if (( ! strncmp (path, "/"SDMOUNT, SDMLEN) ) &&
        ( ( path[SDMLEN] == '/' ) || ( path[SDMLEN] == '\0' ) ) ) return fstFAT;
#endif
    return fstLFS;
#else
    return fstFAT;
#endif
    }

static int fswslash (char c)
    {
    if (c == '/' || c == '\\') return 1;
    return 0;
    }

static int fswvert (char c)
    {
    if (fswslash (c) || c == 0) return 1;
    return 0;
    }

static const char *fswedge (const char *p)
    {
    const char *q = p;
    while (*q)
        {
        if (fswslash (*q)) return q+1;
        q++;
        }
    return q;
    }

static int fswsame (const char *q, const char *p)
    {
    for (;;)
        {
        if (fswvert (*p))
            {
            if (fswvert (*q))
                {
                return 1;
                }
            else
                {
                return 0;
                }
            }
        else if (fswvert (*q))
            {
            return 0;
            }
        else if (*q != *p)
            {
            return 0;
            }
        q++; p++;
        }
    }

static void fswrem (char *pcwd)
    {
    char *p = pcwd, *q = 0;
    while (*p)
        {
        if (fswslash (*p)) q = p;
        p++;
        }
    if (q) *q = 0;
    else *pcwd = 0;
    }

static void fswadd (char *pcwd, const char *p)
    {
    char *q = pcwd;
    while (*q) q++;
    *q++ = '/'; 
    while (! (fswvert (*p))) *q++ = *p++;
    *q = 0;
    }

static char fswcwd[260] = "";
char *myrealpath (const char *restrict p, char *restrict r)
    {
    if (r == 0) r = malloc (260);
    if (r == 0) return 0;
    strcpy (r, fswcwd);
    if (fswsame (p, "/"))
        {
        *r = 0;
        if (!*p) return r;
        p++;
        }
    const char *f = fswedge (p);
    while (f-p > 0)
        {
        if (fswsame (p, "/")||fswsame (p, "./"))
            {
            }
        else if (fswsame (p, "../"))
            {
            fswrem (r);
            }
        else
            {
            fswadd (r, p);
            }
        p = f; f = fswedge (p);
        }
    return r;
    }

static char fswpath[260];
int mychdir (const char *p)
    {
    int err = 0;
    myrealpath (p, fswpath);
    FSTYPE fst = pathtype (fswpath);
#ifdef HAVE_DEV
    if ( fst == fstDEV )
        {
        if ( strcmp (fswpath, "/"DEVMOUNT) ) err = -1;
        }
#endif
#ifdef HAVE_FAT
    if ( fst == fstFAT )
        {
        if ( f_chdir (fat_path (fswpath)) != FR_OK ) err = -1;
        }
#endif
#ifdef HAVE_LFS
    if ( fst == fstLFS )
        {
        lfs_dir_t d;
        if (lfs_dir_open (&lfs_root, &d, fswpath) < 0) err = -1;
        lfs_dir_close (&lfs_root, &d);
        }
#endif
    if ( ! err ) strcpy (fswcwd, fswpath);
    return err;
    }

int mymkdir (const char *p, mode_t m)
    {
    (void) m;
    myrealpath (p, fswpath);
    FSTYPE fst = pathtype (fswpath);
#ifdef HAVE_FAT
    if ( fst == fstFAT )
        {
        if ( f_mkdir (fat_path (fswpath)) != FR_OK ) return -1;
        return 0;
        }
#endif
#ifdef HAVE_LFS
    if ( fst == fstLFS )
        {
        int r = lfs_mkdir (&lfs_root, fswpath);
        if (r < 0) return -1;
        return 0;
        }
#endif
    return -1;
    }

int myrmdir (const char *p)
    {
    myrealpath (p, fswpath);
    FSTYPE fst = pathtype (fswpath);
#ifdef HAVE_FAT
    if ( fst == fstFAT )
        {
        if ( f_unlink (fat_path (fswpath)) != FR_OK ) return -1;
        return 0;
        }
#endif
#ifdef HAVE_LFS
    if ( fst == fstLFS )
        {
        int r = lfs_remove (&lfs_root, fswpath);
        if (r < 0) return -1;
        return 0;
        }
#endif
    return -1;
    }

int mychmod (const char *p, mode_t m)
    {
    return 0;
    }

char *mygetcwd (char *b, size_t s)
    {
    strncpy (b, fswcwd, s);
    return b;
    }

long myftell (FILE *fp)
    {
    FSTYPE fst = get_filetype (fp);
#ifdef HAVE_FAT
    if ( fst == fstFAT )
        {
        return f_tell (fatptr (fp));
        }
#endif
#ifdef HAVE_LFS
    if ( fst == fstLFS )
        {
        int r = lfs_file_tell (&lfs_root, lfsptr (fp));
        if (r < 0) return -1;
        return r;
        }
#endif
    return -1;
    }

int myfseek (FILE *fp, long offset, int whence)
    {
    FSTYPE fst = get_filetype (fp);
#ifdef HAVE_FAT
    if ( fst == fstFAT )
        {
        FIL *pf = fatptr (fp);
        if (whence == SEEK_END) offset += f_size (pf);
        else if (whence == SEEK_CUR) offset += f_tell (pf);
        if ( f_lseek (pf, offset) != FR_OK ) return -1;
        return 0;
        }
#endif
#ifdef HAVE_LFS
    if ( fst == fstLFS )
        {
        int lfs_whence = LFS_SEEK_SET;
        if (whence == SEEK_END) lfs_whence = LFS_SEEK_END;
        else if (whence == SEEK_CUR) lfs_whence = LFS_SEEK_CUR;
        int r = lfs_file_seek (&lfs_root, (void *)fp, offset, lfs_whence);
        if (r < 0) return -1;
        return 0;
        }
#endif
    return -1;
    }

FILE *myfopen (char *p, char *mode)
    {
    FILE *fp = (FILE *) malloc (sizeof (multi_file));
    if ( fp == NULL ) return NULL;
    myrealpath (p, fswpath);
    FSTYPE fst = pathtype (fswpath);
    set_filetype (fp, fst);
#ifdef HAVE_DEV
    if ( fst == fstDEV )
        {
        const char *psDev = dev_path (fswpath) + 1;
        for (int did = 0; did < didCount; ++did)
            {
            if ( ! strcmp (psDev, psDevName[did]) )
                {
                set_devid (fp, did);
                return fp;
                }
            }
        }
#endif
#ifdef HAVE_FAT
    if ( fst == fstFAT )
        {
        unsigned char om = 0;
        switch (mode[0])
            {
            case 'r':
                om = FA_READ;
                if ( mode[1] == '+' ) om |= FA_WRITE;
                break;
            case 'w':
                om = FA_CREATE_ALWAYS | FA_WRITE;
                if ( mode[1] == '+' ) om |= FA_READ;
                break;
            case 'a':
                om = FA_OPEN_APPEND;
                if ( mode[1] == '+' ) om |= FA_READ;
                break;
            }
        if ( f_open (fatptr (fp), fat_path (fswpath), om) == FR_OK ) return fp;
        
        }
#endif
#ifdef HAVE_LFS
    if ( fst == fstLFS )
        {
        enum lfs_open_flags of = 0;
        switch (mode[0])
            {
            case 'r':
                of = LFS_O_RDONLY;
                if ( mode[1] == '+' ) of = LFS_O_RDWR;
                break;
            case 'w':
                of = LFS_O_CREAT | LFS_O_TRUNC | LFS_O_WRONLY;
                if ( mode[1] == '+' ) of = LFS_O_CREAT | LFS_O_TRUNC | LFS_O_RDWR;
                break;
            case 'a':
                of = LFS_O_CREAT | LFS_O_TRUNC | LFS_O_RDONLY | LFS_O_APPEND;
                if ( mode[1] == '+' ) of = LFS_O_CREAT | LFS_O_TRUNC | LFS_O_RDWR | LFS_O_APPEND;
                break;
            }
        if ( lfs_file_open (&lfs_root, lfsptr (fp), fswpath, of) >= 0 ) return fp;
        }
#endif
    free (fp);
    return NULL;
    }

int myfclose (FILE *fp)
    {
    FSTYPE fst = get_filetype (fp);
#ifdef HAVE_FAT
    if ( fst == fstFAT )
        {
        FRESULT fr = f_close (fatptr (fp));
        free (fp);
        return ( fr != FR_OK ) ? -1 : 0;
        }
#endif
#ifdef HAVE_LFS
    if ( fst == fstLFS )
        {
        int r = lfs_file_close (&lfs_root, lfsptr (fp));
        free (fp);
        return ( r < 0 ) ? -1 : 0;
        }
#endif
    return 0;
    }

size_t myfread (void *ptr, size_t size, size_t nmemb, FILE *fp)
    {
    if (fp == stdin)
        {
#undef fread
        return fread (ptr, size, nmemb, stdin);
#define fread myfread
        }
    FSTYPE fst = get_filetype (fp);
#ifdef HAVE_DEV
    if ( fst == fstDEV )
        {
        switch (get_devid (fp))
            {
            case didStdio:
                return fread (ptr, size, nmemb, stdin);
            default:
                return 0;
            }
        }
#endif
#ifdef HAVE_FAT
    if ( fst == fstFAT )
        {
        unsigned int nbyte;
        if (size == 1)
            {
            f_read (fatptr (fp), ptr, nmemb, &nbyte);
            return nbyte;
            }
        else
            {
            for (int i = 0; i < nmemb; ++i)
                {
                FRESULT fr = f_read (fatptr (fp), ptr, size, &nbyte);
                if ( (fr != FR_OK) || (nbyte < size)) return i;
                ptr += size;
                }
            }
        return nmemb;
        }
#endif
#ifdef HAVE_LFS
    if ( fst == fstLFS )
        {
        if (size == 1)
            {
            int r = lfs_file_read (&lfs_root, lfsptr (fp), (char *)ptr, nmemb);
            if (r < 0) return 0;
            return r;
            } 
        for (int i = 0; i < nmemb; ++i)
            {
            if (lfs_file_read (&lfs_root, lfsptr (fp), (char *)ptr, size) < 0) return i;
            ptr += size;
            }
        return nmemb;
        }
#endif
    return 0;
    }

size_t myfwrite (void *ptr, size_t size, size_t nmemb, FILE *fp)
    {
    if (fp == stdout || fp == stderr)
        {
#undef fwrite
        return fwrite (ptr, size, nmemb, stdout);
#define fwrite myfwrite
        }
    FSTYPE fst = get_filetype (fp);
#ifdef HAVE_DEV
    if ( fst == fstDEV )
        {
        switch (get_devid (fp))
            {
            case didStdio:
                return fwrite (ptr, size, nmemb, stdout);
            default:
                return 0;
            }
        }
#endif
#ifdef HAVE_FAT
    if ( fst == fstFAT )
        {
        unsigned int nbyte;
        if (size == 1)
            {
            f_write (fatptr (fp), ptr, nmemb, &nbyte);
            return nbyte;
            }
        else
            {
            for (int i = 0; i < nmemb; ++i)
                {
                FRESULT fr = f_write (fatptr (fp), ptr, size, &nbyte);
                if ( (fr != FR_OK) || (nbyte < size)) return i;
                ptr += size;
                }
            }
        return nmemb;
        }
#endif
#ifdef HAVE_LFS
    if ( fst == fstLFS )
        {
        if (size == 1)
            {
            int r = lfs_file_write (&lfs_root, lfsptr (fp), (char *)ptr, nmemb);
            if (r < 0) return 0;
            return r;
            } 
        for (int i = 0; i < nmemb; ++i)
            {
            if (lfs_file_write (&lfs_root, lfsptr (fp), (char *)ptr, size) < 0) return i;
            ptr += size;
            }
        return nmemb;
        }
#endif
    return 0;
    }

int usleep (useconds_t usec)
    {
    sleep_us (usec);
    return 0;
    }

unsigned int sleep (unsigned int seconds)
    {
    sleep_ms (seconds*1000);
    return 0;
    }

DIR *myopendir (const char *name)
    {
    DIR *dirp = (DIR *) malloc (sizeof (dir_info));
    if ( dirp == NULL ) return NULL;
    myrealpath (name, fswpath);
    FSTYPE fst = pathtype (fswpath);
    set_dirtype (dirp, fst);
    if ( fswpath[0] == '\0' ) ((dir_info *)dirp)->flags = flgRoot;
    else ((dir_info *)dirp)->flags = 0;
#ifdef HAVE_DEV
    if ( fst == fstDEV )
        {
        const char *psDir = dev_path (fswpath);
        if (( *psDir == '\0' ) || (( *psDir == '/' ) && ( psDir[1] == '\0' )))
            {
            ((dir_info *)dirp)->did = 0;
            return dirp;
            }
        }
#endif
#ifdef HAVE_FAT
    if ( fst == fstFAT )
        {
        char *psDir = fat_path (fswpath);
        if ( *psDir == '\0' ) strcpy (psDir, "/");
        if ( f_opendir (fatdir (dirp), psDir) == FR_OK ) return dirp;
        }
#endif
#ifdef HAVE_LFS
    if ( fst == fstLFS )
        {
        if ( lfs_dir_open (&lfs_root, lfsdir (dirp), name) >= 0 ) return dirp;
        }
#endif
    free (dirp);
    return NULL;
    }

static bool fn_special (DIR *dirp, const char *psName)
    {
    if ( (((dir_info *)dirp)->flags & flgRoot ) == 0) return false;
    if (( ! strcmp (psName, ".") ) || ( ! strcmp (psName, "..") )) return true;
#ifdef HAVE_DEV
    if ( ! strcmp (psName, DEVMOUNT) ) return true;
#endif
#if defined(HAVE_LFS) && defined(HAVE_FAT)
    if ( ! strcmp (psName, SDMOUNT) ) return true;
#endif
    return false;
    }

struct dirent *myreaddir (DIR *dirp)
    {
    if ( dirp == NULL ) return NULL;
    struct dirent *pde = deptr (dirp);
    FSTYPE fst = get_dirtype (dirp);
    if ( ((dir_info *)dirp)->flags & flgRoot )
        {
#ifdef HAVE_DEV
        if ( (((dir_info *)dirp)->flags & flgDev) == 0 )
            {
            ((dir_info *)dirp)->flags |= flgDev;
            strcpy (pde->d_name, DEVMOUNT"/");
            return pde;
            }
#endif
#if defined(HAVE_LFS) && defined(HAVE_FAT)
        if ( (((dir_info *)dirp)->flags & flgFat) == 0 )
            {
            ((dir_info *)dirp)->flags |= flgFat;
            strcpy (pde->d_name, SDMOUNT"/");
            return pde;
            }
#endif
        }
#ifdef HAVE_DEV
    if ( fst == fstDEV )
        {
        DEVID did = ((dir_info *)dirp)->did;
        if ( did < didCount )
            {
            ++((dir_info *)dirp)->did;
            strcpy (pde->d_name, psDevName[did]);
            return pde;
            }
        }
#endif
#ifdef HAVE_FAT
    if ( fst == fstFAT )
        {
        FILINFO fi;
        while (true)
            {
            if (( f_readdir (fatdir (dirp), &fi) != FR_OK ) || ( fi.fname[0] == '\0' )) return NULL;
            if ( ! fn_special (dirp, fi.fname) )
                {
                strncpy (pde->d_name, fi.fname, NAME_MAX);
                if (( fi.fattrib & AM_DIR ) && ( strlen (fi.fname) < NAME_MAX )) strcat (pde->d_name, "/");
                return pde;
                }
            }
        }
#endif
#ifdef HAVE_LFS
    if ( fst == fstLFS )
        {
        struct lfs_info r;
        while (true)
            {
            if ( !lfs_dir_read (&lfs_root, lfsdir (dirp), &r) ) return NULL;
            if ( ! fn_special (dirp, r.name) )
                {
                strncpy (pde->d_name, r.name, NAME_MAX);
                if (( r.type == LFS_TYPE_DIR ) && ( strlen (r.name) < NAME_MAX))  strcat (pde->d_name, "/");
                return pde;
                }
            }
        }
#endif
    return NULL;
    }

int myclosedir (DIR *dirp)
    {
    if ( dirp == NULL ) return -1;
    FSTYPE fst = get_dirtype (dirp);
    int err = 0;
#ifdef HAVE_FAT
    if ( fst == fstFAT )
        {
        if ( f_closedir (fatdir (dirp)) != FR_OK ) err = -1;
        }
#endif
#ifdef HAVE_LFS
    if ( fst == fstLFS )
        {
        lfs_dir_close (&lfs_root, lfsdir (dirp));
        }
#endif
    free (dirp);
    }

#ifdef HAVE_LFS
static struct lfs_config lfs_root_cfg = {
    .context = &lfs_root_context,
    .read = lfs_bbc_read,
    .prog = lfs_bbc_prog,
    .erase = lfs_bbc_erase,
    .sync = lfs_bbc_sync,
    .read_size = 1,
    .prog_size = FLASH_PAGE_SIZE,
    .block_size = FLASH_SECTOR_SIZE,
    .block_count = ROOT_SIZE / FLASH_SECTOR_SIZE,
    .cache_size = FLASH_PAGE_SIZE,
    .cache_size = 256,
    .lookahead_size = 32,
    .block_cycles = 256
    };
#endif

extern void syserror (const char *psMsg);
int mount (void)
    {
    int istat = 0;
#ifdef HAVE_LFS
    struct lfs_bbc_config lfs_bbc_cfg =
#ifdef PICO
        {
        .buffer= (uint8_t *)XIP_BASE+ROOT_OFFSET
        };
#else
#error Define location of LFS storage
#endif
    lfs_bbc_createcfg (&lfs_root_cfg, &lfs_bbc_cfg);
    int lfs_err = lfs_mount (&lfs_root, &lfs_root_cfg);
    if (lfs_err)
        {
        lfs_format (&lfs_root, &lfs_root_cfg);
        lfs_err = lfs_mount (&lfs_root, &lfs_root_cfg);
        if (lfs_err)
            {
            syserror ("Unable to format littlefs.");
            istat |= 2;
            }
        }
#endif
#ifdef HAVE_FAT
    static FATFS   vol;
    FRESULT fr = f_mount (&vol, "0:", 1);
    if ( fr != FR_OK )
        {
        syserror ("Failed to mount SD card.");
        istat |= 1;
        }
#endif
    return istat;
    }
