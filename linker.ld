ENTRY(_start);  /* <-- Add semicolon here */
SECTIONS {
    . = 0x8048000; /* Base address */
    .text : {
        *(.text);
        . = ALIGN(4096); /* Ensure page alignment */
    }
    /DISCARD/ : { *(*) }
}
