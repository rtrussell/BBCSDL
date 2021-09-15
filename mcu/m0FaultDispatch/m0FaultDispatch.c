#include "m0FaultDispatch.h"
#include <stdbool.h>
#include <stdint.h>

//m0FaultDispatch (c) 2019 Dmitry Grinberg. http://dmitry.gr   License available on the website




static uint8_t mInExcHandlerAlready = 0;



//we need precise sizes and cannot just use bytes because some HW regs may otherwise cause issues
//on fault, 2 instrs are skipped (faulting instr and next one)
static bool __attribute__((noinline)) m0FaultDispatchRead8(uint32_t addr, void *to)
{
	bool ret = false;
	uint32_t t;
	
	asm volatile(
		"	mov  %1, #0				\n\t"
		"	ldrb %0, [%2]			\n\t"
		"	mov  %1, #1				\n\t"
		"	strb %0, [%3]			\n\t"
		:"=&l"(t), "=&l"(ret)
		:"l"(addr),"l"(to)
	);
	
	return ret;
}

static bool __attribute__((noinline)) m0FaultDispatchRead16(uint32_t addr, void *to)
{
	bool ret = false;
	uint32_t t;
	
	asm volatile(
		"	mov  %1, #0				\n\t"
		"	ldrh %0, [%2]			\n\t"
		"	mov  %1, #1				\n\t"
		"	strh %0, [%3]			\n\t"
		:"=&l"(t), "=&l"(ret)
		:"l"(addr),"l"(to)
	);
	
	return ret;
}

static bool __attribute__((noinline)) m0FaultDispatchRead32(uint32_t addr, void *to)
{
	bool ret = false;
	uint32_t t;
	
	asm volatile(
		"	mov  %1, #0				\n\t"
		"	ldr  %0, [%2]			\n\t"
		"	mov  %1, #1				\n\t"
		"	str %0, [%3]			\n\t"
		:"=&l"(t), "=&l"(ret)
		:"l"(addr),"l"(to)
	);
	
	return ret;
}

static bool __attribute__((noinline)) m0FaultDispatchWrite8(uint32_t addr, const void *to)
{
	bool ret = false;
	uint32_t t;
	
	asm volatile(
		"	ldrb %0, [%3]			\n\t"
		"	mov  %1, #0				\n\t"
		"	strb %0, [%2]			\n\t"
		"	mov  %1, #1				\n\t"
		:"=&l"(t), "=&l"(ret)
		:"l"(addr),"l"(to)
	);
	
	return ret;
}

static bool __attribute__((noinline)) m0FaultDispatchWrite16(uint32_t addr, const void *to)
{
	bool ret = false;
	uint32_t t;
	
	asm volatile(
		"	ldrh %0, [%3]			\n\t"
		"	mov  %1, #0				\n\t"
		"	strh %0, [%2]			\n\t"
		"	mov  %1, #1				\n\t"
		:"=&l"(t), "=&l"(ret)
		:"l"(addr),"l"(to)
	);
	
	return ret;
}

static bool __attribute__((noinline)) m0FaultDispatchWrite32(uint32_t addr, const void *to)
{
	bool ret = false;
	uint32_t t;
	
	asm volatile(
		"	ldr  %0, [%3]			\n\t"
		"	mov  %1, #0				\n\t"
		"	str  %0, [%2]			\n\t"
		"	mov  %1, #1				\n\t"
		:"=&l"(t), "=&l"(ret)
		:"l"(addr),"l"(to)
	);
	
	return ret;
}



static uint32_t dispatchM0faultUtilGetReg(struct M0excFrame* frm, struct M0highRegs *hiRegs, uint32_t regNo)
{
	switch (regNo) {
		case 0:		//fallthrough
		case 1:		//fallthrough
		case 2:		//fallthrough
		case 3:		return frm->r0_r3[regNo - 0];
		case 4:		//fallthrough
		case 5:		//fallthrough
		case 6:		//fallthrough
		case 7:		return hiRegs->r4_r7[regNo - 4];
		case 13:	return (uintptr_t)(frm + 1);
		//other regs not needed for our use
		default:	__builtin_unreachable();
	}
}

