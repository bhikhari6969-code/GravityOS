/**
 * GravityOS — Shell Orchestrator
 * shell.js — Boots the OS, wires all subsystems, manages lifecycle.
 * Copyright (c) 2026 GravityOS Contributors — MIT License
 */

class GravityShell {
    constructor() {
        this.field = null;
        this.wm = null;
        this.intent = null;
        this.gestures = null;
        this.booted = false;
        this.toastId = 0;
    }

    async boot() {
        document.body.classList.add('booting');
        const splash = document.getElementById('boot-splash');
        const progress = document.getElementById('boot-progress-bar');
        const status = document.getElementById('boot-status');

        const steps = [
            ['GravityCore microkernel loaded', 10],
            ['Capability system initialized (64K slots)', 20],
            ['PMM: buddy allocator ready', 28],
            ['VMM: 5-level paging, KASLR enabled', 36],
            ['SLAB allocator online', 42],
            ['IPC subsystem ready (4096 endpoints)', 48],
            ['GravitySec: eBPF tracer active', 56],
            ['GravitySec: zero-trust policy loaded', 62],
            ['GravDisplay compositor initialized', 68],
            ['GravNet: QUIC stack ready', 74],
            ['Gravity Field rendering engine', 80],
            ['GravMind: 7B model loaded (INT4/NPU)', 88],
            ['GravMemory: vector store mounted', 92],
            ['GravityShell: zero-chrome UI ready', 97],
            ['All systems nominal', 100],
        ];

        for (const [msg, pct] of steps) {
            status.textContent = msg;
            progress.style.width = pct + '%';
            await this._sleep(180 + Math.random() * 120);
        }

        await this._sleep(400);

        // Init subsystems
        this._initField();
        this._initWindowManager();
        this._initIntentShell();
        this._initGestures();
        this._initClock();
        this._initDock();
        this._initSystemIndicators();

        // Dismiss splash
        splash.classList.add('done');
        document.body.classList.remove('booting');
        this.booted = true;

        await this._sleep(800);
        splash.style.display = 'none';

        // Welcome toast
        this.toast('GravityOS ready. Press Ctrl+Space for Intent Shell.', 4000);
    }

    // ─── Gravity Field ───
    _initField() {
        this.field = new GravityField('gravity-field');
        this.field.start();
    }

    // ─── Window Manager ───
    _initWindowManager() {
        this.wm = new WindowManager();
    }

    // ─── Intent Shell ───
    _initIntentShell() {
        this.intent = new IntentShell();
        this.intent.onAction = (result) => this._handleAction(result);
    }

    // ─── Gesture Engine ───
    _initGestures() {
        this.gestures = new GestureEngine();

        this.gestures.on('close-window', () => {
            if (this.wm.focusedId) this.wm.close(this.wm.focusedId);
        });

        this.gestures.on('window-switcher-open', () => {
            this._showWindowSwitcher();
        });

        this.gestures.on('window-switcher-next', () => {
            this._cycleWindowSwitcher();
        });

        this.gestures.on('window-switcher-close', () => {
            this._hideWindowSwitcher();
        });
    }

    // ─── Clock ───
    _initClock() {
        const clock = document.getElementById('system-clock');
        const update = () => {
            const now = new Date();
            const h = now.getHours().toString().padStart(2, '0');
            const m = now.getMinutes().toString().padStart(2, '0');
            const s = now.getSeconds().toString().padStart(2, '0');
            clock.textContent = `${h}:${m}:${s}`;
        };
        update();
        setInterval(update, 1000);
    }

    // ─── Dock ───
    _initDock() {
        document.querySelectorAll('.dock-item').forEach(item => {
            item.addEventListener('click', () => {
                const appId = item.dataset.app;
                this.launchApp(appId);
            });
        });
    }

    // ─── System Indicators ───
    _initSystemIndicators() {
        const cpu = document.getElementById('cpu-indicator');
        const mem = document.getElementById('mem-indicator');
        const bat = document.getElementById('battery-indicator');

        const update = () => {
            cpu.textContent = `CPU ${(5 + Math.random() * 15).toFixed(0)}%`;
            mem.textContent = `RAM ${(30 + Math.random() * 10).toFixed(0)}%`;
            bat.textContent = `BAT ${(80 + Math.random() * 15).toFixed(0)}%`;
        };
        update();
        setInterval(update, 3000);
    }

    // ─── App Launcher ───
    launchApp(appId) {
        const app = GravityApps[appId];
        if (!app) {
            this.toast(`Unknown app: ${appId}`);
            return;
        }

        this.wm.create(appId, app.title, app.icon, app.render(), {
            width: app.width,
            height: app.height,
        });
    }

