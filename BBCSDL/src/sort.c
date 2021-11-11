// Shell sort
// iOS does not permit arbitrary code execution so the code which would normally
// go in 'sortlib.bbc' is put here to be compiled with the BBC Basic application

#include <unistd.h>

// Base address for 32-bit offsets into heap:
#if defined(__x86_64__) || defined(__aarch64__)
extern char *userRAM ;
#define zero userRAM
#else
#define zero (char*) 0
#endif

#if defined(__arm__) || defined(__aarch64__) || defined(__EMSCRIPTEN__)
typedef double variant ;
#else
typedef long double variant ;
#endif

typedef struct tagSTR
{
	unsigned int p ; // 32 bit pointer
	unsigned int l ;
} STR, *LPSTR ;

static int compare (void *src, void *dst, unsigned char type)
{
	switch (type)
	    {
		case 1:	return	(*(unsigned char*)dst > *(unsigned char*)src) -
				(*(unsigned char*)dst < *(unsigned char*)src) ; 

		case 4: return	(*(int*)dst > *(int*)src) - (*(int*)dst < *(int*)src) ; 

		case 10:
			if ((*(short*)(dst+8) == 0) && (*(short*)(src+8) == 0))
				goto case40 ;
		    {
			variant d, s ;
			if (*(short*)(dst+8) == 0)
				d = *(long long *)dst ;
			else
				d = *(variant *)dst ;
			if (*(short*)(src+8) == 0)
				s = *(long long *)src ;
			else
				s = *(variant *)src ;
			return (d > s) - (d < s) ;
		    }
		    
		case 8:
		    {
			double d = *(double *)dst ;
			double s = *(double *)src ;
			return (d > s) - (d < s) ;
		    }

		case 40:
		case40:
		    {
			long long d = *(long long*)dst ;
			long long s = *(long long*)src ;
			return (d > s) - (d < s) ;
		    }

		case 136:
		    {
			unsigned int len ;
			STR s = *(STR*)src ;
			STR d = *(STR*)dst ;
			len = s.l ;
			if (len > d.l)
				len = d.l ;
			while (len--)
			    {
				int result = ((*(d.p + zero) > *(s.p + zero)) -
					      (*(d.p + zero) < *(s.p + zero))) ;
				if (result)
					return result ;
				d.p++ ;
				s.p++ ;
			    }
			return (d.l > s.l) - (d.l < s.l) ;
		    }
	    }
	return 0 ;
}

void sortup (int eax, int ebx, int ecx, unsigned int edx, unsigned int esi, unsigned int edi, void *ebp)
{
	unsigned int gap = 0xFFFFFFFF ;
	void *savebp = ebp ;

	if (ecx <= 0)
		return ;

	do
		gap = gap >> 1 ; // n.b. unsigned
	while (gap >= ecx) ;

	do
	    {
		edx = 0 ;
		esi = 0 ;
		edi = esi + gap ;
		do
		    {
			int result = 0 ;
			unsigned int num ;

			ebp = savebp ;
			num = *(unsigned char*)ebp++ ; // number of arrays
			while (num--)
			    {
				unsigned char type = *(unsigned char*)ebp++ ;
				char *ebx = *(char **)ebp ;
				ebp += sizeof(void *) ;
				char *src = ebx + esi * (type & 15) ;
				char *dst = ebx + edi * (type & 15) ;
				result = compare (src, dst, type) ;
				if (result) 
					break ;
			    }
			if (result < 0)
			    {
				ebp = savebp ;
				num = *(unsigned char*)ebp++ ; // number of arrays
				while (num--)
				    {
					unsigned char size = *(unsigned char*)ebp++ & 15 ;
					char *ebx = *(char **)ebp ;
					ebp += sizeof(void *) ;
					char *src = ebx + esi * size ;
					char *dst = ebx + edi * size ;
					while (size--)
					    {
						char tmp = *src ;
						*src++ = *dst ;
						*dst++ = tmp ;
					    }
				    }

				if (esi >= gap)
				    {
					edi = esi ;
					esi -= gap ;
					continue ;
				    }
			    }
			edx += 1 ;
			esi = edx ;
			edi = esi + gap ;
		    }
		while (edi < ecx) ;
		gap = gap >> 1 ;
	    }
	while (gap) ;
	return ;
}

void sortdn (int eax, int ebx, int ecx, unsigned int edx, unsigned int esi, unsigned int edi, void *ebp)
{
	unsigned int gap = 0xFFFFFFFF ;
	void *savebp = ebp ;

	if (ecx <= 0)
		return ;

	do
		gap = gap >> 1 ; // n.b. unsigned
	while (gap >= ecx) ;

	do
	    {
		edx = 0 ;
		esi = 0 ;
		edi = esi + gap ;
		do
		    {
			int result = 0 ;
			unsigned int num ;

			ebp = savebp ;
			num = *(unsigned char*)ebp++ ; // number of arrays
			while (num--)
			    {
				unsigned char type = *(unsigned char*)ebp++ ;
				char *ebx = *(char **)ebp ;
				ebp += sizeof(void *) ;
				char *src = ebx + esi * (type & 15) ;
				char *dst = ebx + edi * (type & 15) ;
				result = compare (src, dst, type) ;
				if (result) 
					break ;
			    }
			if (result > 0)
			    {
				ebp = savebp ;
				num = *(unsigned char*)ebp++ ; // number of arrays
				while (num--)
				    {
					unsigned char size = *(unsigned char*)ebp++ & 15 ;
					char *ebx = *(char **)ebp ;
					ebp += sizeof(void *) ;
					char *src = ebx + esi * size ;
					char *dst = ebx + edi * size ;
					while (size--)
					    {
						char tmp = *src ;
						*src++ = *dst ;
						*dst++ = tmp ;
					    }
				    }

				if (esi >= gap)
				    {
					edi = esi ;
					esi -= gap ;
					continue ;
				    }
			    }
			edx += 1 ;
			esi = edx ;
			edi = esi + gap ;
		    }
		while (edi < ecx) ;
		gap = gap >> 1 ;
	    }
	while (gap) ;
	return ;
}

// Timer callback
// iOS does not permit arbitrary code execution so the code which would normally
// go in 'timerlib.bbc' is put here to be compiled with the BBC Basic application

int putevt (int, int, int, int) ;
typedef struct {char *handler; char *proc; unsigned char *flags; } timerparam ;

unsigned int hook(unsigned int interval, timerparam *param)
{
	while (putevt (param->handler - zero, param->proc - zero, interval, 0x113) == 0)
		usleep(1000) ;
	*(param->flags) |= 0x20 ;
	return interval ;
}