/*
 * i386/i386.h
 *	i386 CPU definitions.
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
 * Bits in CPU eflags register
 */
#define	F_CF 0x00000001			/* Carry */
#define	F_PF 0x00000004			/* Parity */
#define	F_AF 0x00000010			/* BCD stuff */
#define	F_ZF 0x00000040			/* Zero */
#define	F_SF 0x00000080			/* Sign */
#define	F_TF 0x00000100			/* Single step */
#define	F_IF 0x00000200			/* Interrupts */
#define	F_DF 0x00000400			/* Direction */
#define	F_OF 0x00000800			/* Overflow */
#define	F_IOPL 0x00003000		/* IO privilege level */
#define	F_NT 0x00004000			/* Nested task */
#define	F_RF 0x00010000			/* Resume flag */
#define	F_VM 0x00020000			/* Virtual 8086 */

/*
 * Bits in CPU configuration register cr0
 */
#define CR0_PE 0x00000001U		/* Protection enable */
#define CR0_MP 0x00000002U		/* FPU present */
#define CR0_EM 0x00000004U		/* Emulate FP - trap on FPU instructions */
#define CR0_TS 0x00000008U		/* Task switched */
#define CR0_ET 0x00000010U		/* Extension type - FPU present? */
#define CR0_NE 0x00000020U		/* Numeric Error - allow traps on FPU */
#define CR0_WP 0x00010000U		/* Write protect */
#define CR0_AM 0x00040000U		/* Alignment - trap on unaligned refs */
#define CR0_NW 0x20000000U		/* Not write-through - inhibit write-through */
#define CR0_CD 0x40000000U		/* Cache disable */
#define CR0_PG 0x80000000U		/* Paging - use PTEs/CR3 */

/*
 * Various descriptor sizes to bear in mind
 */
#define IDT_ENTRIES 256			/* Number of slots in the IDT */

/*
 * Memory descriptor entries
 */
struct memdesc {
	u32_t md_limit0 : 16;		/* Size bits 0 - 15 */
	u32_t md_base0 : 16;		/* Base address bits 0 - 15 */
	u32_t md_base1 : 8;		/* Base address bits 16 - 23 */
	u32_t md_type : 5;		/* Type */
	u32_t md_dpl : 2;		/* Privellige level */
	u32_t md_present : 1;		/* Present */
	u32_t md_limit1 : 4;		/* Size bits 16 - 19 */
	u32_t md_pad0 : 2;		/* Padding */
	u32_t md_32 : 1;		/* 32 bit size? */
	u32_t md_gran : 1;		/* Granularity (pages if set) */
	u32_t md_base2 : 8;		/* Base address bits 24 - 31 */
};

/*
 * Gateway segment entries
 */
struct gate {
	u32_t g_off0 : 16;		/* Offset bits 0 - 15 */
	u32_t g_sel : 16;		/* Selector */
	u32_t g_stkwds : 5;		/* Stack words to copy (always 0) */
	u32_t g_pad0 : 3;		/* Pad */
	u32_t g_type : 5;		/* Gate type */
	u32_t g_dpl : 2;		/* Privellige level */
	u32_t g_present : 1;		/* Present */
	u32_t g_off1 : 16;		/* Offset bits 16 - 23 */
};

/*
 * Segment types
 */
#define	TYPE_INVAL 0			/* Invalid */
#define	TYPE_LDT 2			/* LDT */
#define	TYPE_TASK 5			/* Task gate */
#define	TYPE_TSS 9			/* TSS */
#define	TYPE_CALL 12			/* Call gate */
#define	TYPE_INTR 14			/* Interrupt gate */
#define	TYPE_TRAP 15			/* Trap gate */
#define	TYPE_MEMRO 16			/* Read only data */
#define	TYPE_MEMRW 18			/* Read and write data */
#define	TYPE_MEMXO 24			/* Execute only text */
#define	TYPE_MEMXR 26			/* Execute and read text */

/*
 * Linear memory description for the lgdt and lidt instructions.  The
 * compiler tries to put l_addr on a long boundary, so we need to use
 * &l.l_len as the argument to "lgdt", etc
 */
struct linmem {
	u16_t l_pad;			/* Padding (unused) */
	u16_t l_len;			/* Length */
	u32_t l_addr;			/* Address */
};

/*
 * Task State Segment.  As Intel mention in their programmer's guides,
 * it's faster to switch between protection domains by explicit saving and
 * restoring of registers within the task state as not all of them need to
 * be saved under all circumstances.  We can't do away with this structure
 * completely however as it is used for transitioning between user and
 * kernel space.  One point worthy of note is that there is no FPU state
 * saved within this structure.
 */
