/*
 * context/i386/context_trap.c
 *	Processor trap management facilities
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
#include "types.h"
#include "cpu.h"
#include "io.h"
#include "memory.h"
#include "isr.h"
#include "debug.h"
#include "trap.h"
#include "context.h"

/*
 * Trap handling functions.
 */
extern void __trap_unused(void);
extern void __trap_divide_by_zero(void);
extern void __trap_debug(void);
extern void __trap_nmi(void);
extern void __trap_breakpoint(void);
extern void __trap_overflow(void);
extern void __trap_bound(void);
extern void __trap_invalid_opcode(void);
extern void __trap_device_not_available(void);
extern void __trap_double_fault(void);
extern void __trap_coprocessor_overrun(void);
extern void __trap_invalid_tss(void);
extern void __trap_segment_not_present(void);
extern void __trap_stack_fault(void);
extern void __trap_general_protection(void);
extern void __trap_page_fault(void);
extern void __trap_floating_point_error(void);
extern void __trap_alignment_check(void);
extern void __trap_irq0(void);
extern void __trap_irq1(void);
extern void __trap_irq2(void);
extern void __trap_irq3(void);
extern void __trap_irq4(void);
extern void __trap_irq5(void);
extern void __trap_irq6(void);
extern void __trap_irq7(void);
extern void __trap_irq8(void);
extern void __trap_irq9(void);
extern void __trap_irq10(void);
extern void __trap_irq11(void);
extern void __trap_irq12(void);
extern void __trap_irq13(void);
extern void __trap_irq14(void);
extern void __trap_irq15(void);

/*
 * idt_entry()
 *	Establish an individual IDT slot
 */
static void idt_entry(struct gate *g, void (*f)(void), u8_t gtype)
{
	g->g_off0 = (u32_t)f & 0xffff;	/* Address of entry point */
	g->g_off1 = ((u32_t)f >> 16) & 0xffff;
	g->g_sel = 0x10;		/* Entry point selector */
	g->g_stkwds = 0;		/* No stack words reserved */
	g->g_type = gtype;
	g->g_dpl = 0;			/* Privillege level */
	g->g_present = 1;		/* Descriptor is present */
}

/*
 * trap_init()
 *	Initialize the interrupt (trap) descriptor table
 */
void trap_init(addr_t idt_addr)
{
	int i;
	struct linmem l;
	static struct idt_info {
		u8_t idt_slot;
		u8_t idt_type;
		void (*idt_fun)(void);
	} idt_table[] = {
		{TRAP_DIVIDE_BY_ZERO, TYPE_INTR, __trap_divide_by_zero},
		{TRAP_DEBUG, TYPE_INTR, __trap_debug},
		{TRAP_NMI, TYPE_INTR, __trap_nmi},
		{TRAP_BREAKPOINT, TYPE_INTR, __trap_breakpoint},
		{TRAP_OVERFLOW, TYPE_INTR, __trap_overflow},
		{TRAP_BOUND, TYPE_INTR, __trap_bound},
		{TRAP_INVALID_OPCODE, TYPE_INTR, __trap_invalid_opcode},
		{TRAP_DEVICE_NOT_AVAILABLE, TYPE_INTR, __trap_device_not_available},
		{TRAP_DOUBLE_FAULT, TYPE_INTR, __trap_double_fault},
		{TRAP_COPROCESSOR_OVERRUN, TYPE_INTR, __trap_coprocessor_overrun},
		{TRAP_INVALID_TSS, TYPE_INTR, __trap_invalid_tss},
		{TRAP_SEGMENT_NOT_PRESENT, TYPE_INTR, __trap_segment_not_present},
		{TRAP_STACK_FAULT, TYPE_INTR, __trap_stack_fault},
		{TRAP_GENERAL_PROTECTION, TYPE_INTR, __trap_general_protection},
		{TRAP_PAGE_FAULT, TYPE_INTR, __trap_page_fault},
		{TRAP_FLOATING_POINT_ERROR, TYPE_INTR, __trap_floating_point_error},
		{TRAP_ALIGNMENT_CHECK, TYPE_INTR, __trap_alignment_check},
		{IRQ2TRAP(0), TYPE_INTR, __trap_irq0},
		{IRQ2TRAP(1), TYPE_INTR, __trap_irq1},
		{IRQ2TRAP(2), TYPE_INTR, __trap_irq2},
		{IRQ2TRAP(3), TYPE_INTR, __trap_irq3},
		{IRQ2TRAP(4), TYPE_INTR, __trap_irq4},
		{IRQ2TRAP(5), TYPE_INTR, __trap_irq5},
		{IRQ2TRAP(6), TYPE_INTR, __trap_irq6},
		{IRQ2TRAP(7), TYPE_INTR, __trap_irq7},
		{IRQ2TRAP(8), TYPE_INTR, __trap_irq8},
		{IRQ2TRAP(9), TYPE_INTR, __trap_irq9},
		{IRQ2TRAP(10), TYPE_INTR, __trap_irq10},
		{IRQ2TRAP(11), TYPE_INTR, __trap_irq11},
		{IRQ2TRAP(12), TYPE_INTR, __trap_irq12},
		{IRQ2TRAP(13), TYPE_INTR, __trap_irq13},
		{IRQ2TRAP(14), TYPE_INTR, __trap_irq14},
		{IRQ2TRAP(15), TYPE_INTR, __trap_irq15},
		{0xff, 0, NULL}
	};
	struct idt_info *idf;
	
	/*
	 * Next make everything initially look unhandled
	 */
	for (i = 0; i < IDT_ENTRIES; i++) {
		idt_entry((struct gate *)idt_addr + i, __trap_unused, TYPE_INTR);
	}

	/*
	 * Now fill in the individual CPU traps
	 */
	idf = idt_table;
	do {
		idt_entry((struct gate *)idt_addr + idf->idt_slot, idf->idt_fun, idf->idt_type);
		idf++;
	} while (idf->idt_slot != 0xff);

	/*
	 * Initialize the interrupt descriptor table
	 */
	l.l_len = (sizeof(struct gate) * IDT_ENTRIES) - 1;
	l.l_addr = idt_addr;
	asm volatile ("lidt (%%eax)\n\t"
			"jmp 1f\n"
			"1:\n\t"
			: /* No output */
			: "a" (&l.l_len));
}

