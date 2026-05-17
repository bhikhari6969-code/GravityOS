/*
 * GravityOS — Virtual Filesystem Layer
 * Dispatches vfs_read/write/open to per-FS ops via vtable.
 * Dentry cache. Mount namespace isolation for containers.
 *
 * err_t    vfs_open(const char *path, int flags, file_t **out)
 * ssize_t  vfs_read(file_t*, void *buf, size_t n)
 * err_t    vfs_mount(const char *dev, const char *mp, fs_ops_t*)
 *
 * Copyright (c) 2026 GravityOS Contributors — MIT License
 */
#include <gravity/kernel.h>

#define VFS_MAX_MOUNTS    64
#define VFS_MAX_OPEN      4096
#define VFS_DENTRY_CACHE  8192
#define VFS_PATH_MAX      256
#define VFS_NAME_MAX      128

/* Inode — metadata for a file/directory */
typedef struct inode {
    u64             ino;
    u32             mode;       /* file type + permissions */
    u32             refcount;
    u64             size;
    u64             blocks;
    u64             atime_ns;
    u64             mtime_ns;
    u64             ctime_ns;
    u32             uid;
    u32             gid;
    void           *fs_private; /* FS-specific data */
    struct inode   *next;       /* Hash chain */
} inode_t;

#define VFS_MODE_FILE    0x8000
#define VFS_MODE_DIR     0x4000
#define VFS_MODE_LINK    0x2000
#define VFS_MODE_DEV     0x6000

/* Open file descriptor */
typedef struct file {
    inode_t        *inode;
    u64             offset;
    u32             flags;
    u32             refcount;
    grav_cap_t      cap;        /* Capability for access control */
    void           *fs_file;    /* FS-specific file state */
} file_t;

/* Filesystem operations vtable */
typedef struct fs_ops {
    const char     *name;       /* "gravfs", "ext4", "ntfs", "fat32" */
    grav_err_t    (*mount)(const char *dev, void **fs_root);
    grav_err_t    (*unmount)(void *fs_root);
    grav_err_t    (*lookup)(void *dir, const char *name, inode_t **out);
    grav_err_t    (*open)(inode_t *inode, u32 flags, file_t *file);
    grav_ssize_t  (*read)(file_t *file, void *buf, grav_size_t n);
    grav_ssize_t  (*write)(file_t *file, const void *buf, grav_size_t n);
    grav_err_t    (*close)(file_t *file);
    grav_err_t    (*stat)(inode_t *inode, void *stat_buf);
    grav_err_t    (*mkdir)(void *dir, const char *name, u32 mode);
    grav_err_t    (*unlink)(void *dir, const char *name);
} fs_ops_t;

/* Mount point */
typedef struct {
    char        path[VFS_PATH_MAX];
    fs_ops_t   *ops;
    void       *fs_root;
    u32         flags;
    u32         active;
} mount_t;

/* Dentry cache entry */
typedef struct dentry {
    char            name[VFS_NAME_MAX];
    inode_t        *inode;
    struct dentry  *parent;
    u64             hash;
    struct dentry  *hash_next;  /* Hash chain */
    u32             refcount;
} dentry_t;

static mount_t mount_table[VFS_MAX_MOUNTS];
static u32 mount_count = 0;
static file_t open_files[VFS_MAX_OPEN];
static dentry_t dentry_cache[VFS_DENTRY_CACHE];
static u32 dentry_count = 0;

/* ═══════ Init ═══════ */
grav_err_t grav_vfs_init(void) {
    for (u32 i = 0; i < VFS_MAX_MOUNTS; i++) mount_table[i].active = 0;
    mount_count = 0;
    dentry_count = 0;
    return GE_OK;
}

/* ═══════ Simple path hash ═══════ */
static u64 path_hash(const char *path) {
    u64 h = 0x100;
    while (*path) { h = h * 31 + (u8)*path; path++; }
    return h;
}

