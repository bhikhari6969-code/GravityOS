/**
 * GravityOS — Window Manager + Apps
 */

// ═══ Window Manager ═══
function createWin(title, icon, html, w=600, h=400) {
    const id = ++winId;
    const x = 60 + (id%5)*35, y = 40 + (id%5)*30;
    const el = document.createElement('div');
    el.className = 'win'; el.id = 'win-'+id;
    el.style.cssText = `left:${x}px;top:${y}px;width:${w}px;height:${h}px;z-index:${zIdx++}`;
    el.innerHTML = `<div class="win-bar" data-wid="${id}"><div class="win-dots"><button class="win-dot red" onclick="closeWin(${id})"></button><button class="win-dot yellow" onclick="minWin(${id})"></button><button class="win-dot green" onclick="maxWin(${id})"></button></div><span class="win-title">${icon} ${title}</span><div style="width:50px"></div></div><div class="win-body" id="wb-${id}">${html}</div>`;
    document.getElementById('desktop').appendChild(el);
    wins.set(id, {id,el,x,y,w,h,max:false,prev:null,title});
    // drag
    const bar = el.querySelector('.win-bar');
    bar.addEventListener('mousedown', e => { if(e.target.classList.contains('win-dot'))return; dragState={id,sx:e.clientX,sy:e.clientY,ox:el.offsetLeft,oy:el.offsetTop}; e.preventDefault(); });
    bar.addEventListener('dblclick', () => maxWin(id));
    el.addEventListener('mousedown', () => focusWin(id));
    focusWin(id);
    return id;
}

document.addEventListener('mousemove', e => {
    if (!dragState) return;
    const win = wins.get(dragState.id); if (!win) return;
    win.x = dragState.ox + e.clientX - dragState.sx;
    win.y = dragState.oy + e.clientY - dragState.sy;
    win.el.style.left = win.x+'px'; win.el.style.top = win.y+'px';
});
document.addEventListener('mouseup', () => { dragState = null; });

function focusWin(id) {
    wins.forEach(w => w.el.classList.remove('focused'));
    const w = wins.get(id); if(!w)return;
    w.el.classList.add('focused'); w.el.style.zIndex = zIdx++; focusedWin = id;
}
function closeWin(id) {
    const w = wins.get(id); if(!w)return;
    w.el.classList.add('closing');
    setTimeout(() => { w.el.remove(); wins.delete(id); if(focusedWin===id)focusedWin=null; }, 220);
}
function minWin(id) {
    const w = wins.get(id); if(!w)return;
    w.el.style.transition='all 0.25s ease'; w.el.style.opacity='0'; w.el.style.transform='scale(0.5) translateY(80px)';
    setTimeout(()=>{w.el.style.display='none';w.el.style.transition='';w.el.style.opacity='';w.el.style.transform='';},300);
}
function maxWin(id) {
    const w = wins.get(id); if(!w)return;
    if(w.max){const b=w.prev;w.el.style.cssText=`left:${b.x}px;top:${b.y}px;width:${b.w}px;height:${b.h}px;z-index:${zIdx++};transition:all 0.3s cubic-bezier(0.16,1,0.3,1)`;w.el.style.borderRadius='';w.max=false;}
    else{w.prev={x:w.x,y:w.y,w:w.w,h:w.h};w.el.style.cssText=`left:0;top:0;width:100vw;height:100vh;z-index:${zIdx++};border-radius:0;transition:all 0.3s cubic-bezier(0.16,1,0.3,1)`;w.max=true;}
    setTimeout(()=>w.el.style.transition='',350);
}
function tileAll() {
    const v=[]; wins.forEach(w=>{if(w.el.style.display!=='none')v.push(w);}); if(!v.length)return;
    const cols=Math.ceil(Math.sqrt(v.length)),rows=Math.ceil(v.length/cols);
    const cw=innerWidth/cols,ch=innerHeight/rows,p=6;
    v.forEach((w,i)=>{const c=i%cols,r=Math.floor(i/cols);w.el.style.transition='all 0.4s cubic-bezier(0.16,1,0.3,1)';w.el.style.left=(c*cw+p)+'px';w.el.style.top=(r*ch+p)+'px';w.el.style.width=(cw-p*2)+'px';w.el.style.height=(ch-p*2)+'px';w.el.style.borderRadius='';w.max=false;setTimeout(()=>w.el.style.transition='',450);});
    toast('Windows auto-tiled');
}

