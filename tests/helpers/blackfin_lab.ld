ENTRY(_start)
SECTIONS { . = 0x1000; .text : { *(.text*) } . = ALIGN(16); .rodata : { *(.rodata*) } .data : { *(.data*) } . = ALIGN(16); .bss : { *(.bss*) *(COMMON) } }
