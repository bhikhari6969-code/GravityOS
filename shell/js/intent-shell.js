/**
 * GravityOS — Intent Shell
 * AI-powered command interface. Replaces the traditional app launcher.
 * Type/speak what you want → AI routes it.
 * Ctrl+Space to open. ESC to close.
 * Copyright (c) 2026 GravityOS Contributors — MIT License
 */

class IntentShell {
    constructor() {
        this.overlay = document.getElementById('intent-overlay');
        this.input = document.getElementById('intent-input');
        this.results = document.getElementById('intent-results');
        this.contextCount = document.getElementById('intent-context-count');
        this.isOpen = false;
        this.selectedIndex = -1;
        this.currentResults = [];
        this.contextMemory = [];
        this.onAction = null; // Callback for executing actions

        // Known apps and commands
        this.apps = [
            { id: 'notes',    name: 'GravNote',     icon: '📝', desc: 'Notes & documents' },
            { id: 'files',    name: 'GravFiles',    icon: '📁', desc: 'File manager' },
            { id: 'browser',  name: 'GravBrowser',  icon: '🌐', desc: 'Web browser' },
            { id: 'terminal', name: 'GravTerminal', icon: '▸_', desc: 'Terminal emulator' },
            { id: 'media',    name: 'GravMedia',    icon: '♫',  desc: 'Media player' },
            { id: 'settings', name: 'Settings',     icon: '⚙',  desc: 'System settings' },
        ];

        this.systemCommands = [
            { id: 'tile',      name: 'Tile Windows',       icon: '⊞', desc: 'Auto-arrange all windows', action: 'tile' },
            { id: 'darkmode',  name: 'Toggle Dark Mode',   icon: '◐', desc: 'Switch theme', action: 'toggle_dark' },
            { id: 'lock',      name: 'Lock Screen',        icon: '🔒', desc: 'Lock GravityOS', action: 'lock' },
            { id: 'health',    name: 'System Health',       icon: '♥', desc: 'CPU, RAM, disk status', action: 'health' },
            { id: 'security',  name: 'Security Overview',   icon: '🛡', desc: 'GravitySec dashboard', action: 'security' },
            { id: 'about',     name: 'About GravityOS',     icon: '✦', desc: 'v0.1.0 Singularity', action: 'about' },
        ];

        this._bindEvents();
    }

    _bindEvents() {
        // Keyboard shortcut: Ctrl+Space or Cmd+Space
        document.addEventListener('keydown', e => {
            if ((e.ctrlKey || e.metaKey) && e.code === 'Space') {
                e.preventDefault();
                this.toggle();
            }
            if (e.key === 'Escape' && this.isOpen) {
                this.close();
            }
        });

        // Input handling
        this.input.addEventListener('input', () => this._onInput());
        this.input.addEventListener('keydown', e => this._onKeyDown(e));

        // Click outside to close
        this.overlay.addEventListener('click', e => {
            if (e.target === this.overlay) this.close();
        });
    }

    toggle() {
        if (this.isOpen) this.close();
        else this.open();
    }

    open() {
        this.isOpen = true;
        this.overlay.classList.remove('hidden');
        this.overlay.querySelector('#intent-shell').className = 'g-glass-heavy anim-intent-reveal';
        this.input.value = '';
        this.input.focus();
        this._showDefaults();
        this._updateContextCount();
    }

    close() {
        this.isOpen = false;
        const shell = this.overlay.querySelector('#intent-shell');
        shell.className = 'g-glass-heavy anim-intent-dismiss';
        setTimeout(() => {
            this.overlay.classList.add('hidden');
            this.results.innerHTML = '';
            this.selectedIndex = -1;
        }, 250);
    }

    _onInput() {
        const query = this.input.value.trim();
        if (!query) {
            this._showDefaults();
            return;
        }
        this._search(query);
    }

    _onKeyDown(e) {
        if (e.key === 'ArrowDown') {
            e.preventDefault();
            this.selectedIndex = Math.min(this.selectedIndex + 1, this.currentResults.length - 1);
            this._highlightSelected();
        } else if (e.key === 'ArrowUp') {
            e.preventDefault();
            this.selectedIndex = Math.max(this.selectedIndex - 1, 0);
            this._highlightSelected();
        } else if (e.key === 'Enter') {
            e.preventDefault();
            if (this.selectedIndex >= 0 && this.currentResults[this.selectedIndex]) {
                this._executeResult(this.currentResults[this.selectedIndex]);
            } else if (this.currentResults.length > 0) {
                this._executeResult(this.currentResults[0]);
            } else {
                // Free-form AI query
                this._executeAIQuery(this.input.value);
            }
        }
    }

