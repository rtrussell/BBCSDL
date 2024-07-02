;/*****************************************************************\
;*	BBC BASIC for SDL 2.0 (Windows edition)			   *
;*	Copyright (c) R. T. Russell, 2024			   *
;*	aPLib courtesy of Joergen Ibsen, www.ibsensoftware.com	   *
;*								   *
;*	UNPACK.NAS Compressed file unpacker (64-bit version)	   *
;\*****************************************************************/
;
	GLOBAL  unpack,verify
	EXTERN  ReadFile,SetFilePointer
;
	SEGMENT .text
;
BUFSIZ  EQU	512
;
; Parameters: rcx = hFile
;	      rdx = lpBuffer
;	      r8  = nNumberOfBytesToVerify
;     Result: eax = non-zero if different
;  Preserved: rbx, rbp, rdi, rsi, r12, r13, r14, r15
;
verify:
	push	rbp
	push	rsi
	push	rdi
	mov	rbp,rsp		; copy stack pointer
	xchg	rcx,r8		; rcx = nBytes, r8 = hFile
	mov	rdi,rdx		; lpBuffer

	mov	edx,BUFSIZ
	neg	rdx
	and	rsp,rdx		; align stack pointer
	neg	edx
	sub	rsp,rdx		; make space on stack
	mov	rsi,rsp

vloop:	mov	eax,ecx
	cmp	eax,edx		; remaining >- BUFSIZ ?
	jbe	vloop1
	mov	eax,edx		; If so, use BUFSIZ
	
vloop1:	push	r8
	push	rax		; Bytes to read
	push	rcx		; Bytes remaining
	push	rdx		; BUFSIZ

	mov	rcx,r8		; hFile
	mov	rdx,rsi		; lpBuffer
	mov	r8,rax		; nNumberOfBytesToRead
	xor	r9d,r9d		; lpNumberOfBytesRead
	push	byte 0		; lpOverlapped

	sub	rsp,32		; Shadow space
	call	ReadFile
	add	rsp,40

	pop	rdx
	pop	rax		; rax = bytes remaining
	pop	rcx		; rcx = bytes just read
	pop	r8

	sub	eax,ecx		; Adjust bytes remaining
	jrcxz	vloop2
	push	rsi
	repz	cmpsb		; Compare file with memory
	pop	rsi

vloop2:	xchg	eax,ecx		; ecx <- remaining, eax <- result
	jnz	vloopx		; Different, so bailout
	or	ecx,ecx		; Anything remaining?
	jnz	vloop		; Continue comparing

vloopx:	mov	rsp,rbp		; Restore stack pointer
	pop	rdi
	pop	rsi
	pop	rbp
	ret
;
; Parameters: rcx = hFile
;	      rdx = lpBuffer
;	      r8 = nNumberOfBytesToRead
;	      r9 = lpNumberOfBytesRead
;	[rsp+40] = lpOverlapped
;  Preserved: rbx, rbp, rdi, rsi, r12, r13, r14, r15
;
unpack:
	and	qword [rsp+40],byte 1	; Test bit 0, zero remainder
	jz	near ReadFile	; n.b. lpOverlapped = NULL

	push	rbp
	mov	rbp,rsp		; copy stack pointer
	mov	r10,rcx		; hFile

	mov	eax,BUFSIZ
	neg	rax
	and	rsp,rax		; align stack pointer
	mov	rcx,rsp
	neg	rax
	sub	rsp,rax		; make space on stack

	push	rbx
	push	rsi
	push	rdi

	push	rdx

	mov	rsi,rcx
	mov	rdi,rdx

	cld
	mov	dl,80h
	xor	ebx,ebx

literal:
	call	getbyte
	stosb
	mov	bl,2
nexttag:
	call	getbit
	jnc	literal

	xor	ecx,ecx
	call	getbit
	jnc	codepair
	xor	eax,eax
	call	getbit
	jnc	shortmatch
	mov	bl,2
	inc	ecx
	mov	al,10h
getmorebits:
	call	getbit
	adc	al,al
	jnc	getmorebits
	jnz	domatch
	stosb
	jmp	short nexttag
;
codepair:
	call	getgamma_no_ecx
	sub	ecx,ebx
	jnz	normalcodepair
	call	getgamma
	jmp	short domatch_lastpos
;
shortmatch:
	call	getbyte
	shr	eax,1
	jz	donedepacking
	adc	ecx,ecx
	jmp	short domatch_with_2inc
;
normalcodepair:
	xchg	eax,ecx
	dec	eax
	shl	eax,8
	call	getbyte
	call	getgamma

	cmp	eax,32000
	jae	short domatch_with_2inc
	cmp	ah,5
	jae	short domatch_with_inc
	cmp	eax,7fh
	ja	short domatch_new_lastpos

domatch_with_2inc:
	inc	ecx

domatch_with_inc:
	inc	ecx

domatch_new_lastpos:
	xchg	eax,r8d
domatch_lastpos:
	mov	eax,r8d

	mov	bl,1

domatch:
	push	rsi
	mov	rsi,rdi
	sub	rsi,rax
	rep	movsb
	pop	rsi
	jmp	nexttag
;
getbit:
	add	dl,dl
	jnz	stillbitsleft
        xchg    al,dl
        pushf
        call    getbyte
        popf
        xchg    al,dl
	adc	dl,dl
stillbitsleft:
	ret
;
getgamma:
	xor	ecx,ecx
getgamma_no_ecx:
	inc	ecx
getgammaloop:
	call	getbit
	adc	ecx,ecx
	call	getbit
	jc	getgammaloop
	ret
;
donedepacking:
	pop	rdx
	sub	rdi,rdx

	xchg	rdi,[r9]	; [r9] = nRead, last block size = [r9]
	and	esi,BUFSIZ-1	; Get offset into buffer
	jz	noptradj
	sub	rsi,rdi		; How much to adjust pointer (negative)

	push	rcx
	push	rdx
	push	r8
	push	r9		; lpNumberOfBytesRead
	push	r10

	mov	rcx,r10		; hFile	
	mov	rdx,rsi		; DistanceToMove (negative)
	xor	r8d,r8d		; lpDistanceToMoveHigh
	mov	r9d,1		; dwMoveMethod = FILE_CURRENT

	sub	rsp,32		; Shadow space
        call    SetFilePointer
	add	rsp,32

	pop	r10
	pop	r9
	pop	r8
	pop	rdx
	pop	rcx

noptradj:
	pop	rdi
	pop	rsi
	pop	rbx

	mov	rsp,rbp
	pop	rbp
	ret
;
; In:  r10 = hFile (to refill buffer)
;      r9 = lpNumberOfBytesRead
;      rsi = buffer pointer (on stack)
; Out: [r9] = nRead (if buffer refilled)
;      al = byte read
;      
getbyte:
	test	rsi,BUFSIZ-1	; Buffer empty ?
	jnz	getbyt1

	push	rax
	push	rcx
	push	rdx
	push	r8
	push	r9		; lpNumberOfBytesRead
	push	r10

	mov	eax,BUFSIZ
	sub	rsi,rax

	mov	rcx,r10		; hFile
	mov	rdx,rsi		; lpBuffer
	mov	r8,rax		; nNumberOfBytesToRead
	push	byte 0		; lpOverlapped

	sub	rsp,32		; Shadow space
	call	ReadFile
	add	rsp,40

	pop	r10
	pop	r9
	pop	r8
	pop	rdx
	pop	rcx
	pop	rax
getbyt1:
	lodsb
	ret