// ═══ Apps ═══
const APPS = {
    terminal: { title:'GravTerminal', icon:'▸_', w:680, h:420, render: () => {
        const lines = [
            '<span style="color:#555">GravityOS v0.1.0 Singularity — GravTerminal</span>',
            '<span style="color:#555">GravityCore microkernel · 64 syscalls · Capability-secured</span>',
            '<span style="color:#555">Type \'help\' for commands.</span>','',
            '<span class="prompt">gravity</span><span style="color:#444">@</span><span style="color:#6e56cf">singularity</span> <span class="path">~</span> <span style="color:#555">$</span> uname -a',
            '<span style="color:#888">GravityOS 0.1.0 Singularity x86_64 GravityCore 64-syscall microkernel</span>','',
            '<span class="prompt">gravity</span><span style="color:#444">@</span><span style="color:#6e56cf">singularity</span> <span class="path">~</span> <span style="color:#555">$</span> gravsec status',
            '<span style="color:#30a46c">● GravitySec: active (autonomous)</span>',
            '<span style="color:#888">  ├─ eBPF tracer: 42 processes</span>',
            '<span style="color:#888">  ├─ GravVault: TPM sealed</span>',
            '<span style="color:#888">  ├─ Firewall: per-process rules</span>',
            '<span style="color:#888">  └─ Threat level: <span style="color:#30a46c">nominal</span></span>','',
            '<span class="prompt">gravity</span><span style="color:#444">@</span><span style="color:#6e56cf">singularity</span> <span class="path">~</span> <span style="color:#555">$</span> neofetch',
            '<span style="color:#6e56cf">  ██████╗ </span> <span style="color:#888">OS: GravityOS 0.1.0 Singularity</span>',
            '<span style="color:#6e56cf">  ██╔═══╝ </span> <span style="color:#888">Kernel: GravityCore (microkernel)</span>',
            '<span style="color:#6e56cf">  ██║ ██╗ </span> <span style="color:#888">Arch: x86_64</span>',
            '<span style="color:#6e56cf">  ██║  ██║</span> <span style="color:#888">Shell: GravTerminal</span>',
            '<span style="color:#6e56cf">  ██████╔╝</span> <span style="color:#888">AI: GravMind 7B (INT4/NPU)</span>',
            '<span style="color:#6e56cf">  ╚═════╝ </span> <span style="color:#888">Security: GravitySec autonomous</span>','',
        ];
        return `<div class="app-terminal" id="term-out">${lines.map(l=>'<div>'+l+'</div>').join('')}<div><span class="prompt">gravity</span><span style="color:#444">@</span><span style="color:#6e56cf">singularity</span> <span class="path">~</span> <span style="color:#555">$</span> <input id="term-in" onkeydown="termCmd(event)" autofocus></div></div>`;
    }},
    files: { title:'GravFiles', icon:'📁', w:700, h:480, render: () => {
        const f = ['Documents 📁','Downloads 📂','Pictures 🖼','Music 🎵','Projects 📁','GravityOS 📁','report.pdf 📄','design.fig 🎨','notes.md 📝','config.yaml ⚙','photo.jpg 🖼','backup.tar.gz 📦'];
        return `<div style="display:flex;gap:8px;padding:0 0 10px;border-bottom:1px solid var(--g-border-subtle);margin-bottom:10px;align-items:center;font-size:12px;color:var(--g-text-tertiary)"><span>←</span><span>→</span><div style="flex:1;padding:4px 12px;background:var(--g-bg);border:1px solid var(--g-border);border-radius:999px;font-family:var(--g-font-mono);font-size:11px;color:var(--g-text-secondary)">/ home / nitin</div></div><div class="file-grid">${f.map(x=>{const[n,i]=x.split(' ');return`<div class="file-card"><span class="fi">${i}</span><span class="fn">${n}</span></div>`;}).join('')}</div><div style="display:flex;justify-content:space-between;padding:8px 0 0;border-top:1px solid var(--g-border-subtle);margin-top:8px;font-size:10px;color:var(--g-text-quaternary);font-family:var(--g-font-mono)"><span>12 items</span><span>GravFS · Encrypted</span></div>`;
    }},
    notes: { title:'GravNote', icon:'📝', w:580, h:460, render: () =>
        `<div style="height:100%;display:flex;flex-direction:column"><div style="display:flex;gap:6px;padding:0 0 8px;border-bottom:1px solid var(--g-border-subtle);margin-bottom:8px"><button style="font-size:10px;background:var(--g-bg-surface);border:1px solid var(--g-border);padding:3px 10px;border-radius:4px;cursor:pointer;color:var(--g-text-secondary);text-transform:uppercase;letter-spacing:0.04em">+ New</button><button style="font-size:10px;background:transparent;border:none;padding:3px 10px;cursor:pointer;color:var(--g-text-quaternary);text-transform:uppercase;letter-spacing:0.04em">All</button><div style="flex:1"></div><span style="font-size:10px;color:var(--g-text-quaternary)">✦ AI Summarize</span></div><div class="app-notes" style="flex:1"><textarea placeholder="Start typing... Markdown supported.\n\n# My Note\n\nGravMind remembers context across all your notes.\n\n- [ ] Review kernel scheduler\n- [x] PMM buddy allocator ✓\n\n> 'Every element must earn its place.'"></textarea></div><div style="display:flex;justify-content:space-between;padding:6px 0 0;border-top:1px solid var(--g-border-subtle);margin-top:6px;font-size:9px;color:var(--g-text-quaternary);font-family:var(--g-font-mono)"><span>UTF-8 · Markdown</span><span>Auto-saved</span></div></div>`
    ,},
    browser: { title:'GravBrowser', icon:'◎', w:850, h:560, render: () =>
        `<div style="display:flex;flex-direction:column;height:100%;margin:-16px"><div style="display:flex;gap:6px;padding:8px;background:var(--g-bg-surface);border-bottom:1px solid var(--g-border-subtle);align-items:center"><span style="cursor:pointer;color:var(--g-text-quaternary)">←</span><span style="cursor:pointer;color:var(--g-text-quaternary)">→</span><span style="cursor:pointer;color:var(--g-text-quaternary)">⟳</span><input style="flex:1;padding:5px 14px;border-radius:999px;background:var(--g-bg);border:1px solid var(--g-border);color:var(--g-text-secondary);font-family:var(--g-font-mono);font-size:11px;outline:none" value="gravity://home"><span style="cursor:pointer;color:var(--g-text-quaternary)">☰</span></div><div style="flex:1;display:flex;align-items:center;justify-content:center;flex-direction:column;gap:20px"><div style="font-family:var(--g-font-display);font-size:42px;font-weight:200;color:var(--g-text-primary)">GravBrowser</div><div style="color:var(--g-text-quaternary);font-size:13px">Privacy-first · AI-assisted · Zero tracking</div><div style="display:flex;gap:16px;margin-top:8px">${['📰 News','🔒 Privacy','✦ AI Summary'].map(t=>`<div style="text-align:center;cursor:pointer;padding:14px 16px;border-radius:10px;background:var(--g-bg-surface);min-width:72px"><div style="font-size:20px;margin-bottom:4px">${t.split(' ')[0]}</div><div style="font-size:9px;color:var(--g-text-quaternary);text-transform:uppercase;letter-spacing:0.06em">${t.split(' ')[1]}</div></div>`).join('')}</div></div></div>`
    ,},
    media: { title:'GravMedia', icon:'♫', w:460, h:380, render: () =>
        `<div style="display:flex;flex-direction:column;height:100%;align-items:center;justify-content:center;gap:20px"><div style="width:140px;height:140px;border-radius:16px;background:linear-gradient(135deg,#1a1a2e,#16213e,#0f3460);display:flex;align-items:center;justify-content:center;font-size:40px;box-shadow:0 12px 40px rgba(0,0,0,0.4)">♫</div><div style="text-align:center"><div style="font-size:16px;font-weight:500;color:var(--g-text-bright)">Gravity Waves</div><div style="font-size:11px;color:var(--g-text-quaternary);margin-top:2px">GravityOS — Ambient</div></div><div style="width:70%;height:2px;background:var(--g-border);border-radius:1px"><div style="width:35%;height:100%;background:var(--g-accent);border-radius:1px"></div></div><div style="display:flex;gap:20px;align-items:center"><button style="background:transparent;border:none;color:var(--g-text-tertiary);font-size:18px;cursor:pointer">⏮</button><button style="background:var(--g-text-primary);border:none;color:var(--g-bg);width:40px;height:40px;border-radius:50%;font-size:16px;cursor:pointer;display:flex;align-items:center;justify-content:center">▶</button><button style="background:transparent;border:none;color:var(--g-text-tertiary);font-size:18px;cursor:pointer">⏭</button></div></div>`
    ,},
    settings: { title:'Settings', icon:'⚙', w:560, h:480, render: () => {
        const s = (l,v) => `<div class="settings-row"><span class="sl">${l}</span><span class="sv">${v}</span></div>`;
        const t = (l,on) => `<div class="settings-row"><span class="sl">${l}</span><div class="toggle${on?' on':''}" onclick="this.classList.toggle('on')"></div></div>`;
        return `<div style="max-height:100%;overflow-y:auto"><div class="settings-group"><div class="settings-group-title">System</div>${s('Version','0.1.0 Singularity')}${s('Kernel','GravityCore (64 syscalls)')}${s('Architecture','x86_64')}</div><div class="settings-group"><div class="settings-group-title">Gravity AI</div>${t('GravMind',true)}${t('Voice ("Hey Gravity")',true)}${t('GravSight (Screen AI)',false)}${t('Privacy Mode',true)}${t('Cloud Sync',false)}</div><div class="settings-group"><div class="settings-group-title">Security</div>${s('GravitySec','<span style="color:var(--g-green)">● Active</span>')}${s('GravVault','<span style="color:var(--g-green)">● TPM Sealed</span>')}${t('Firewall',true)}${s('Secure Boot','<span style="color:var(--g-green)">● Verified</span>')}</div><div class="settings-group"><div class="settings-group-title">Display</div>${s('Theme','Monochrome Dark')}${t('Gravity Field',true)}${t('Animations',true)}</div></div>`;
    }},
    sysmon: { title:'System Health', icon:'♥', w:480, h:400, render: () =>
        `<div style="font-family:var(--g-font-mono);font-size:12px"><div style="font-family:var(--g-font-display);font-size:18px;font-weight:300;margin-bottom:14px;color:var(--g-text-bright)">System Health</div><div style="display:grid;grid-template-columns:1fr 1fr;gap:10px">${[['CPU','47°C','12% · 8 cores'],['Memory','5.2 GB','of 16 GB · 32%'],['Disk','98%','GravFS encrypted'],['Security','<span style="color:var(--g-green)">● Nominal</span>','42 procs monitored']].map(([t,v,d])=>`<div style="padding:14px;background:var(--g-bg-surface);border-radius:10px"><div style="font-size:9px;color:var(--g-text-quaternary);text-transform:uppercase;letter-spacing:0.06em;margin-bottom:6px">${t}</div><div style="font-size:22px;font-weight:300;color:var(--g-text-bright)">${v}</div><div style="color:var(--g-text-quaternary);font-size:10px;margin-top:3px">${d}</div></div>`).join('')}</div><div style="margin-top:12px;padding:10px;background:var(--g-bg-surface);border-radius:8px"><div style="font-size:9px;color:var(--g-text-quaternary);text-transform:uppercase;letter-spacing:0.06em;margin-bottom:4px">GravMind AI</div><div style="color:var(--g-text-tertiary);font-size:11px">7B params · INT4 · NPU accelerated · 340ms latency</div></div></div>`
    ,}
};

