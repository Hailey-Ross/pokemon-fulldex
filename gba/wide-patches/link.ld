/* Our code goes in the free space at the end of the ROM; game addresses come from the version file. */
MEMORY { free (rx) : ORIGIN = 0x08F00000, LENGTH = 0x10000 }
SECTIONS
{
    .text : { *(.text*) *(.rodata*) *(.glue_7*) *(.vfp11_veneer) *(.v4_bx) } > free
    .data : { *(.data*) } > free
    .bss : { *(.bss*) *(COMMON) } > free
    /DISCARD/ : { *(.ARM.*) *(.comment) }
}
gWide = 0x0203FF00;
