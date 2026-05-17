/**
 * GravityOS — Boot, Intent Shell, Keyboard
 */

// ═══ Intent Shell ═══
const intentApps = [
    {id:'terminal',name:'GravTerminal',icon:'▸_',desc:'Terminal emulator'},
    {id:'files',name:'GravFiles',icon:'📁',desc:'File manager'},
    {id:'notes',name:'GravNote',icon:'📝',desc:'Notes & documents'},
    {id:'browser',name:'GravBrowser',icon:'◎',desc:'Privacy-first browser'},
    {id:'media',name:'GravMedia',icon:'♫',desc:'Media player'},
    {id:'settings',name:'Settings',icon:'⚙',desc:'System configuration'},
    {id:'sysmon',name:'System Health',icon:'♥',desc:'Performance monitor'},
];
const intentCmds = [
    {name:'Tile Windows',icon:'⊞',desc:'Auto-arrange windows',action:'tile'},
    {name:'About GravityOS',icon:'✦',desc:'v0.1.0 Singularity',action:'about'},
    {name:'Toggle Gravity Field',icon:'◐',desc:'Enable/disable particles',action:'field'},
    {name:'Lock Screen',icon:'🔒',desc:'Lock GravityOS',action:'lock'},
];

let intentOpen = false, intentSel = -1, intentItems = [];

function openIntent() {
    intentOpen = true;
    const ov = document.getElementById('intent-overlay');
    ov.classList.add('open');
    document.getElementById('intent-field').value = '';
    document.getElementById('intent-field').focus();
    showIntentDefaults();
}
function closeIntent() {
    intentOpen = false;
    document.getElementById('intent-overlay').classList.remove('open');
    intentSel = -1;
}
function showIntentDefaults() {
    intentItems = intentApps.map(a => ({...a, type:'app'}));
    renderIntent(intentItems);
}
function searchIntent(q) {
    q = q.toLowerCase();
    const results = [];
    intentApps.forEach(a => { if (a.name.toLowerCase().includes(q)||a.desc.toLowerCase().includes(q)||a.id.includes(q)) results.push({...a,type:'app'}); });
    intentCmds.forEach(c => { if (c.name.toLowerCase().includes(q)||c.desc.toLowerCase().includes(q)) results.push({...c,type:'cmd'}); });
    if (!results.length) results.push({name:'Ask GravMind: "'+q+'"',icon:'✦',desc:'AI will process locally',type:'ai',query:q});
    intentItems = results; intentSel = 0;
    renderIntent(results);
}
function renderIntent(items) {
    document.getElementById('intent-results').innerHTML = items.map((r,i) =>
        `<div class="ir${i===intentSel?' sel':''}" onclick="execIntent(${i})"><div class="ir-icon">${r.icon}</div><div class="ir-text"><div class="ir-name">${r.name}</div><div class="ir-desc">${r.desc||''}</div></div><span class="ir-badge">${r.type==='app'?'↵ open':'↵'}</span></div>`
    ).join('');
}
function execIntent(i) {
    const r = intentItems[i]; if (!r) return;
    memories.push({what:r.name,when:Date.now()});
    document.getElementById('intent-mem').textContent = memories.length+' memories';
    closeIntent();
    if (r.type === 'app') launchApp(r.id);
    else if (r.action === 'tile') tileAll();
    else if (r.action === 'about') showAbout();
    else if (r.action === 'field') toggleField();
    else if (r.action === 'lock') { toast('Screen locked'); }
    else if (r.type === 'ai') { toast('GravMind: processing "'+r.query+'"'); createWin('GravMind','✦',`<div style="padding:8px"><div style="display:flex;gap:10px"><div style="width:28px;height:28px;background:var(--g-bg-surface);border-radius:50%;display:flex;align-items:center;justify-content:center;flex-shrink:0">✦</div><div><div style="font-weight:500;font-size:14px;margin-bottom:4px">GravMind</div><div style="color:var(--g-text-tertiary);font-size:12px;line-height:1.7">Processing: "${r.query}"<br><br>This is handled by the on-device 7B LLM (INT4/NPU). No data leaves your device.</div><div style="font-size:10px;color:var(--g-text-quaternary);font-family:var(--g-font-mono);margin-top:10px">Latency: 340ms · Privacy: local-only</div></div></div></div>`,480,280); }
}

document.getElementById('intent-field').addEventListener('input', e => {
    const q = e.target.value.trim();
    if (!q) showIntentDefaults(); else searchIntent(q);
});
document.getElementById('intent-field').addEventListener('keydown', e => {
    if (e.key === 'ArrowDown') { e.preventDefault(); intentSel = Math.min(intentSel+1, intentItems.length-1); renderIntent(intentItems); }
    else if (e.key === 'ArrowUp') { e.preventDefault(); intentSel = Math.max(intentSel-1, 0); renderIntent(intentItems); }
    else if (e.key === 'Enter') { e.preventDefault(); if (intentSel >= 0) execIntent(intentSel); else if (intentItems.length) execIntent(0); }
});

