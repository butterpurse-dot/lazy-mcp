// All audio is synthesised with WebAudio - zero asset files, zero licensing risk, tiny download.
const clamp = (v, a, b) => (v < a ? a : v > b ? b : v);

const SHOTS = {
  pistol:  { f: 2600, d: 0.13, g: 0.5, thump: 150 },
  smg:     { f: 3200, d: 0.09, g: 0.42, thump: 170 },
  rifle:   { f: 1900, d: 0.16, g: 0.65, thump: 120 },
  shotgun: { f: 1100, d: 0.3, g: 0.95, thump: 80 },
  sniper:  { f: 900, d: 0.7, g: 1, thump: 60 },
  melee:   { f: 5000, d: 0.12, g: 0.25, thump: 0 },
};

export class Sfx {
  constructor() { this.ctx = null; this.vol = 0.6; this.l = { x: 0, z: 0, yaw: 0 }; }

  unlock() {
    if (!this.ctx) {
      const AC = window.AudioContext || window.webkitAudioContext;
      if (!AC) return;
      this.ctx = new AC(); this.master = this.ctx.createGain(); this.master.gain.value = this.vol; this.master.connect(this.ctx.destination);
      const len = this.ctx.sampleRate * 1.5, buf = this.ctx.createBuffer(1, len, this.ctx.sampleRate), d = buf.getChannelData(0);
      for (let i = 0; i < len; i++) d[i] = Math.random() * 2 - 1;
      this.noise = buf; this.ambient();
    }
    if (this.ctx.state === 'suspended') this.ctx.resume();
  }

  setVolume(v) { this.vol = clamp(v, 0, 1); if (this.master) this.master.gain.value = this.vol; }
  listen(x, z, yaw) { this.l.x = x; this.l.z = z; this.l.yaw = yaw; }

  /** Output node for a sound, spatialised against the listener when `p` is given. */
  out(p) {
    const c = this.ctx, g = c.createGain();
    if (!p) { g.connect(this.master); return { node: g, att: 1 }; }
    const dx = p.x - this.l.x, dz = p.z - this.l.z, d = Math.hypot(dx, dz), att = 1 / (1 + d / 9);
    const pan = c.createStereoPanner ? c.createStereoPanner() : null;
    if (pan) { pan.pan.value = clamp((dx * Math.cos(this.l.yaw) - dz * Math.sin(this.l.yaw)) / (d + 2), -1, 1); g.connect(pan); pan.connect(this.master); } else g.connect(this.master);
    return { node: g, att };
  }

  burst({ f, d, g, type = 'lowpass', q = 0.7, p, sweep = 0 }) {
    if (!this.ctx) return;
    const c = this.ctx, t = c.currentTime, { node, att } = this.out(p);
    const src = c.createBufferSource(); src.buffer = this.noise; src.playbackRate.value = 0.8 + Math.random() * 0.4;
    const fl = c.createBiquadFilter(); fl.type = type; fl.frequency.setValueAtTime(f, t); fl.Q.value = q;
    if (sweep) fl.frequency.exponentialRampToValueAtTime(Math.max(40, f * sweep), t + d);
    node.gain.setValueAtTime(g * att, t); node.gain.exponentialRampToValueAtTime(0.001, t + d);
    src.connect(fl); fl.connect(node); src.start(t, Math.random()); src.stop(t + d + 0.05);
  }

  tone({ f, f2 = f, d, g, type = 'sine', p, at = 0 }) {
    if (!this.ctx) return;
    const c = this.ctx, t = c.currentTime + at, { node, att } = this.out(p), o = c.createOscillator();
    o.type = type; o.frequency.setValueAtTime(f, t); o.frequency.exponentialRampToValueAtTime(Math.max(20, f2), t + d);
    node.gain.setValueAtTime(g * att, t); node.gain.exponentialRampToValueAtTime(0.001, t + d);
    o.connect(node); o.start(t); o.stop(t + d + 0.05);
  }

  shot(cls, p) {
    const s = SHOTS[cls] ?? SHOTS.pistol;
    this.burst({ f: s.f, d: s.d, g: s.g, p, sweep: 0.3 });
    if (s.thump) this.tone({ f: s.thump, f2: 40, d: s.d * 1.2, g: s.g * 0.9, p });
    if (cls === 'melee') this.burst({ f: 800, d: 0.15, g: 0.3, type: 'bandpass', sweep: 4, p });
  }
  hit(head) { this.tone({ f: head ? 1800 : 1100, f2: head ? 1500 : 900, d: 0.07, g: 0.35, type: 'square' }); }
  hurt() { this.tone({ f: 120, f2: 50, d: 0.18, g: 0.7 }); this.burst({ f: 600, d: 0.1, g: 0.3 }); }
  step(p) { this.burst({ f: 380, d: 0.07, g: 0.22, p }); }
  reload(p) { this.tone({ f: 900, f2: 600, d: 0.04, g: 0.2, type: 'square', p }); this.tone({ f: 500, f2: 300, d: 0.05, g: 0.25, type: 'square', p, at: 0.45 }); }
  dry(p) { this.tone({ f: 700, f2: 650, d: 0.03, g: 0.2, type: 'square', p }); }
  step2() {}
  dash(p) { this.burst({ f: 500, d: 0.35, g: 0.7, type: 'bandpass', sweep: 6, q: 1.2, p }); }
  howl(p) { this.tone({ f: 260, f2: 620, d: 0.6, g: 0.5, type: 'sawtooth', p }); this.tone({ f: 620, f2: 380, d: 0.8, g: 0.45, type: 'sawtooth', p, at: 0.55 }); }
  beep(hurry) { this.tone({ f: hurry ? 1500 : 1000, f2: hurry ? 1500 : 1000, d: 0.08, g: 0.3, type: 'square' }); }
  plant() { [0, 0.12, 0.24].forEach((at, i) => this.tone({ f: 400 + i * 150, f2: 400 + i * 150, d: 0.18, g: 0.35, type: 'triangle', at })); }
  boom() { this.burst({ f: 500, d: 2.2, g: 1, sweep: 0.05 }); this.tone({ f: 70, f2: 22, d: 1.8, g: 1 }); }
  win(good) { (good ? [523, 659, 784, 1046] : [392, 330, 262, 196]).forEach((f, i) => this.tone({ f, f2: f, d: 0.4, g: 0.3, type: 'triangle', at: i * 0.14 })); }
  click() { this.tone({ f: 1200, f2: 900, d: 0.04, g: 0.2, type: 'square' }); }

  ambient() {
    const c = this.ctx, src = c.createBufferSource(); src.buffer = this.noise; src.loop = true;
    const f = c.createBiquadFilter(); f.type = 'lowpass'; f.frequency.value = 260; const g = c.createGain(); g.gain.value = 0.07;
    const lfo = c.createOscillator(), lg = c.createGain(); lfo.frequency.value = 0.12; lg.gain.value = 0.04; lfo.connect(lg); lg.connect(g.gain);
    src.connect(f); f.connect(g); g.connect(this.master); src.start(); lfo.start();
  }
}
