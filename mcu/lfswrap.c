/*  lfswrap.c -- Wrappers for LittleFS and other glue.
    Written 2021 by Eric Olson
    FatFS support added by Memotech-Bill

    This is free software released under the exact same terms as
    stated in license.txt for the Basic interpreter.  */

#define	SDMOUNT	"sdcard"	// SD Card mount point
#define SDMLEN	( strlen (SDMOUNT) + 1 )

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef PICO
// #include <pico/time.h>
#endif

#include "lfswrap.h"
#ifdef HAVE_LFS
#include "lfsmcu.h"

lfs_t lfs_root;
lfs_bbc_t lfs_root_context;
#endif

#ifdef HAVE_FAT
#ifdef HAVE_LFS		// Both FAT and LFS

static inline bool isfat (const char *path)
    {
    if (( ! strncmp (path, "/"SDMOUNT, SDMLEN) ) &&
	( ( path[SDMLEN] == '/' ) || ( path[SDMLEN] == '\0' ) ) ) return true;
    return false;
    }

static inline const char *fat_path (const char *path)
    {
    return path + SDMLEN;
    }

typedef struct
    {
    bool bFAT;
    union
	{
	FIL	   fat_ft;
	lfs_file_t lfs_ft;
	};
    } multi_file;

static inline bool is_fatptr (FILE *fp)
    {
    return ((multi_file *)fp)->bFAT;
    }

static inline void set_fatptr (FILE *fp, bool bFAT)
    {
    ((multi_file *)fp)->bFAT = bFAT;
    }

static inline FIL *fatptr (FILE *fp)
    {
    return &((multi_file *)fp)->fat_ft;
    }

static inline lfs_file_t *lfsptr (FILE *fp)
    {
    return &((multi_file *)fp)->lfs_ft;
    }

typedef struct
    {
    enum { dfLFS, dfRoot, dfFstRoot, dfFAT } df;
    struct dirent   de;
    union
	{
	DIR	    fat_dt;
	lfs_dir_t   lfs_dt;
	};
    } dir_info;

static inline bool is_fatdir (DIR *dirp)
    {
    return (((dir_info *)dirp)->df == dfFAT);
    }

static inline void set_fatdir (DIR *dirp, bool bFAT)
    {
    ((dir_info *)dirp)->df = bFAT ? dfFAT : dfLFS;
    }

static inline DIR * fatdir (DIR *dirp)
    {
    return &((dir_info *)dirp)->fat_dt;
    }

static inline lfs_dir_t * lfsdir (DIR *dirp)
    {
    return &((dir_info *)dirp)->lfs_dt;
    }

static inline struct dirent * deptr (DIR *dirp)
    {
    return &((dir_info *)dirp)->de;
    }

#else	// Only HAVE_FAT

static inline bool isfat (const char *path)
    {
    return true;
    }

static inline const char *fat_path (const char *path)
    {
    return path;
    }

typedef FIL multi_file;

static inline bool is_fatptr (FILE *fp)
    {
    return true;
    }

static inline void set_fatptr (FILE *fp, bool bFAT)
    {
    }

static inline FIL *fatptr (FILE *fp)
    {
    return (FIL *)fp;
    }

typedef struct
    {
    struct dirent   de;
    DIR	    	    fat_dt;
    } dir_info;

static inline bool is_fatdir (DIR *dirp)
    {
    return true;
    }

static inline void set_fatdir (DIR *dirp, bool bFAT)
    {
    }

static inline DIR * fatdir (DIR *dirp)
    {
    return &((dir_info *)dirp)->fat_dt;
    }

static inline struct dirent * deptr (DIR *dirp)
    {
    return &((dir_info *)dirp)->de;
    }

#endif
#else
#ifdef HAVE_LFS	// Only HAVE_LFS

typedef lfs_file_t multi_file;

static inline void set_fatptr (FILE *fp, bool bFAT)
    {
    }

static inline lfs_file_t *lfsptr (FILE *fp)
    {
    return (lfs_file_t *)fp;
    }

typedef struct
    {
    struct dirent   de;
    lfs_dir_t       lfs_dt;
    } dir_info;

