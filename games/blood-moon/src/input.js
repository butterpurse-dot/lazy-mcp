const LOOK = 0.0022; // radians per pixel at sensitivity 1

/** Keyboard + mouse -> sim.intent and view angles. Keeps no game rules. */
export class Input {
  constructor(canvas, getSim) {
    this.canvas = canvas; this.getSim = getSim; this.keys = new Set(); this.sens = 1; this.locked = false;
    this.enabled = false; this.onKey = () => {}; this.onLockChange = () => {}; this.onClick = () => {};
    document.addEventListener('keydown', (e) => this.key(e, true));
    document.addEventListener('keyup', (e) => this.key(e, false));
    document.addEventListener('mousemove', (e) => this.look(e));
    document.addEventListener('mousedown', (e) => this.mouse(e, true));
    document.addEventListener('mouseup', (e) => this.mouse(e, false));
    document.addEventListener('contextmenu', (e) => this.enabled && e.preventDefault());
    document.addEventListener('pointerlockchange', () => { this.locked = document.pointerLockElement === canvas; this.onLockChange(this.locked); });
    window.addEventListener('blur', () => this.reset());
  }

  lock() { try { const p = this.canvas.requestPointerLock(); if (p?.catch) p.catch(() => {}); } catch { /* denied */ } }
  unlock() { if (document.pointerLockElement) document.exitPointerLock(); }

  reset() {
    this.keys.clear(); const it = this.getSim().intent;
    Object.assign(it, { mx: 0, mz: 0, walk: false, jump: false, fire: false, use: false, scope: false });
  }

  key(e, down) {
    if (!this.enabled || e.repeat) return;
    if (['Tab', 'Space', 'F1', 'F3'].includes(e.code) || e.code.startsWith('Arrow')) e.preventDefault();
    down ? this.keys.add(e.code) : this.keys.delete(e.code);
    const it = this.getSim().intent;
    if (down) {
      if (e.code === 'KeyR') it.reload = true;
      else if (e.code === 'KeyF') it.ability = true;
      else if (e.code === 'Digit1') it.slot = 'primary';
      else if (e.code === 'Digit2') it.slot = 'secondary';
      else if (e.code === 'Digit3') it.slot = 'melee';
      this.onKey(e.code, true);
    } else this.onKey(e.code, false);
    this.axes();
  }

  axes() {
    const k = this.keys, it = this.getSim().intent;
    it.mz = (k.has('KeyW') || k.has('ArrowUp') ? 1 : 0) - (k.has('KeyS') || k.has('ArrowDown') ? 1 : 0);
    it.mx = (k.has('KeyD') || k.has('ArrowRight') ? 1 : 0) - (k.has('KeyA') || k.has('ArrowLeft') ? 1 : 0);
    it.walk = k.has('ShiftLeft') || k.has('ShiftRight'); it.jump = k.has('Space'); it.use = k.has('KeyE');
  }

  look(e) {
    if (!this.locked || !this.enabled) return;
    const sim = this.getSim(), p = sim.player;
    if (!p || !p.alive) return;
    const k = LOOK * this.sens * (p.scoped ? 0.4 : 1);
    p.yaw -= e.movementX * k; p.pitch = Math.max(-1.5, Math.min(1.5, p.pitch - e.movementY * k));
  }

  mouse(e, down) {
    if (!this.enabled) return;
    const it = this.getSim().intent;
    if (e.button === 0) { it.fire = down && this.locked; if (down) this.onClick(); }
    else if (e.button === 2) it.scope = down && this.locked;
  }
}
