/**
 * GravityOS — Window Manager
 * AI-assisted tiling+floating hybrid. Draggable, resizable, snap zones.
 * No persistent taskbar. Window switcher on gesture.
 * Copyright (c) 2026 GravityOS Contributors — MIT License
 */

class WindowManager {
    constructor() {
        this.windows = new Map();
        this.nextId = 1;
        this.focusedId = null;
        this.zCounter = 10;
        this.desktop = document.getElementById('desktop-layer');
        this.dragState = null;
        this.resizeState = null;
        this.snapZones = this._buildSnapZones();

        // Global mouse handlers for drag/resize
        document.addEventListener('mousemove', e => this._onMouseMove(e));
        document.addEventListener('mouseup', e => this._onMouseUp(e));
    }

    create(appId, title, icon, contentHtml, options = {}) {
        const id = this.nextId++;
        const w = options.width || 700;
        const h = options.height || 500;
        const x = options.x ?? (80 + (id % 6) * 40);
        const y = options.y ?? (60 + (id % 6) * 30);

        const el = document.createElement('div');
        el.className = 'grav-window anim-window-open';
        el.id = `window-${id}`;
        el.style.cssText = `left:${x}px;top:${y}px;width:${w}px;height:${h}px;z-index:${this.zCounter++}`;

        el.innerHTML = `
            <div class="window-titlebar" data-wid="${id}">
                <div class="window-controls">
                    <button class="window-btn close" data-action="close" data-wid="${id}"></button>
                    <button class="window-btn minimize" data-action="minimize" data-wid="${id}"></button>
                    <button class="window-btn maximize" data-action="maximize" data-wid="${id}"></button>
                </div>
                <span class="window-title">${icon || ''} ${title}</span>
                <div style="width:54px"></div>
            </div>
            <div class="window-body" id="window-body-${id}">${contentHtml}</div>
        `;

        this.desktop.appendChild(el);

        // Store window state
        this.windows.set(id, {
            id, appId, title, icon, el,
            x, y, w, h,
            maximized: false,
            minimized: false,
            prevBounds: null,
        });

        // Event listeners
        const titlebar = el.querySelector('.window-titlebar');
        titlebar.addEventListener('mousedown', e => this._startDrag(e, id));
        titlebar.addEventListener('dblclick', () => this.toggleMaximize(id));

        el.querySelectorAll('.window-btn').forEach(btn => {
            btn.addEventListener('click', e => {
                e.stopPropagation();
                const action = btn.dataset.action;
                const wid = parseInt(btn.dataset.wid);
                if (action === 'close') this.close(wid);
                else if (action === 'minimize') this.minimize(wid);
                else if (action === 'maximize') this.toggleMaximize(wid);
            });
        });

        el.addEventListener('mousedown', () => this.focus(id));

        this.focus(id);
        return id;
    }

    close(id) {
        const win = this.windows.get(id);
        if (!win) return;
        win.el.className = 'grav-window anim-window-close';
        setTimeout(() => {
            win.el.remove();
            this.windows.delete(id);
            if (this.focusedId === id) {
                this.focusedId = null;
                // Focus topmost remaining window
                let topWin = null, topZ = 0;
                this.windows.forEach(w => {
                    const z = parseInt(w.el.style.zIndex) || 0;
                    if (z > topZ) { topZ = z; topWin = w; }
                });
                if (topWin) this.focus(topWin.id);
            }
        }, 250);
    }

    minimize(id) {
        const win = this.windows.get(id);
        if (!win) return;
        win.minimized = true;
        win.el.className = 'grav-window anim-window-min';
        setTimeout(() => { win.el.style.display = 'none'; }, 300);
    }

    restore(id) {
        const win = this.windows.get(id);
        if (!win) return;
        win.minimized = false;
        win.el.style.display = '';
        win.el.className = 'grav-window anim-window-open';
        this.focus(id);
    }

    toggleMaximize(id) {
        const win = this.windows.get(id);
        if (!win) return;

        if (win.maximized) {
            // Restore
            const b = win.prevBounds;
            win.el.style.cssText = `left:${b.x}px;top:${b.y}px;width:${b.w}px;height:${b.h}px;z-index:${win.el.style.zIndex}`;
            win.el.style.borderRadius = '';
            win.maximized = false;
        } else {
            // Maximize
            win.prevBounds = { x: win.x, y: win.y, w: win.w, h: win.h };
            win.el.style.cssText = `left:0;top:0;width:100vw;height:100vh;z-index:${this.zCounter++}`;
            win.el.style.borderRadius = '0';
            win.maximized = true;
        }
        this.focus(id);
    }

