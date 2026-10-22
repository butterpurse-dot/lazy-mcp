import * as THREE from 'three';
import { buildWorld } from './world.js';
import { buildCharacter, buildViewmodel, buildGun, setViewGun, disposeTree } from './models.js';
import { nameTexture } from './textures.js';
import { Fx } from './fx.js';
import { TEAMS } from '../config/teams.js';
import { weaponOf } from '../core/actor.js';
import { EYE } from '../core/combat.js';
import { forward } from '../core/util.js';

const BASE_FOV = 82;

/** Reads sim state each frame and draws it. Never mutates the simulation. */
export class Renderer {
  constructor(canvas, sim) {
    this.sim = sim; this.canvas = canvas;
    this.gl = new THREE.WebGLRenderer({ canvas, antialias: true, powerPreference: 'high-performance' });
    this.gl.setPixelRatio(Math.min(window.devicePixelRatio || 1, 1.75));
    this.gl.toneMapping = THREE.ACESFilmicToneMapping; this.gl.toneMappingExposure = 1.35;
    this.scene = new THREE.Scene();
    this.camera = new THREE.PerspectiveCamera(BASE_FOV, 1, 0.05, 400); this.camera.rotation.order = 'YXZ';
    this.scene.add(this.camera);
    this.world = buildWorld(this.scene, sim.grid, sim.map);
    this.fx = new Fx(this.scene);
    this.chars = new Map(); this.vms = {};
    for (const team of Object.keys(TEAMS)) { const vm = buildViewmodel(team); vm.visible = false; vm.renderOrder = 999; vm.traverse((o) => (o.renderOrder = 999)); this.camera.add(vm); this.vms[team] = vm; }
    for (const a of sim.actors) this.addActor(a);
    this.kick = 0; this.bob = 0; this.fov = BASE_FOV; this.shake = 0; this.t = 0; this.viewId = -1; this.lastPos = null;
    this.subscribe();
    this.resize(); this._onResize = () => this.resize(); window.addEventListener('resize', this._onResize);
  }

  addActor(a) {
    const c = buildCharacter(a.team); c.root.visible = false; this.scene.add(c.root);
    c.label = new THREE.Sprite(new THREE.SpriteMaterial({ map: nameTexture(a.name, TEAMS[a.team].css), depthTest: false, transparent: true }));
    c.label.scale.set(1.3, 0.33, 1); c.label.position.set(0, 2.3, 0); c.label.renderOrder = 10; c.root.add(c.label);
    this.chars.set(a.id, c);
  }

  subscribe() {
    const { bus } = this.sim;
    bus.on('shot', ({ actor, weapon, origin, ends, melee }) => {
      const fps = actor.id === this.viewId && !this.chase;
      if (fps && !melee) this.kick = 1;
      const f = forward(actor.yaw, actor.pitch + actor.punch), r = { x: Math.cos(actor.yaw), z: -Math.sin(actor.yaw) };
      const mz = fps ? 0.75 : 0.9 + (weapon.cls === 'sniper' ? 0.5 : 0);
      const muzzle = { x: origin.x + f.x * mz + r.x * 0.2, y: origin.y + f.y * mz - 0.17, z: origin.z + f.z * mz + r.z * 0.2 };
      if (!melee) { this.fx.flash(muzzle, weapon.cls === 'shotgun' ? 0.9 : 0.55); }
      const color = actor.team === 'vampire' ? 0xff6070 : 0xa0c8ff;
      for (const e of ends) {
        if (!melee) this.fx.tracer(muzzle, e, color);
        if (e.hit === 'wall') this.fx.burst(e, 'spark', 5);
        else if (e.hit === 'actor') this.fx.burst(e, 'blood', 9);
      }
    });
    bus.on('hit', ({ victim }) => { if (victim.id === this.viewId) this.shake = 0.5; });
    bus.on('explode', ({ x, z }) => { this.fx.explode(x, z, this.sim.rules.sigilRadius); this.shake = 1.5; });
    bus.on('roundStart', () => { this.fx.setSigil(null); });
    bus.on('plant', () => this.fx.setSigil(this.sim.sigil));
  }

  resize() {
    const w = this.canvas.clientWidth || window.innerWidth, h = this.canvas.clientHeight || window.innerHeight;
    this.gl.setSize(w, h, false); this.camera.aspect = w / h; this.camera.updateProjectionMatrix();
  }