    // ─── Action Handler (from Intent Shell) ───
    _handleAction(result) {
        switch (result.action) {
            case 'launch':
                this.launchApp(result.id);
                break;

            case 'tile':
                this.wm.autoTile();
                this.toast('Windows auto-tiled');
                break;

            case 'health':
                this._showHealthReport();
                break;

            case 'security':
                this.launchApp('settings');
                break;

            case 'about':
                this._showAbout();
                break;

            case 'generate_app':
                this._generateApp(result.query);
                break;

            case 'search':
                this.toast(`Searching: "${result.query}"`);
                break;

            case 'ai_query':
                this._aiResponse(result.query || result.name);
                break;

            case 'toggle_dark':
                this.toast('Theme: Monochrome Dark (default)');
                break;

            case 'lock':
                this.toast('Screen locked');
                break;

            default:
                this.toast(`Action: ${result.action || result.name}`);
        }
    }

    // ─── Health Report ───
    _showHealthReport() {
        const html = `
            <div style="font-family:var(--g-font-mono);font-size:var(--g-text-sm);">
                <div class="g-heading" style="font-size:var(--g-text-lg);margin-bottom:16px;">System Health</div>
                <div style="display:grid;grid-template-columns:1fr 1fr;gap:12px;">
                    <div style="padding:16px;background:var(--g-bg-surface);border-radius:var(--g-radius-md);">
                        <div class="g-caption" style="margin-bottom:8px;">CPU</div>
                        <div style="font-size:24px;font-weight:300;">47°C</div>
                        <div style="color:var(--g-text-tertiary);font-size:11px;margin-top:4px;">12% load · 8 cores</div>
                    </div>
                    <div style="padding:16px;background:var(--g-bg-surface);border-radius:var(--g-radius-md);">
                        <div class="g-caption" style="margin-bottom:8px;">Memory</div>
                        <div style="font-size:24px;font-weight:300;">5.2 GB</div>
                        <div style="color:var(--g-text-tertiary);font-size:11px;margin-top:4px;">of 16 GB · 32% used</div>
                    </div>
                    <div style="padding:16px;background:var(--g-bg-surface);border-radius:var(--g-radius-md);">
                        <div class="g-caption" style="margin-bottom:8px;">Disk</div>
                        <div style="font-size:24px;font-weight:300;">98%</div>
                        <div style="color:var(--g-text-tertiary);font-size:11px;margin-top:4px;">health · GravFS encrypted</div>
                    </div>
                    <div style="padding:16px;background:var(--g-bg-surface);border-radius:var(--g-radius-md);">
                        <div class="g-caption" style="margin-bottom:8px;">Security</div>
                        <div style="font-size:24px;font-weight:300;color:#00cc66;">● Nominal</div>
                        <div style="color:var(--g-text-tertiary);font-size:11px;margin-top:4px;">42 processes monitored</div>
                    </div>
                </div>
                <div style="margin-top:16px;padding:12px;background:var(--g-bg-surface);border-radius:var(--g-radius-md);">
                    <div class="g-caption" style="margin-bottom:8px;">GravMind AI</div>
                    <div style="color:var(--g-text-secondary);font-size:12px;">7B params · INT4 quantized · NPU accelerated · 340ms avg latency</div>
                </div>
            </div>`;
        this.wm.create('health', 'System Health', '♥', html, { width: 500, height: 420 });
    }

    // ─── About ───
    _showAbout() {
        const html = `
            <div style="display:flex;flex-direction:column;align-items:center;justify-content:center;height:100%;text-align:center;gap:16px;">
                <div class="g-display" style="font-size:64px;font-weight:200;">G</div>
                <div class="g-display" style="font-size:24px;letter-spacing:0.15em;font-weight:300;">GRAVITY OS</div>
                <div class="g-mono" style="color:var(--g-text-tertiary);font-size:11px;">v0.1.0 — Codename: Singularity</div>
                <div style="color:var(--g-text-secondary);max-width:300px;font-size:13px;line-height:1.6;margin-top:8px;">
                    The first OS that is simultaneously maximally compatible and maximally minimal.
                    AI is not an add-on — it is the shell.
                </div>
                <div style="margin-top:16px;display:flex;gap:16px;">
                    <span class="g-caption">GravityCore</span>
                    <span class="g-caption">64 Syscalls</span>
                    <span class="g-caption">Capability Security</span>
                </div>
                <div class="g-caption g-mono" style="margin-top:24px;color:var(--g-text-tertiary);">
                    MIT License · 2026
                </div>
            </div>`;
        this.wm.create('about', 'About', '✦', html, { width: 420, height: 400 });
    }