static inline void set_fatdir (DIR *dirp, bool bFAT)
    {
    }

static inline lfs_dir_t * lfsdir (DIR *dirp)
    {
    return &((dir_info *)dirp)->lfs_dt;
    }

static inline struct dirent * deptr (DIR *dirp)
    {
    return &((dir_info *)dirp)->de;
    }

#endif
#endif

static int fswslash(char c)
    {
    if(c=='/'||c=='\\') return 1;
    return 0;
    }

static int fswvert(char c)
    {
    if(fswslash(c)||c==0) return 1;
    return 0;
    }

static const char *fswedge(const char *p)
    {
    const char *q=p;
    while(*q)
	{
	if(fswslash(*q)) return q+1;
	q++;
	}
    return q;
    }

static int fswsame(const char *q,const char *p)
    {
    for(;;)
	{
	if(fswvert(*p))
	    {
	    if(fswvert(*q))
		{
		return 1;
		} else {
		return 0;
		}
	    } else if(fswvert(*q)) {
	    return 0;
	    } else if(*q!=*p) {
	    return 0;
	    }
	q++; p++;
	}
    }

static void fswrem(char *pcwd)
    {
    char *p=pcwd,*q=0;
    while(*p)
	{
	if(fswslash(*p)) q=p;
	p++;
	}
    if(q) *q=0;
    else *pcwd=0;
    }

static void fswadd(char *pcwd,const char *p)
    {
    char *q=pcwd;
    while(*q) q++;
    *q++='/'; 
    while(!(fswvert(*p))) *q++=*p++;
    *q=0;
    }

static char fswcwd[260]="";
char *myrealpath(const char *restrict p, char *restrict r)
    {
    if(r==0) r=malloc(260);
    if(r==0) return 0;
    strcpy(r,fswcwd);
    if(fswsame(p,"/"))
	{
	*r=0;
	if(!*p) return r;
	p++;
	}
    const char *f=fswedge(p);
    while(f-p>0)
	{
	if(fswsame(p,"/")||fswsame(p,"./"))
	    {
	    } else if(fswsame(p,"../"))
	    {
	    fswrem(r);
	    } else {
	    fswadd(r,p);
	    }
	p=f; f=fswedge(p);
	}
    return r;
    }

static char fswpath[260];
int mychdir(const char *p)
    {
    myrealpath(p,fswpath);
#ifdef HAVE_FAT
    if ( isfat (fswpath) )
	{
	if ( f_chdir (fat_path (fswpath)) != FR_OK ) return -1;
	strcpy(fswcwd,fswpath);
	return 0;
	}
#endif
#ifdef HAVE_LFS
    lfs_dir_t d;
    if(lfs_dir_open(&lfs_root,&d,fswpath)<0)
	{
	return -1;
	}
    lfs_dir_close(&lfs_root,&d);
    strcpy(fswcwd,fswpath);
    return 0;
#endif
    }

int mymkdir(const char *p, mode_t m)
    {
    (void) m;
    myrealpath(p,fswpath);
#ifdef HAVE_FAT
    if ( isfat (fswpath) )
	{
	printf ("mkdir (%s)\n", fswpath);
	if ( f_mkdir (fat_path (fswpath)) != FR_OK ) return -1;
	return 0;
	}
#endif
#ifdef HAVE_LFS
    int r=lfs_mkdir(&lfs_root,fswpath);
    if(r<0) return -1;
    return 0;
#endif
    }

int myrmdir(const char *p)
    {
    myrealpath(p,fswpath);
#ifdef HAVE_FAT
    if ( isfat (fswpath) )
	{
	if ( f_unlink (fat_path (fswpath)) != FR_OK ) return -1;
	return 0;
	}
#endif
#ifdef HAVE_LFS
    int r=lfs_remove(&lfs_root,fswpath);
    if(r<0) return -1;
    return 0;
#endif
    }

int mychmod(const char *p, mode_t m)
    {
    return 0;
    }

char *mygetcwd(char *b, size_t s)
    {
    strncpy(b,fswcwd,s);
    return b;
    }

