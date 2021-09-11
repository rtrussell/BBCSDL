/*  fault.c -- Use m0FaultDispatch by Dmitry Grinberg to decode
               Cortex-M0 faults and provide CPU state

    For information about m0FaultDispatch see

        https://dmitry.gr/?r=05.Projects&proj=27.%20m0FaultDispatch

    Note that Dmitry's code is freely licensed for use in hobby and
    other non-commerical products.  We are using it here only as a
    debugging aid.  If you wish to remove this functionality, please
    modify the CMakeFile.txt and then in bbccon.c change

    exception_set_exclusive_handler(HARDFAULT_EXCEPTION,HardFault_Handler);

	to

    exception_set_exclusive_handler(HARDFAULT_EXCEPTION,sigbus);

*/

#include <stdio.h>
#include <stdlib.h>

#include "m0FaultDispatch.h"

void m0FaultHandle(struct M0excFrame *exc, struct M0highRegs *hiRegs,
	enum M0faultReason reason, uint32_t addr){
	static const char *names[] = {
		[M0faultMemAccessFailR] = "Memory read failed",
		[M0faultMemAccessFailW] = "Memory write failed",
		[M0faultUnalignedAccess] = "Data alignment fault",
		[M0faultInstrFetchFail] = "Instr fetch failure",
		[M0faultUnalignedPc] = "Code alignment fault",
		[M0faultUndefInstr16] = "Undef 16-bit instr",
		[M0faultUndefInstr32] = "Undef 32-bit instr",
		[M0faultBkptInstr] = "Breakpoint hit",
		[M0faultArmMode] = "ARM mode entered",
		[M0faultUnclassifiable] = "Unclassified fault",
	};

	printf("%s sr = 0x%08lx\n",
		(reason<sizeof(names)/sizeof(*names) &&
		names[reason])?names[reason]:"????",exc->sr);
	printf("R0  = 0x%08lx  R8  = 0x%08lx\n",
		exc->r0_r3[0], hiRegs->r8_r11[0]);
	printf("R1  = 0x%08lx  R9  = 0x%08lx\n",
		exc->r0_r3[1], hiRegs->r8_r11[1]);
	printf("R2  = 0x%08lx  R10 = 0x%08lx\n",
		exc->r0_r3[2], hiRegs->r8_r11[2]);
	printf("R3  = 0x%08lx  R11 = 0x%08lx\n",
		exc->r0_r3[3], hiRegs->r8_r11[3]);
	printf("R4  = 0x%08lx  R12 = 0x%08lx\n",
		hiRegs->r4_r7[0], exc->r12);
	printf("R5  = 0x%08lx  SP  = 0x%08lx\n",
		hiRegs->r4_r7[1], (uint32_t)(exc + 1));
	printf("R6  = 0x%08lx  LR  = 0x%08lx\n",
		hiRegs->r4_r7[2], exc->lr);
	printf("R7  = 0x%08lx  PC  = 0x%08lx\n",
		hiRegs->r4_r7[3], exc->pc);
	
	switch (reason) {
case M0faultMemAccessFailR:
		printf(" -> failed to read 0x%08lx\n", addr);
		break;
case M0faultMemAccessFailW:
		printf(" -> failed to write 0x%08lx\n", addr);
		break;
case M0faultUnalignedAccess:
		printf(" -> unaligned access to 0x%08lx\n", addr);
		break;
case M0faultUndefInstr16:
		printf(" -> undef instr16: 0x%04x\n", ((uint16_t*)exc->pc)[0]);
		break;
case M0faultUndefInstr32:
		printf(" -> undef instr32: 0x%04x 0x%04x\n",
			((uint16_t*)exc->pc)[0], ((uint16_t*)exc->pc)[1]);
default:
		break;
	}
	while(1);
}