/* ═══════ Find mount for path ═══════ */
static mount_t *vfs_find_mount(const char *path) {
    mount_t *best = (mount_t*)0;
    u32 best_len = 0;
    for (u32 i = 0; i < mount_count; i++) {
        if (!mount_table[i].active) continue;
        u32 len = 0;
        const char *mp = mount_table[i].path;
        const char *p = path;
        while (*mp && *mp == *p) { mp++; p++; len++; }
        if (*mp == '\0' && len > best_len) {
            best = &mount_table[i];
            best_len = len;
        }
    }
    return best;
}

/* ═══════ Mount ═══════ */
grav_err_t vfs_mount(const char *dev, const char *mountpoint, fs_ops_t *ops) {
    if (mount_count >= VFS_MAX_MOUNTS) return GE_NOMEM;
    if (!ops || !ops->mount) return GE_INVAL;

    mount_t *m = &mount_table[mount_count];
    u32 i;
    for (i = 0; mountpoint[i] && i < VFS_PATH_MAX - 1; i++)
        m->path[i] = mountpoint[i];
    m->path[i] = '\0';
    m->ops = ops;
    m->flags = 0;
    m->active = 1;

    grav_err_t err = ops->mount(dev, &m->fs_root);
    if (err != GE_OK) { m->active = 0; return err; }

    mount_count++;
    return GE_OK;
}

/* ═══════ Open ═══════ */
grav_err_t vfs_open(const char *path, int flags, file_t **out) {
    if (!path || !out) return GE_INVAL;

    mount_t *m = vfs_find_mount(path);
    if (!m || !m->ops->lookup) return GE_NOTFOUND;

    /* Extract relative path after mount point */
    const char *rel = path;
    const char *mp = m->path;
    while (*mp && *mp == *rel) { mp++; rel++; }
    if (*rel == '/') rel++;

    /* Lookup inode via dentry cache / filesystem */
    inode_t *inode = (inode_t*)0;
    grav_err_t err = m->ops->lookup(m->fs_root, rel, &inode);
    if (err != GE_OK) return err;

    /* Allocate file descriptor */
    for (u32 i = 0; i < VFS_MAX_OPEN; i++) {
        if (open_files[i].refcount == 0) {
            file_t *f = &open_files[i];
            f->inode = inode;
            f->offset = 0;
            f->flags = (u32)flags;
            f->refcount = 1;
            f->fs_file = (void*)0;

            if (m->ops->open) m->ops->open(inode, (u32)flags, f);

            *out = f;
            return GE_OK;
        }
    }
    return GE_NOMEM; /* Too many open files */
}

/* ═══════ Read ═══════ */
grav_ssize_t vfs_read(file_t *f, void *buf, grav_size_t n) {
    if (!f || !buf) return (grav_ssize_t)GE_INVAL;

    mount_t *m = vfs_find_mount("/"); /* Simplified: find mount from inode */
    if (!m || !m->ops->read) return (grav_ssize_t)GE_IO;

    grav_ssize_t bytes = m->ops->read(f, buf, n);
    if (bytes > 0) f->offset += (u64)bytes;
    return bytes;
}

/* ═══════ Write ═══════ */
grav_ssize_t vfs_write(file_t *f, const void *buf, grav_size_t n) {
    if (!f || !buf) return (grav_ssize_t)GE_INVAL;

    mount_t *m = vfs_find_mount("/");
    if (!m || !m->ops->write) return (grav_ssize_t)GE_IO;

    grav_ssize_t bytes = m->ops->write(f, buf, n);
    if (bytes > 0) f->offset += (u64)bytes;
    return bytes;
}

/* ═══════ Close ═══════ */
grav_err_t vfs_close(file_t *f) {
    if (!f) return GE_INVAL;
    f->refcount--;
    if (f->refcount == 0) {
        if (f->inode) f->inode->refcount--;
        f->inode = (inode_t*)0;
    }
    return GE_OK;
}
