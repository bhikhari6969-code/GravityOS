/**
 * GravityOS — Native GravityApps
 * All 6 native apps built with the GravityKit pattern.
 * Each app returns HTML content for its window body.
 * Copyright (c) 2026 GravityOS Contributors — MIT License
 */

const GravityApps = {

    // ═══════ GravNote — Notes & Documents ═══════
    notes: {
        title: 'GravNote',
        icon: '📝',
        width: 600,
        height: 500,
        render() {
            return `
                <div class="app-notes" style="height:100%;display:flex;flex-direction:column;">
                    <div style="display:flex;gap:8px;padding:0 0 12px;border-bottom:1px solid var(--g-border-subtle);margin-bottom:12px;">
                        <button class="g-caption" style="background:var(--g-bg-surface);border:1px solid var(--g-border);padding:4px 12px;border-radius:var(--g-radius-sm);cursor:pointer;color:var(--g-text-secondary);">+ New</button>
                        <button class="g-caption" style="background:transparent;border:none;padding:4px 12px;cursor:pointer;color:var(--g-text-tertiary);">All Notes</button>
                        <button class="g-caption" style="background:transparent;border:none;padding:4px 12px;cursor:pointer;color:var(--g-text-tertiary);">Favorites</button>
                        <div style="flex:1"></div>
                        <span class="g-caption" style="color:var(--g-text-tertiary);padding:4px;">✦ AI Summarize</span>
                    </div>
                    <textarea placeholder="Start typing... Markdown supported.

# My Note

Write freely. GravMind remembers context across all your notes.

- [ ] Task one
- [x] Task two (completed)

> \"Every element must earn its place.\" — Dieter Rams" style="flex:1;"></textarea>
                    <div style="display:flex;justify-content:space-between;padding:8px 0 0;border-top:1px solid var(--g-border-subtle);margin-top:8px;">
                        <span class="g-caption g-mono" style="color:var(--g-text-tertiary);">UTF-8 · Markdown</span>
                        <span class="g-caption g-mono" style="color:var(--g-text-tertiary);">Auto-saved</span>
                    </div>
                </div>`;
        }
    },

    // ═══════ GravFiles — File Manager ═══════
    files: {
        title: 'GravFiles',
        icon: '📁',
        width: 750,
        height: 520,
        render() {
            const files = [
                { name: 'Documents', icon: '📁', type: 'folder' },
                { name: 'Downloads', icon: '📂', type: 'folder' },
                { name: 'Pictures', icon: '🖼', type: 'folder' },
                { name: 'Music', icon: '🎵', type: 'folder' },
                { name: 'Projects', icon: '📁', type: 'folder' },
                { name: 'GravityOS', icon: '📁', type: 'folder' },
                { name: 'report.pdf', icon: '📄', type: 'file' },
                { name: 'design.fig', icon: '🎨', type: 'file' },
                { name: 'notes.md', icon: '📝', type: 'file' },
                { name: 'config.yaml', icon: '⚙', type: 'file' },
                { name: 'photo.jpg', icon: '🖼', type: 'file' },
                { name: 'backup.tar.gz', icon: '📦', type: 'file' },
            ];
            return `
                <div style="display:flex;flex-direction:column;height:100%;">
                    <div style="display:flex;gap:8px;padding:0 0 12px;border-bottom:1px solid var(--g-border-subtle);margin-bottom:12px;align-items:center;">
                        <button class="browser-btn">←</button>
                        <button class="browser-btn">→</button>
                        <div class="browser-url" style="padding:4px 12px;font-size:12px;">/ home / user</div>
                        <span class="g-caption" style="cursor:pointer;color:var(--g-text-tertiary);">🔍</span>
                    </div>
                    <div class="file-grid" style="flex:1;overflow-y:auto;">
                        ${files.map(f => `
                            <div class="file-item">
                                <span class="file-item-icon">${f.icon}</span>
                                <span class="file-item-name">${f.name}</span>
                            </div>
                        `).join('')}
                    </div>
                    <div style="display:flex;justify-content:space-between;padding:8px 0 0;border-top:1px solid var(--g-border-subtle);margin-top:8px;">
                        <span class="g-caption g-mono" style="color:var(--g-text-tertiary);">12 items</span>
                        <span class="g-caption g-mono" style="color:var(--g-text-tertiary);">GravFS · Encrypted</span>
                    </div>
                </div>`;
        }
    },

    // ═══════ GravBrowser — Web Browser ═══════
    browser: {
        title: 'GravBrowser',
        icon: '🌐',
        width: 900,
        height: 600,
        render() {
            return `
                <div style="display:flex;flex-direction:column;height:100%;margin:-16px;padding:0;">
                    <div class="browser-toolbar">
                        <button class="browser-btn">←</button>
                        <button class="browser-btn">→</button>
                        <button class="browser-btn">⟳</button>
                        <input class="browser-url" value="gravity://home" placeholder="Search or enter URL">
                        <button class="browser-btn">☰</button>
                    </div>
                    <div style="flex:1;display:flex;align-items:center;justify-content:center;flex-direction:column;gap:24px;padding:40px;">
                        <div class="g-display" style="font-size:48px;font-weight:200;color:var(--g-text-primary);letter-spacing:-0.03em;">GravBrowser</div>
                        <div style="color:var(--g-text-tertiary);font-size:14px;">Privacy-first. AI-assisted. Zero tracking.</div>
                        <div style="display:flex;gap:24px;margin-top:16px;">
                            <div style="text-align:center;cursor:pointer;padding:16px;border-radius:var(--g-radius-md);background:var(--g-bg-surface);min-width:80px;">
                                <div style="font-size:24px;margin-bottom:4px;">📰</div>
                                <div class="g-caption">News</div>
                            </div>
                            <div style="text-align:center;cursor:pointer;padding:16px;border-radius:var(--g-radius-md);background:var(--g-bg-surface);min-width:80px;">
                                <div style="font-size:24px;margin-bottom:4px;">🔒</div>
                                <div class="g-caption">Privacy</div>
                            </div>
                            <div style="text-align:center;cursor:pointer;padding:16px;border-radius:var(--g-radius-md);background:var(--g-bg-surface);min-width:80px;">
                                <div style="font-size:24px;margin-bottom:4px;">✦</div>
                                <div class="g-caption">AI Summary</div>
                            </div>
                        </div>
                    </div>
                </div>`;
        }
    },

    // ═══════ GravTerminal — Terminal Emulator ═══════
    terminal: {
        title: 'GravTerminal',
        icon: '▸_',
        width: 700,
        height: 450,
        render() {
            return `
                <div class="app-terminal" style="height:100%;margin:-16px;padding:16px;overflow-y:auto;">
                    <div class="terminal-line" style="color:var(--g-text-tertiary);">GravityOS v0.1.0 Singularity — GravTerminal</div>
                    <div class="terminal-line" style="color:var(--g-text-tertiary);">GravityCore microkernel · 64 syscalls · Capability-secured</div>
                    <div class="terminal-line" style="color:var(--g-text-tertiary);">Type 'help' for commands. AI suggestions enabled.</div>
                    <div class="terminal-line">&nbsp;</div>
                    <div class="terminal-line"><span style="color:#00cc66;">gravity</span><span style="color:var(--g-text-tertiary);">@</span><span style="color:#6699ff;">singularity</span> <span style="color:var(--g-text-tertiary);">~</span> <span style="color:#888;">$</span> uname -a</div>
                    <div class="terminal-line" style="color:var(--g-text-secondary);">GravityOS 0.1.0 Singularity x86_64 GravityCore</div>
                    <div class="terminal-line">&nbsp;</div>
                    <div class="terminal-line"><span style="color:#00cc66;">gravity</span><span style="color:var(--g-text-tertiary);">@</span><span style="color:#6699ff;">singularity</span> <span style="color:var(--g-text-tertiary);">~</span> <span style="color:#888;">$</span> gravsec status</div>
                    <div class="terminal-line" style="color:#00cc66;">● GravitySec: active (autonomous mode)</div>
                    <div class="terminal-line" style="color:var(--g-text-secondary);">  ├─ eBPF tracer: monitoring 42 processes</div>
                    <div class="terminal-line" style="color:var(--g-text-secondary);">  ├─ GravVault: TPM sealed, 12 keys stored</div>
                    <div class="terminal-line" style="color:var(--g-text-secondary);">  ├─ Firewall: per-process rules active</div>
                    <div class="terminal-line" style="color:var(--g-text-secondary);">  └─ Threat level: <span style="color:#00cc66;">nominal</span></div>
                    <div class="terminal-line">&nbsp;</div>
                    <div class="terminal-line"><span style="color:#00cc66;">gravity</span><span style="color:var(--g-text-tertiary);">@</span><span style="color:#6699ff;">singularity</span> <span style="color:var(--g-text-tertiary);">~</span> <span style="color:#888;">$</span> <span class="terminal-cursor"></span></div>
                </div>`;
        }
    },

    // ═══════ GravMedia — Media Player ═══════
    media: {
        title: 'GravMedia',
        icon: '♫',
        width: 500,
        height: 400,
        render() {
            return `
                <div style="display:flex;flex-direction:column;height:100%;align-items:center;justify-content:center;gap:24px;">
                    <div style="width:180px;height:180px;border-radius:var(--g-radius-xl);background:linear-gradient(135deg, #1a1a2e, #16213e, #0f3460);display:flex;align-items:center;justify-content:center;font-size:48px;box-shadow:var(--g-shadow-xl);">
                        ♫
                    </div>
                    <div style="text-align:center;">
                        <div style="font-size:var(--g-text-lg);font-weight:var(--g-weight-medium);color:var(--g-text-bright);">Gravity Waves</div>
                        <div style="font-size:var(--g-text-sm);color:var(--g-text-tertiary);margin-top:4px;">GravityOS — Ambient</div>
                    </div>
                    <div style="width:80%;height:3px;background:var(--g-border);border-radius:2px;position:relative;">
                        <div style="width:35%;height:100%;background:var(--g-text-primary);border-radius:2px;"></div>
                    </div>
                    <div style="display:flex;gap:24px;align-items:center;">
                        <button style="background:transparent;border:none;color:var(--g-text-secondary);font-size:20px;cursor:pointer;">⏮</button>
                        <button style="background:var(--g-text-primary);border:none;color:var(--g-bg);width:48px;height:48px;border-radius:50%;font-size:20px;cursor:pointer;display:flex;align-items:center;justify-content:center;">▶</button>
                        <button style="background:transparent;border:none;color:var(--g-text-secondary);font-size:20px;cursor:pointer;">⏭</button>
                    </div>
                    <div style="display:flex;gap:16px;">
                        <span class="g-caption" style="cursor:pointer;">🔀 Shuffle</span>
                        <span class="g-caption" style="cursor:pointer;">🔁 Repeat</span>
                        <span class="g-caption" style="cursor:pointer;">✦ AI Mix</span>
                    </div>
                </div>`;
        }
    },

    // ═══════ Settings ═══════
    settings: {
        title: 'Settings',
        icon: '⚙',
        width: 600,
        height: 520,
        render() {
            return `
                <div style="max-height:100%;overflow-y:auto;">
                    <div class="settings-section">
                        <div class="settings-title">System</div>
                        <div class="settings-row"><span class="settings-label">GravityOS Version</span><span class="settings-value">0.1.0 Singularity</span></div>
                        <div class="settings-row"><span class="settings-label">Kernel</span><span class="settings-value">GravityCore (64 syscalls)</span></div>
                        <div class="settings-row"><span class="settings-label">Architecture</span><span class="settings-value">x86_64</span></div>
                    </div>
                    <div class="settings-section">
                        <div class="settings-title">Gravity AI</div>
                        <div class="settings-row"><span class="settings-label">GravMind</span><div class="toggle on" onclick="this.classList.toggle('on')"></div></div>
                        <div class="settings-row"><span class="settings-label">Voice ("Hey Gravity")</span><div class="toggle on" onclick="this.classList.toggle('on')"></div></div>
                        <div class="settings-row"><span class="settings-label">GravSight (Screen AI)</span><div class="toggle" onclick="this.classList.toggle('on')"></div></div>
                        <div class="settings-row"><span class="settings-label">Privacy Mode</span><div class="toggle on" onclick="this.classList.toggle('on')"></div></div>
                        <div class="settings-row"><span class="settings-label">Cloud Sync</span><div class="toggle" onclick="this.classList.toggle('on')"></div></div>
                    </div>
                    <div class="settings-section">
                        <div class="settings-title">Security</div>
                        <div class="settings-row"><span class="settings-label">GravitySec</span><span class="settings-value" style="color:#00cc66;">● Active</span></div>
                        <div class="settings-row"><span class="settings-label">GravVault (TPM)</span><span class="settings-value" style="color:#00cc66;">● Sealed</span></div>
                        <div class="settings-row"><span class="settings-label">Firewall</span><div class="toggle on" onclick="this.classList.toggle('on')"></div></div>
                        <div class="settings-row"><span class="settings-label">Secure Boot</span><span class="settings-value" style="color:#00cc66;">● Verified</span></div>
                    </div>
                    <div class="settings-section">
                        <div class="settings-title">Display</div>
                        <div class="settings-row"><span class="settings-label">Theme</span><span class="settings-value">Monochrome Dark</span></div>
                        <div class="settings-row"><span class="settings-label">Gravity Field</span><div class="toggle on" onclick="this.classList.toggle('on')"></div></div>
                        <div class="settings-row"><span class="settings-label">Animations</span><div class="toggle on" onclick="this.classList.toggle('on')"></div></div>
                    </div>
                </div>`;
        }
    },
};

window.GravityApps = GravityApps;