long myftell(FILE *fp)
    {
#ifdef HAVE_FAT
    if ( is_fatptr (fp) )
	{
	return f_tell (fatptr (fp));
	}
#endif
#ifdef HAVE_LFS
    int r = lfs_file_tell(&lfs_root, lfsptr (fp));
    if (r < 0) return -1;
    return r;
#endif
    }

int myfseek(FILE *fp, long offset, int whence)
    {
#ifdef HAVE_FAT
    if ( is_fatptr (fp) )
	{
	FIL *pf = fatptr (fp);
	if (whence == SEEK_END) offset += f_size (pf);
	else if (whence == SEEK_CUR) offset += f_tell (pf);
	if ( f_lseek (pf, offset) != FR_OK ) return -1;
	return 0;
	}
#endif
#ifdef HAVE_LFS
    int lfs_whence=LFS_SEEK_SET;
    if(whence==SEEK_END) lfs_whence=LFS_SEEK_END;
    else if(whence==SEEK_CUR) lfs_whence=LFS_SEEK_CUR;
    int r=lfs_file_seek(&lfs_root,(void *)fp,offset,lfs_whence);
    if(r<0) return -1;
    return 0;
#endif
    }

FILE *myfopen(char *p, char *mode)
    {
#if defined(HAVE_FAT) || defined(HAVE_LFS)
    FILE *fp = (FILE *) malloc (sizeof (multi_file));
    if ( fp == NULL ) return NULL;
    myrealpath(p,fswpath);
#else
    return NULL;
#endif
#ifdef HAVE_FAT
    if ( isfat (fswpath) )
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
	if ( f_open (fatptr (fp), fat_path (fswpath), om) != FR_OK )
	    {
	    free (fp);
	    return NULL;
	    }
	set_fatptr (fp, true);
	return fp;
	}
#endif
#ifdef HAVE_LFS
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
    if ( lfs_file_open (&lfs_root, lfsptr (fp), fswpath, of) < 0 )
	{
	free (fp);
	return NULL;
	}
    set_fatptr (fp, false);
    return fp;
#endif
    }

int myfclose(FILE *fp)
    {
#ifdef HAVE_FAT
    if ( is_fatptr (fp) )
	{
	FRESULT fr = f_close (fatptr (fp));
	free (fp);
	return ( fr != FR_OK ) ? -1 : 0;
	}
#endif
#ifdef HAVE_LFS
    int r = lfs_file_close (&lfs_root, lfsptr (fp));
    free (fp);
    return ( r < 0 ) ? -1 : 0;
#endif
    }

size_t myfread(void *ptr, size_t size, size_t nmemb, FILE *fp)
    {
    if (fp == stdin)
	{
#undef fread
	return fread(ptr, size, nmemb, stdin);
#define fread myfread
	}
#ifdef HAVE_FAT
    if ( is_fatptr (fp) )
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
		if ((fr != FR_OK) || (nbyte < size)) return i;
		ptr += size;
		}
	    }
	return nmemb;
	}
#endif
#ifdef HAVE_LFS
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
#endif
    }

size_t myfwrite(void *ptr, size_t size, size_t nmemb, FILE *fp)
    {
    if (fp == stdout || fp == stderr)
	{
#undef fwrite
	return fwrite (ptr, size, nmemb, stdout);
#define fwrite myfwrite
	}
#ifdef HAVE_FAT
    if ( is_fatptr (fp) )
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
		if ((fr != FR_OK) || (nbyte < size)) return i;
		ptr += size;
		}
	    }
	return nmemb;
	}
#endif
#ifdef HAVE_LFS
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
#endif
    }

int usleep(useconds_t usec)
    {
    sleep_us(usec);
    return 0;
    }

unsigned int sleep(unsigned int seconds)
    {
    sleep_ms(seconds*1000);
    return 0;
    }

