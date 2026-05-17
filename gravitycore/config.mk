# GravityCore Kernel Build Configuration
# Usage: make ARCH=x86_64 DEBUG=1

ARCH        ?= x86_64
DEBUG       ?= 0
KASLR       ?= 1
KASAN       ?= 0
OPTLEVEL    ?= -O2

# Toolchain — clang + lld (no GNU dependency)
CC          := clang
AS          := clang
LD          := ld.lld
OBJCOPY     := llvm-objcopy
OBJDUMP     := llvm-objdump

# Target triples
ifeq ($(ARCH),x86_64)
    TARGET  := x86_64-unknown-none-elf
    ASFLAGS := -m64
endif
ifeq ($(ARCH),arm64)
    TARGET  := aarch64-unknown-none-elf
    ASFLAGS :=
endif
ifeq ($(ARCH),riscv64)
    TARGET  := riscv64-unknown-none-elf
    ASFLAGS := -march=rv64gc
endif

# Flags
CFLAGS  := --target=$(TARGET) $(OPTLEVEL) -ffreestanding -fno-builtin \
           -fno-stack-protector -mno-red-zone -nostdlib -nostdinc \
           -Wall -Wextra -Werror -std=c17 \
           -fno-exceptions -fno-rtti \
           -DGRAV_ARCH_$(ARCH) -DGRAV_VERSION=\"0.1.0\"

ifeq ($(DEBUG),1)
    CFLAGS += -g -DGRAV_DEBUG=1
endif
ifeq ($(KASLR),1)
    CFLAGS += -DGRAV_KASLR=1 -fPIE
endif
ifeq ($(KASAN),1)
    CFLAGS += -fsanitize=kernel-address -DGRAV_KASAN=1
endif

LDFLAGS := -T linker.ld --gc-sections -z noexecstack

# Directories
INCLUDE := -Iinclude -I.