void __attribute__((used)) m0FaultDispatchInterpInstr(struct M0excFrame* exc, struct M0highRegs* hiRegs, uint16_t instr)
{
	enum M0faultReason reason = M0faultUnclassifiable;
	uint32_t t, base, addr, accessSz;
	bool testStore = false;
	
	// mInExcHandlerAlready is still set
	
	switch (instr >> 11) {
		case 0b01000:
			if ((instr & 0x05c0) == 0x0500)			//cmp.hi with both lo regs
				reason = M0faultUndefInstr16;
			break;
		
		case 0b01001:	//load from literal pool (alignment guaranteed by instr)
			addr = ((exc->pc + 4) &~ 3) + ((instr & 0xff) << 2);
			if (!m0FaultDispatchRead32(addr, &t))
				reason = M0faultMemAccessFailR;
			break;
		
		case 0b01010:	//load/store register
		case 0b01011:
			switch ((instr >> 9) & 0x07) {
				case 0b000:		//STR
					testStore = true;
					//fallthrough
				case 0b100:		//LDR
					accessSz = 4;
					break;
				case 0b001:		//STRH
					testStore = true;
					//fallthrough
				case 0b101:		//LDRH
				case 0b111:		//LDRSH
					accessSz = 2;
					break;
				case 0b010:		//STRB
					testStore = true;
					//fallthrough
				case 0b110:		//LDRB
				case 0b011:		//LDRSB
					accessSz = 1;
					break;
			}
			t = dispatchM0faultUtilGetReg(exc, hiRegs, (instr >> 6) & 0x07);
			goto load_store_check;
			
		case 0b01100:	//str imm
			testStore = true;
			//fallthrough
		case 0b01101:	//ldr imm
			accessSz = 4;
			goto load_store_check_get_imm;

		case 0b01110:	//strb imm
			testStore = true;
			//fallthrough
		case 0b01111:	//ldrb imm
			accessSz = 1;
			goto load_store_check_get_imm;

		case 0b10000:	//strh imm
			testStore = true;
			//fallthrough
		
		case 0b10001:	//ldrh imm
			accessSz = 2;
			goto load_store_check_get_imm;
		
load_store_check_get_imm:
			t = (instr >> 6) & 0x1f;
			t *= accessSz;

load_store_check:
			addr = dispatchM0faultUtilGetReg(exc, hiRegs, (instr >> 3) & 0x07);

load_store_check_have_base:
			addr += t;
			
			if (addr & (accessSz - 1))
				reason = M0faultUnalignedAccess;
			else {
				switch (accessSz) {
					case 1:
						if (!m0FaultDispatchRead8(addr, &t) || (testStore && !m0FaultDispatchWrite8(addr, &t)))
							reason = testStore ? M0faultMemAccessFailW : M0faultMemAccessFailR;
						break;
					
					case 2:
						if (!m0FaultDispatchRead16(addr, &t) || (testStore && !m0FaultDispatchWrite16(addr, &t)))
							reason = testStore ? M0faultMemAccessFailW : M0faultMemAccessFailR;
						break;
					
					case 4:
						
						if (!m0FaultDispatchRead32(addr, &t) || (testStore && !m0FaultDispatchWrite32(addr, &t)))
							reason = testStore ? M0faultMemAccessFailW : M0faultMemAccessFailR;
						break;
					
					default:
						__builtin_unreachable();
				}
			}
			break;
		
		case 0b10010:	//sp-based store
			testStore = true;
			//fallthrough
		
		case 0b10011:	//sp-based load
			t = (instr & 0xff) * 4;
			accessSz = 4;
			addr = dispatchM0faultUtilGetReg(exc, hiRegs, 13);
			goto load_store_check_have_base;
		
		case 0b10111:
			if ((instr & 0x0700) == 0x0600)
				reason = M0faultBkptInstr;
			//push/pop are here too, but if they fail, we'd fail to stash and not ever get here
			break;
		
		case 0b11000:	//STMIA
			testStore = true;
			//fallthrough
		case 0b11001:	//LDMIA
			addr = dispatchM0faultUtilGetReg(exc, hiRegs, (instr >> 8) & 0x07);
			if (addr & 3)							//unaligned base?
				reason = M0faultUnalignedAccess;
			else if (!(instr & 0xff))				//LDM/STM with empty reg set
				reason = M0faultUndefInstr16;
			else {
				uint32_t i;
				
				for (i = instr & 0xff; i; i &= (i - 1), addr += 4) {
					
					if (!m0FaultDispatchRead32(addr, &t) || (testStore && !m0FaultDispatchWrite32(addr, &t))) {
						
						reason = testStore ? M0faultMemAccessFailW : M0faultMemAccessFailR;;
						break;
					}
				}
			}
			break;
		
		case 0b11011:
			if ((instr & 0x0700) == 0x0600)			//UDF
				reason = M0faultUndefInstr16;
			break;
	}

	mInExcHandlerAlready = 0;
	m0FaultHandle(exc, hiRegs, reason, addr);
}

