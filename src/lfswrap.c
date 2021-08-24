/*  lfswrap.c -- Wrappers for LittleFS and other glue for the Pico.
    Written 2021 by Eric Olson

    This is free software released under the exact same terms as
    stated in license.txt for the Basic interpreter.  */

#include <stdio.h>
#include <string.h>

#include "lfswrap.h"

lfs_t lfs_root;
lfs_bbc_t lfs_root_context;

static int picoslash(char c){
	if(c=='/'||c=='\\') return 1;
	return 0;
}

static int picovert(char c){
	if(picoslash(c)||c==0) return 1;
	return 0;
}

static const char *picoedge(const char *p){
	const char *q=p;
	while(*q){
		if(picoslash(*q)) return q+1;
		q++;
	}
	return q;
}

static int picosame(const char *q,const char *p){
	for(;;){
		if(picovert(*p)){
			if(picovert(*q)){
				return 1;
			} else {
				return 0;
			}
		} else if(picovert(*q)) {
			return 0;
		} else if(*q!=*p) {
			return 0;
		}
		q++; p++;
	}
}

static void picorem(char *pcwd){
	char *p=pcwd,*q=0;
	while(*p){
		if(picoslash(*p)) q=p;
		p++;
	}
	if(q) *q=0;
	else *pcwd=0;
}

static void picoadd(char *pcwd,const char *p){
	char *q=pcwd;
	while(*q) q++;
	*q++='/'; 
	while(!(picovert(*p))) *q++=*p++;
	*q=0;
}

static char picocwd[260]="";
char *myrealpath(const char *restrict p, char *restrict r){
	if(r==0) r=malloc(260);
	if(r==0) return 0;
	strcpy(r,picocwd);
	if(picosame(p,"/")){
		*r=0;
		if(!*p) return r;
		p++;
	}
	const char *f=picoedge(p);
	while(f-p>0){
		if(picosame(p,"/")||picosame(p,"./")){
		} else if(picosame(p,"../")){
			picorem(r);
		} else {
			picoadd(r,p);
		}
		p=f; f=picoedge(p);
	}
	return r;
}

static char picopath[260];
int mychdir(const char *p){
	myrealpath(p,picopath);
	lfs_dir_t d;
	if(lfs_dir_open(&lfs_root,&d,picopath)<0){
		return -1;
	}
	lfs_dir_close(&lfs_root,&d);
	strcpy(picocwd,picopath);
	return 0;
}

int mymkdir(const char *p, mode_t m){
	(void) m;
	myrealpath(p,picopath);
	int r=lfs_mkdir(&lfs_root,picopath);
	if(r<0) return -1;
	return 0;
}

int myrmdir(const char *p){
	myrealpath(p,picopath);
	int r=lfs_remove(&lfs_root,picopath);
	if(r<0) return -1;
	return 0;
}

int mychmod(const char *p, mode_t m){
	return 0;
}

char *mygetcwd(char *b, size_t s){
	strncpy(b,picocwd,s);
	return b;
}

long myftell(FILE *fp){
	int r=lfs_file_tell(&lfs_root,(void *)fp);
	if(r<0) return -1;
	return r;
}

int myfseek(FILE *fp, long offset, int whence){
	int lfs_whence=LFS_SEEK_SET;
	if(whence==SEEK_END) lfs_whence=LFS_SEEK_END;
	else if(whence==SEEK_CUR) lfs_whence=LFS_SEEK_CUR;
	int r=lfs_file_seek(&lfs_root,(void *)fp,offset,lfs_whence);
	if(r<0) return -1;
	return 0;
}

FILE *myfopen(char *p, char *mode){
	myrealpath(p,picopath);
	lfs_file_t *filep=malloc(sizeof(lfs_file_t));
	int r=lfs_file_open(&lfs_root,filep,picopath,
		mode[0]=='r'?LFS_O_RDONLY:LFS_O_RDWR|LFS_O_CREAT);
	if(r<0) {
		free(filep);
		return 0;
	}
	return (FILE *)filep;
}

int myfclose(FILE *stream){
	int r=lfs_file_close(&lfs_root,(lfs_file_t *)stream);
	free(stream);
	if(r<0) return -1;
	return 0;
}

size_t myfread(void *ptr, size_t size, size_t nmemb, FILE *stream){
	if(stream==stdin){
#undef fread
		return fread(ptr,size,nmemb,stdin);
#define fread myfread
	}
	if(size==1){
		int r=lfs_file_read(&lfs_root,(lfs_file_t *)stream,
			(char *)ptr,nmemb);
		if(r<0) return 0;
		return r;
	} 
	for(int i=0;i<nmemb;i++){
		if(lfs_file_read(&lfs_root,(lfs_file_t *)stream,
			(char *)ptr+i*size,size)<0) return i;
	}
	return nmemb;
}

size_t myfwrite(void *ptr, size_t size, size_t nmemb, FILE *stream){
	if(stream==stdout||stream==stderr){
#undef fwrite
		return fwrite(ptr,size,nmemb,stdout);
#define fwrite myfwrite
	}
	if(size==1){
		int r=lfs_file_write(&lfs_root,(lfs_file_t *)stream,
			(char *)ptr,nmemb);
		if(r<0) return 0;
		return r;
	} 
	for(int i=0;i<nmemb;i++){
		if(lfs_file_write(&lfs_root,(lfs_file_t *)stream,
			(char *)ptr+i*size,size)<0) return i;
	}
	return nmemb;
}

int usleep(useconds_t usec){
	sleep_us(usec);
	return 0;
}

unsigned int sleep(unsigned int seconds){
	sleep_ms(seconds*1000);
	return 0;
}

typedef struct
    {
    struct dirent   de;
    lfs_dir_t       ld;
    } dir_info;

extern DIR *myopendir(const char *name)
    {
    dir_info *pdi = (dir_info *) malloc (sizeof (dir_info));
    if ( pdi == NULL ) return NULL;
    if ( lfs_dir_open(&lfs_root, &(pdi->ld), name) < 0 )
        {
        free (pdi);
        return NULL;
        }
    return (DIR *) pdi;
    }

extern struct dirent *myreaddir(DIR *dirp)
    {
    if ( dirp == NULL ) return NULL;
    dir_info *pdi = (dir_info *) dirp;
    struct lfs_info r;
    if ( !lfs_dir_read (&lfs_root, &(pdi->ld), &r) ) return NULL;
    strncpy (pdi->de.d_name, r.name, NAME_MAX);
    return &(pdi->de);
    }

extern int myclosedir(DIR *dirp)
    {
    if ( dirp == NULL ) return -1;
    dir_info *pdi = (dir_info *) dirp;
    lfs_dir_close (&lfs_root, &(pdi->ld));
    free (pdi);
    return 0;
    }

