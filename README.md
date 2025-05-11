# HalOS
HalOS is a i686 "Operating System" made in C. Runs entirely in ring 0, maybe its badly written but its mine and that whats counts.

## Features
- vesa graphics with 32bit colour and psf2 font
- networking stack support (arp, icmp, ipv4, tcp, telnet)
- drivers
    - rtl8139
    - fat32
    - ata pci mode
- bitmap memory management (physical and virtual memory managers, paging)
- games
    - pong
    - snake
- shell to interact with all of that