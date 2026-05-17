/**
 * GravityOS — Gesture Engine
 * Edge swipes, multi-touch, mouse gestures, proximity detection.
 * Copyright (c) 2026 GravityOS Contributors — MIT License
 */

class GestureEngine {
    constructor() {
        this.callbacks = {};
        this.mouseY = 0;
        this.mouseX = 0;
        this.edgeThreshold = 6;
        this.dockVisible = false;
        this.barVisible = false;
        this.dockTimeout = null;
        this.barTimeout = null;

        this.dock = document.getElementById('app-dock');
        this.bar = document.getElementById('system-bar');

        this._bindProximity();
        this._bindKeyboard();
    }

    on(event, callback) {
        if (!this.callbacks[event]) this.callbacks[event] = [];
        this.callbacks[event].push(callback);
    }

    _emit(event, data) {
        (this.callbacks[event] || []).forEach(cb => cb(data));
    }

    _bindProximity() {
        document.addEventListener('mousemove', e => {
            this.mouseX = e.clientX;
            this.mouseY = e.clientY;

            // Bottom edge → show dock
            if (e.clientY > window.innerHeight - this.edgeThreshold) {
                this._showDock();
            } else if (e.clientY < window.innerHeight - 100) {
                this._hideDock();
            }

            // Top edge → show system bar
            if (e.clientY < this.edgeThreshold) {
                this._showBar();
            } else if (e.clientY > 50) {
                this._hideBar();
            }

            // Cursor glow effect
            this._updateCursorGlow(e.clientX, e.clientY);
        });

        // Touch: swipe up from bottom = dock
        let touchStartY = 0;
        document.addEventListener('touchstart', e => {
            touchStartY = e.touches[0].clientY;
        });
        document.addEventListener('touchend', e => {
            const dy = touchStartY - e.changedTouches[0].clientY;
            if (dy > 50 && touchStartY > window.innerHeight - 50) {
                this._showDock();
                this._emit('swipe-up', {});
            }
            if (dy < -50 && touchStartY < 50) {
                this._showBar();
                this._emit('swipe-down', {});
            }
        });
    }

    _bindKeyboard() {
        // Alt+Tab → window switcher
        let altTabActive = false;
        document.addEventListener('keydown', e => {
            if (e.altKey && e.key === 'Tab') {
                e.preventDefault();
                if (!altTabActive) {
                    altTabActive = true;
                    this._emit('window-switcher-open', {});
                }
                this._emit('window-switcher-next', {});
            }
        });
        document.addEventListener('keyup', e => {
            if (e.key === 'Alt' && altTabActive) {
                altTabActive = false;
                this._emit('window-switcher-close', {});
            }
        });

        // Ctrl+W → close focused window
        document.addEventListener('keydown', e => {
            if ((e.ctrlKey || e.metaKey) && e.key === 'w') {
                e.preventDefault();
                this._emit('close-window', {});
            }
        });
    }

    _showDock() {
        if (this.dockVisible) return;
        this.dockVisible = true;
        clearTimeout(this.dockTimeout);
        this.dock.classList.add('visible');
    }

    _hideDock() {
        if (!this.dockVisible) return;
        clearTimeout(this.dockTimeout);
        this.dockTimeout = setTimeout(() => {
            this.dockVisible = false;
            this.dock.classList.remove('visible');
        }, 600);
    }

    _showBar() {
        if (this.barVisible) return;
        this.barVisible = true;
        clearTimeout(this.barTimeout);
        this.bar.classList.add('visible');
    }

    _hideBar() {
        if (!this.barVisible) return;
        clearTimeout(this.barTimeout);
        this.barTimeout = setTimeout(() => {
            this.barVisible = false;
            this.bar.classList.remove('visible');
        }, 800);
    }

    _updateCursorGlow(x, y) {
        let glow = document.getElementById('cursor-glow');
        if (!glow) {
            glow = document.createElement('div');
            glow.id = 'cursor-glow';
            document.body.appendChild(glow);
        }
        glow.style.left = x + 'px';
        glow.style.top = y + 'px';
    }
}

window.GestureEngine = GestureEngine;
