/*
 * GravityOS — Universal Runtime Technology (URT)
 * urt_core.h — Master header for the cross-platform app compatibility layer
 *
 * URT allows GravityOS to run Windows, macOS, Linux, and Android apps
 * by combining binary translation (GravBT), OS compatibility shims,
 * and lightweight namespace sandboxing (GravContainer).
 *
 * Copyright (c) 2026 GravityOS Contributors — MIT License
 */
#ifndef _URT_CORE_H
#define _URT_CORE_H

#include <gravity/kernel.h>

/* ═══════ URT Runtime Types ═══════ */
typedef enum {
    URT_RUNTIME_NATIVE     = 0,  /* .grav native app */
    URT_RUNTIME_LINUX      = 1,  /* ELF, Linux syscalls */
    URT_RUNTIME_WINDOWS    = 2,  /* PE/COFF, Win32/WinRT */
    URT_RUNTIME_MACOS      = 3,  /* Mach-O, Darwin syscalls */
    URT_RUNTIME_ANDROID    = 4,  /* APK, ART/Dalvik */
    URT_RUNTIME_WASM       = 5,  /* WebAssembly (future) */
} urt_runtime_t;

/* Binary format detection */
typedef enum {
    URT_FORMAT_ELF         = 0x7F454C46,  /* \x7FELF */
    URT_FORMAT_PE          = 0x5A4D,      /* MZ */
    URT_FORMAT_MACHO       = 0xFEEDFACF,  /* Mach-O 64 */
    URT_FORMAT_DEX         = 0x6465780A,  /* dex\n */
    URT_FORMAT_GRAV        = 0x47525641,  /* GRVA */
} urt_format_t;

/* ═══════ URT App Context ═══════ */
typedef struct {
    urt_runtime_t   runtime;
    urt_format_t    format;
    grav_pid_t      pid;
    grav_cap_t      sandbox_cap;
    char            name[128];
    char            path[256];

    /* Binary translation state */
    void           *bt_context;
    u64             bt_cache_hits;
    u64             bt_cache_misses;

    /* Compatibility layer state */
    void           *compat_state;

    /* Sandbox (GravContainer) */
    u32             container_id;
    grav_vaddr_t    vfs_root;      /* Private VFS mount namespace */
    grav_cap_t      net_cap;       /* Network capability (may be null) */
    grav_cap_t      display_cap;
    grav_cap_t      audio_cap;
} urt_app_t;

/* ═══════ URT API ═══════ */

/* Core lifecycle */
grav_err_t urt_init(void);
grav_err_t urt_detect_format(const u8 *header, u32 size, urt_format_t *out);
grav_err_t urt_load_app(const char *path, urt_app_t **out);
grav_err_t urt_exec_app(urt_app_t *app, int argc, const char **argv);
grav_err_t urt_kill_app(urt_app_t *app);

/* ═══════ GravBT — Binary Translation (LLVM IR) ═══════ */
typedef struct {
    grav_vaddr_t  source_pc;     /* Original instruction address */
    grav_vaddr_t  target_pc;     /* Translated code address */
    u32           size;          /* Translated block size */
    u32           exec_count;    /* Hot-path tracking */
} bt_cache_entry_t;

grav_err_t  gravbt_init(void);
grav_err_t  gravbt_translate_block(urt_app_t *app, grav_vaddr_t pc);
void       *gravbt_lookup_cache(urt_app_t *app, grav_vaddr_t pc);
grav_err_t  gravbt_flush_cache(urt_app_t *app);

/* ═══════ Compatibility Shims ═══════ */

/* LinuxURT — native ELF, full Linux syscall compat */
grav_err_t linuxurt_init(void);
grav_err_t linuxurt_load_elf(urt_app_t *app, const u8 *data, u64 size);
grav_err_t linuxurt_syscall(urt_app_t *app, u64 nr, u64 a0, u64 a1,
                             u64 a2, u64 a3, u64 a4, u64 a5, i64 *ret);

/* WinURT — PE/COFF, Win32 API surface */
grav_err_t winurt_init(void);
grav_err_t winurt_load_pe(urt_app_t *app, const u8 *data, u64 size);
grav_err_t winurt_call_api(urt_app_t *app, const char *dll, 
                            const char *func, u64 *args, u32 argc, u64 *ret);

/* MacURT — Mach-O, Obj-C runtime, Core Foundation */
grav_err_t macurt_init(void);
grav_err_t macurt_load_macho(urt_app_t *app, const u8 *data, u64 size);
grav_err_t macurt_mach_trap(urt_app_t *app, u32 trap_num, u64 *args, u64 *ret);

/* DroidURT — Android ART, APK loading */
grav_err_t droidurt_init(void);
grav_err_t droidurt_load_apk(urt_app_t *app, const char *apk_path);
grav_err_t droidurt_start_activity(urt_app_t *app, const char *class_name);

/* ═══════ GravContainer — Lightweight Sandbox ═══════ */
typedef struct {
    u32           id;
    grav_pid_t    root_pid;
    grav_vaddr_t  vfs_root;
    grav_cap_t    caps[64];
    u32           cap_count;
    u32           flags;
} grav_container_t;

grav_err_t grav_container_create(urt_runtime_t type, grav_container_t **out);
grav_err_t grav_container_destroy(grav_container_t *c);
grav_err_t grav_container_add_cap(grav_container_t *c, grav_cap_t cap);

#endif /* _URT_CORE_H */
