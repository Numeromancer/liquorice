/*
 * bootconst.h
 *	Boot time constants - memory addresses and characeristics, etc
 */

#define SECTOR_SIZE 0x200	/* Number of bytes in a sector */

#define SETUP_SECS 4		/* Number of setup-sectors */

#define BOOT_SEG 0x07C0		/* Original address of boot-sector */
#define INIT_SEG 0x9000		/* We move boot-sector here */
#define SETUP_SEG 0x9800	/* Setup starts here */
#define SYS_SEG	 0x0100		/* System loaded at 0x00001000 */

#define BASE_RAM 0x000a0000	/* Size of base RAM */

#define BIOSREF_SEG 0x9000	/* BIOS information is placed here at boot */
#define SETUP_OFFS 0x8000	/* Offset of setup data wrt BIOSREFSEG */

#define NBPG 0x1000		/* Number of bytes in a page */
#define NPTPG 0x0400		/* Number of table entries per page */
