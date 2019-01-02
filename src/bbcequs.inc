;/*****************************************************************\
;*       BBC BASIC for SDL 2.0 (32-bit)                            *
;*       Copyright (c) R. T. Russell, 2000-2017                    *
;*                                                                 *
;*       BBCEQUS.INC Assembler equates include file                *
;*       Version 0.16b, 02-Feb-2017                                *
;\*****************************************************************/
;
MAX_PORTS       EQU     4       ; Maximum number of i/o ports
MAX_FILES       EQU     8       ; Maximum number of open files
PAGE_OFFSET     EQU     31C00H  ; Same in BBCSDL.H
WMU_NEWDIR      EQU     8001H   ; Must be the same in BBCSDL.H
WMU_BYE         EQU     8003H   ; Must be the same in BBCSDL.H
WMU_REALLOC     EQU     8004H   ; Must be the same in BBCSDL.H
WMU_WAVEOPEN    EQU     8005H   ; Must be the same in BBCSDL.H
WMU_WAVECLOSE   EQU     8006H   ; Must be the same in BBCSDL.H
WMU_PLAYMIDI    EQU     8007H   ; Must be the same in BBCSDL.H
PALETTE_SIZE    EQU     16      ; Must be the same in BBCSDL.H
XSCREEN         EQU     2048    ; Must be the same in BBCSDL.H
YSCREEN         EQU     2048    ; Must be the same in BBCSDL.H
STACK_NEEDED    EQU     512     ; Reduced from 1024
SCROLL_GAP      EQU     6       ; Number of lines 'overlap'
SOUNDQE         EQU     4       ; Number of bytes per sound entry
SOUNDQL         EQU     5*SOUNDQE ; Number of bytes per channel
;
bel:	equ	7
bs:	equ	8
lf:	equ	0ah
ff:	equ	0ch
cr:	equ	0dh
esc:	equ	1bh
del:    equ     7FH
;
bit0:	equ	1
bit1:	equ	2
bit2:	equ	4
bit3:	equ	8
bit4:	equ	10h
bit5:	equ	20h
bit6:	equ	40h
bit7:	equ	80h
;
;Bits in [_flags]:
;MUST BE THE SAME IN BBCSDL.H
;
kill    equ     bit0    ;Kill program (close)
sstep   equ     bit1    ;Single step
paus    equ     bit2    ;Pause execution
phase   equ     bit3    ;MODE 7 flash phase
flash   equ     bit4    ;Flash MODE 7
alert   equ     bit5    ;Event pending
escdis	equ	bit6	;ESCape disabled
escflg	equ	bit7	;ESCape pressed
;
;Bits in [vflags]:
;
ioflag  equ     bit0    ;Insert/overtype
egaflg  equ     bit1    ;EGA-compatible modes (*EGA [ON])
cgaflg  equ     bit2    ;CGA-compatible modes (*EGA OFF)
ptflag  equ     bit3    ;VDU 2 active
hrgflg  equ     bit4    ;VDU 5 active
vdudis  equ     bit5    ;VDU 21 active
ufont   equ     bit6    ;User font selected
utf8    equ     bit7    ;UTF-8 mode selected
;
;Tokens:
;
tand:   equ     80h
tdiv:   equ     81h
teor:   equ     82h
tmod:   equ     83h
tor:    equ     84h
terror:	equ	85h
tline:  equ     86h
toff:   equ     87h
tstep:  equ     88h
tspc:   equ     89h
ttab:   equ     8ah
telse:	equ	8bh
tthen:  equ     8ch
tlino:  equ     8dh
funtok: equ     8dh
ttimer: equ     91h
teval:  equ     0a0h    ;V3.0
tfn:	equ	0a4h
tlen:   equ     0a9h
tto:    equ     0b8h
tleft:  equ     0c0h
tmid:   equ     0c1h
tright: equ     0c2h
tcmd:   equ     0c7h    ;n.b. changed
twhile: equ     0c7h
tcase:  equ     0c8h
twhen:  equ     0c9h
tof:    equ     0cah
tendcase: equ   0cbh
totherwise: equ 0cch
tendif: equ     0cdh
tendwhile: equ  0ceh
tclose: equ     0d9h
tdata:	equ	0dch
tdef:   equ     0ddh
tdim:   equ     0deh
tend:   equ     0e0h
tfor:   equ     0e3h
tgosub: equ     0e4h
tgoto:  equ     0e5h
tif:	equ	0e7h
tinput: equ     0e8h
tlocal:	equ	0eah
tmode:	equ	0ebh
tmove:  equ     0ech
tnext:  equ     0edh
ton:    equ     0eeh
tproc:	equ	0f2h
trem:   equ     0f4h
trepeat:equ     0f5h
treport:equ     0f6h
trestor:equ     0f7h
treturn:equ     0f8h
tstop:  equ     0fah
ttrace: equ     0fch
tuntil: equ     0fdh
twidth: equ     0feh
tfill:  equ     3
tmouse: equ     4
trect:  equ     7
tswap:  equ     8
tsys:   equ     9
ttint:  equ     10
tby:    equ     15
texit:  equ     16
;
toklo:	equ	8fh
tokhi:	equ	93h
offsit:	equ	0cfh-toklo
;