/*
 * Any of the traps that we don't handle any other way get a default handler.
 */
void trap_unused() __attribute__ ((weak, alias ("trap_stray")));
void trap_divide_by_zero() __attribute__ ((weak, alias ("trap_stray")));
void trap_debug() __attribute__ ((weak, alias ("trap_stray")));
void trap_nmi() __attribute__ ((weak, alias ("trap_stray")));
void trap_breakpoint() __attribute__ ((weak, alias ("trap_stray")));
void trap_overflow() __attribute__ ((weak, alias ("trap_stray")));
void trap_bound() __attribute__ ((weak, alias ("trap_stray")));
void trap_invalid_opcode() __attribute__ ((weak, alias ("trap_stray")));
void trap_double_fault() __attribute__ ((weak, alias ("trap_stray")));
void trap_coprocessor_overrun() __attribute__ ((weak, alias ("trap_stray")));
void trap_invalid_tss() __attribute__ ((weak, alias ("trap_stray")));
void trap_segment_not_present() __attribute__ ((weak, alias ("trap_stray")));
void trap_stack_fault() __attribute__ ((weak, alias ("trap_stray")));
void trap_general_protection() __attribute__ ((weak, alias ("trap_stray")));
void trap_page_fault() __attribute__ ((weak, alias ("trap_stray")));
void trap_floating_point_error() __attribute__ ((weak, alias ("trap_stray")));
void trap_alignment_check() __attribute__ ((weak, alias ("trap_stray")));
void trap_irq0() __attribute__ ((weak, alias ("trap_stray")));
void trap_irq1() __attribute__ ((weak, alias ("trap_stray")));
void trap_irq2() __attribute__ ((weak, alias ("trap_stray")));
void trap_irq3() __attribute__ ((weak, alias ("trap_stray")));
void trap_irq4() __attribute__ ((weak, alias ("trap_stray")));
void trap_irq5() __attribute__ ((weak, alias ("trap_stray")));
void trap_irq6() __attribute__ ((weak, alias ("trap_stray")));
void trap_irq7() __attribute__ ((weak, alias ("trap_stray")));
void trap_irq8() __attribute__ ((weak, alias ("trap_stray")));
void trap_irq9() __attribute__ ((weak, alias ("trap_stray")));
void trap_irq10() __attribute__ ((weak, alias ("trap_stray")));
void trap_irq11() __attribute__ ((weak, alias ("trap_stray")));
void trap_irq12() __attribute__ ((weak, alias ("trap_stray")));
void trap_irq13() __attribute__ ((weak, alias ("trap_stray")));
void trap_irq14() __attribute__ ((weak, alias ("trap_stray")));
void trap_irq15() __attribute__ ((weak, alias ("trap_stray")));

/*
 * trap_stray()
 *	Handler for any spurious traps
 */
void trap_stray() __attribute__ ((cdecl));
void trap_stray(struct trap_frame tf)
{
	debug_stop();
	
	debug_print_pstr("\n\rSTRAY TRAP: faulting cr2: ");
	debug_print_addr(get_cr2());
	debug_print_pstr("\n\rflt trapframe: ");
	debug_print_addr((addr_t)&tf);
	debug_print_pstr("\n\rflt type: ");
	debug_print_addr(tf.tf_trapnum);
	debug_print_pstr("\n\rflt eip: ");
	debug_print_addr(tf.tf_eip);
	debug_print_pstr("\n\r");
	debug_print_stack();
	while (1);
}

/*
 * trap_device_not_available()
 *	Handler for co-processor emulation traps.
 */
void trap_device_not_available() __attribute__ ((cdecl));
void trap_device_not_available(struct trap_frame tf)
{
	/*
	 * Enable access to the FPU.
	 */
	set_cr0(get_cr0() & (~CR0_EM));
		
	/*
	 * Has the current context already used the FPU?  If it has then we
	 * restore its state from the saved data; if it hasn't then we
	 * initialize a new state.
	 */
	if (current_context->c_cpu.cc_flags & CC_FPU_SAVED) {
		asm volatile ("frstor (%0)\n\t"
				: /* No output */
				: "r" (&current_context->c_cpu.cc_fpu));
	} else {
		asm volatile ("fnclex\n\t"
				"finit\n\t"
				::);
	}
}

/*
 * cpu_init()
 */
void cpu_init(addr_t descriptors)
{
	trap_init(descriptors);
}