DIR *myopendir(const char *name)
    {
#if defined(HAVE_FAT) || defined(HAVE_LFS)
    DIR *dirp = (DIR *) malloc (sizeof (dir_info));
    if ( dirp == NULL ) return NULL;
    myrealpath(name, fswpath);
#else
    return NULL;
#endif
#ifdef HAVE_FAT
    if ( isfat (fswpath) )
	{
	if ( fswpath[7] == '\0' ) strcpy (&fswpath[7], "/");
	if ( f_opendir (fatdir(dirp), fat_path(fswpath)) != FR_OK )
	    {
	    free (dirp);
	    return NULL;
	    }
	set_fatdir (dirp, true);
	return dirp;
	}
#endif
#ifdef HAVE_LFS
    if ( lfs_dir_open(&lfs_root, lfsdir(dirp), name) < 0 )
        {
        free (dirp);
        return NULL;
        }
    set_fatdir (dirp, false);
#ifdef HAVE_FAT
    if ( fswpath[0] == '\0' )
	{
	((dir_info *)dirp)->df = dfFstRoot;
	}
#endif
    return (DIR *) dirp;
#endif
    }

struct dirent *myreaddir(DIR *dirp)
    {
    if ( dirp == NULL ) return NULL;
#ifdef HAVE_FAT
    if ( is_fatdir(dirp) )
	{
	FILINFO fi;
	if ( f_readdir (fatdir(dirp), &fi) != FR_OK ) return NULL;
	if ( fi.fname[0] == '\0' ) return NULL;
	struct dirent *pde = deptr (dirp);
	strncpy (pde->d_name, fi.fname, NAME_MAX);
	if ( fi.fattrib & AM_DIR ) strcat (pde->d_name, "/");
	return pde;
	}
#endif
#ifdef HAVE_LFS
    struct dirent *pde = deptr (dirp);
#ifdef HAVE_FAT
    // Insert mount point as first root directory entry
    if (((dir_info *)dirp)->df == dfFstRoot )
	{
	((dir_info *)dirp)->df = dfRoot;
	strcpy (pde->d_name, SDMOUNT"/");
	return pde;
	}
#endif
    struct lfs_info r;
    if ( !lfs_dir_read (&lfs_root, lfsdir(dirp), &r) ) return NULL;
#ifdef HAVE_FAT
    // Hide any root entry with the same name as the mount point
    if ((((dir_info *)dirp)->df == dfRoot) && (!strcmp (r.name, SDMOUNT)))
	{
	if ( !lfs_dir_read (&lfs_root, lfsdir(dirp), &r) ) return NULL;
	}
#endif
    strncpy (pde->d_name, r.name, NAME_MAX);
    if ( r.type == LFS_TYPE_DIR )  strcat (pde->d_name, "/");
    return pde;
#endif
    }

int myclosedir(DIR *dirp)
    {
    if ( dirp == NULL ) return -1;
#ifdef HAVE_FAT
    if ( is_fatdir(dirp) )
	{
	FRESULT fr = f_closedir (fatdir(dirp));
	free (dirp);
	return ( fr != FR_OK ) ? -1 : 0;
	}
#endif
#ifdef HAVE_LFS
    lfs_dir_close (&lfs_root, lfsdir(dirp));
    free (dirp);
    return 0;
#endif
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

extern void waitconsole();
int mount (void)
    {
    int istat = 0;
#ifdef HAVE_LFS
    struct lfs_bbc_config lfs_bbc_cfg =
#ifdef PICO
	{
	.buffer=(uint8_t *)XIP_BASE+ROOT_OFFSET
	};
#else
#error Define location of LFS storage
#endif
    lfs_bbc_createcfg(&lfs_root_cfg, &lfs_bbc_cfg);
    int lfs_err = lfs_mount(&lfs_root, &lfs_root_cfg);
    if (lfs_err)
	{
	lfs_format(&lfs_root, &lfs_root_cfg);
	lfs_err = lfs_mount(&lfs_root, &lfs_root_cfg);
	if (lfs_err)
	    {
	    waitconsole();
	    printf("unable to format littlefs\n");
	    istat |= 2;
	    }
	}
#endif
#ifdef HAVE_FAT
    static FATFS   vol;
    FRESULT fr = f_mount (&vol, "0:", 1);
    if ( fr != FR_OK )
	{
	waitconsole();
	printf ("Failed to mount SD card.\n");
	istat |= 1;
	}
#endif
    return istat;
    }