    _showDefaults() {
        this.currentResults = this.apps.map(a => ({
            type: 'app', ...a, action: 'launch',
        }));
        this._renderResults(this.currentResults);
        this.selectedIndex = -1;
    }

    _search(query) {
        const q = query.toLowerCase();
        const results = [];

        // Intent detection
        const intent = this._detectIntent(q);

        // Search apps
        this.apps.forEach(app => {
            if (app.name.toLowerCase().includes(q) || app.id.includes(q) || app.desc.toLowerCase().includes(q)) {
                results.push({ type: 'app', ...app, action: 'launch', subtitle: `Open ${app.name}` });
            }
        });

        // Search system commands
        this.systemCommands.forEach(cmd => {
            if (cmd.name.toLowerCase().includes(q) || cmd.id.includes(q) || cmd.desc.toLowerCase().includes(q)) {
                results.push({ type: 'command', ...cmd, subtitle: cmd.desc });
            }
        });

        // Intent-based results
        if (intent) {
            results.unshift(intent);
        }

        // AI catch-all
        if (results.length === 0) {
            results.push({
                type: 'ai',
                id: 'ai-query',
                name: `Ask GravMind: "${query}"`,
                icon: '✦',
                desc: 'AI will process your request',
                action: 'ai_query',
                query: query,
                subtitle: 'Powered by on-device AI',
            });
        }

        this.currentResults = results;
        this._renderResults(results);
        this.selectedIndex = results.length > 0 ? 0 : -1;
        this._highlightSelected();
    }

    _detectIntent(q) {
        // App creation
        if (q.startsWith('make me') || q.startsWith('create') || q.startsWith('build me')) {
            const desc = q.replace(/^(make me|create|build me)\s*/i, '');
            return {
                type: 'generate', id: 'gen', name: `Generate: ${desc}`,
                icon: '⚡', action: 'generate_app', query: desc,
                subtitle: 'AI will create a micro-app',
            };
        }

        // Window management
        if (q.includes('tile') || q.includes('arrange') || q.includes('split')) {
            return {
                type: 'command', id: 'tile', name: 'Auto-Tile Windows',
                icon: '⊞', action: 'tile', subtitle: 'Arrange all windows in a grid',
            };
        }

        // System health
        if (q.includes('health') || q.includes('temperature') || q.includes('battery') || q.includes('performance')) {
            return {
                type: 'command', id: 'health', name: 'System Health Check',
                icon: '♥', action: 'health', subtitle: 'CPU, RAM, disk, battery status',
            };
        }

        // File search
        if (q.startsWith('find') || q.startsWith('search')) {
            const term = q.replace(/^(find|search)\s*/i, '');
            return {
                type: 'search', id: 'search', name: `Search: ${term}`,
                icon: '🔍', action: 'search', query: term,
                subtitle: 'Search files, apps, and memory',
            };
        }

        return null;
    }

    _renderResults(results) {
        this.results.innerHTML = results.map((r, i) => `
            <div class="intent-result anim-result-in" data-index="${i}" style="animation-delay:${i * 30}ms">
                <div class="intent-result-icon">${r.icon || '•'}</div>
                <div class="intent-result-text">
                    <div class="intent-result-title">${r.name}</div>
                    <div class="intent-result-subtitle">${r.subtitle || r.desc || ''}</div>
                </div>
                <span class="intent-result-action">${r.type === 'app' ? '↵ open' : r.type === 'generate' ? '⚡ gen' : '↵'}</span>
            </div>
        `).join('');

        // Click handlers
        this.results.querySelectorAll('.intent-result').forEach(el => {
            el.addEventListener('click', () => {
                const idx = parseInt(el.dataset.index);
                this._executeResult(this.currentResults[idx]);
            });
        });
    }

    _highlightSelected() {
        this.results.querySelectorAll('.intent-result').forEach((el, i) => {
            el.classList.toggle('selected', i === this.selectedIndex);
        });
    }

    _executeResult(result) {
        // Store in context memory
        this.contextMemory.push({
            input: this.input.value,
            result: result.name,
            timestamp: Date.now(),
        });

        this.close();

        if (this.onAction) {
            this.onAction(result);
        }
    }

    _executeAIQuery(query) {
        this._executeResult({
            type: 'ai', id: 'ai-query', name: query,
            icon: '✦', action: 'ai_query', query,
        });
    }

    _updateContextCount() {
        if (this.contextCount) {
            this.contextCount.textContent = `${this.contextMemory.length} memories`;
        }
    }
}

window.IntentShell = IntentShell;
