# GravityCore Kernel — Top-Level Makefile
# Usage: make ARCH=x86_64 [DEBUG=1] [KASLR=1] [KASAN=1]
include config.mk

# ═══════ Source Files ═══════
ARCH_SRCS_x86_64 := \
    arch/x86_64/boot.S \
    arch/x86_64/syscall_entry.S

ARCH_SRCS_arm64 := \
    arch/arm64/entry.S

ARCH_SRCS_riscv64 := \
    arch/riscv64/entry.S

KERNEL_SRCS := \
    kernel/init/main.c \
    kernel/init/panic.c \
    kernel/sched/sched.c \
    kernel/sched/runqueue.c \
    kernel/sched/cfs.c \
    kernel/sched/rt.c \
    kernel/mm/vmm.c \
    kernel/mm/pmm.c \
    kernel/mm/slab.c \
    kernel/mm/page_fault.c \
    kernel/ipc/ipc.c \
    kernel/ipc/endpoint.c \
    kernel/ipc/cap_ipc.c \
    kernel/cap/cap.c \
    kernel/cap/cap_alloc.c \
    kernel/cap/cap_derive.c \
    kernel/irq/irq.c \
    kernel/irq/softirq.c

SYSCALL_SRCS := \
    syscall/syscall_table.c \
    syscall/syscall_dispatch.c \
    syscall/sc_mem.c \
    syscall/sc_ipc.c \
    syscall/sc_cap.c \
    syscall/sc_thread.c \
    syscall/sc_io.c \
    syscall/sc_time.c

VFS_SRCS := \
    vfs/vfs.c \
    vfs/dentry.c \
    vfs/inode.c \
    vfs/mount.c

DRIVER_SRCS := \
    drivers/uart/uart_early.c \
    drivers/timer/hpet.c

# ═══════ Build Targets ═══════
ALL_SRCS := $(ARCH_SRCS_$(ARCH)) $(KERNEL_SRCS) $(SYSCALL_SRCS) $(VFS_SRCS) $(DRIVER_SRCS)
OBJS := $(patsubst %.c,build/$(ARCH)/%.o,$(patsubst %.S,build/$(ARCH)/%.o,$(ALL_SRCS)))
KERNEL_BIN := build/$(ARCH)/gravitycore.elf
KERNEL_IMG := build/$(ARCH)/gravitycore.img

.PHONY: all clean

all: $(KERNEL_BIN)
	@echo "╔═══════════════════════════════════════╗"
	@echo "║  GravityCore built for $(ARCH)        ║"
	@echo "╚═══════════════════════════════════════╝"
	@$(OBJDUMP) -h $(KERNEL_BIN) | head -20

$(KERNEL_BIN): $(OBJS)
	@mkdir -p $(dir $@)
	$(LD) $(LDFLAGS) -o $@ $^
	@echo "[LD] $@"

build/$(ARCH)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INCLUDE) -c $< -o $@
	@echo "[CC] $<"

build/$(ARCH)/%.o: %.S
	@mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) $(INCLUDE) -c $< -o $@
	@echo "[AS] $<"

clean:
	rm -rf build/

# ═══════ QEMU Targets ═══════
qemu-x86: $(KERNEL_BIN)
	qemu-system-x86_64 -kernel $(KERNEL_BIN) -m 512M -serial stdio \
		-no-reboot -no-shutdown -d int,cpu_reset

qemu-arm: $(KERNEL_BIN)
	qemu-system-aarch64 -M virt -cpu cortex-a72 -kernel $(KERNEL_BIN) \
		-m 512M -serial stdio -nographic

qemu-riscv: $(KERNEL_BIN)
	qemu-system-riscv64 -M virt -kernel $(KERNEL_BIN) \
		-m 512M -serial stdio -nographic