function launchApp(id) { const a = APPS[id]; if (!a) return toast('Unknown: '+id); createWin(a.title, a.icon, a.render(), a.w, a.h); }
function showAbout() {
    createWin('About','✦',`<div style="display:flex;flex-direction:column;align-items:center;justify-content:center;height:100%;text-align:center;gap:12px"><div style="font-family:var(--g-font-display);font-size:56px;font-weight:200;color:var(--g-text-bright)">G</div><div style="font-family:var(--g-font-display);font-size:22px;letter-spacing:0.12em;font-weight:300;color:var(--g-text-bright)">GRAVITY OS</div><div style="font-family:var(--g-font-mono);font-size:10px;color:var(--g-text-quaternary)">v0.1.0 — Codename: Singularity</div><div style="color:var(--g-text-tertiary);max-width:280px;font-size:12px;line-height:1.7;margin-top:4px">The first OS that is simultaneously maximally compatible and maximally minimal. AI is not an add-on — it is the shell.</div><div style="display:flex;gap:12px;margin-top:8px">${['GravityCore','64 Syscalls','Capability Security'].map(t=>`<span style="font-size:9px;color:var(--g-text-quaternary);text-transform:uppercase;letter-spacing:0.06em;font-family:var(--g-font-mono)">${t}</span>`).join('')}</div><div style="font-size:9px;color:var(--g-text-quaternary);font-family:var(--g-font-mono);margin-top:16px">MIT License · 2026 · Nitin Raj</div></div>`,400,380);
}

