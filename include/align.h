#ifndef _ALIGN_H
#define _ALIGN_H

// A variant holds an 80-bit long double, a 64-bit long long or a string descriptor.
// n.b. GCC pads a long double to 16 bytes (128 bits) for alignment reasons but only
// the least-significant 80-bits need to be stored on the heap, in files etc.
// When a long double is 64-bits rather than 80-bits (e.g. ARM) it will be necessary
// to force the type word (.i.t or .s.t member) to a value other than 0 or -1.
typedef union __attribute__ ((packed)) __attribute__ ((aligned (4))) tagVAR
{
#if defined(__arm__) || defined(__aarch64__) || defined(__EMSCRIPTEN__)
    double f ;
#else
        long double f ;
#endif
        struct
        {
          long long n ;
          short t ; // = 0
        } i ;
        struct
        {
          heapptr p ; // Assumed to be 32 bits
          unsigned int l ; // Must be unsigned for overflow tests in 'math'
          short t ; // = -1
        } s ;
    struct
    {
      double d ;
      short t ; // unused (loadn/storen only)
    } d ;
} VAR, *LPVAR ;

// Helper macros to fix alignment problem:
#if defined(__i386__) || defined(__x86_64__) || defined(__EMSCRIPTEN__) || __ARM_FEATURE_UNALIGNED
#define ILOAD(p)    *(int*)(p)
#define ISTORE(p,i) *(int*)(p) = i 
#define TLOAD(p)    *(intptr_t*)(p)
#define TSTORE(p,i) *(intptr_t*)(p) = i 
#define ULOAD(p)    *(unsigned int*)(p)
#define USTORE(p,i) *(unsigned int*)(p) = i 
#define SLOAD(p)    *(unsigned short*)(p)
#define SSTORE(p,i) *(unsigned short*)(p) = i 
#define VLOAD(p)    *(void**)(p)
#define VSTORE(p,i) *(void**)(p) = i 
#define CLOAD(p)    *(char**)(p)
#define CSTORE(p,i) *(char**)(p) = i 
#define NLOAD(p)    *(VAR*)(p)
#define NSTORE(p,i) *(VAR*)(p) = i
#else
typedef __attribute__((aligned(1))) int unaligned_int;
typedef __attribute__((aligned(1))) intptr_t unaligned_intptr_t;
typedef __attribute__((aligned(1))) unsigned int unaligned_uint;
typedef __attribute__((aligned(1))) unsigned short unaligned_short;
typedef __attribute__((aligned(1))) void* unaligned_void_ptr;
typedef __attribute__((aligned(1))) char* unaligned_char_ptr;
typedef __attribute__((aligned(1))) VAR unaligned_VAR;

static inline int ILOAD(void* p){
	if((int)p&0x03) return *((unaligned_int*)p);
	else return *((int*)p);
}
static inline void ISTORE(void* p, int i){
	if((int)p&0x03) *((unaligned_int*)p) = i;
	else *((int *)p) = i;
}
static inline intptr_t TLOAD(void* p) { return *((unaligned_intptr_t*)p); }
static inline void TSTORE(void* p, intptr_t i) { *((unaligned_intptr_t*)p) = i; }
static inline unsigned int ULOAD(void* p) { return *((unaligned_uint*)p); }
static inline void USTORE(void* p, unsigned int i) { *((unaligned_uint*)p) = i; }
static inline unsigned short SLOAD(void* p) { return *((unaligned_short*)p); }
static inline void SSTORE(void* p, unsigned short i) { *((unaligned_short*)p) = i; }
static inline void* VLOAD(void* p) { return *((unaligned_void_ptr*)p); }
static inline void VSTORE(void* p, void* i) { *((unaligned_void_ptr*)p) = i; }
static inline char* CLOAD(void* p) { return *((unaligned_char_ptr*)p); }
static inline void CSTORE(void* p, char* i) { *((unaligned_char_ptr*)p) = i; }
static inline VAR NLOAD(void* p) { return *((unaligned_VAR*)p); }
static inline void NSTORE(void* p, VAR i) { *((unaligned_VAR*)p) = i; }
#endif

#endif //_ALIGN_H
