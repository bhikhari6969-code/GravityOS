/*
 * GravityOS — GravityKit SDK
 * gravitykit.h — Native application framework for .grav apps
 *
 * Apps are capability-declared at build time.
 * Reactive UI via GravUI (declarative, like SwiftUI).
 * All apps share GravityData universal data layer.
 * Apps compile to .grav format: signed, delta-updatable, sandbox-ready.
 *
 * Copyright (c) 2026 GravityOS Contributors — MIT License
 */
#ifndef _GRAVITYKIT_H
#define _GRAVITYKIT_H

#include <gravity/kernel.h>
#include <gravity/ipc.h>

/* ═══════ App Manifest ═══════ */
typedef struct {
    char        app_id[128];        /* Reverse-DNS: com.gravity.notes */
    char        name[64];
    char        version[16];
    char        author[64];
    u32         min_os_version[3];  /* major, minor, patch */
    u64         capabilities;       /* Bitmask of GC_* caps requested */
    u32         flags;
} grav_manifest_t;

#define GRAV_APP_SANDBOXED    (1U << 0)
#define GRAV_APP_BACKGROUND   (1U << 1)
#define GRAV_APP_SYSTEM       (1U << 2)

/* ═══════ App Lifecycle ═══════ */
typedef struct grav_app grav_app_t;

typedef struct {
    grav_err_t (*on_init)(grav_app_t *app);
    grav_err_t (*on_start)(grav_app_t *app);
    grav_err_t (*on_suspend)(grav_app_t *app);
    grav_err_t (*on_resume)(grav_app_t *app);
    grav_err_t (*on_stop)(grav_app_t *app);
    grav_err_t (*on_destroy)(grav_app_t *app);
    grav_err_t (*on_intent)(grav_app_t *app, const char *action, void *data);
} grav_app_delegate_t;

struct grav_app {
    grav_manifest_t      manifest;
    grav_app_delegate_t *delegate;
    grav_pid_t           pid;
    grav_cap_t           caps[32];
    u32                  cap_count;
    void                *user_data;
    u32                  state;     /* 0=init, 1=running, 2=suspended, 3=stopped */
};

/* App entry point macro */
#define GRAV_APP_MAIN(delegate_ptr, manifest_ptr) \
    grav_app_t __grav_app; \
    void _start(void) { \
        __grav_app.delegate = (delegate_ptr); \
        __grav_app.manifest = *(manifest_ptr); \
        grav_app_run(&__grav_app); \
    }

grav_err_t grav_app_run(grav_app_t *app);
void       grav_app_exit(grav_app_t *app, i32 code);

/* ═══════ GravUI — Declarative UI Framework ═══════ */

/* View types */
typedef enum {
    GRAVUI_TEXT = 1,
    GRAVUI_BUTTON,
    GRAVUI_INPUT,
    GRAVUI_IMAGE,
    GRAVUI_CONTAINER,   /* VStack/HStack/ZStack */
    GRAVUI_LIST,
    GRAVUI_SCROLL,
    GRAVUI_SPACER,
    GRAVUI_DIVIDER,
    GRAVUI_TOGGLE,
    GRAVUI_SLIDER,
    GRAVUI_CANVAS,
} gravui_type_t;

/* Layout direction */
typedef enum {
    GRAVUI_VERTICAL = 0,
    GRAVUI_HORIZONTAL,
    GRAVUI_LAYERED,     /* ZStack */
} gravui_axis_t;

/* Color */
typedef struct {
    u8 r, g, b, a;
} gravui_color_t;

#define GRAVUI_BLACK   ((gravui_color_t){  5,   5,   5, 255})
#define GRAVUI_WHITE   ((gravui_color_t){250, 250, 250, 255})
#define GRAVUI_GRAY    ((gravui_color_t){136, 136, 136, 255})
#define GRAVUI_CLEAR   ((gravui_color_t){  0,   0,   0,   0})

/* View node (tree structure) */
typedef struct gravui_view {
    gravui_type_t       type;
    u32                 id;

    /* Content */
    char                text[256];
    gravui_color_t      color;
    gravui_color_t      bg_color;
    float               font_size;
    u32                 font_weight;

    /* Layout */
    gravui_axis_t       axis;
    float               padding[4];     /* top, right, bottom, left */
    float               spacing;
    float               width, height;  /* 0 = auto */
    float               corner_radius;
    float               opacity;

    /* Interaction */
    void              (*on_tap)(struct gravui_view *self, void *ctx);
    void              (*on_change)(struct gravui_view *self, void *ctx);
    void               *user_ctx;

    /* Tree */
    struct gravui_view *children;
    u32                 child_count;
    struct gravui_view *parent;
} gravui_view_t;