// ═══ Terminal Commands ═══
function termCmd(e) {
    if (e.key !== 'Enter') return;
    const inp = e.target, cmd = inp.value.trim(), out = document.getElementById('term-out');
    if (!cmd) return;
    const line = `<div><span class="prompt">gravity</span><span style="color:#444">@</span><span style="color:#6e56cf">singularity</span> <span class="path">~</span> <span style="color:#555">$</span> ${cmd}</div>`;
    let res = '';
    const cmds = {
        help: '<span style="color:#888">Available: help, uname, whoami, date, uptime, clear, ls, cat, echo, neofetch, gravsec, gravmind, exit</span>',
        uname: '<span style="color:#888">GravityOS 0.1.0 Singularity x86_64 GravityCore</span>',
        whoami: '<span style="color:#888">gravity</span>',
        date: `<span style="color:#888">${new Date().toString()}</span>`,
        uptime: '<span style="color:#888">up 0 days, '+Math.floor(Math.random()*12)+':'+String(Math.floor(Math.random()*60)).padStart(2,'0')+', load: 0.12, 0.08, 0.05</span>',
        ls: '<span style="color:#6e56cf">Documents/</span>  <span style="color:#6e56cf">Downloads/</span>  <span style="color:#6e56cf">Projects/</span>  <span style="color:#888">config.yaml  notes.md  report.pdf</span>',
        clear: '__CLEAR__',
        gravmind: '<span style="color:#888">GravMind: 7B parameters · INT4 quantized · NPU accelerated · Status: <span style="color:#30a46c">ready</span></span>',
    };
    if (cmd === 'clear') { out.innerHTML = ''; }
    else {
        res = cmds[cmd] || `<span style="color:var(--g-red)">gsh: command not found: ${cmd}</span>`;
        inp.parentElement.innerHTML = `<span class="prompt">gravity</span><span style="color:#444">@</span><span style="color:#6e56cf">singularity</span> <span class="path">~</span> <span style="color:#555">$</span> ${cmd}`;
        out.insertAdjacentHTML('beforeend', `<div>${res}</div><div><span class="prompt">gravity</span><span style="color:#444">@</span><span style="color:#6e56cf">singularity</span> <span class="path">~</span> <span style="color:#555">$</span> <input id="term-in" onkeydown="termCmd(event)" autofocus></div>`);
        document.getElementById('term-in')?.focus();
    }
    out.scrollTop = out.scrollHeight;
}
