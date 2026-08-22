ENTRY(Init)

MEMORY
{
  DEV_INFO (r) : ORIGIN = 0x00000000, LENGTH = 1K
  RAM (xrw) : ORIGIN = 0x20000004, LENGTH = 128K - 4
}

_estack = ORIGIN(RAM) + LENGTH(RAM);

PHDRS
{
  loader PT_LOAD FLAGS(7); /* Read + write + execute, matching ST loaders. */
  device_info PT_LOAD FLAGS(4); /* Read only. */
}

SECTIONS
{
  .text :
  {
    . = ALIGN(4);
    KEEP(*(.text.Init))
    KEEP(*(.text.Write))
    KEEP(*(.text.SectorErase))
    KEEP(*(.text.MassErase))
    KEEP(*(.text.CheckSum))
    KEEP(*(.text.Verify))
    *(.text .text.*)
    *(.rodata .rodata.*)
    . = ALIGN(4);
  } > RAM :loader

  .data :
  {
    . = ALIGN(4);
    *(.data .data.*)
    . = ALIGN(4);
  } > RAM :loader

  .bss (NOLOAD) :
  {
    . = ALIGN(4);
    *(.bss .bss.* COMMON)
    . = ALIGN(8);
    _loader_end = .;
  } > RAM :loader

  ASSERT(_loader_end <= ORIGIN(RAM) + LENGTH(RAM),
         "External loader exceeds DTCM")
  /* CubeProgrammer currently places its first write buffer at relocated
     address 0x24002260.  Keep loader code/state below the matching offset. */
  ASSERT(_loader_end <= ORIGIN(RAM) + 0x225C,
         "External loader overlaps CubeProgrammer write buffer")

  .Dev_Info :
  {
    . = ALIGN(4);
    KEEP(*(.Dev_Info))
    . = ALIGN(4);
  } > DEV_INFO :device_info

  /DISCARD/ :
  {
    *(.comment)
    *(.note*)
    *(.ARM.exidx*)
    *(.ARM.extab*)
    *(.ARM.attributes)
  }
}
