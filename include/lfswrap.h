#ifndef LFSWRAP_H
#define LFSWRAP_H

#include "lfspico.h"

extern lfs_t lfs_root;
extern lfs_bbc_t lfs_root_context;

extern char *myrealpath(const char *restrict p, char *restrict r);
extern int mychdir(const char *p);
extern int mymkdir(const char *p, mode_t m);
extern int myrmdir(const char *p);
extern int mychmod(const char *p, mode_t m);
extern char *mygetcwd(char *b, size_t s);
extern int myusleep(useconds_t usec);
extern unsigned int mysleep(unsigned int seconds);
extern long myftell(FILE *fp);
extern int myfseek(FILE *fp, long offset, int whence);
extern FILE *myfopen(char *pathname, char *mode);
extern int myfclose(FILE *stream);
extern size_t myfread(void *ptr, size_t size, size_t nmemb, FILE *stream);
extern size_t myfwrite(void *ptr, size_t size, size_t nmemb, FILE *stream);

#define realpath myrealpath
#define chdir mychdir
#define mkdir mymkdir
#define rmdir myrmdir
#define remove myrmdir
#define chmod mychmod
#define getcwd mygetcwd
#define usleep myusleep
#define sleep mysleep
#define fclose myfclose
#define fopen myfopen
#define fread myfread
#define fwrite myfwrite

#endif
