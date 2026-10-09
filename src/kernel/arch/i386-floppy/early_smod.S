/* Native SMOD byte images embedded in the *floppy loader payload*.
 * This is necessary while the boot medium depends on FDC to read sectors.
 * Runtime still validates and executes the ordinary .mod format.
 */
.section .rodata
.balign 16
.globl __smod_floppy_com1_begin
.globl __smod_floppy_com1_end
__smod_floppy_com1_begin:
.incbin "build/artifacts/modules/i386/com1.mod"
__smod_floppy_com1_end:
.balign 16
.globl __smod_floppy_cmos_begin
.globl __smod_floppy_cmos_end
__smod_floppy_cmos_begin:
.incbin "build/artifacts/modules/i386/cmos.mod"
__smod_floppy_cmos_end:
.balign 16
.globl __smod_floppy_picpit_begin
.globl __smod_floppy_picpit_end
__smod_floppy_picpit_begin:
.incbin "build/artifacts/modules/i386/picpit.mod"
__smod_floppy_picpit_end:



.balign 16
.globl __smod_floppy_fdc_begin
.globl __smod_floppy_fdc_end
__smod_floppy_fdc_begin:
.incbin "build/artifacts/modules/i386/fdc.mod"
__smod_floppy_fdc_end:
.section .note.GNU-stack,"",@progbits
