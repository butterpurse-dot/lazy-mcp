import { describe, it, expect } from 'vitest';
import { MAP, buildRows } from '../src/config/map.js';
import { WEAPONS, canUse } from '../src/config/weapons.js';
import { RULES } from '../src/config/rules.js';
import { Grid, WALL_H } from '../src/core/grid.js';
import { Match } from '../src/core/match.js';
import { rayVsActor, applyDamage, calcDamage } from '../src/core/combat.js';
import { Sim } from '../src/core/sim.js';
import { mulberry32 } from '../src/core/rng.js';

const mkGrid = () => new Grid(buildRows(MAP), MAP.cell, MAP.zones);

describe('map', () => {
  const g = mkGrid();
  it('is rectangular with solid borders', () => {
    const rows = buildRows(MAP);
    expect(rows).toHaveLength(MAP.height);
    rows.forEach((r) => expect(r).toHaveLength(MAP.width));
    expect(rows[0]).toBe('#'.repeat(MAP.width));
  });
  it('connects every spawn to both bomb sites', () => {
    for (const team of ['vampire', 'werewolf']) for (const [cx, cz] of MAP.spawns[team]) {
      expect(g.solid(cx, cz)).toBe(false);
      for (const s of ['A', 'B']) {
        const z = g.zoneCenter(s);
        expect(g.findPath(g.center(cx), g.center(cz), z.x, z.z), `${team} ${cx},${cz} -> ${s}`).not.toBeNull();
      }
    }
  });
  it('keeps site zones free of props', () => {
    for (const id of ['A', 'B']) {
      const [x1, z1, x2, z2] = MAP.zones[id];
      for (let z = z1; z <= z2; z++) for (let x = x1; x <= x2; x++) expect(g.solid(x, z)).toBe(false);
    }
  });
});

describe('grid', () => {
  const g = mkGrid();
  it('blocks movement into walls and slides along them', () => {
    const p = { x: g.center(20), z: g.center(2) };
    g.move(p, -100, 0, 0.42);
    expect(g.hitsWall(p.x, p.z, 0.41)).toBe(false);
    expect(p.x).toBeGreaterThan(18 * g.cell);
  });
  it('raycasts walls, crates (can shoot over) and floor', () => {
    const o = { x: g.center(20), z: g.center(2) };
    const hit = g.raycast(o.x, 1.5, o.z, -1, 0, 0, 100);
    expect(hit.kind).toBe('wall');
    expect(hit.x).toBeCloseTo(18 * g.cell, 3);
    expect(g.raycast(o.x, 1.5, o.z, 0, -1, 0, 100).kind).toBe('floor');
    // crate at (23,10): low ray blocked, high ray passes
    const cx = g.center(23), cz = g.center(14);
    expect(g.raycast(cx, 0.8, cz, 0, 0, -1, 6.5)?.kind).toBe('wall');
    expect(g.raycast(cx, 1.65, cz, 0, 0, -1, 6.5)).toBeNull();
    expect(WALL_H).toBeGreaterThan(1.65);
  });
  it('finds smooth paths that avoid walls', () => {
    const p = g.findPath(g.center(23), g.center(34), g.center(23), g.center(4));
    expect(p.length).toBeGreaterThan(0);
    let prev = { x: g.center(23), z: g.center(34) };
    for (const q of p) { expect(g.walkable(prev.x, prev.z, q.x, q.z, 0.45)).toBe(true); prev = q; }
  });
});

describe('combat', () => {
  const actor = { pos: { x: 0, y: 0, z: -10 } };
  it('hits the body, flags headshots, misses wide shots', () => {
    const o = { x: 0, y: 1.2, z: 0 };
    expect(rayVsActor(o, { x: 0, y: 0, z: -1 }, actor, 50).head).toBe(false);
    expect(rayVsActor({ ...o, y: 1.6 }, { x: 0, y: 0, z: -1 }, actor, 50).head).toBe(true);
    expect(rayVsActor({ ...o, x: 2 }, { x: 0, y: 0, z: -1 }, actor, 50)).toBeNull();
    expect(rayVsActor(o, { x: 0, y: 0, z: -1 }, actor, 5)).toBeNull();
  });
  it('armor reduces HP loss and depletes', () => {
    const v = { hp: 100, armor: 100 };
    const lost = applyDamage(v, 40, 0.5);
    expect(lost).toBeLessThan(40);
    expect(v.armor).toBeLessThan(100);
    expect(applyDamage({ hp: 100, armor: 0 }, 40, 0.5)).toBe(40);
  });
  it('applies headshot multiplier and range falloff', () => {
    const w = WEAPONS.moonrifle;
    expect(calcDamage(w, 0, true)).toBeCloseTo(w.damage * w.headMul);
    expect(calcDamage(w, 50, false)).toBeLessThan(w.damage);
  });
  it('has a sane arsenal', () => {
    for (const w of Object.values(WEAPONS)) {
      expect(w.damage).toBeGreaterThan(0); expect(w.rate).toBeGreaterThan(0);
      if (!w.melee) { expect(w.mag).toBeGreaterThan(0); expect(w.reload).toBeGreaterThan(0); }
    }
    expect(canUse(WEAPONS.bloodcarbine, 'werewolf')).toBe(false);
  });
});

describe('match rules', () => {
  const alive = (v, w) => ({ vampire: v, werewolf: w });
  const live = () => { const m = new Match(); m.startRound(); m.tick(RULES.freezeTime + 0.1, alive(5, 5)); return m; };
  it('runs freeze -> live', () => { const m = live(); expect(m.phase).toBe('live'); expect(m.buyOpen).toBe(true); });
  it('werewolves win on timeout, vampires on elimination', () => {
    let m = live(); m.tick(RULES.roundTime + 1, alive(5, 5));
    expect(m.result).toMatchObject({ winner: 'werewolf', reason: 'time' });
    m = live(); m.tick(0.1, alive(2, 0));
    expect(m.result).toMatchObject({ winner: 'vampire', reason: 'elimination' });
  });
  it('plant extends the round; cleanse and detonation resolve it', () => {
    let m = live(); expect(m.plant('A')).toBe(true); expect(m.plant('B')).toBe(false);
    m.tick(0.1, alive(0, 3)); expect(m.phase).toBe('live'); // vampires dead but sigil armed
    m.cleanse(); expect(m.result.reason).toBe('cleansed');
    m = live(); m.plant('B'); m.tick(RULES.sigilFuse + 1, alive(3, 3));
    expect(m.result).toMatchObject({ winner: 'vampire', reason: 'detonated' });
  });
  it('pays win/loss bonuses with a capped loss streak', () => {
    const m = new Match();
    const rewards = [];
    for (let i = 0; i < 7; i++) { m.startRound(); m.tick(10, alive(5, 5)); m.tick(0.1, alive(5, 0)); rewards.push(m.result.rewards.werewolf); }
    expect(rewards[0]).toBe(RULES.lossBase);
    expect(rewards[1]).toBe(RULES.lossBase + RULES.lossStep);
    expect(rewards[6]).toBe(RULES.lossBase + RULES.lossStep * RULES.lossMaxStreak);
    expect(m.result.rewards.vampire).toBe(RULES.winReward);
    expect(m.winner).toBe('vampire');
    expect(m.tick(RULES.endDelay + 1, alive(5, 5))).toBe('matchOver');
  });
});

describe('simulation', () => {
  it('lets the player buy only what they can afford, own and use', () => {
    const sim = new Sim({ rng: mulberry32(1), playerTeam: 'vampire' });
    const p = sim.player; p.money = 3000;
    expect(sim.buy(p, 'moonrifle').ok).toBe(false);
    expect(sim.buy(p, 'bloodcarbine').ok).toBe(true);
    expect(p.loadout.primary).toBe('bloodcarbine'); expect(p.money).toBe(300);
    expect(sim.buy(p, 'silverbolt').ok).toBe(false);
    sim.match.phase = 'live'; sim.match.elapsed = 999;
    expect(sim.buy(p, 'armor').ok).toBe(false);
  });

  it('plays full all-bot matches to completion without errors, NaNs or stalls', () => {
    for (const seed of [7, 42]) {
      const sim = new Sim({ rng: mulberry32(seed), autopilot: true, difficulty: 'hard' });
      const ev = { kills: 0, plants: 0, ends: 0, over: false };
      sim.bus.on('kill', () => ev.kills++); sim.bus.on('plant', () => ev.plants++);
      sim.bus.on('roundEnd', () => ev.ends++); sim.bus.on('matchOver', () => (ev.over = true));
      for (let t = 0; t < 60 * 60 * 30 && !ev.over; t++) {
        sim.update(1 / 60);
        if (t % 120 === 0) for (const a of sim.actors) { expect(Number.isFinite(a.pos.x + a.pos.z + a.hp)).toBe(true); expect(sim.grid.hitsWall(a.pos.x, a.pos.z, 0.3)).toBe(false); }
      }
      expect(ev.over, `seed ${seed} finished`).toBe(true);
      expect(ev.kills).toBeGreaterThan(5);
      expect(ev.ends).toBeGreaterThanOrEqual(7);
      expect(ev.plants).toBeGreaterThan(0);
    }
  }, 120000);
});

