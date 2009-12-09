/*
 * Linker script used to build the Liquorice kernel image.  We generate an
 * a.out format file here (rather than ELF) as this is easier to handle for
 * the bootstrapping code.  Of course as gcc is configured to build ELF files
 * we can't just tell ld to build an a.out file because there are lots of
 * sections in the ELF output that the a.out format doesn't support.  As a
 * result we need this script to map the various ELF sections to the text,
 * data and bss sections that a.out supports.
 */
OUTPUT_FORMAT("elf32-i386")

OUTPUT_ARCH(i386)

SECTIONS {
	.text 0x1000 : {
		*(.text)
		*(.rodata)
	}
	
	__text_end = . ;

	. = ALIGN(4096);
		
	.data : {
		*(.data)
	}
	
	__data_end = . ;
	
	.bss : {
		*(.bss)
	}

	__bss_end = . ;
	
	/DISCARD/ : {
		*(.comment)
		*(.note)
	}
}
