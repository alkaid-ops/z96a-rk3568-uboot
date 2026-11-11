/*
 * Z96A U-Boot RAM chain-loader stub.
 *
 * Purpose: boot a freshly built U-Boot proper from the currently running
 * U-Boot without touching eMMC.  Usage on the U-Boot prompt:
 *
 *   load usb 0:1 0x00800000 <u-boot-fit-dtb.bin>
 *   load usb 0:1 0x10000000 z96a_chainload.bin
 *   go 0x10000000
 *
 * The stub is position independent (only movz/br, no literal pool) and
 * does not need relocation.  It leaves MMU/caches as the old U-Boot set
 * them up: the old page tables identity-map all DRAM, and the new U-Boot
 * rebuilds its own tables before it needs anything else.  If the new
 * image hangs, power-cycle the board and the eMMC U-Boot boots as before.
 *
 * Build:
 *   aarch64-linux-gnu-gcc -c -nostdlib -o z96a_chainload.o z96a_chainload.S
 *   aarch64-linux-gnu-objcopy -O binary z96a_chainload.o z96a_chainload.bin
 */

	.section .text
	.global _start
_start:
	/* U-Boot proper finds its DTB appended at _end, x0 is unused. */
	mov	x0, xzr

	/* 0x00800000 = CONFIG_TEXT_BASE of the mainline U-Boot build. */
	movz	x1, #0x0080, lsl #16
	br	x1
