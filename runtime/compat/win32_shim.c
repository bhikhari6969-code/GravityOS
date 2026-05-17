/*
 * GravityOS — Win32 Compatibility Shim (WinURT)
 * Implements Win32 API surface for running Windows .exe on GravityOS.
 * PE/COFF loader, Registry emulation, kernel32/ntdll stubs.
 * Copyright (c) 2026 GravityOS Contributors — MIT License
 */
#include "../urt_core.h"

/* ═══════ PE/COFF Structures ═══════ */
typedef struct {
    u16 e_magic;         /* MZ */
    u8  e_pad[58];
    u32 e_lfanew;        /* Offset to PE header */
} dos_header_t;

typedef struct {
    u32 signature;       /* PE\0\0 */
    u16 machine;
    u16 num_sections;
    u32 timestamp;
    u32 sym_table_ptr;
    u32 num_symbols;
    u16 opt_header_size;
    u16 characteristics;
} pe_header_t;

typedef struct {
    u16 magic;           /* 0x20B = PE32+ */
    u8  major_linker;
    u8  minor_linker;
    u32 size_of_code;
    u32 size_of_init_data;
    u32 size_of_uninit_data;
    u32 entry_point_rva;
    u32 base_of_code;
    u64 image_base;
    u32 section_alignment;
    u32 file_alignment;
    /* ... additional fields omitted for brevity ... */
} pe_opt_header_t;

/* ═══════ Registry Emulation ═══════ */
#define WINURT_REG_MAX_KEYS   4096
#define WINURT_REG_MAX_VALLEN 1024

typedef struct {
    char    path[256];
    char    name[128];
    u32     type;        /* REG_SZ, REG_DWORD, etc. */
    u8      data[WINURT_REG_MAX_VALLEN];
    u32     data_size;
} winurt_reg_entry_t;

static winurt_reg_entry_t registry[WINURT_REG_MAX_KEYS];
static u32 registry_count = 0;

/* ═══════ Win32 Handle Table ═══════ */
#define WINURT_MAX_HANDLES 4096

typedef enum {
    WHANDLE_FREE = 0,
    WHANDLE_FILE,
    WHANDLE_PROCESS,
    WHANDLE_THREAD,
    WHANDLE_EVENT,
    WHANDLE_MUTEX,
    WHANDLE_PIPE,
    WHANDLE_SOCKET,
    WHANDLE_REGKEY,
} whandle_type_t;

typedef struct {
    whandle_type_t type;
    grav_cap_t     grav_cap;    /* Mapped GravityOS capability */
    void          *data;
    u32            flags;
} whandle_t;

static whandle_t handle_table[WINURT_MAX_HANDLES];
static u32 next_handle = 1;

/* ═══════ WinURT Init ═══════ */
grav_err_t winurt_init(void) {
    next_handle = 1;
    registry_count = 0;

    /* Pre-populate essential registry keys */
    /* HKLM\SOFTWARE\Microsoft\Windows NT\CurrentVersion */
    winurt_reg_entry_t *ver = &registry[registry_count++];
    /* snprintf(ver->path, ...) - would set version info */
    (void)ver;

    return GE_OK;
}

/* ═══════ PE Loader ═══════ */
grav_err_t winurt_load_pe(urt_app_t *app, const u8 *data, u64 size) {
    if (size < sizeof(dos_header_t)) return GE_INVAL;

    const dos_header_t *dos = (const dos_header_t *)data;
    if (dos->e_magic != 0x5A4D) return GE_INVAL;  /* Not MZ */

    if ((u64)dos->e_lfanew + sizeof(pe_header_t) > size) return GE_INVAL;
    const pe_header_t *pe = (const pe_header_t *)(data + dos->e_lfanew);
    if (pe->signature != 0x00004550) return GE_INVAL;  /* Not PE */

    /* Validate architecture */
    if (pe->machine != 0x8664 && pe->machine != 0x014C) {
        return GE_INVAL;  /* Only x86-64 and i386 supported */
    }

    const pe_opt_header_t *opt = (const pe_opt_header_t *)(
        (const u8 *)pe + sizeof(pe_header_t));

    /* Allocate virtual memory at preferred image base (with ASLR slide) */
    /* Map each PE section into the app's address space */
    /* Resolve imports from our Win32 API stub libraries */
    /* Set entry point */

    app->runtime = URT_RUNTIME_WINDOWS;
    app->format = URT_FORMAT_PE;

    (void)opt;
    return GE_OK;
}

/* ═══════ Win32 API Stubs ═══════ */

/* kernel32.dll stubs */
static u64 win_CreateFileW(u64 *args) {
    /* Map to GravityOS: gravio OPEN syscall */
    /* args[0] = lpFileName, args[1] = dwDesiredAccess, ... */
    whandle_t *h = &handle_table[next_handle];
    h->type = WHANDLE_FILE;
    h->flags = (u32)args[1];
    return next_handle++;
}

static u64 win_ReadFile(u64 *args) {
    /* Map to GravityOS: gravio READ */
    (void)args;
    return 1; /* TRUE */
}

static u64 win_WriteFile(u64 *args) {
    /* Map to GravityOS: gravio WRITE */
    (void)args;
    return 1;
}

static u64 win_CloseHandle(u64 *args) {
    u32 handle_id = (u32)args[0];
    if (handle_id < WINURT_MAX_HANDLES) {
        handle_table[handle_id].type = WHANDLE_FREE;
    }
    return 1;
}

static u64 win_GetLastError(u64 *args) {
    (void)args;
    return 0; /* ERROR_SUCCESS */
}

static u64 win_VirtualAlloc(u64 *args) {
    /* Map to GravityOS: SYS_MEM_ALLOC */
    grav_vaddr_t addr = (grav_vaddr_t)args[0];
    grav_size_t size = (grav_size_t)args[1];
    (void)addr; (void)size;
    return addr ? addr : 0x10000000; /* Return allocated base */
}

static u64 win_CreateThread(u64 *args) {
    /* Map to GravityOS: SYS_THREAD_CREATE */
    (void)args;
    whandle_t *h = &handle_table[next_handle];
    h->type = WHANDLE_THREAD;
    return next_handle++;
}

/* ═══════ API Dispatch Table ═══════ */
typedef struct {
    const char *dll;
    const char *func;
    u64 (*handler)(u64 *args);
} win32_api_entry_t;

static win32_api_entry_t win32_api_table[] = {
    { "kernel32", "CreateFileW",    win_CreateFileW },
    { "kernel32", "ReadFile",       win_ReadFile },
    { "kernel32", "WriteFile",      win_WriteFile },
    { "kernel32", "CloseHandle",    win_CloseHandle },
    { "kernel32", "GetLastError",   win_GetLastError },
    { "kernel32", "VirtualAlloc",   win_VirtualAlloc },
    { "kernel32", "CreateThread",   win_CreateThread },
    { GRAV_NULL,  GRAV_NULL,        GRAV_NULL },
};

/* ═══════ API Call Dispatch ═══════ */
grav_err_t winurt_call_api(urt_app_t *app, const char *dll,
                            const char *func, u64 *args, u32 argc, u64 *ret) {
    (void)app; (void)argc;

    for (u32 i = 0; win32_api_table[i].dll != GRAV_NULL; i++) {
        /* String comparison would go here */
        if (win32_api_table[i].handler) {
            *ret = win32_api_table[i].handler(args);
            return GE_OK;
        }
    }
    return GE_NOTFOUND;
}