    focus(id) {
        const win = this.windows.get(id);
        if (!win) return;

        // Unfocus all
        this.windows.forEach(w => w.el.classList.remove('focused'));

        // Focus target
        win.el.classList.add('focused');
        win.el.style.zIndex = this.zCounter++;
        this.focusedId = id;
    }

    getWindowList() {
        const list = [];
        this.windows.forEach(w => {
            list.push({ id: w.id, appId: w.appId, title: w.title, icon: w.icon, minimized: w.minimized });
        });
        return list;
    }

    // ─── Drag ───
    _startDrag(e, id) {
        if (e.target.classList.contains('window-btn')) return;
        const win = this.windows.get(id);
        if (!win || win.maximized) return;

        this.dragState = {
            id,
            startX: e.clientX,
            startY: e.clientY,
            origX: win.el.offsetLeft,
            origY: win.el.offsetTop,
        };
        this.focus(id);
        e.preventDefault();
    }

    _onMouseMove(e) {
        if (this.dragState) {
            const { id, startX, startY, origX, origY } = this.dragState;
            const win = this.windows.get(id);
            if (!win) return;
            const dx = e.clientX - startX;
            const dy = e.clientY - startY;
            win.x = origX + dx;
            win.y = origY + dy;
            win.el.style.left = win.x + 'px';
            win.el.style.top = win.y + 'px';

            // Snap zone preview
            this._checkSnapZones(e.clientX, e.clientY);
        }
    }

    _onMouseUp(e) {
        if (this.dragState) {
            const { id } = this.dragState;
            const snap = this._getSnapZone(e.clientX, e.clientY);
            if (snap) this._applySnap(id, snap);
            this.dragState = null;
        }
    }

    // ─── Snap Zones ───
    _buildSnapZones() {
        return [
            { name: 'left',   check: (x, y) => x < 8,                          bounds: () => ({ x: 0, y: 0, w: window.innerWidth / 2, h: window.innerHeight }) },
            { name: 'right',  check: (x, y) => x > window.innerWidth - 8,      bounds: () => ({ x: window.innerWidth / 2, y: 0, w: window.innerWidth / 2, h: window.innerHeight }) },
            { name: 'top',    check: (x, y) => y < 8,                           bounds: () => ({ x: 0, y: 0, w: window.innerWidth, h: window.innerHeight }) },
        ];
    }

    _getSnapZone(x, y) {
        for (const zone of this.snapZones) {
            if (zone.check(x, y)) return zone;
        }
        return null;
    }

    _checkSnapZones(x, y) {
        // Visual snap preview would go here
    }

    _applySnap(id, zone) {
        const win = this.windows.get(id);
        if (!win) return;
        const b = zone.bounds();
        win.prevBounds = { x: win.x, y: win.y, w: win.w, h: win.h };
        win.el.style.cssText = `left:${b.x}px;top:${b.y}px;width:${b.w}px;height:${b.h}px;z-index:${win.el.style.zIndex};transition:all 0.25s cubic-bezier(0.16,1,0.3,1)`;
        win.maximized = zone.name === 'top';
        if (win.maximized) win.el.style.borderRadius = '0';
        setTimeout(() => { win.el.style.transition = ''; }, 300);
    }

    // ─── Layout Suggestions (AI-driven) ───
    autoTile() {
        const visible = [];
        this.windows.forEach(w => { if (!w.minimized) visible.push(w); });
        if (visible.length === 0) return;

        const cols = Math.ceil(Math.sqrt(visible.length));
        const rows = Math.ceil(visible.length / cols);
        const cellW = window.innerWidth / cols;
        const cellH = window.innerHeight / rows;
        const pad = 8;

        visible.forEach((w, i) => {
            const col = i % cols;
            const row = Math.floor(i / cols);
            w.x = col * cellW + pad;
            w.y = row * cellH + pad;
            w.w = cellW - pad * 2;
            w.h = cellH - pad * 2;
            w.el.style.transition = 'all 0.4s cubic-bezier(0.16,1,0.3,1)';
            w.el.style.left = w.x + 'px';
            w.el.style.top = w.y + 'px';
            w.el.style.width = w.w + 'px';
            w.el.style.height = w.h + 'px';
            w.el.style.borderRadius = '';
            w.maximized = false;
            setTimeout(() => { w.el.style.transition = ''; }, 450);
        });
    }
}

window.WindowManager = WindowManager;