struct tss {
	u32_t t_link;
	u32_t t_esp0;
	u32_t t_ss0;
	u32_t t_esp1;
	u32_t t_ss1;
	u32_t t_esp2;
	u32_t t_ss2;
	u32_t t_cr3;
	u32_t t_eip;
	u32_t t_eflags;
	u32_t t_eax;
	u32_t t_ecx;
	u32_t t_edx;
	u32_t t_ebx;
	u32_t t_esp;
	u32_t t_ebp;
	u32_t t_esi;
	u32_t t_edi;
	u32_t t_es;
	u32_t t_cs;
	u32_t t_ss;
	u32_t t_ds;
	u32_t t_fs;
	u32_t t_gs;
	u32_t t_ldt;
	u16_t t_debugtrap;
	u16_t t_iomap;
};

/*
 * Page table entry bits
 */
#define PTE_P 0x00000001U		/* Present */
#define PTE_RW 0x00000002U		/* Writable */
#define	PTE_US 0x00000004U		/* User accessible */
#define PTE_PWT 0x00000008U		/* Page write through */
#define PTE_PCD 0x00000010U		/* Page cache disable */
#define PTE_A 0x00000020U		/* Accessed */
#define PTE_D 0x00000040U		/* Dirty */
#define PTE_AVAIL 0x00000e00U		/* Available bits */
#define PTE_PGNUM 0xfffff000U		/* Reduce to page number */

/*
 * Trap numbers
 */
#define TRAP_DIVIDE_BY_ZERO 0x00
#define TRAP_DEBUG 0x01
#define TRAP_NMI 0x02
#define TRAP_BREAKPOINT 0x03
#define TRAP_OVERFLOW 0x04
#define TRAP_BOUND 0x05
#define TRAP_INVALID_OPCODE 0x06
#define TRAP_DEVICE_NOT_AVAILABLE 0x07
#define TRAP_DOUBLE_FAULT 0x08
#define TRAP_COPROCESSOR_OVERRUN 0x09
#define TRAP_INVALID_TSS 0x0a
#define TRAP_SEGMENT_NOT_PRESENT 0x0b
#define TRAP_STACK_FAULT 0x0c
#define TRAP_GENERAL_PROTECTION 0x0d
#define TRAP_PAGE_FAULT 0x0e
#define TRAP_FLOATING_POINT_ERROR 0x10
#define TRAP_ALIGNMENT_CHECK 0x11

/*
 * Initial software interrupt vector for ISA IRQs
 */
#define IRQ_BASE 0x20

/*
 * IRQ numbers are calculated with this macro
 */
#define IRQ2TRAP(i) (i + IRQ_BASE)

/*
 * Prototypes.
 */
extern void cpu_init(addr_t descriptors);

/*
 * get_cr0()
 *	Return the value of the processor config register cr0
 */
extern inline u32_t get_cr0(void)
{
	register u32_t res;
	
	asm volatile ("movl %%cr0,%0\n\t"
			: "=r" (res)
			: /* No input */);
	return res;
}

/*
 * set_cr0()
 *	Set the value of the processor config register cr0
 */
extern inline void set_cr0(u32_t addr)
{
	asm volatile ("movl %0,%%cr0\n\t"
			: /* No output */
			: "r" (addr));
}

/*
 * get_cr2()
 *	Get the value of the processor config register cr2 - the fault
 *	address register
 */
extern inline u32_t get_cr2(void)
{
	register u32_t res;
	
	asm volatile ("movl %%cr2,%0\n\t"
			: "=r" (res)
			: /* No input */);
	return res;
}

/*
 * get_cr3()
 *	Get the value of the processor config register cr3 - the L1 page
 *	table pointer
 */
extern inline u32_t get_cr3(void)
{
	register u32_t res;
	
	asm volatile ("movl %%cr3,%0\n\t"
			: "=r" (res)
			: /* No input */);
	return res;
}

/*
 * set_cr3()
 *	Set the value of the processor config register cr3 - the L1 page
 *	table pointer
 */
extern inline void set_cr3(u32_t addr)
{
	asm volatile ("movl %0,%%cr3\n\t"
			: /* No output */
			: "r" (addr));
}

/*
 * get_stack_pointer()
 */
extern inline addr_t get_stack_pointer(void)
{
	addr_t sp;
	
	asm volatile ("movl %%esp, %0\n\t"
			: "=g" (sp)
			: /* No input */);

	return sp;
}
