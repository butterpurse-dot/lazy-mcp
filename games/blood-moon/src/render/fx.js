import * as THREE from 'three';
import { glowTexture, sigilTexture } from './textures.js';

const MAX_P = 320;

export class Fx {
  constructor(scene) {
    this.scene = scene; this.t = 0;
    const glow = glowTexture();
    // tracers
    this.tracers = Array.from({ length: 48 }, () => {
      const m = new THREE.Mesh(new THREE.BoxGeometry(0.025, 0.025, 1), new THREE.MeshBasicMaterial({ color: 0xffd9a0, transparent: true, blending: THREE.AdditiveBlending, depthWrite: false, fog: false }));
      m.visible = false; m.userData.life = 0; scene.add(m); return m;
    });
    this.ti = 0;
    // muzzle flashes
    this.flashes = Array.from({ length: 10 }, () => {
      const s = new THREE.Sprite(new THREE.SpriteMaterial({ map: glow, blending: THREE.AdditiveBlending, depthWrite: false, depthTest: false }));
      s.visible = false; s.userData.life = 0; scene.add(s); return s;
    });
    this.fi = 0;
    this.flashLight = new THREE.PointLight(0xffb060, 0, 14, 2); scene.add(this.flashLight); this.flashLife = 0;
    // particles
    this.pos = new Float32Array(MAX_P * 3); this.col = new Float32Array(MAX_P * 3); this.vel = new Float32Array(MAX_P * 3);
    this.life = new Float32Array(MAX_P); this.grav = new Float32Array(MAX_P); this.pi = 0;
    this.pos.fill(-999);
    const g = new THREE.BufferGeometry();
    g.setAttribute('position', new THREE.BufferAttribute(this.pos, 3)); g.setAttribute('color', new THREE.BufferAttribute(this.col, 3));
    this.points = new THREE.Points(g, new THREE.PointsMaterial({ size: 0.09, vertexColors: true, transparent: true, depthWrite: false }));
    this.points.frustumCulled = false; scene.add(this.points);
    // explosion
    this.blast = new THREE.Mesh(new THREE.SphereGeometry(1, 20, 14), new THREE.MeshBasicMaterial({ color: 0xff2a14, transparent: true, opacity: 0, blending: THREE.AdditiveBlending, depthWrite: false, fog: false }));
    this.blast.visible = false; this.blastT = 0; this.blastR = 14; scene.add(this.blast);
    this.blastLight = new THREE.PointLight(0xff3a20, 0, 60, 1.5); scene.add(this.blastLight);
    // sigil
    this.sigil = new THREE.Group(); this.sigil.visible = false;
    const disc = new THREE.Mesh(new THREE.PlaneGeometry(4.4, 4.4), new THREE.MeshBasicMaterial({ map: sigilTexture(), transparent: true, blending: THREE.AdditiveBlending, depthWrite: false }));
    disc.rotation.x = -Math.PI / 2; disc.position.y = 0.05;
    const beam = new THREE.Mesh(new THREE.CylinderGeometry(0.1, 0.5, 7, 12, 1, true), new THREE.MeshBasicMaterial({ color: 0xff2040, transparent: true, opacity: 0.3, blending: THREE.AdditiveBlending, depthWrite: false, side: THREE.DoubleSide }));
    beam.position.y = 3.5;
    this.sigilLight = new THREE.PointLight(0xff1a3a, 30, 18, 1.6); this.sigilLight.position.y = 1.2;
    this.sigil.add(disc, beam, this.sigilLight); this.sigil.userData = { disc, beam }; scene.add(this.sigil);
  }

  tracer(from, to, color) {
    const m = this.tracers[this.ti++ % this.tracers.length];
    const len = Math.hypot(to.x - from.x, to.y - from.y, to.z - from.z);
    if (len < 0.5) return;
    m.position.set((from.x + to.x) / 2, (from.y + to.y) / 2, (from.z + to.z) / 2);
    m.lookAt(to.x, to.y, to.z); m.scale.set(1, 1, len); m.material.color.setHex(color);
    m.visible = true; m.userData.life = 0.07;
  }

