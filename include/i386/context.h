/*
 * i386/context.h
 *	Processor-specific context info.
 *
 * Copyright (C) 2000 David J. Hudson <dave@humbug.demon.co.uk>
 *
 * This file is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.
 *
 * You can redistribute this file and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software Foundation;
 * either version 2 of the License, or (at your discretion) any later version.
 * See the accompanying file "copying-gpl.txt" for more details.
 *
 * As a special exception to the GPL, permission is granted for additional
 * uses of the text contained in this file.  See the accompanying file
 * "copying-liquorice.txt" for details.
 */

/*
 * The hardware state of the x87 style FPU.  The layout of the FPU registers
 * is important as it matches that used by the "frstor" opcode.
 */
struct i386_fpu {
	u32_t if_cw;
	u32_t if_sw;
	u32_t if_tw;
	u32_t if_ip;
	u32_t if_cs;
	u32_t if_oo;
	u32_t if_os;
	u32_t if_st[20];
};

/*
 * Flags related to the use of the CPU.
 */
#define CC_FPU_SAVED 0x01		/* FPU state is currently saved in the context */
#define CC_FPU_USED 0x02		/* FPU has been used and needs to be saved as the next context switch */

/*
 * CPU-specific context information.
 */
struct cpu_context {
	u32_t cc_esp;			/* CPU stack register */
	struct i386_fpu cc_fpu;		/* FPU registers */
	u8_t cc_flags;			/* Flags specific to the running of this context */
};
