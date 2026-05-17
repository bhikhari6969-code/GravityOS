/**
 * GravityOS — Gravity Field
 * Physics-based particle wallpaper system.
 * Particles react to mouse/touch with gravitational attraction.
 * Copyright (c) 2026 GravityOS Contributors — MIT License
 */

class GravityField {
    constructor(canvasId) {
        this.canvas = document.getElementById(canvasId);
        this.ctx = this.canvas.getContext('2d');
        this.particles = [];
        this.mouse = { x: -1000, y: -1000, active: false };
        this.config = {
            particleCount: 800,
            maxSpeed: 0.4,
            mouseRadius: 200,
            mouseForce: 0.02,
            friction: 0.98,
            connectionDistance: 100,
            connectionOpacity: 0.04,
            particleMinSize: 0.5,
            particleMaxSize: 2,
            baseColor: { r: 255, g: 255, b: 255 },
        };
        this.running = false;
        this.resize();
        window.addEventListener('resize', () => this.resize());
        this.canvas.addEventListener('mousemove', e => {
            this.mouse.x = e.clientX;
            this.mouse.y = e.clientY;
            this.mouse.active = true;
        });
        this.canvas.addEventListener('mouseleave', () => {
            this.mouse.active = false;
        });
        this.canvas.addEventListener('touchmove', e => {
            e.preventDefault();
            this.mouse.x = e.touches[0].clientX;
            this.mouse.y = e.touches[0].clientY;
            this.mouse.active = true;
        }, { passive: false });
        this.canvas.addEventListener('touchend', () => {
            this.mouse.active = false;
        });
    }

    resize() {
        this.canvas.width = window.innerWidth;
        this.canvas.height = window.innerHeight;
    }

    init() {
        this.particles = [];
        const { particleCount, maxSpeed, particleMinSize, particleMaxSize } = this.config;
        for (let i = 0; i < particleCount; i++) {
            this.particles.push({
                x: Math.random() * this.canvas.width,
                y: Math.random() * this.canvas.height,
                vx: (Math.random() - 0.5) * maxSpeed,
                vy: (Math.random() - 0.5) * maxSpeed,
                size: particleMinSize + Math.random() * (particleMaxSize - particleMinSize),
                opacity: 0.1 + Math.random() * 0.4,
                life: Math.random(),
            });
        }
    }

    start() {
        if (this.running) return;
        this.running = true;
        this.init();
        this.loop();
    }

    stop() {
        this.running = false;
    }

    loop() {
        if (!this.running) return;
        this.update();
        this.draw();
        requestAnimationFrame(() => this.loop());
    }

    update() {
        const { mouseRadius, mouseForce, friction } = this.config;
        const w = this.canvas.width;
        const h = this.canvas.height;

        for (const p of this.particles) {
            // Mouse gravitational pull
            if (this.mouse.active) {
                const dx = this.mouse.x - p.x;
                const dy = this.mouse.y - p.y;
                const dist = Math.sqrt(dx * dx + dy * dy);
                if (dist < mouseRadius && dist > 1) {
                    const force = mouseForce * (1 - dist / mouseRadius);
                    p.vx += (dx / dist) * force;
                    p.vy += (dy / dist) * force;
                }
            }

            // Apply friction
            p.vx *= friction;
            p.vy *= friction;

            // Update position
            p.x += p.vx;
            p.y += p.vy;

            // Wrap around edges
            if (p.x < 0) p.x = w;
            if (p.x > w) p.x = 0;
            if (p.y < 0) p.y = h;
            if (p.y > h) p.y = 0;

            // Gentle oscillation
            p.opacity = 0.15 + 0.25 * Math.sin(p.life * Math.PI * 2 + performance.now() * 0.001);
            p.life += 0.0002;
        }
    }

    draw() {
        const ctx = this.ctx;
        const { connectionDistance, connectionOpacity, baseColor } = this.config;

        ctx.clearRect(0, 0, this.canvas.width, this.canvas.height);

        // Draw connections
        ctx.strokeStyle = `rgba(${baseColor.r}, ${baseColor.g}, ${baseColor.b}, ${connectionOpacity})`;
        ctx.lineWidth = 0.5;
        for (let i = 0; i < this.particles.length; i++) {
            for (let j = i + 1; j < this.particles.length; j++) {
                const a = this.particles[i];
                const b = this.particles[j];
                const dx = a.x - b.x;
                const dy = a.y - b.y;
                const distSq = dx * dx + dy * dy;
                if (distSq < connectionDistance * connectionDistance) {
                    const dist = Math.sqrt(distSq);
                    const alpha = connectionOpacity * (1 - dist / connectionDistance);
                    ctx.globalAlpha = alpha;
                    ctx.beginPath();
                    ctx.moveTo(a.x, a.y);
                    ctx.lineTo(b.x, b.y);
                    ctx.stroke();
                }
            }
        }

        // Draw particles
        for (const p of this.particles) {
            ctx.globalAlpha = p.opacity;
            ctx.fillStyle = `rgb(${baseColor.r}, ${baseColor.g}, ${baseColor.b})`;
            ctx.beginPath();
            ctx.arc(p.x, p.y, p.size, 0, Math.PI * 2);
            ctx.fill();
        }

        ctx.globalAlpha = 1;
    }

    setColor(r, g, b) {
        this.config.baseColor = { r, g, b };
    }
}

// Export
window.GravityField = GravityField;