  flash(p, size = 0.5) {
    const s = this.flashes[this.fi++ % this.flashes.length];
    s.position.set(p.x, p.y, p.z); s.scale.setScalar(size * (0.8 + Math.random() * 0.5)); s.visible = true; s.userData.life = 0.05;
    this.flashLight.position.set(p.x, p.y, p.z); this.flashLight.intensity = 14; this.flashLife = 0.05;
  }

  burst(p, kind, n = 8) {
    for (let k = 0; k < n; k++) {
      const i = this.pi++ % MAX_P, a = Math.random() * 6.283, s = (kind === 'blood' ? 1.5 : 2.5) * Math.random() + 0.5;
      this.pos.set([p.x, p.y, p.z], i * 3);
      this.vel.set([Math.cos(a) * s, Math.random() * s * 0.9 + 0.4, Math.sin(a) * s], i * 3);
      this.life[i] = 0.35 + Math.random() * 0.35; this.grav[i] = kind === 'blood' ? 9 : 5;
      if (kind === 'blood') this.col.set([0.6 + Math.random() * 0.3, 0.02, 0.05], i * 3); else this.col.set([1, 0.7 + Math.random() * 0.3, 0.3], i * 3);
    }
  }

  explode(x, z, radius) {
    this.blast.position.set(x, 0.5, z); this.blastR = radius; this.blastT = 1.4; this.blast.visible = true; this.blastLight.position.set(x, 3, z);
    this.burst({ x, y: 1, z }, 'spark', 60);
  }

  setSigil(s) {
    this.sigil.visible = !!s;
    if (s) this.sigil.position.set(s.x, 0, s.z);
  }

  update(dt, sigilFrac = 1) {
    this.t += dt;
    for (const m of this.tracers) if (m.visible && (m.userData.life -= dt) <= 0) m.visible = false;
    for (const s of this.flashes) if (s.visible && (s.userData.life -= dt) <= 0) s.visible = false;
    if (this.flashLife > 0 && (this.flashLife -= dt) <= 0) this.flashLight.intensity = 0;
    let dirty = false;
    for (let i = 0; i < MAX_P; i++) {
      if (this.life[i] <= 0) continue;
      this.life[i] -= dt; dirty = true;
      if (this.life[i] <= 0) { this.pos[i * 3 + 1] = -999; continue; }
      this.vel[i * 3 + 1] -= this.grav[i] * dt;
      for (let k = 0; k < 3; k++) this.pos[i * 3 + k] += this.vel[i * 3 + k] * dt;
      if (this.pos[i * 3 + 1] < 0.02) { this.pos[i * 3 + 1] = 0.02; this.vel[i * 3 + 1] *= -0.2; }
    }
    if (dirty) { this.points.geometry.attributes.position.needsUpdate = true; this.points.geometry.attributes.color.needsUpdate = true; }
    if (this.blastT > 0) {
      this.blastT -= dt; const k = 1 - this.blastT / 1.4;
      this.blast.scale.setScalar(Math.max(0.1, this.blastR * Math.min(1, k * 2.2))); this.blast.material.opacity = Math.max(0, 0.7 * (1 - k));
      this.blastLight.intensity = Math.max(0, 600 * (1 - k)); if (this.blastT <= 0) this.blast.visible = false;
    }
    if (this.sigil.visible) {
      const { disc, beam } = this.sigil.userData, rate = 4 + (1 - sigilFrac) * 14;
      disc.rotation.z += dt * 0.6; const p = 0.6 + 0.4 * Math.sin(this.t * rate);
      beam.material.opacity = 0.15 + 0.3 * p; this.sigilLight.intensity = 14 + 30 * p;
    }
  }
}
