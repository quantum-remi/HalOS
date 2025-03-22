# HAL Operating System

A minimalist, experimental operating system built from scratch with a focus on learning and education.

## Overview

HAL OS is a custom operating system that implements core OS functionality from the ground up. It features a basic shell interface, memory management, and various demo applications to showcase different aspects of operating system development.

## Features

### Core Features

- Custom bootloader with multiboot support
- Protected mode CPU initialization
- Interrupt handling and IDT setup
- Physical and virtual memory management
- kernel heap allocation
- Simple device drivers (keyboard, timer, etc.)

### User Interface

- Text-mode console with basic shell
- Command history and input handling
- VESA graphics support with demo applications
- Interactive commands and utilities

### Demo Applications

- Snake game implementation
- VESA graphics demonstrations
- System information utilities
- Memory testing tools
- Hardware detection

### Hardware Support

- x86 architecture support
- PCI device enumeration
- PS/2 keyboard support
- Basic IDE drive support
- VESA graphics modes

## Building

### Prerequisites

- GCC cross-compiler for i686-elf
- NASM assembler
- GNU Make
- QEMU or VirtualBox for testing

### Build Instructions

1. Clone the repository
2. Install the required tools
3. Run `make` in the project root
4. Use `make dev` to test in QEMU

## Running

The system can be run in:

- QEMU (recommended for development)
- VirtualBox
- Real hardware (limited testing)

## Contributing

Contributions are welcome! Please feel free to:

- Report bugs and issues
- Submit pull requests
- Suggest new features
- Improve documentation

## License

This project is open source and available under BSD license.