/* Builder functions (SwiftUI-style chaining) */
gravui_view_t *gravui_text(const char *content);
gravui_view_t *gravui_button(const char *label, void (*action)(gravui_view_t*, void*), void *ctx);
gravui_view_t *gravui_input(const char *placeholder);
gravui_view_t *gravui_vstack(gravui_view_t **children, u32 count);
gravui_view_t *gravui_hstack(gravui_view_t **children, u32 count);
gravui_view_t *gravui_spacer(void);
gravui_view_t *gravui_divider(void);
gravui_view_t *gravui_toggle(const char *label, u8 *binding);
gravui_view_t *gravui_list(gravui_view_t **items, u32 count);

/* Modifiers */
gravui_view_t *gravui_padding(gravui_view_t *v, float all);
gravui_view_t *gravui_foreground(gravui_view_t *v, gravui_color_t c);
gravui_view_t *gravui_background(gravui_view_t *v, gravui_color_t c);
gravui_view_t *gravui_font_size(gravui_view_t *v, float size);
gravui_view_t *gravui_font_weight(gravui_view_t *v, u32 weight);
gravui_view_t *gravui_corner_radius(gravui_view_t *v, float r);
gravui_view_t *gravui_frame(gravui_view_t *v, float w, float h);

/* Render the view tree to GravDisplay compositor */
grav_err_t gravui_render(grav_app_t *app, gravui_view_t *root);

/* ═══════ GravityData — Universal Data Layer ═══════ */

/* All apps share context through GravityData.
 * Schema-less document store. Privacy-scoped. */

typedef enum {
    GRAVDATA_PRIVATE = 0,    /* Only this app */
    GRAVDATA_SHARED  = 1,    /* All apps with user consent */
    GRAVDATA_SYSTEM  = 2,    /* OS-level (settings, prefs) */
} gravdata_scope_t;

typedef struct {
    char    key[128];
    u8     *value;
    u32     value_size;
    u32     type;            /* 0=raw, 1=string, 2=int, 3=float, 4=json */
    gravdata_scope_t scope;
    u64     timestamp;
} gravdata_entry_t;

grav_err_t gravdata_put(const char *key, const void *value, u32 size, 
                         u32 type, gravdata_scope_t scope);
grav_err_t gravdata_get(const char *key, void *buf, u32 buf_size, u32 *out_size);
grav_err_t gravdata_delete(const char *key);
grav_err_t gravdata_query(const char *prefix, gravdata_entry_t *results, 
                           u32 max_results, u32 *count);
grav_err_t gravdata_subscribe(const char *key, void (*callback)(const char *key, void *ctx), void *ctx);

/* ═══════ System Services IPC ═══════ */

/* File access (via VFS service) */
grav_err_t grav_file_open(const char *path, u32 flags, grav_cap_t *out);
grav_err_t grav_file_read(grav_cap_t file, void *buf, u32 size, u32 *bytes_read);
grav_err_t grav_file_write(grav_cap_t file, const void *buf, u32 size);
grav_err_t grav_file_close(grav_cap_t file);

/* Network (via GravNet service) */
grav_err_t grav_net_connect(const char *host, u16 port, grav_cap_t *out);
grav_err_t grav_net_send(grav_cap_t conn, const void *buf, u32 size);
grav_err_t grav_net_recv(grav_cap_t conn, void *buf, u32 size, u32 *bytes_read);

/* Display (via GravDisplay compositor) */
grav_err_t grav_display_create_surface(u32 width, u32 height, grav_cap_t *out);
grav_err_t grav_display_submit_frame(grav_cap_t surface, void *framebuffer);

/* AI (via GravMind service) */
grav_err_t grav_ai_query(const char *prompt, char *response, u32 max_len);
grav_err_t grav_ai_embed(const char *text, float *vector, u32 dim);

/* Notifications */
grav_err_t grav_notify(const char *title, const char *body, u32 urgency);

#endif /* _GRAVITYKIT_H */