void __attribute__((used,naked)) HardFault_Handler(void)
{
	asm volatile(
		
		".macro push_hi_regs_with_lr						\n\t"
		"	push   {r4-r7, lr}								\n\t"
		"	mov    r4, r8									\n\t"
		"	mov    r5, r9									\n\t"
		"	mov    r6, r10									\n\t"
		"	mov    r7, r11									\n\t"
		"	push   {r4-r7}									\n\t"
		"	mov    r1, sp									\n\t"
		".endm												\n\t"
		
		".macro pop_hi_regs_with_pc							\n\t"
		"	pop   {r4-r7}									\n\t"
		"	mov   r8, r4									\n\t"
		"	mov   r9, r5									\n\t"
		"	mov   r10, r6									\n\t"
		"	mov   r11, r7									\n\t"
		"	pop   {r4-r7, pc}								\n\t"
		".endm												\n\t"
	
		"	mov   r0, lr									\n\t"
		"	lsr   r0, #3									\n\t"
		"	bcs   1f										\n\t"
		"	mrs   r0, msp									\n\t"	//grab the appropriate SP
		"	b     2f										\n\t"
		"1:													\n\t"
		"	mrs   r0, psp									\n\t"
		"2:													\n\t"
		
		"check_if_reentry:									\n\t"
		"	ldr   r3, =%0									\n\t"	//&mInExcHandlerAlready
		"	ldrb  r1, [r3]									\n\t"
		"	cmp   r1, #0									\n\t"
		"	bne   is_reentry								\n\t"
		
		"check_for_arm_mode:								\n\t"
		"	ldr   r1, [r0, #0x1c]							\n\t"	//load exc->sr
		"	lsr   r2, r1, #25								\n\t"	//shift out the T bit
		"	bcs   check_pc_align							\n\t"	//if it is set, further testing to be done here
		
		"arm_mode_entered:									\n\t"
		"	mov   r2, %1									\n\t"	//M0faultArmMode
		"	b     report_fault								\n\t"

		"check_pc_align:									\n\t"
		"	ldr   r1, [r0, #0x18]							\n\t"
		"	lsr   r2, r1, #1								\n\t"
		"	bcc   complex_checks							\n\t"	//PC is aligned, go do complex checks
		
		"pc_misaligned:										\n\t"
		"	mov   r2, %2									\n\t"	//M0faultUnalignedPc
		"	b     report_fault								\n\t"
		
		"complex_checks:									\n\t"	//complex check requires dropping to a higher IPSR. we use 4		
		//craft a frame we can "return" to, and save all we need in there
		"	mov   r1, lr									\n\t"	//LR we'll need to properly return!
		"   adr   r2, complex_checks_code					\n\t"	//stashed PC
		"	ldr   r3, =%3									\n\t"	//desired SR
		"	push  {r0-r3}									\n\t"	//stashed r12 (exc), lr, pc, sr
		"	sub   sp, #16									\n\t"	//stashed r1,r2,r3,r12,lr
		"	ldr   r0, =0xfffffff1							\n\t"	//get an lr to go to (handler mode, main stack)
		
		//record that we're doing this
		"	ldr   r3, =%0									\n\t"	//&mInExcHandlerAlready
		"	mov   r2, #1									\n\t"
		"	strb  r2, [r3]									\n\t"
		
		//do it
		"	bx    r0										\n\t"
		
		//code that executes at IPSR == 4 (and thus can fault again if needed without locking up)
		//r0 still has exc frame
		".balign 4											\n\t"	//needed for "adr"-ing this
		"complex_checks_code:								\n\t"
		"	mov   r0, r12									\n\t"	//get exc
		"	ldr   r1, [r0, #0x18]							\n\t"	//grab PC
		"	ldrh  r2, [r1]									\n\t"	//try to get instr (THIS CAN FAULT)
		"	b     instr_fetched								\n\t"
		
		"fetch_failed:										\n\t"	//re-entry handler drops us here
		"	mov   r2, %5									\n\t"	//M0faultInstrFetchFail
		"	b     report_fault								\n\t"
		
		"instr_fetched:										\n\t"
		"	lsr   r3, r2, #11								\n\t"
		"	cmp   r3, #0x1C									\n\t"
		"	bls   instr_is_16_bits_long						\n\t"
		
		"instr_is_32_bits_long:								\n\t"	//there are no 32-bit memory access instrs, so if we're here, it is an undefined instr, but first verify we cna fetch the second half
		"	ldrh  r2, [r1, #2]								\n\t"	//try to get instr's second half(THIS CAN FAULT)
		"	b     instr_fetched_half2						\n\t"
		
		"fetch_failed_half2:								\n\t"	//re-entry handler drops us here
		"	mov   r2, %5									\n\t"	//M0faultInstrFetchFail
		"	b     report_fault								\n\t"
		
		"instr_fetched_half2:								\n\t"
		"	mov   r2, %4									\n\t"	//M0faultUndefInstr32
		"	b     report_fault								\n\t"
		
		"instr_is_16_bits_long:								\n\t"	//16-bit instrs might be memory access instrs and thus we need to interp them
		"	push_hi_regs_with_lr							\n\t"
		"	bl    m0FaultDispatchInterpInstr				\n\t"
		"	pop_hi_regs_with_pc								\n\t"
		
		"report_fault:										\n\t"
		"	mov   r3, #0									\n\t"
		"	ldr   r1, =%0									\n\t"	//&mInExcHandlerAlready
		"	strb  r3, [r1]									\n\t"	//clear it
		"	push_hi_regs_with_lr							\n\t"
		"	bl    m0FaultHandle								\n\t"
		"	pop_hi_regs_with_pc								\n\t"
		
		"is_reentry:										\n\t"	//r3 is still &mInExcHandlerAlready, r1 is the value
		"	ldr   r2, [r0, #0x18]							\n\t"	//preemptively get pc
		"	add   r2, #4									\n\t"	//skip 2 instrs
		"	str   r2, [r0, #0x18]							\n\t"	//store pc
		"	bx    lr										\n\t"
		
		".ltorg												\n\t"
		:
		:"i"(&mInExcHandlerAlready), "I"(M0faultArmMode), "I"(M0faultUnalignedPc), "i"(0x01000004),
			"I"(M0faultUndefInstr32), "I"(M0faultInstrFetchFail)
	);
}
