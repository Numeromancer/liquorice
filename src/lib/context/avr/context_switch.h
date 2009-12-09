/*
 * context/avr/context_switch.h
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
 * Context switch frame layout.
 *	This is the layout of the stack after a context has been switched-out.
 *
 * Remember of course that the bottom of the structure (the first line) was the
 * last thing to have been pushed onto the stack and the top (the last line)
 * was the first!
 */
struct context_switch_frame {
	u8_t csf_r29;
	u8_t csf_r28;
	u8_t csf_r17;
	u8_t csf_r16;
	u8_t csf_r15;
	u8_t csf_r14;
	u8_t csf_r13;
	u8_t csf_r12;
	u8_t csf_r11;
	u8_t csf_r10;
	u8_t csf_r9;
	u8_t csf_r8;
	u8_t csf_r7;
	u8_t csf_r6;
	u8_t csf_r5;
	u8_t csf_r4;
	u8_t csf_r3;
	u8_t csf_r2;
	u8_t csf_pchi;
	u8_t csf_pclo;
};

/*
 * context_switch_prologue()
 */
extern inline void context_switch_prologue(struct context *c)
{
	asm volatile ("push r2\n\t"
			"push r3\n\t"
			"push r4\n\t"
			"push r5\n\t"
			"push r6\n\t"
			"push r7\n\t"
			"push r8\n\t"
			"push r9\n\t"
			"push r10\n\t"
			"push r11\n\t"
			"push r12\n\t"
			"push r13\n\t"
			"push r14\n\t"
			"push r15\n\t"
			"push r16\n\t"
			"push r17\n\t"
			"push r28\n\t"
			"push r29\n\t"
			"in %A0, %1\n\t"
			"in %B0, %2\n\t"
			: "=r" (c->c_cpu.cc_sp)
			: "I" (SPL), "I" (SPH));
}

/*
 * context_switch_epilogue()
 */
extern inline void context_switch_epilogue(struct context *c)
{
	asm volatile ("out %1, %A0\n\t"
			"out %2, %B0\n\t"
			"pop r29\n\t"
			"pop r28\n\t"
			"pop r17\n\t"
			"pop r16\n\t"
			"pop r15\n\t"
			"pop r14\n\t"
			"pop r13\n\t"
			"pop r12\n\t"
			"pop r11\n\t"
			"pop r10\n\t"
			"pop r9\n\t"
			"pop r8\n\t"
			"pop r7\n\t"
			"pop r6\n\t"
			"pop r5\n\t"
			"pop r4\n\t"
			"pop r3\n\t"
			"pop r2\n\t"
			: /* No output */
			: "r" (c->c_cpu.cc_sp), "I" (SPL), "I" (SPH));
}

/*
 * context_switch_frame_build()
 */
extern inline void context_switch_frame_build(struct context_switch_frame *csf, struct context *c)
{
	extern void context_enter(void);

	c->c_cpu.cc_sp = ((addr_t)csf) - 1;
	csf->csf_pchi = (unsigned char)((((addr_t)context_enter) >> 8) & 0xff);
	csf->csf_pclo = (unsigned char)(((addr_t)context_enter) & 0xff);
}

/*
 * startup_mark()
 */
extern inline void startup_mark(void)
{
	asm volatile (".global startup\n"
			"startup:\n\t"
			::);
}

/*
 * jump_startup()
 */
extern inline void jump_startup(void)
{
	asm volatile ("rjmp startup\n\t" ::);
}

/*
 * cpu_context_init()
 *	Initialize the CPU-specific part of the context structure.
 */
extern inline void cpu_context_init(struct cpu_context *cc)
{
}
