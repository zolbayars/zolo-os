# ZoloOS

A hobby x86 operating system built from scratch for learning OS internals.

## Features

- **VGA text mode** (80x25 color terminal)
- **GDT/IDT/IRQ** — full interrupt infrastructure with PIC remapping
- **PIT timer** at 100 Hz
- **PS/2 keyboard driver** with scan code translation
- **Physical memory manager** — bitmap frame allocator
- **Paging** — two-level x86 identity mapping (16 MiB + dynamic regions)
- **Kernel heap** — bump allocator (64 KiB)
- **Preemptive multitasking** — round-robin scheduler with context switching
- **Interactive shell** — built-in commands: `help`, `clear`, `echo`, `meminfo`, `uptime`, `tasks`, `reboot`
- **VESA framebuffer** — 1024x768x32 graphics mode with window manager (requires GRUB boot)

## Prerequisites

Install the required packages (macOS with Homebrew):

```bash
arch -arm64 brew install nasm llvm lld qemu
```

> **Note:** On Apple Silicon Macs, use `arch -arm64 brew install` to avoid Rosetta issues.

## Build

```bash
make
```

## Run

```bash
make run
```

This launches QEMU with the kernel in VGA text mode. You'll see the boot sequence and an interactive shell.

## Debug

```bash
make debug
```

Runs QEMU with the monitor on stdio. Type `info registers` to inspect CPU state, `quit` to exit.

## Clean

```bash
make clean
```

## Project Structure

```
boot/         Assembly files (multiboot header, GDT/IDT stubs, context switch)
kernel/       C kernel sources (drivers, memory, scheduler, shell)
include/      Shared headers (multiboot structs, type definitions)
linker.ld     Linker script (kernel loaded at 1 MiB)
Makefile      Cross-compilation build system
```
