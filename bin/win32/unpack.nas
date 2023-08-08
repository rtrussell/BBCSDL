;/*****************************************************************\
;*       BBC BASIC for SDL 2.0 (Windows edition)                   *
;*       Copyright (c) R. T. Russell, 2023                         *
;*       aPLib courtesy of Joergen Ibsen, www.ibsensoftware.com    *
;*                                                                 *
;*       UNPACK.NAS Compressed file unpacker                       *
;\*****************************************************************/
;
        GLOBAL  unpack@20,verify@12
        EXTERN  ReadFile@20,SetFilePointer@16
;
        SEGMENT .text
;
BUFSIZ  EQU     512
;
verify@12:
        pushad
        mov     ebp,esp         ; Save stack pointer
        mov     edi,[ebp+40]    ; Data pointer
        mov     ecx,[ebp+44]    ; Data size (bytes)
        mov     ebx,-BUFSIZ     ; V1.1
        and     esp,ebx         ; Align
        neg     ebx             ; mask -> BUFSIZ
        sub     esp,ebx         ; Make space on stack
        mov     esi,esp         ; Buffer pointer
vloop:  mov     eax,ecx         ; Get remaining size
        cmp     eax,ebx         ; Remaining > BUFSIZ ?
        jbe     vloop1
        mov     eax,ebx         ; If so, use BUFSIZ
vloop1: push    eax             ; Save bytes to load/compare
        push    ecx             ; Save bytes remaining
        push    eax             ; Make space for NumberOfBytesRead
        mov     ecx,esp
        mov     edx,[ebp+36]    ; File handle
        push    byte 0          ; lpOverlapped
        push    ecx             ; lpNumberOfBytesRead
        push    eax             ; nNumberOfBytesToRead
        push    esi             ; lpBuffer
        push    edx             ; hFile
        call    ReadFile@20
        pop     eax             ; Discard NumberOfBytesRead
        pop     eax             ; eax = bytes remaining
        pop     ecx             ; ecx = bytes to compare
        sub     eax,ecx         ; Adjust bytes remaining
        jecxz   vloop2
        push    esi
        repz    cmpsb           ; Compare file with memory
        pop     esi
vloop2: xchg    eax,ecx         ; ecx <- remaining, eax <- result
        jnz     vloopx          ; Different, so bailout
        or      ecx,ecx         ; Anything remaining?
        jnz     vloop           ; Continue comparing
vloopx: mov     esp,ebp         ; Restore stack pointer
        mov     [ebp+28],eax    ; Result
        popad
        ret     12              ; WINAPI calling convention
;
unpack@20:
        and     dword [esp+20],byte 1 ; Test bit 0, zero remainder
        jz      near ReadFile@20     ; n.b. lpOverlapped = NULL
        pushad
        mov     ebp,esp
        mov     edx,[ebp+36]    ; hFile
        mov     edi,[ebp+40]    ; lpBuffer
        mov     eax,[ebp+48]    ; &nRead
        mov     eax,[eax]       ; nRead
        mov     ebx,-BUFSIZ     ; Mask
        and     esp,ebx         ; Align stack pointer
        mov     esi,esp         ; Initial buffer pointer
        neg     ebx             ; Mask -> BUFSIZ
        sub     esp,ebx         ; Make space on stack for buffer
        push    edx             ; hFile [esi-4]
        push    eax             ; Space for last block size [esi-8]
        push    eax             ; nRead [esi-12]
        push    ebp

        mov     dl,80H
        xor     ebx,ebx
literal:
        call    getbyte
        stosb
        mov     bl,2
nexttag:
        call    getbit
        jnc     literal
        xor     ecx,ecx
        xor     eax,eax 
        call    getbit
        jnc     codepair
        call    getbit
        jnc     shortmatch
        mov     bl,2
        inc     ecx
        mov     al,10H
getmorebits:
        call    getbit
        adc     al,al
        jnc     getmorebits
        jnz     domatch
        stosb
nextt1: jmp     short nexttag
;        
codepair:
        call    getgamma
        sub     ecx,ebx
        jnz     normalcodepair
        call    getgamma
        jmp     short domatch_lastpos
;        
shortmatch:
        call    getbyte
        shr     eax,1
        rcl     ecx,1            
        jnz     domatch_with_2inc

        pop     ebp             ; saved stack pointer
        pop     eax             ; discard SerialNumber
        pop     eax             ; last block size
        pop     edx             ; hFile
        mov     esp,ebp         ; Restore stack pointer
        and     esi,BUFSIZ-1    ; Get offset into buffer
        jz      noptradj
        sub     esi,eax         ; How much to adjust pointer
        push    byte 1          ; dwMoveMethod = FILE_CURRENT
        push    byte 0          ; lpDistanceToMoveHigh
        push    esi             ; DistanceToMove (negative)
        push    edx             ; hFile
        call    SetFilePointer@16
noptradj:
        sub     edi,[ebp+40]
        mov     esi,[ebp+48]    ; &nRead
        mov     [esi],edi       ; return unpacked length
        popad
        ret     20              ; WINAPI calling convention
;        
normalcodepair:
        xchg    eax,ecx
        dec     eax
        shl     eax,8
        call    getbyte
        call    getgamma
        cmp     eax,32000
        jae     domatch_with_2inc
        cmp     ah,5
        jae     domatch_with_inc
        cmp     eax,byte 7FH
        ja      domatch_new_lastpos
domatch_with_2inc:
        inc     ecx
domatch_with_inc:
        inc     ecx
domatch_new_lastpos:
        xchg    eax,ebp
domatch_lastpos:
        mov     eax,ebp
        mov     bl,1
domatch:
        push    esi
        mov     esi,edi
        sub     esi,eax
        rep     movsb
        pop     esi
        jmp     short nextt1
;        
getbit:
        add     dl,dl
        jnz     stillbitsleft
        xchg    al,dl
        pushf
        call    getbyte
        popf
        xchg    al,dl
        adc     dl,dl
stillbitsleft:
        ret
;        
getgamma:
        inc     ecx
getgammaloop:
        call    getbit
        adc     ecx,ecx
        call    getbit
        jc      getgammaloop
        ret
;
getbyte:
        test    esi,BUFSIZ-1
        jnz     getbyt1
        sub     esi,BUFSIZ      ; Address start of buffer
        pushad
        push    eax             ; Make space for NumberOfBytesRead
        mov     ecx,esp
        push    byte 0          ; lpOverlapped
        push    ecx             ; lpNumberOfBytesRead
        push    dword BUFSIZ    ; nNumberOfBytesToRead
        push    esi             ; lpBuffer
        push    dword [esi-4]   ; hFile
        call    ReadFile@20
        pop     dword [esi-8]   ; NumberOfBytesRead (may not be BUFSIZ)
        popad
getbyt1:
        lodsb
        ret
