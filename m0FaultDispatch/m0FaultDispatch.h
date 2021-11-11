#ifndef _M0_FAULT_DISPATCH_H_
#define _M0_FAULT_DISPATCH_H_

#include <stdint.h>

struct M0excFrame {
	
	uint32_t r0_r3[4];	//r0..r3
	uint32_t r12, lr, pc, sr;
};

struct M0highRegs {
	
	uint32_t r8_r11[4];	//r8..r11
	uint32_t r4_r7[4];	//r4..r7
};

//all faults are recoverable/resumable. modify "exc" and/or "hiRegs" and return to resume :)
//you will be called from HardFault context or a similar situation. IRQs will likely be off.
//"where is SP?" falting SP will always be at (exc + 1)

enum M0faultReason {
	M0faultMemAccessFailR,	//a memory access failed (read).				addr is valid - contains address that was accessed
	M0faultMemAccessFailW,	//a memory access failed (write).				addr is valid - contains address that was accessed
	M0faultUnalignedAccess,	//an unaligned memory access was attempted		addr is valid - contains address that was accessed
	M0faultUnalignedPc,		//an unaligned addr ended up in PC				see exc->pc
	M0faultInstrFetchFail,	//failed to fetch instructions					(may not be accurate in exec-only memory)
	M0faultUndefInstr16,	//an undefined 16-bit instr was executed		exc->pc points to the invalid instruction
	M0faultUndefInstr32,	//an undefined 32-bit instr was executed		exc->pc points to the invalid instruction
	M0faultBkptInstr,		//a BKPT instr was executed						exc->pc points to the breakpoint instruction
	M0faultArmMode,			//an attempt was made to switch to ARM mode		exc->pc has addr, exc->sr is missing the T bit
	M0faultUnclassifiable,	//some other sort of thing
};

void m0FaultHandle(struct M0excFrame *exc, struct M0highRegs *hiRegs, enum M0faultReason reason, uint32_t addr);



#endif