// ═══ Keyboard Shortcuts ═══
document.addEventListener('keydown', e => {
    if ((e.ctrlKey||e.metaKey) && e.code === 'Space') { e.preventDefault(); intentOpen ? closeIntent() : openIntent(); }
    if (e.key === 'Escape' && intentOpen) closeIntent();
    if ((e.ctrlKey||e.metaKey) && e.key === 'w') { e.preventDefault(); if (focusedWin) closeWin(focusedWin); }
    if ((e.ctrlKey||e.metaKey) && e.key === 't') { e.preventDefault(); launchApp('terminal'); }
    if ((e.ctrlKey||e.metaKey) && e.key === 'g') { e.preventDefault(); tileAll(); }
});
document.getElementById('intent-overlay').addEventListener('click', e => { if (e.target.id === 'intent-overlay') closeIntent(); });

// ═══ Login ═══
document.getElementById('login-input').addEventListener('keydown', e => {
    if (e.key === 'Enter') {
        document.getElementById('login-screen').classList.add('dismissed');
        setTimeout(startBoot, 800);
    }
});

// ═══ OOBE ═══
function finishOOBE() {
    const name = document.getElementById('oobe-name').value.trim() || 'Admin';
    document.getElementById('login-name').textContent = name;
    document.getElementById('login-avatar').textContent = name.charAt(0).toUpperCase();
    document.getElementById('oobe-screen').style.display = 'none';
    
    // Show login screen
    document.getElementById('login-screen').style.display = 'flex';
    document.getElementById('login-input').focus();
    updateClock();
}

// ═══ Boot Sequence ═══
async function runBios() {
    const log = document.getElementById('bios-log');
    const msgs = [
        "GravityOS Boot Manager v1.0",
        "Copyright (c) 2026 GravityOS Foundation",
        "",
        "Initializing CPU... <span class='bios-ok'>OK</span> (64 cores detected)",
        "Checking Memory... <span class='bios-ok'>OK</span> (65536 MB OK)",
        "Mounting RootFS... <span class='bios-ok'>OK</span> (ext4, ro)",
        "Loading GravityCore microkernel...",
        "Setting up capability space...",
        "Executing PID 1 (GravityInit)..."
    ];
    for (const msg of msgs) {
        log.innerHTML += `<div class="bios-line">${msg}</div>`;
        await new Promise(r => setTimeout(r, 100 + Math.random() * 200));
    }
    await new Promise(r => setTimeout(r, 600));
    document.getElementById('bios-screen').style.display = 'none';
    
    // Assume first boot
    const isFirstBoot = !localStorage.getItem('gravityos_init');
    if (isFirstBoot) {
        localStorage.setItem('gravityos_init', 'true');
        document.getElementById('oobe-screen').style.display = 'flex';
        document.getElementById('oobe-name').focus();
    } else {
        document.getElementById('login-screen').style.display = 'flex';
        document.getElementById('login-input').focus();
        updateClock();
    }
}

async function startBoot() {
    const splash = document.getElementById('boot-splash');
    const fill = document.getElementById('boot-fill');
    const log = document.getElementById('boot-log');
    splash.style.display = 'flex';

    const steps = [
        ['GravityCore microkernel loaded', 8],
        ['Capability system: 64K token slots', 16],
        ['PMM: buddy allocator (orders 0-10)', 24],
        ['VMM: 5-level paging, KASLR enabled', 32],
        ['SLAB allocator: per-type caches online', 38],
        ['IPC subsystem: 4096 endpoints ready', 44],
        ['IRQ dispatch: APIC/IOAPIC configured', 50],
        ['GravitySec: eBPF tracer armed', 56],
        ['GravitySec: zero-trust policy loaded', 62],
        ['GravDisplay: compositor initialized', 68],
        ['GravNet: QUIC stack ready', 73],
        ['URT: Win32/Linux/macOS shims loaded', 78],
        ['Gravity Field: render engine ready', 83],
        ['GravMind: 7B model loaded (INT4/NPU)', 90],
        ['GravMemory: vector store mounted', 94],
        ['GravityShell: zero-chrome UI ready', 97],
        ['All systems nominal ✓', 100],
    ];

    for (const [msg, pct] of steps) {
        log.textContent = msg;
        fill.style.width = pct + '%';
        await new Promise(r => setTimeout(r, 140 + Math.random() * 100));
    }
    await new Promise(r => setTimeout(r, 500));

    // Init systems
    initParticles();
    drawField();

    splash.classList.add('done');
    await new Promise(r => setTimeout(r, 1000));
    splash.style.display = 'none';

    toast('GravityOS ready. Press Ctrl+Space for Intent Shell.', 4000);
    // Auto-launch terminal for demo impact
    setTimeout(() => launchApp('terminal'), 600);
}

// ═══ Start ═══
// Use window.onload to ensure DOM is ready
window.addEventListener('load', () => {
    runBios();
});
