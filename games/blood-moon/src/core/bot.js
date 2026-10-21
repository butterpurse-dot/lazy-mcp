import { DIFFICULTY } from '../config/rules.js';
import { emptyIntent, eyeOf } from './util.js';
import { weaponOf, ammoOf } from './actor.js';

const TAU = Math.PI * 2;
const angDiff = (a, b) => { let d = (b - a) % TAU; if (d > Math.PI) d -= TAU; if (d < -Math.PI) d += TAU; return d; };
const clamp = (v, a, b) => (v < a ? a : v > b ? b : v);

export function createBrain(difficulty, rng) {
  return {
    diff: DIFFICULTY[difficulty] ?? DIFFICULTY.normal, path: null, pi: 0, goalKey: '', repathT: 0,
    target: null, lostT: 0, reactT: 0, aimHead: false, scanT: rng() * 0.12,
    strafeDir: 0, strafeT: 0, investigate: null, hold: null, guard: null, watchYaw: null, lookT: 0, lookYaw: 0,
    stuckT: 0, stuckN: 0, lastX: 0, lastZ: 0,
  };
}

function canSee(sim, a, b, fov) {
  const dx = b.pos.x - a.pos.x, dz = b.pos.z - a.pos.z, d = Math.hypot(dx, dz);
  if (d > 85) return false;
  if (d > 5 && (-Math.sin(a.yaw) * dx - Math.cos(a.yaw) * dz) / d < Math.cos((fov / 2) * Math.PI / 180)) return false;
  return sim.grid.visible(eyeOf(a), { x: b.pos.x, y: b.pos.y + 1.2, z: b.pos.z });
}

function pickGuard(sim, a, b) {
  const s = sim.sigil;
  for (let i = 0; i < 25; i++) {
    const x = s.x + (sim.rng() - 0.5) * 16, z = s.z + (sim.rng() - 0.5) * 16;
    if (sim.grid.hitsWall(x, z, 0.5)) continue;
    if (sim.grid.visible({ x, y: 1.5, z }, { x: s.x, y: 1, z: s.z })) return { x, z };
  }
  return { x: s.x, z: s.z };
}

/** Decide where to go and whether to hold the "use" key. */
function chooseGoal(sim, a, b) {
  const m = sim.match, inv = b.investigate;
  const s = sim.sigil;
  if (inv && inv.t > 0) {
    // attackers escort the planter, defenders stay near their post until the sigil is down
    const leash = b.hold ? Math.hypot(inv.x - b.hold.x, inv.z - b.hold.z) : 0;
    if (a.team === 'werewolf' ? !s && leash < 22 : a.id !== sim.plan.planter && !s) return { x: inv.x, z: inv.z };
  }
  if (a.team === 'vampire') {
    if (s) return { ...(b.guard ??= pickGuard(sim, a, b)) };
    if (a.id === sim.plan.planter) { const z = sim.grid.zoneCenter(sim.plan.site); return { ...z, use: !!sim.grid.zoneAt(a.pos.x, a.pos.z) && Math.hypot(a.pos.x - z.x, a.pos.z - z.z) < 5 }; }
    return b.hold ?? { x: a.pos.x, z: a.pos.z };
  }
  if (s) {
    if (a.id === sim.plan.cleanser) return { x: s.x, z: s.z, use: Math.hypot(a.pos.x - s.x, a.pos.z - s.z) < 2.2 };
    return { ...(b.guard ??= pickGuard(sim, a, b)) };
  }
  return b.hold ?? { x: a.pos.x, z: a.pos.z };
}

export function think(sim, a, dt) {
  const it = emptyIntent(), b = a.brain, m = sim.match, w = weaponOf(a), am = ammoOf(a);
  if (m.phase === 'freeze' || m.phase === 'matchover') return it;
  if (b.investigate) { b.investigate.t -= dt; if (b.investigate.t <= 0 || Math.hypot(a.pos.x - b.investigate.x, a.pos.z - b.investigate.z) < 2) b.investigate = null; }
  b.repathT -= dt;

  // --- perception (throttled) ---
  if ((b.scanT -= dt) <= 0) {
    b.scanT = 0.12;
    let best = null, bd = Infinity;
    for (const e of sim.actors) {
      if (!e.alive || e.team === a.team) continue;
      const d = (e.pos.x - a.pos.x) ** 2 + (e.pos.z - a.pos.z) ** 2;
      if (d < bd && canSee(sim, a, e, b.diff.fov)) { bd = d; best = e; }
    }
    if (best) {
      if (best !== b.target) { b.reactT = b.diff.react * (0.7 + sim.rng() * 0.6); b.aimHead = sim.rng() < b.diff.headAim; }
      b.target = best; b.lostT = 0;
    } else if (b.target && (b.lostT += 0.12) > 0.5) {
      b.investigate = { x: b.target.pos.x, z: b.target.pos.z, t: 6 }; b.target = null;
    }
  }
  const tgt = b.target && b.target.alive ? b.target : null;
  if (!tgt) b.target = null;
  const visible = tgt && b.lostT === 0;

  // weapon housekeeping
  const pa = a.loadout.primary && a.ammo[a.loadout.primary];
  const want = pa && pa.mag + pa.reserve > 0 ? 'primary' : 'secondary';
  if (a.slot !== want && a.slot !== 'melee') it.slot = want;
  if (a.slot === 'melee' && a.loadout[want]) it.slot = want;
  if (!tgt && !w.melee && am.mag < w.mag * 0.5 && am.reserve > 0) it.reload = true;

  let yawGoal = null, moveDir = null;

  if (visible) {
    // --- engage ---
    const eye = eyeOf(a), ty = tgt.pos.y + (b.aimHead ? 1.6 : 1.1);
    const dx = tgt.pos.x - eye.x, dz = tgt.pos.z - eye.z, dy = ty - eye.y, hd = Math.hypot(dx, dz);
    const wantYaw = Math.atan2(-dx, -dz), wantPitch = Math.atan2(dy, hd) - a.punch * 0.7;
    const ey = angDiff(a.yaw, wantYaw), ep = wantPitch - a.pitch, maxT = b.diff.turn * dt;
    a.yaw += clamp(ey, -maxT, maxT); a.pitch += clamp(ep, -maxT, maxT);
    b.reactT -= dt;
    const tol = Math.atan(0.45 / Math.max(hd, 1)) + 0.025;
    it.scope = true;
    if (b.reactT <= 0 && Math.abs(ey) < tol && Math.abs(ep) < tol * 1.5 && (!w.melee || hd < w.range)) it.fire = w.auto || w.melee ? true : !a.prevFire;
    if (am.mag <= 0 && am.reserve <= 0 && !w.melee) it.slot = 'melee';
    b.strafeT -= dt;
    if (b.strafeT <= 0) { b.strafeT = 0.5 + sim.rng() * 0.9; b.strafeDir = [-1, 0, 0, 1][Math.floor(sim.rng() * 4)]; }
    if (w.melee || w.range < 45) { it.mz = hd > 2 ? 1 : 0; it.mx = b.strafeDir * 0.4; }
    else { it.mx = b.strafeDir; it.mz = hd < 5 ? -0.6 : 0; }
    if (w.scoped) { it.mx = 0; it.mz = 0; }
    return it;
  }

  // --- navigate ---
  const goal = chooseGoal(sim, a, b);
  const gd = Math.hypot(goal.x - a.pos.x, goal.z - a.pos.z);
  if (goal.use) it.use = true;
  const key = `${Math.round(goal.x)}:${Math.round(goal.z)}`;
  if (gd > 1.2 && !(goal.use && gd < 1.5)) {
    if (b.goalKey !== key || !b.path || b.repathT <= 0) { b.path = sim.grid.findPath(a.pos.x, a.pos.z, goal.x, goal.z) ?? []; b.pi = 0; b.goalKey = key; b.repathT = 3; }
    let wp = b.path[b.pi];
    while (wp && Math.hypot(wp.x - a.pos.x, wp.z - a.pos.z) < 0.7) wp = b.path[++b.pi];
    const tx = wp ? wp.x : goal.x, tz = wp ? wp.z : goal.z;
    const dx = tx - a.pos.x, dz = tz - a.pos.z, d = Math.hypot(dx, dz) || 1;
    moveDir = { x: dx / d, z: dz / d }; yawGoal = Math.atan2(-moveDir.x, -moveDir.z);
  } else if (!goal.use || gd > 0.4) {
    // arrived: scan the surroundings like a player holding an angle
    b.lookT -= dt;
    if (b.lookT <= 0) { b.lookT = 1.2 + sim.rng() * 1.8; b.lookYaw = (b.watchYaw ?? a.yaw) + (sim.rng() - 0.5) * 1.6; }
    yawGoal = b.lookYaw;
  }
  if (yawGoal !== null) { const mt = b.diff.turn * 0.6 * dt; a.yaw += clamp(angDiff(a.yaw, yawGoal), -mt, mt); }
  a.pitch += clamp(-a.pitch, -dt, dt);
  if (moveDir) {
    const fx = -Math.sin(a.yaw), fz = -Math.cos(a.yaw);
    it.mz = moveDir.x * fx + moveDir.z * fz; it.mx = moveDir.x * Math.cos(a.yaw) + moveDir.z * -Math.sin(a.yaw);
    // stuck detection
    if ((b.stuckT += dt) > 0.6) {
      b.stuckT = 0;
      if (Math.hypot(a.pos.x - b.lastX, a.pos.z - b.lastZ) < 0.25) { b.stuckN++; b.repathT = 0; it.jump = true; if (b.stuckN > 4) { b.stuckN = 0; b.guard = null; b.path = null; } }
      else b.stuckN = 0;
      b.lastX = a.pos.x; b.lastZ = a.pos.z;
    }
  }
  return it;
}
