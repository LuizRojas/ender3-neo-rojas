/* Bootloader: 28 KB em 0x08000000. A aplicacao comeca em 0x08007000. */
MEMORY
{
  FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 28K
  RAM   (rwx) : ORIGIN = 0x20000000, LENGTH = 48K
}
_estack = ORIGIN(RAM) + LENGTH(RAM);

SECTIONS
{
  .isr_vector : { KEEP(*(.isr_vector)) } > FLASH
  .text : { *(.text*) *(.rodata*) . = ALIGN(4); } > FLASH
  _sidata = LOADADDR(.data);
  .data : { _sdata = .; *(.data*) . = ALIGN(4); _edata = .; } > RAM AT > FLASH
  .bss  : { _sbss = .; *(.bss*) *(COMMON) . = ALIGN(4); _ebss = .; } > RAM
  /DISCARD/ : { *(.ARM.exidx*) *(.comment) }
}
