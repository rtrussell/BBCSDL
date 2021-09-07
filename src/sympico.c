/*  sympico.c -- Symbol table for SDK on the Pico.

	This is free software released under the exact same terms as
	stated in license.txt for the Basic interpreter.  */

#include <string.h>
#include "pico/stdlib.h"

typedef struct { char *s; void *p; } symbols;
#define SYMADD(X) { #X, &X }
const symbols sdkfuncs[]={
#include "sympico.i"
};

void* sympico(char* name) {
    int first = 0, last = sizeof(sdkfuncs) / sizeof(symbols) - 1;
    while (first <= last) {
        int middle = (first + last) / 2, r = strcmp(name, sdkfuncs[middle].s);
        if (r < 0) last = middle - 1;
        else if (r > 0) first = middle + 1;
        else return sdkfuncs[middle].p;
    }
    return NULL;
}