  /** Choose what the camera shows. view = { actor, chase } */
  render(dt, view) {
    const sim = this.sim, a = view.actor; this.t += dt; this.chase = view.chase; this.viewId = a.id;
    this.world.flicker(this.t);
    const fuse = sim.match.sigil ? sim.match.sigil.fuse / sim.rules.sigilFuse : 1;
    this.fx.update(dt, fuse);

    // characters
    for (const act of sim.actors) {
      const c = this.chars.get(act.id), self = act === a && !view.chase;
      c.root.visible = !self && (act.alive || act.deathT < 6);
      if (!c.root.visible) continue;
      c.root.position.set(act.pos.x, act.pos.y, act.pos.z); c.root.rotation.y = act.yaw;
      const sp = Math.hypot(act.vel.x, act.vel.z), ph = (c.phase = (c.phase || 0) + sp * dt * 2.6), sw = Math.min(1, sp / 4) * 0.8;
      c.legL.rotation.x = Math.sin(ph) * sw; c.legR.rotation.x = -Math.sin(ph) * sw;
      c.body.position.y = act.alive ? Math.abs(Math.sin(ph)) * 0.04 * Math.min(1, sp / 4) : 0;
      c.armL.rotation.x = c.armR.rotation.x = -1.3 + (act.fireCd > 0.05 ? 0.12 : 0) - (act.pitch * 0.6);
      c.head.rotation.x = act.pitch * 0.7;
      const w = weaponOf(act);
      if (c.gunCls !== w.cls) { for (const g of c.gun.children) disposeTree(g); c.gun.clear(); c.gun.add(buildGun(w.cls)); c.gunCls = w.cls; }
      c.root.rotation.x = act.alive ? 0 : -Math.min(1, act.deathT * 3) * Math.PI / 2;
      for (const m of c.mats) m.emissive.copy(c.glow).r += act.hurtT > 0 ? act.hurtT * 2 : 0;
      c.label.visible = act.alive && act.team === a.team && act !== a;
    }

    // camera
    const eye = { x: a.pos.x, y: a.pos.y + EYE, z: a.pos.z }, cam = this.camera;
    let fov = BASE_FOV;
    if (view.chase) {
      const f = forward(a.yaw, 0), want = { x: eye.x - f.x * 3.6, y: eye.y + 0.7, z: eye.z - f.z * 3.6 };
      const dx = want.x - eye.x, dy = want.y - eye.y, dz = want.z - eye.z, len = Math.hypot(dx, dy, dz);
      const hit = sim.grid.raycast(eye.x, eye.y, eye.z, dx / len, dy / len, dz / len, len);
      const k = hit ? Math.max(0.15, (hit.dist - 0.2) / len) : 1;
      cam.position.set(eye.x + dx * k, eye.y + dy * k, eye.z + dz * k);
      cam.rotation.set(-0.18, a.yaw, 0);
    } else {
      this.shake = Math.max(0, this.shake - dt * 3);
      const s = this.shake * 0.012;
      cam.position.set(eye.x, eye.y, eye.z);
      cam.rotation.set(a.pitch + a.punch + (Math.random() - 0.5) * s, a.yaw + (Math.random() - 0.5) * s, 0);
      if (a.scoped) fov = 26; else if (a.dashT > 0) fov = BASE_FOV + 14; else if (a.howlT > 0) fov = BASE_FOV + 5;
    }
    this.fov += (fov - this.fov) * Math.min(1, dt * 14);
    if (Math.abs(cam.fov - this.fov) > 0.01) { cam.fov = this.fov; cam.updateProjectionMatrix(); }

    // viewmodel
    for (const t of Object.keys(this.vms)) this.vms[t].visible = false;
    if (!view.chase && a.alive && !a.scoped) {
      const vm = this.vms[a.team], w = weaponOf(a), { holder } = vm.userData;
      vm.visible = true; setViewGun(vm, w.cls);
      const sp = Math.hypot(a.vel.x, a.vel.z); this.bob += sp * dt * 1.7;
      this.kick = Math.max(0, this.kick - dt * 9);
      const reload = a.reloading ? Math.sin(Math.min(1, (w.reload - a.reloadT) / w.reload) * Math.PI) : 0;
      const draw = Math.max(0, a.fireCd - 0.05) > 0.25 ? 0.2 : 0;
      vm.scale.setScalar(0.8);
      vm.position.set(0.24 + Math.sin(this.bob) * 0.008 * Math.min(1, sp / 4), -0.22 - Math.abs(Math.cos(this.bob)) * 0.01 * Math.min(1, sp / 4) - reload * 0.28 - draw, -0.55 + this.kick * 0.07);
      vm.rotation.set(this.kick * 0.1 + reload * 0.5, 0, -reload * 0.4);
      holder.position.z = w.cls === 'sniper' ? 0.1 : 0;
    }
    this.gl.render(this.scene, cam);
  }

  dispose() { window.removeEventListener('resize', this._onResize); this.gl.dispose(); }
}