    // ─── App Generation ───
    _generateApp(description) {
        this.toast(`⚡ Generating app: "${description}"...`);
        setTimeout(() => {
            const html = `
                <div style="display:flex;flex-direction:column;align-items:center;justify-content:center;height:100%;gap:16px;">
                    <div style="font-size:48px;">⚡</div>
                    <div class="g-heading" style="font-size:var(--g-text-lg);">AI-Generated App</div>
                    <div style="color:var(--g-text-secondary);font-size:13px;text-align:center;max-width:280px;">"${description}"</div>
                    <div class="g-caption g-mono" style="color:var(--g-text-tertiary);">Generated by GravMind · Sandboxed</div>
                    <div style="margin-top:16px;padding:16px;background:var(--g-bg-surface);border-radius:var(--g-radius-md);width:80%;text-align:center;">
                        <div style="font-size:24px;margin-bottom:8px;">🎯</div>
                        <div style="color:var(--g-text-primary);">App content would render here</div>
                    </div>
                </div>`;
            this.wm.create('generated', `⚡ ${description}`, '⚡', html, { width: 400, height: 380 });
            this.toast('App generated and sandboxed ✓');
        }, 1500);
    }

    // ─── AI Response ───
    _aiResponse(query) {
        const html = `
            <div style="padding:8px;">
                <div style="display:flex;gap:12px;margin-bottom:16px;">
                    <div style="width:32px;height:32px;background:var(--g-bg-surface);border-radius:50%;display:flex;align-items:center;justify-content:center;flex-shrink:0;">✦</div>
                    <div>
                        <div class="g-heading" style="font-size:var(--g-text-base);margin-bottom:4px;">GravMind</div>
                        <div style="color:var(--g-text-secondary);font-size:13px;line-height:1.6;">
                            Processing your request: "${query}"<br><br>
                            This would be handled by the on-device 7B parameter LLM (INT4 quantized on NPU).
                            GravMind processes all queries locally — no data leaves your device.
                        </div>
                        <div class="g-caption g-mono" style="margin-top:12px;color:var(--g-text-tertiary);">
                            Latency: 340ms · Privacy: local-only · Model: GravMind-7B
                        </div>
                    </div>
                </div>
            </div>`;
        this.wm.create('ai', 'GravMind', '✦', html, { width: 500, height: 300 });
    }

    // ─── Window Switcher ───
    _showWindowSwitcher() {
        const switcher = document.getElementById('window-switcher');
        const list = document.getElementById('switcher-list');
        const windows = this.wm.getWindowList();

        if (windows.length === 0) return;

        list.innerHTML = windows.map((w, i) => `
            <div class="switcher-item ${i === 0 ? 'selected' : ''}" data-wid="${w.id}">
                <div style="font-size:28px;">${w.icon || '▪'}</div>
                <div class="switcher-item-title">${w.title}</div>
            </div>
        `).join('');

        switcher.classList.remove('hidden');
        this._switcherIndex = 0;
        this._switcherWindows = windows;
    }

    _cycleWindowSwitcher() {
        if (!this._switcherWindows || this._switcherWindows.length === 0) return;
        this._switcherIndex = (this._switcherIndex + 1) % this._switcherWindows.length;
        const items = document.querySelectorAll('.switcher-item');
        items.forEach((el, i) => el.classList.toggle('selected', i === this._switcherIndex));
    }

    _hideWindowSwitcher() {
        const switcher = document.getElementById('window-switcher');
        switcher.classList.add('hidden');
        if (this._switcherWindows && this._switcherWindows[this._switcherIndex]) {
            const targetId = this._switcherWindows[this._switcherIndex].id;
            this.wm.restore(targetId);
            this.wm.focus(targetId);
        }
    }

    // ─── Toast Notifications ───
    toast(message, duration = 3000) {
        const container = document.getElementById('toast-container');
        const id = ++this.toastId;
        const el = document.createElement('div');
        el.className = 'toast g-glass anim-toast-in';
        el.id = `toast-${id}`;
        el.textContent = message;
        container.appendChild(el);

        setTimeout(() => {
            el.className = 'toast g-glass anim-toast-out';
            setTimeout(() => el.remove(), 200);
        }, duration);
    }

    // ─── Utility ───
    _sleep(ms) {
        return new Promise(r => setTimeout(r, ms));
    }
}

// ═══════ Boot ═══════
document.addEventListener('DOMContentLoaded', () => {
    const shell = new GravityShell();
    window.gravityShell = shell;
    shell.boot();
});
