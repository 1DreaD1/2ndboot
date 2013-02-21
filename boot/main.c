#include "types.h"
#include "stdio.h"
#include "error.h"
#include "atag.h"
#include "common.h"
#include "memory.h"
#include "hw_misc.h"
#include "images.h"
#include "mbm.h"

#define ARCH_NUMBER 2241

/* OMAP Registers */

#define WDTIMER2_BASE						0x48314000

void critical_error(error_t err)
{
	printf("Critical error %d\n", (int)err);
	delay(1000);

	/* DPLL reset is the recommended way to restart the SoC 
		- set RST_DPLL3 bit of PRM_RSTCTRL register */
	write32(0x4, 0x48307250);

	while (1);
}

void __attribute__((__naked__)) l2cache_enable(void)
{
	/* We have ES3.1 OMAP3430 on the phones at least, so we can set it directly */
	__asm__ volatile 
	(
		"stmfd sp!, {r4,lr}\n"
		"mrc p15, 0, r4, c1, c0, 1\n"
		"orr r4, r4, #0x2\n"
		"mcr p15, 0, r4, c1, c0, 1\n"
		"ldmfd sp!, {r4,pc}\n"
	);
}

void __attribute__((__naked__)) enter_kernel(int zero, int arch, void *atags, int kern_addr) 
{
	__asm__ volatile (
		"bx r3\n"
	);
}

void board_init()
{
	int i;
	
	/* 
	 * This is probably not mandatory, but just in case setup
	 * them to state in which they exited MBM
	 */
	
	/* Stop & Reset GPTIMER1 */
	modify_register32(0x48318000 + 0x24, 0x1, 0x0);
	
	write32(1, 0x48318000 + 0x10);
	while ((read32(0x48318000 + 0x14) & 1) != 1);
	
	/* 
	 * MBM boots OMAP3430 with 500 MHz. 
	 */
	
	/* Unlock PLL */ 
	modify_register32(0x48004904, 0x7, 0x5);
	while ((read32(0x48004924) & 1) != 0);
	
	/* Set the dividers */
	modify_register32(0x48004944, 0x1F, 0x1);
	modify_register32(0x48004940, 0x07 << 18, 0x01 << 18);
	modify_register32(0x48004940, 0x7FF << 8, 0xFA << 8);
	modify_register32(0x48004940, 0x7F, 0x0C);
	
	/* Set frequency */
	modify_register32(0x48004904, 0xF << 4, 0x7 << 4);
	
	/* Relock PLL */
	i = 0;
	
	modify_register32(0x48004904, 0x7, 0x7);
	while ((read32(0x48004924) & 1) != 1)
	{
		i++;
		if (i > 0x2000)
		{
			/* Kick it again */
			i = 0;
			modify_register32(0x48004904, 0x7, 0x7);
		}
	}

	/* Unlock DPPL5 */
	modify_register32(0x48004D04, 0x7, 0x1);
	while((read32(0x48004D24) & 1) != 0);
}

int main() 
{
	void* atags;
	struct memory_image image;
	
	printf("=== HBOOT START ===\n");

	/* Complete images */
	if (image_complete() != NULL)
	{
		printf("CRC check failed.\n");
		critical_error(IMG_NOT_PROVIDED);
	}

	image_dump_stats();
	write32(2, 0x48200010);
  	while(!(read32(0x48200014)&1));
	
	if (image_find(IMG_LINUX, &image) != NULL)
	{
		printf("KERNEL FOUND!\n");
		atags = atag_build();

		/* Reset MPU Interrupt controller */
		write32(0x2, 0x48200000 + 0x10);
		while (!(read32(0x48200000 + 0x14) & 0x1));

		/* Reinit board */
		board_init();
		delay(500);
		printf("Board initialized.\n");

		/* Disable Watchdog (seqeunce taken from MBM) */
		write32(0xAAAA, WDTIMER2_BASE + 0x48);
		delay(500);
		
		write32(0x5555, WDTIMER2_BASE + 0x48);
		delay(500);
		
		printf("Watchdog disabled.\n");
		
		printf("BOOTING KERNEL!\n");
		l2cache_enable();
		delay(500);
		enter_kernel(0, ARCH_NUMBER, atags, KERNEL_DEST);
	}
	else 
		critical_error(IMG_NOT_PROVIDED);

  return 0;
}
