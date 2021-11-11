//  bbuart.h - Driver routines for serial ports

#ifndef H_BBUART
#define H_BBUART

#include "sconfig.h"

bool uopen (int uid, SERIAL_CONFIG *sc);
void uclose (int uid);
int uread (char *ptr, int size, int nmemb, int uid);
int uwrite (char *ptr, int size, int nmemb, int uid);

#endif
