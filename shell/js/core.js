/**
 * GravityOS — Premium Shell Engine (Core)
 * Copyright (c) 2026 GravityOS Contributors — MIT License
 */

// ═══ State ═══
let winId = 0, zIdx = 10, focusedWin = null, dragState = null, fieldOn = true;
const wins = new Map();
const memories = [];

// ═══ Gravity Field (Particle Wallpaper) ═══
const canvas = document.getElementById('gravity-canvas');
const ctx = canvas.getContext('2d');
let particles = [];

function resizeCanvas() { canvas.width = innerWidth; canvas.height = innerHeight; }
resizeCanvas(); addEventListener('resize', resizeCanvas);

let mx = -999, my = -999, mActive = false;
canvas.addEventListener('mousemove', e => { mx = e.clientX; my = e.clientY; mActive = true; });
canvas.addEventListener('mouseleave', () => mActive = false);

function initParticles() {
    particles = [];
    for (let i = 0; i < 600; i++) {
        particles.push({
            x: Math.random() * canvas.width, y: Math.random() * canvas.height,
            vx: (Math.random() - 0.5) * 0.3, vy: (Math.random() - 0.5) * 0.3,
            s: 0.5 + Math.random() * 1.5, o: 0.1 + Math.random() * 0.3, l: Math.random()
        });
    }
}

function drawField() {
    if (!fieldOn) { ctx.clearRect(0, 0, canvas.width, canvas.height); requestAnimationFrame(drawField); return; }
    ctx.clearRect(0, 0, canvas.width, canvas.height);
    const w = canvas.width, h = canvas.height;
    for (const p of particles) {
        if (mActive) {
            const dx = mx - p.x, dy = my - p.y, d = Math.sqrt(dx*dx + dy*dy);
            if (d < 180 && d > 1) { const f = 0.015 * (1 - d/180); p.vx += dx/d*f; p.vy += dy/d*f; }
        }
        p.vx *= 0.985; p.vy *= 0.985; p.x += p.vx; p.y += p.vy;
        if (p.x < 0) p.x = w; if (p.x > w) p.x = 0; if (p.y < 0) p.y = h; if (p.y > h) p.y = 0;
        p.o = 0.12 + 0.18 * Math.sin(p.l * 6.28 + performance.now() * 0.0008); p.l += 0.0002;
        ctx.globalAlpha = p.o; ctx.fillStyle = '#fff';
        ctx.beginPath(); ctx.arc(p.x, p.y, p.s, 0, 6.28); ctx.fill();
    }
    // connections
    ctx.strokeStyle = 'rgba(255,255,255,0.025)'; ctx.lineWidth = 0.5;
    for (let i = 0; i < particles.length; i += 3) {
        for (let j = i + 3; j < particles.length; j += 3) {
            const a = particles[i], b = particles[j];
            const dx = a.x-b.x, dy = a.y-b.y, d2 = dx*dx+dy*dy;
            if (d2 < 6400) { ctx.globalAlpha = 0.03*(1-Math.sqrt(d2)/80); ctx.beginPath(); ctx.moveTo(a.x,a.y); ctx.lineTo(b.x,b.y); ctx.stroke(); }
        }
    }
    ctx.globalAlpha = 1;
    requestAnimationFrame(drawField);
}

function toggleField() { fieldOn = !fieldOn; toast(fieldOn ? 'Gravity Field enabled' : 'Gravity Field disabled'); }

// ═══ Clock ═══
function updateClock() {
    const now = new Date();
    const h = String(now.getHours()).padStart(2,'0'), m = String(now.getMinutes()).padStart(2,'0'), s = String(now.getSeconds()).padStart(2,'0');
    const el = document.getElementById('bar-clock');
    if (el) el.textContent = h+':'+m+':'+s;
    const lt = document.getElementById('login-time');
    if (lt) lt.textContent = h+':'+m;
    const ld = document.getElementById('login-date');
    if (ld) { const days = ['Sunday','Monday','Tuesday','Wednesday','Thursday','Friday','Saturday']; const months = ['January','February','March','April','May','June','July','August','September','October','November','December']; ld.textContent = days[now.getDay()]+', '+months[now.getMonth()]+' '+now.getDate(); }
}
setInterval(updateClock, 1000); updateClock();

// ═══ System Indicators ═══
setInterval(() => {
    const c = document.getElementById('bar-cpu'), m = document.getElementById('bar-mem'), b = document.getElementById('bar-bat');
    if (c) c.textContent = 'CPU '+(5+Math.random()*12|0)+'%';
    if (m) m.textContent = 'RAM '+(28+Math.random()*8|0)+'%';
    if (b) b.textContent = 'BAT '+(85+Math.random()*10|0)+'%';
}, 4000);

// ═══ Toast ═══
function toast(msg, dur=3000) {
    const c = document.getElementById('toasts');
    const el = document.createElement('div');
    el.className = 'toast-item'; el.textContent = msg; c.appendChild(el);
    setTimeout(() => { el.classList.add('out'); setTimeout(() => el.remove(), 250); }, dur);
}

// ═══ Notifications ═══
function toggleNotifs() {
    document.getElementById('notif-panel').classList.toggle('open');
}

// ═══ Context Menu ═══
const ctxMenu = document.getElementById('ctx-menu');
document.addEventListener('contextmenu', e => {
    e.preventDefault();
    ctxMenu.style.left = Math.min(e.clientX, innerWidth-220)+'px';
    ctxMenu.style.top = Math.min(e.clientY, innerHeight-250)+'px';
    ctxMenu.classList.add('show');
});
document.addEventListener('click', () => ctxMenu.classList.remove('show'));

// ═══ Proximity: Dock & Bar ═══
document.addEventListener('mousemove', e => {
    const dock = document.getElementById('dock'), bar = document.getElementById('system-bar');
    if (e.clientY > innerHeight - 8) dock.classList.add('visible');
    else if (e.clientY < innerHeight - 90) dock.classList.remove('visible');
    if (e.clientY < 6) bar.classList.add('visible');
    else if (e.clientY > 44) bar.classList.remove('visible');
    // close notif panel on left click away
    if (e.clientX < innerWidth - 330) document.getElementById('notif-panel').classList.remove('open');
});
