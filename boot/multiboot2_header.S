.section .multiboot2
.align 8

.set MB2_MAGIC, 0xE85250D6
.set MB2_ARCH,  0

.long MB2_MAGIC
.long MB2_ARCH
.long mb2_end - mb2_start
.long -(MB2_MAGIC + MB2_ARCH + (mb2_end - mb2_start))

mb2_start:
/* End tag */
.short 0
.short 0
.long 8
mb2_end:
