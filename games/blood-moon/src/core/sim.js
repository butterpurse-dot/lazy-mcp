import { RULES, DIFFICULTY } from '../config/rules.js';
import { MAP, buildRows } from '../config/map.js';
import { TEAMS, TEAM_IDS, other } from '../config/teams.js';
import { WEAPONS, ARMOR, canUse } from '../config/weapons.js';
import { Grid } from './grid.js';
import { Match } from './match.js';
import { Emitter } from './emitter.js';
import { RADIUS, rayVsActor, calcDamage, applyDamage } from './combat.js';
import { createActor, weaponOf, ammoOf, giveWeapon, giveArmor, resetLoadout, refillAmmo } from './actor.js';
import { createBrain, think } from './bot.js';
import { emptyIntent, eyeOf, forward } from './util.js';

export { emptyIntent, eyeOf, forward };

const DEG = Math.PI / 180;

/**
 * Authoritative game world. No rendering/DOM here: the renderer, HUD and audio subscribe to
 * `sim.bus` events and read state - so the same class can later run on a server for multiplayer.
 */
export class Sim {
  constructor(opts = {}) {
    const o = { rules: RULES, map: MAP, playerTeam: 'vampire', difficulty: 'normal', autopilot: false, rng: Math.random, ...opts };
    this.rules = o.rules; this.map = o.map; this.rng = o.rng; this.difficulty = o.difficulty;
    this.grid = new Grid(buildRows(o.map), o.map.cell, o.map.zones);
    this.match = new Match(o.rules);
    this.bus = new Emitter();
    this.time = 0; this.sigil = null; this.plan = { site: 'A', planter: 0, cleanser: 0 };
    this.intent = emptyIntent(); this.player = null; this.actors = []; this._spotT = 0; this._planT = 0; this._seen = null;
    for (const team of TEAM_IDS) {
      const names = [...TEAMS[team].names].sort(() => this.rng() - 0.5);
      for (let i = 0; i < o.rules.teamSize; i++) {
        const a = createActor({ name: names[i % names.length], team, bot: true });
        a.brain = createBrain(this.difficulty, this.rng);
        this.actors.push(a);
      }
    }
    if (!o.autopilot) this.setPlayer(o.playerTeam);
    this.bus.on('shot', (e) => this.hear(e.actor));
    this.restart();
  }

  /** Hand control of one actor to the human (or null = all bots / attract mode). Call restart() afterwards. */
  setPlayer(team) {
    if (this.player) { this.player.bot = true; this.player.brain = createBrain(this.difficulty, this.rng); this.player = null; }
    if (!team) return;
    const a = this.actors.find((x) => x.team === team);
    a.bot = false; a.brain = null; this.player = a; this.intent = emptyIntent();
  }

  setDifficulty(d) {
    this.difficulty = d;
    for (const a of this.actors) if (a.brain) a.brain.diff = DIFFICULTY[d] ?? DIFFICULTY.normal;
  }

  restart() {
    this.match.reset();
    for (const a of this.actors) { a.kills = a.deaths = 0; a.money = this.rules.startMoney; a.alive = false; }
    this.beginRound();
  }

  // ---------- round flow ----------
  beginRound() {
    const m = this.match; m.startRound(); this.sigil = null; this._seen = null;
    for (const team of TEAM_IDS) {
      const pts = [...this.map.spawns[team]].sort(() => this.rng() - 0.5);
      this.actors.filter((a) => a.team === team).forEach((a, i) => this.spawn(a, pts[i % pts.length], team));
    }
    this.planRound();
    for (const a of this.actors) if (a.bot) this.botBuy(a);
    this.bus.emit('roundStart', { round: m.round });
  }

  spawn(a, [cx, cz], team) {
    if (this.match.round === 1 || !a.alive) resetLoadout(a); else refillAmmo(a);
    const t = TEAMS[team];
    a.maxHp = a.hp = t.maxHp; a.alive = true;
    a.pos = { x: this.grid.center(cx), y: 0, z: this.grid.center(cz) }; a.vel = { x: 0, y: 0, z: 0 };
    a.yaw = team === 'werewolf' ? Math.PI : 0; a.pitch = 0; a.punch = 0; a.recoil = 0;
    a.fireCd = 0.3; a.reloading = false; a.abilityCd = 0; a.howlT = 0; a.dashT = 0; a.actionT = 0; a.actionType = null;
    a.spotted = { vampire: 0, werewolf: 0 }; a.killer = null; a.grounded = true; a.deathT = 0;
    if (a.brain) { Object.assign(a.brain, { path: null, goalKey: '', target: null, investigate: null, guard: null, hold: null, stuckN: 0 }); }
  }

  planRound() {
    const rnd = (r) => { for (let i = 0; i < 40; i++) { const cx = r[0] + Math.floor(this.rng() * (r[2] - r[0] + 1)), cz = r[1] + Math.floor(this.rng() * (r[3] - r[1] + 1)); if (!this.grid.solid(cx, cz)) return { x: this.grid.center(cx), z: this.grid.center(cz) }; } return null; };
    this.plan.site = this.rng() < 0.5 ? 'A' : 'B';
    const sites = ['A', 'B', 'A', 'B', 'M'].sort(() => this.rng() - 0.5);
    let i = 0;
    for (const a of this.actors) {
      if (!a.brain) continue;
      a.brain.hold = a.team === 'vampire' ? rnd(this.map.holds[this.rng() < 0.75 ? this.plan.site : 'M']) : rnd(this.map.holds[sites[i++ % sites.length]]);
      a.brain.watchYaw = null;
    }
    this.updatePlan();
  }

  updatePlan() {
    const near = (team, p) => { let best = null, bd = Infinity; for (const a of this.actors) if (a.alive && a.team === team) { const d = (a.pos.x - p.x) ** 2 + (a.pos.z - p.z) ** 2; if (d < bd) { bd = d; best = a; } } return best; };
    const pl = this.actors.find((a) => a.id === this.plan.planter);
    if (!pl || !pl.alive) this.plan.planter = near('vampire', this.grid.zoneCenter(this.plan.site))?.id ?? 0;
    if (this.sigil) this.plan.cleanser = near('werewolf', this.sigil)?.id ?? 0;
  }

  botBuy(a) {
    const m = a.money, t = a.team, has = !!a.loadout.primary;
    const afford = (id) => canUse(WEAPONS[id], t) && WEAPONS[id].price + (a.armor < 50 ? ARMOR.price : 0) <= m;
    if (!has) {
      const pick = ['silverbolt', t === 'vampire' ? 'bloodcarbine' : 'moonrifle', this.rng() < 0.5 ? 'gravegun' : 'cryptsmg', 'cryptsmg'].filter(afford);
      const choice = pick[this.rng() < 0.15 && pick.length > 1 ? 1 : (pick[0] === 'silverbolt' && this.rng() < 0.7 ? 1 : 0)] ?? pick[0];
      if (choice) this.buy(a, choice);
    }
    if (a.armor < 50) this.buy(a, 'armor');
  }

  /** Purchase validation lives here so UI, bots (and later a server) share one code path. */
  buy(a, id) {
    if (!a.alive || !this.match.buyOpen) return { ok: false, reason: 'Buy time is over' };
    if (id === 'armor') {
      if (a.armor >= ARMOR.value) return { ok: false, reason: 'Already armored' };
      if (a.money < ARMOR.price) return { ok: false, reason: 'Not enough money' };
      a.money -= ARMOR.price; giveArmor(a); return { ok: true };
    }
    const w = WEAPONS[id];
    if (!w || !w.price) return { ok: false, reason: 'Unknown item' };
    if (!canUse(w, a.team)) return { ok: false, reason: `Not available to ${TEAMS[a.team].name}` };
    if (a.loadout[w.slot] === id) return { ok: false, reason: 'Already owned' };
    if (a.money < w.price) return { ok: false, reason: 'Not enough money' };
    a.money -= w.price; giveWeapon(a, id);
    return { ok: true };
  }

  // ---------- main loop ----------
  update(dt) {
    dt = Math.min(dt, 0.05); this.time += dt;
    const m = this.match;
    if (m.phase === 'matchover') return;
    this.stepSpotting(dt);
    if ((this._planT -= dt) <= 0) { this._planT = 0.5; this.updatePlan(); }
    for (const a of this.actors) {
      if (!a.alive) { a.deathT += dt; continue; }
      this.step(a, a === this.player ? this.intent : think(this, a, dt), dt);
    }
    this.separate();
    const it = this.intent; it.reload = it.ability = false; it.slot = null;
    const ev = m.tick(dt, this.aliveCounts());
    this.checkEnd();
    if (ev === 'live') this.bus.emit('live', {});
    else if (ev === 'next') this.beginRound();
    else if (ev === 'matchOver') this.bus.emit('matchOver', { winner: m.winner });
  }

  aliveCounts() {
    const c = { vampire: 0, werewolf: 0 };
    for (const a of this.actors) if (a.alive) c[a.team]++;
    return c;
  }

  checkEnd() {
    const r = this.match.result;
    if (!r || r === this._seen) return;
    this._seen = r;
    if (r.reason === 'detonated' && this.sigil) {
      this.bus.emit('explode', { x: this.sigil.x, z: this.sigil.z });
      for (const a of this.actors) {
        if (a.alive && Math.hypot(a.pos.x - this.sigil.x, a.pos.z - this.sigil.z) < this.rules.sigilRadius) this.kill(a, null, null, false);
      }
    }
    for (const a of this.actors) a.money = Math.min(this.rules.maxMoney, a.money + r.rewards[a.team]);
    this.bus.emit('roundEnd', r);
  }

  // ---------- per-actor simulation ----------
  step(a, it, dt) {
    const m = this.match, team = TEAMS[a.team];
    const frozen = m.phase === 'freeze' || m.phase === 'idle', canAct = m.phase === 'live' || m.phase === 'ended';
    // An expiring cooldown carries its sub-tick overshoot into the next shot so real fire rate matches the weapon spec
    // (clamping to 0 rounds every interval up to whole ticks: a 10/s rifle fired 8.6/s at 60 Hz). Idle guns bank nothing.
    a.fireCd = a.fireCd > 0 ? Math.max(-dt, a.fireCd - dt) : 0; a.abilityCd = Math.max(0, a.abilityCd - dt); a.howlT = Math.max(0, a.howlT - dt);
    a.hurtT = Math.max(0, a.hurtT - dt); a.recoil = Math.max(0, a.recoil - 10 * dt); a.punch *= Math.exp(-7 * dt);
    for (const t of TEAM_IDS) a.spotted[t] = Math.max(0, a.spotted[t] - dt);

    // weapon switch / reload bookkeeping
    if (it.slot && a.loadout[it.slot] && it.slot !== a.slot) { a.slot = it.slot; a.reloading = false; a.fireCd = 0.35; this.bus.emit('switch', { actor: a }); }
    const w = weaponOf(a), am = ammoOf(a);
    if (a.reloading) { a.reloadT -= dt; if (a.reloadT <= 0) this.finishReload(a, w, am); }
    a.scoped = !!(w.scoped && it.scope && !a.reloading);

    // objective action (plant / cleanse): holds the actor in place
    let acting = null;
    if (m.phase === 'live' && it.use && a.grounded) {
      if (a.team === 'vampire' && !this.sigil && this.grid.zoneAt(a.pos.x, a.pos.z)) acting = 'plant';
      else if (a.team === 'werewolf' && this.sigil && Math.hypot(a.pos.x - this.sigil.x, a.pos.z - this.sigil.z) < 2.6) acting = 'cleanse';
    }
    if (acting) {
      a.actionType = acting; a.actionT += dt;
      if (acting === 'plant' && a.actionT >= this.rules.plantTime) this.plantSigil(a);
      else if (acting === 'cleanse' && a.actionT >= this.rules.cleanseTime) { m.cleanse(); this.bus.emit('cleanse', { actor: a }); a.actionT = 0; }
    } else { a.actionT = 0; a.actionType = null; }

    // ability
    if (canAct && it.ability && a.abilityCd <= 0) this.useAbility(a, it);

    // movement
    const sy = Math.sin(a.yaw), cy = Math.cos(a.yaw);
    let wx = it.mx * cy + it.mz * -sy, wz = it.mx * -sy + it.mz * -cy;
    const wl = Math.hypot(wx, wz); if (wl > 1) { wx /= wl; wz /= wl; }
    let speed = team.speed * w.moveMul * (it.walk ? 0.5 : 1) * (a.howlT > 0 ? TEAMS.werewolf.ability.speedMul : 1);
    if (a.scoped) speed *= 0.6;
    if (frozen || acting || m.phase === 'matchover') { wx = wz = 0; }
    if (a.dashT > 0) { a.dashT -= dt; a.vel.x = a.dashDir.x * 36; a.vel.z = a.dashDir.z * 36; }
    else { const k = Math.min(1, (a.grounded ? 14 : 2.5) * dt); a.vel.x += (wx * speed - a.vel.x) * k; a.vel.z += (wz * speed - a.vel.z) * k; }
    if (it.jump && a.grounded && !frozen && !acting) { a.vel.y = 7; a.grounded = false; }
    a.vel.y -= 22 * dt; a.pos.y += a.vel.y * dt;
    if (a.pos.y <= 0) { a.pos.y = 0; a.vel.y = 0; a.grounded = true; }
    const moved = this.grid.move(a.pos, a.vel.x * dt, a.vel.z * dt, RADIUS);
    if (a.grounded && !it.walk && moved > 0) {
      a.stepDist += moved;
      if (a.stepDist > 2.3) { a.stepDist = 0; this.bus.emit('step', { actor: a }); }
    }

    // firing
    const wantFire = it.fire && (w.auto || w.melee || !a.prevFire);
    a.prevFire = it.fire;
    if (canAct && !acting && !a.reloading) {
      if (wantFire && a.fireCd <= 1e-6) this.fire(a, w, am);
      else if (!w.melee && am.mag <= 0 && am.reserve > 0) this.startReload(a, w);
      else if (it.reload && !w.melee && am.mag < w.mag && am.reserve > 0) this.startReload(a, w);
    }
  }

  separate() {
    const A = this.actors;
    for (let i = 0; i < A.length; i++) {
      if (!A[i].alive) continue;
      for (let j = i + 1; j < A.length; j++) {
        if (!A[j].alive) continue;
        const dx = A[j].pos.x - A[i].pos.x, dz = A[j].pos.z - A[i].pos.z, d = Math.hypot(dx, dz);
        if (d > 0.01 && d < 0.8) { const k = (0.8 - d) * 0.25 / d; this.grid.move(A[i].pos, -dx * k, -dz * k, RADIUS); this.grid.move(A[j].pos, dx * k, dz * k, RADIUS); }
      }
    }
  }

  plantSigil(a) {
    const site = this.grid.zoneAt(a.pos.x, a.pos.z);
    if (!site || !this.match.plant(site)) return;
    this.sigil = { site, x: a.pos.x, z: a.pos.z, planter: a.id };
    a.money = Math.min(this.rules.maxMoney, a.money + this.rules.plantReward);
    a.actionT = 0; a.actionType = null;
    this.bus.emit('plant', { actor: a, site });
    this.updatePlan();
    for (const b of this.actors) if (b.brain && b.team === 'werewolf' && b.alive) b.brain.investigate = { x: this.sigil.x, z: this.sigil.z, t: 8 };
  }

  useAbility(a, it) {
    const ab = TEAMS[a.team].ability;
    a.abilityCd = ab.cooldown;
    if (ab.id === 'shadowstep') {
      let dx = it.mx * Math.cos(a.yaw) - it.mz * Math.sin(a.yaw), dz = -it.mx * Math.sin(a.yaw) - it.mz * Math.cos(a.yaw);
      if (!dx && !dz) { dx = -Math.sin(a.yaw); dz = -Math.cos(a.yaw); }
      const l = Math.hypot(dx, dz); a.dashDir = { x: dx / l, z: dz / l }; a.dashT = ab.distance / 36;
    } else {
      a.howlT = ab.duration;
      for (const e of this.actors) if (e.alive && e.team !== a.team) e.spotted[a.team] = ab.reveal;
    }
    this.bus.emit('ability', { actor: a, id: ab.id });
  }

  startReload(a, w) {
    const am = a.ammo[w.id];
    if (a.reloading || am.mag >= w.mag || am.reserve <= 0) return;
    a.reloading = true; a.reloadT = w.reload; this.bus.emit('reload', { actor: a });
  }

  finishReload(a, w, am) {
    const n = Math.min(w.mag - am.mag, am.reserve);
    am.mag += n; am.reserve -= n; a.reloading = false;
  }

  fire(a, w, am) {
    if (w.melee) return this.melee(a, w);
    if (am.mag <= 0) { a.fireCd = 0.25; this.bus.emit('dry', { actor: a }); return; }
    am.mag--; a.fireCd = Math.min(0, a.fireCd) + 1 / w.rate;
    const speedFrac = Math.min(1, Math.hypot(a.vel.x, a.vel.z) / TEAMS[a.team].speed);
    let spread = w.spread + w.moveSpread * speedFrac + (a.grounded ? 0 : 4) + a.recoil;
    if (w.scoped && !a.scoped) spread += 5;
    if (a.brain) spread += a.brain.diff.extraSpread * (0.55 + 0.45 * speedFrac);
    const pitch = a.pitch + a.punch, sy = Math.sin(a.yaw), cy = Math.cos(a.yaw), sp = Math.sin(pitch), cp = Math.cos(pitch);
    const f = forward(a.yaw, pitch), r = { x: cy, z: -sy }, up = { x: sy * sp, y: cp, z: cy * sp };
    const o = eyeOf(a), ends = [];
    for (let i = 0; i < w.pellets; i++) {
      const ang = Math.tan(spread * DEG * Math.sqrt(this.rng())), phi = this.rng() * Math.PI * 2, c = Math.cos(phi) * ang, s = Math.sin(phi) * ang;
      let dx = f.x + r.x * c + up.x * s, dy = f.y + up.y * s, dz = f.z + r.z * c + up.z * s;
      const l = Math.hypot(dx, dy, dz); dx /= l; dy /= l; dz /= l;
      ends.push(this.trace(a, w, o, { x: dx, y: dy, z: dz }));
    }
    a.recoil = Math.min(6, a.recoil + w.kick * 0.5);
    const k = w.kick * DEG * (0.85 + 0.3 * this.rng());
    a.punch = Math.min(0.14, a.punch + k); a.yaw += (this.rng() - 0.5) * k * 0.6;
    this.bus.emit('shot', { actor: a, weapon: w, origin: o, ends });
  }

  trace(a, w, o, d) {
    const wall = this.grid.raycast(o.x, o.y, o.z, d.x, d.y, d.z, w.range);
    let best = wall ? wall.dist : w.range, victim = null, head = false;
    for (const e of this.actors) {
      if (!e.alive || e.team === a.team) continue;
      const h = rayVsActor(o, d, e, best);
      if (h && h.t < best) { best = h.t; victim = e; head = h.head; }
    }
    const end = { x: o.x + d.x * best, y: o.y + d.y * best, z: o.z + d.z * best, hit: victim ? 'actor' : wall ? wall.kind : null };
    if (victim) this.damage(a, victim, w, calcDamage(w, best, head), head, end);
    return end;
  }

  melee(a, w) {
    a.fireCd = Math.min(0, a.fireCd) + 1 / w.rate;
    const f = forward(a.yaw, a.pitch), o = eyeOf(a);
    const wall = this.grid.raycast(o.x, o.y, o.z, f.x, f.y, f.z, w.range);
    let best = wall ? wall.dist : w.range, victim = null;
    for (const e of this.actors) {
      if (!e.alive || e.team === a.team) continue;
      const h = rayVsActor(o, f, e, best);
      if (h && h.t < best) { best = h.t; victim = e; }
    }
    const end = { x: o.x + f.x * best, y: o.y + f.y * best, z: o.z + f.z * best, hit: victim ? 'actor' : wall ? wall.kind : null };
    if (victim) {
      const back = -Math.sin(victim.yaw) * f.x + -Math.cos(victim.yaw) * f.z > 0.5;
      this.damage(a, victim, w, w.damage * (back ? 2.5 : 1), false, end);
    }
    this.bus.emit('shot', { actor: a, weapon: w, origin: o, ends: [end], melee: true });
  }

  damage(attacker, victim, w, dmg, head, point) {
    const lost = applyDamage(victim, dmg, w.pen);
    victim.hurtT = 0.4; victim.hurtFrom = { x: attacker.pos.x, z: attacker.pos.z };
    if (victim.brain) victim.brain.investigate = { x: attacker.pos.x, z: attacker.pos.z, t: 6 };
    this.bus.emit('hit', { attacker, victim, damage: lost, head, point });
    if (victim.hp <= 0) this.kill(victim, attacker, w, head);
  }

  kill(v, k, w, head) {
    v.alive = false; v.hp = 0; v.deaths++; v.deathT = 0; v.killer = k; v.actionT = 0; v.actionType = null; v.vel.x = v.vel.z = 0;
    if (k) {
      k.kills++;
      k.money = Math.min(this.rules.maxMoney, k.money + (w?.killReward ?? this.rules.killReward));
      k.hp = Math.min(k.maxHp, k.hp + TEAMS[k.team].killHeal);
    }
    this.bus.emit('kill', { killer: k, victim: v, weapon: w, head });
  }

  // ---------- awareness ----------
  hear(shooter) {
    for (const b of this.actors) {
      if (!b.brain || !b.alive || b.team === shooter.team || b.brain.target) continue;
      if (Math.hypot(b.pos.x - shooter.pos.x, b.pos.z - shooter.pos.z) < 32) b.brain.investigate = { x: shooter.pos.x, z: shooter.pos.z, t: 5 };
    }
  }

  /** Teammate line-of-sight feeds the radar (and Howl reveals through walls). */
  stepSpotting(dt) {
    if ((this._spotT -= dt) > 0) return;
    this._spotT = 0.2;
    for (const e of this.actors) {
      if (!e.alive) continue;
      const t = other(e.team);
      if (e.spotted[t] > 0.25) continue;
      for (const s of this.actors) {
        if (!s.alive || s.team !== t) continue;
        const dx = e.pos.x - s.pos.x, dz = e.pos.z - s.pos.z, d = Math.hypot(dx, dz);
        if (d > 80 || (-Math.sin(s.yaw) * dx - Math.cos(s.yaw) * dz) / (d || 1) < 0.5) continue;
        if (this.grid.visible(eyeOf(s), { x: e.pos.x, y: e.pos.y + 1.2, z: e.pos.z })) { e.spotted[t] = 0.9; break; }
      }
    }
  }
}
