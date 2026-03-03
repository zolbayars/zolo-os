# =============================================================================
# ZoloOS Makefile
# =============================================================================
#
# Cross-compilation setup for building an x86 (i686) OS on Apple Silicon Mac.
#
# Why we can't just use `gcc` or `cc`:
#   Our Mac runs ARM64 (Apple Silicon). We need to produce x86 32-bit ELF
#   binaries. This requires a cross-compiler. Luckily, clang/LLVM is a
#   cross-compiler by default - we just pass --target=i686-none-elf.
#
# Why we use Homebrew's LLVM instead of Apple's clang:
#   Apple's clang works for compiling, but the macOS system linker (`ld`)
#   can only produce Mach-O binaries (the macOS format). We need ELF
#   binaries (the format x86 PCs and QEMU expect). Homebrew's LLVM
#   includes `ld.lld`, a linker that can produce ELF.
#
# Build flow:
#   .asm files --[nasm]--> .o (object files)
#   .c files   --[clang]--> .o (object files)
#   all .o files --[ld.lld + linker.ld]--> kernel.elf
#   kernel.elf --[qemu -kernel]--> running OS
#

# ---------------------------------------------------------------------------
# Toolchain paths
# ---------------------------------------------------------------------------
LLVM_PREFIX = /opt/homebrew/opt/llvm/bin
CC          = $(LLVM_PREFIX)/clang
LD          = $(LLVM_PREFIX)/ld.lld
ASM         = nasm
QEMU        = qemu-system-i386

# ---------------------------------------------------------------------------
# Compiler flags (explained)
# ---------------------------------------------------------------------------
# --target=i686-none-elf  : Cross-compile to 32-bit x86, no OS, ELF format
# -ffreestanding          : Don't assume a hosted environment (no libc, no main)
# -fno-builtin            : Don't replace loops with calls to memset/memcpy
#                           (those functions don't exist yet - WE have to write them)
# -fno-stack-protector    : Don't insert stack canary checks (the __stack_chk_fail
#                           function doesn't exist in our kernel)
# -nostdlib               : Don't link against any standard library
# -Wall -Wextra           : Enable warnings - very helpful for catching bugs
# -O1                     : Light optimization. -O0 can need stack protector stubs,
#                           -O2 can optimize away things we're debugging
# -g                      : Include debug symbols (for GDB debugging via QEMU)
# -std=c11                : Use C11 standard
# -m32                    : Generate 32-bit code (redundant with target, but explicit)
#
CFLAGS = --target=i686-none-elf \
         -ffreestanding \
         -fno-builtin \
         -fno-stack-protector \
         -nostdlib \
         -Wall -Wextra \
         -O1 -g \
         -std=c11 -m32 \
         -Iinclude -Ikernel

# NASM flags: produce 32-bit ELF object files
ASFLAGS = -f elf32

# Linker flags: use our linker script, no standard libs, no dynamic linker
LDFLAGS = -T linker.ld -nostdlib --no-dynamic-linker

# ---------------------------------------------------------------------------
# Source files and object files
# ---------------------------------------------------------------------------
BUILD_DIR = build

# Find all source files
ASM_SRCS = $(wildcard boot/*.asm)
C_SRCS   = $(wildcard kernel/*.c)

# Convert source paths to object file paths in build/
ASM_OBJS = $(patsubst boot/%.asm,$(BUILD_DIR)/%.o,$(ASM_SRCS))
C_OBJS   = $(patsubst kernel/%.c,$(BUILD_DIR)/%.o,$(C_SRCS))
ALL_OBJS = $(ASM_OBJS) $(C_OBJS)

# The final kernel binary
KERNEL = $(BUILD_DIR)/kernel.elf

# ---------------------------------------------------------------------------
# Targets
# ---------------------------------------------------------------------------

# Default: build the kernel
all: $(KERNEL)

# Link all object files into the kernel ELF binary
$(KERNEL): $(ALL_OBJS)
	$(LD) $(LDFLAGS) -o $@ $^

# Compile assembly files
$(BUILD_DIR)/%.o: boot/%.asm | $(BUILD_DIR)
	$(ASM) $(ASFLAGS) $< -o $@

# Compile C files
$(BUILD_DIR)/%.o: kernel/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Create build directory if it doesn't exist
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Run in QEMU
# -kernel: Boot an ELF binary directly (QEMU has built-in multiboot support)
# -m 128M: Give the VM 128 MiB of RAM
run: $(KERNEL)
	$(QEMU) -kernel $< -m 128M

# Run with QEMU monitor on stdio (for debugging)
# Type 'info registers' in the terminal to see CPU state
# Type 'quit' to exit
debug: $(KERNEL)
	$(QEMU) -kernel $< -m 128M -monitor stdio -d int,cpu_reset 2>/dev/null

# Clean build artifacts
clean:
	rm -rf $(BUILD_DIR)/*

# Phony targets (not actual files)
.PHONY: all run debug clean