describe('player actions', () => {
  const goLive = (sim) => { for (let i = 0; i < 60 * (RULES.freezeTime + 0.5); i++) sim.update(1 / 60); expect(sim.match.phase).toBe('live'); };
  const quiet = (sim) => { for (const a of sim.actors) if (a !== sim.player) { Object.assign(a.brain, { scanT: 1e9, target: null, investigate: null, hold: { x: a.pos.x, z: a.pos.z } }); } };

  it('plants the sigil by holding use inside a site and pays the planter', () => {
    const sim = new Sim({ rng: mulberry32(3), playerTeam: 'vampire' });
    goLive(sim); const p = sim.player, z = sim.grid.zoneCenter('A');
    for (const a of sim.actors) if (a !== p && a.team === 'werewolf') a.alive = false; // keep the round open for the test
    sim.actors.find((a) => a.team === 'werewolf').alive = true;
    p.pos.x = z.x; p.pos.z = z.z; sim.intent.use = true;
    const money = p.money;
    for (let i = 0; i < 60 * 1.5; i++) sim.update(1 / 60);
    expect(sim.sigil).toBeNull(); expect(p.actionType).toBe('plant');
    for (let i = 0; i < 60 * 2; i++) sim.update(1 / 60);
    expect(sim.sigil?.site).toBe('A'); expect(sim.match.sigil.fuse).toBeLessThan(RULES.sigilFuse);
    expect(p.money).toBe(money + RULES.plantReward);
  });

  it('cannot plant outside a site', () => {
    const sim = new Sim({ rng: mulberry32(3), playerTeam: 'vampire' });
    goLive(sim); sim.intent.use = true;
    for (let i = 0; i < 60 * 4; i++) sim.update(1 / 60);
    expect(sim.sigil).toBeNull();
  });

  it('shooting an enemy in the open damages and eventually kills them; teammates are safe', () => {
    const sim = new Sim({ rng: mulberry32(9), playerTeam: 'vampire' });
    goLive(sim); quiet(sim);
    const p = sim.player, foe = sim.actors.find((a) => a.team === 'werewolf'), mate = sim.actors.find((a) => a.team === 'vampire' && a !== p);
    p.pos = { x: 36, y: 0, z: 20 }; foe.pos = { x: 36, y: 0, z: 14 }; mate.pos = { x: 36.3, y: 0, z: 17 };
    p.yaw = 0; p.pitch = Math.atan2(1.2 - 1.65, 6); foe.hp = 100; foe.armor = 0; const mateHp = mate.hp;
    const kills = []; sim.bus.on('kill', (k) => kills.push(k));
    for (let i = 0; i < 60 * 3 && foe.alive; i++) { sim.intent.fire = i % 2 === 0; sim.update(1 / 60); p.pitch = Math.atan2(1.2 - 1.65, 6); p.yaw = 0; }
    expect(foe.hp).toBeLessThan(100); expect(mate.hp).toBe(mateHp);
    expect(foe.alive || kills.some((k) => k.victim === foe && k.killer === p)).toBe(true);
  });

  it('uses shadow step with a cooldown and reloads from reserve', () => {
    const sim = new Sim({ rng: mulberry32(4), playerTeam: 'vampire' });
    goLive(sim); quiet(sim); const p = sim.player; p.pos = { x: 39.5, y: 0, z: 36 };
    const x0 = p.pos.z; sim.intent.ability = true; sim.update(1 / 60);
    for (let i = 0; i < 20; i++) sim.update(1 / 60);
    expect(x0 - p.pos.z).toBeGreaterThan(4); expect(p.abilityCd).toBeGreaterThan(10);
    p.ammo[p.loadout.secondary].mag = 3; sim.intent.reload = true; sim.update(1 / 60);
    for (let i = 0; i < 60 * 3; i++) sim.update(1 / 60);
    expect(p.ammo[p.loadout.secondary].mag).toBe(12);
  });
});
