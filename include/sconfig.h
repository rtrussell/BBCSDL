//  sconfig.h - Specify details of serial port connection

#ifndef H_SCONFIG
#define H_SCONFIG

typedef struct
    {
    int     baud;
    int     parity;
    int     data;
    int     stop;
    int     tx;
    int     rx;
    int     cts;
    int     rts;
    } SERIAL_CONFIG;

#endif
