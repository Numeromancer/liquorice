/*
 * context/i386/context_switch.h
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
	u32_t csf_ebx;
	u32_t csf_ebp;
	u32_t csf_esi;
	u32_t csf_edi;
	u32_t csf_ebp_saved;
	u32_t csf_eip;
};

/*
 * context_switch_prologue()
 */
extern inline void context_switch_prologue(struct context *c)
{
	/*
	 * First up look if we've used the FPU during the execution of this
	 * context.  If we have then we save it now and disable the FPU again.
	 */
	if (c->c_cpu.cc_flags & CC_FPU_USED) {
		asm volatile ("fnsave (%0)\n\t"
				: /* No output */
				: "r" (&c->c_cpu.cc_fpu));
		c->c_cpu.cc_flags &= (~CC_FPU_USED);
		
		set_cr0(get_cr0() | CR0_EM);
	}
	 	
	/*
	 * General purpose register saves.
	 */
	asm volatile ("pushl %%edi\n\t"
			"pushl %%esi\n\t"
			"pushl %%ebp\n\t"
			"pushl %%ebx\n\t"
			"movl %%esp, %0\n\t"
			: "=g" (c->c_cpu.cc_esp)
			: /* No input */);
}

/*
 * context_switch_epilogue()
 */
extern inline void context_switch_epilogue(struct context *c)
{
	asm volatile ("movl %0, %%esp\n\t"
			"popl %%ebx\n\t"
			"popl %%ebp\n\t"
			"popl %%esi\n\t"
			"popl %%edi\n\t"
			: /* No output */
			: "g" (c->c_cpu.cc_esp));
}

/*
 * context_switch_frame_build()
 */
extern inline void context_switch_frame_build(struct context_switch_frame *csf, struct context *c)
{
	extern void context_enter(void);

	csf->csf_ebp = (addr_t)&csf->csf_ebp_saved;
	c->c_cpu.cc_esp = ((addr_t)csf);
	csf->csf_eip = (addr_t)context_enter;
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
	asm volatile ("jmp startup\n\t" ::);
}

/*
 * cpu_context_init()
 *	Initialize the CPU-specific part of the context structure.
 */
extern inline void cpu_context_init(struct cpu_context *cc)
{
	cc->cc_flags = 0;
}
