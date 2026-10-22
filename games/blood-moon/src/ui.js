import { TEAMS } from './config/teams.js';
import { WEAPONS, ARMOR, BUY_CATEGORIES, canUse } from './config/weapons.js';
import { weaponOf, ammoOf } from './core/actor.js';
import { WALL_H } from './core/grid.js';

const $ = (id) => document.getElementById(id);
const REASONS = { elimination: 'All enemies eliminated', time: 'Time ran out', detonated: 'The Blood Sigil detonated', cleansed: 'The Blood Sigil was cleansed' };
const fmt = (s) => `${Math.floor(Math.max(0, s) / 60)}:${String(Math.floor(Math.max(0, s) % 60)).padStart(2, '0')}`;

/** DOM HUD. Writes only when a value changes and never uses innerHTML (names are untrusted-safe). */
export class UI {
  constructor(sim) {
    this.cache = new Map(); this.timers = {};
    this.el = Object.fromEntries(['hud', 'clock', 'roundinfo', 'scoreV', 'scoreW', 'aliveV', 'aliveW', 'killfeed', 'crosshair', 'hitmarker', 'scope', 'vignette', 'banner',
      'hint', 'progress', 'progressBar', 'progressLabel', 'spec', 'hp', 'armor', 'money', 'wname', 'mag', 'reserve', 'slots', 'ability', 'abName', 'abState', 'abBar',
      'buy', 'buyGrid', 'buyMoney', 'buyMsg', 'board', 'boardBody', 'boardTitle', 'radar'].map((i) => [i, $(i)]));
    this.radar = this.el.radar.getContext('2d');
    this.bindSim(sim);
  }

  /** Write a style property only when its value changed (avoids per-frame style recalcs). */
  sty(key, el, prop, v) { if (this.cache.get(key) !== v) { this.cache.set(key, v); if (prop.startsWith('--')) el.style.setProperty(prop, v); else el.style[prop] = v; } }

  txt(key, el, v) { if (this.cache.get(key) !== v) { this.cache.set(key, v); el.textContent = v; } }
  cls(key, el, name, on) { const k = key + name; if (this.cache.get(k) !== on) { this.cache.set(k, on); el.classList.toggle(name, on); } }

  bindSim(sim) {
    this.sim = sim; this.mapCanvas = null;
    for (const off of this.offs ?? []) off();
    const { bus } = sim, name = (a) => (a === sim.player ? 'You' : a.name);
    this.offs = [
      bus.on('kill', ({ killer, victim, weapon, head }) => {
        const li = document.createElement('li');
        const span = (a) => { const s = document.createElement('span'); s.className = a.team === 'vampire' ? 'cv' : 'cw'; s.textContent = name(a); return s; };
        const mid = document.createElement('span'); mid.className = 'cn'; mid.textContent = ` ${weapon ? weapon.name : 'Blood Sigil'}${head ? ' ⌖' : ''} → `;
        if (killer) li.append(span(killer), mid, span(victim)); else li.append(span(victim), Object.assign(document.createElement('span'), { className: 'cn', textContent: ' obliterated' }));
        if (killer === sim.player || victim === sim.player) li.classList.add('me');
        this.el.killfeed.prepend(li); setTimeout(() => li.remove(), 6000);
        while (this.el.killfeed.children.length > 6) this.el.killfeed.lastChild.remove();
      }),
      bus.on('hit', ({ attacker, head }) => { if (attacker === sim.player) this.pulse('hitmarker', 'on', 120, head ? 'head' : null); }),
      bus.on('roundStart', ({ round }) => this.banner(`Round ${round}`, 'Buy phase — press B for the armory', 'n', 3500)),
      bus.on('live', () => this.banner('', '', 'n', 0)),
      bus.on('plant', () => this.banner('Blood Sigil planted', 'Werewolves: cleanse it!  Vampires: defend it!', 'v', 3000)),
      bus.on('roundEnd', (r) => this.banner(`${TEAMS[r.winner].name} win`, REASONS[r.reason], r.winner === 'vampire' ? 'v' : 'w', 5500)),
    ];
  }

  pulse(key, cls, ms, extra) {
    const el = this.el[key]; el.classList.add(cls); if (extra) el.classList.add(extra);
    clearTimeout(this.timers[key]); this.timers[key] = setTimeout(() => { el.classList.remove(cls, extra ?? cls); }, ms);
  }

  banner(title, sub, tone, ms) {
    const b = this.el.banner; clearTimeout(this.timers.banner);
    if (!title) { b.classList.remove('on'); return; }
    b.className = `on ${tone}`; b.replaceChildren(document.createTextNode(title), Object.assign(document.createElement('small'), { textContent: sub }));
    if (ms) this.timers.banner = setTimeout(() => b.classList.remove('on'), ms);
  }

  show(on) { this.el.hud.hidden = !on; }

  update(view, chase) {
    const sim = this.sim, m = sim.match, p = view.actor, E = this.el;
    const alive = sim.aliveCounts();
    this.txt('sV', E.scoreV, m.score.vampire); this.txt('sW', E.scoreW, m.score.werewolf);
    this.txt('aV', E.aliveV, alive.vampire); this.txt('aW', E.aliveW, alive.werewolf);
    const t = m.phase === 'live' && m.sigil ? m.sigil.fuse : m.timer;
    this.txt('clk', E.clock, m.phase === 'live' && m.sigil ? `☠ ${Math.ceil(t)}` : fmt(m.phase === 'freeze' ? m.timer : t));
    this.cls('clk', E.clock, 'warn', (m.phase === 'live' && !!m.sigil) || (m.phase === 'live' && t < 10));
    this.txt('ri', E.roundinfo, m.phase === 'freeze' ? 'BUY PHASE' : `Round ${m.round} · first to ${sim.rules.winRounds}`);

    // vitals
    const w = weaponOf(p), am = ammoOf(p);
    this.txt('hp', E.hp, Math.ceil(Math.max(0, p.hp))); this.txt('ar', E.armor, Math.ceil(p.armor));
    this.cls('hp', E.hp.parentElement, 'low', p.alive && p.hp < 30);
    this.txt('mo', E.money, `$${p.money}`);
    this.txt('wn', E.wname, w.name); this.txt('mg', E.mag, w.melee ? '—' : am.mag); this.txt('rs', E.reserve, w.melee ? '' : `/ ${am.reserve}`);
    this.txt('sl', E.slots, ['primary', 'secondary', 'melee'].map((s) => p.loadout[s] ? `${s === 'primary' ? 1 : s === 'secondary' ? 2 : 3}${p.slot === s ? '*' : ''}` : '').join(''));
    if (this.cache.get('slk') !== E.slots.textContent) {
      this.cache.set('slk', E.slots.textContent);
      E.slots.replaceChildren(...['primary', 'secondary', 'melee'].filter((s) => p.loadout[s]).map((s, i) => Object.assign(document.createElement('span'), { className: p.slot === s ? 'cur' : '', textContent: `${s === 'primary' ? 1 : s === 'secondary' ? 2 : 3} ${WEAPONS[p.loadout[s]].name.split(' ').pop()}` })));
    }
    // ability
    const ab = TEAMS[p.team].ability;
    this.txt('abn', E.abName, ab.name); this.txt('abs', E.abState, p.abilityCd > 0 ? `${Math.ceil(p.abilityCd)}s` : ab.hint);
    this.cls('ab', E.ability, 'ready', p.abilityCd <= 0);
    this.sty('abBar', E.abBar, 'transform', `scaleX(${(p.abilityCd > 0 ? 1 - p.abilityCd / ab.cooldown : 1).toFixed(2)})`);

    // crosshair, scope, damage
    const spread = (w.spread + w.moveSpread * Math.min(1, Math.hypot(p.vel.x, p.vel.z) / 5.5) + p.recoil + (p.grounded ? 0 : 4)) * 2.2;
    this.sty('gap', E.crosshair, '--gap', `${Math.round(3 + spread)}px`);
    E.crosshair.hidden = chase || !p.alive || p.scoped || w.melee;
    E.scope.hidden = !p.scoped;
    this.sty('vig', E.vignette, 'opacity', String(!chase && p.hurtT > 0 ? Math.min(1, p.hurtT * 2.2).toFixed(2) : 0));

    // objective prompt + progress
    let hint = '';
    if (!chase && p.alive && m.phase === 'live') {
      if (p.team === 'vampire' && !sim.sigil && sim.grid.zoneAt(p.pos.x, p.pos.z)) hint = `Hold E to plant the Blood Sigil (site ${sim.grid.zoneAt(p.pos.x, p.pos.z)})`;
      else if (p.team === 'werewolf' && sim.sigil && Math.hypot(p.pos.x - sim.sigil.x, p.pos.z - sim.sigil.z) < 2.6) hint = 'Hold E to cleanse the Blood Sigil';
      else if (p.team === 'vampire' && !sim.sigil) hint = `Plant at site ${sim.plan.site} or ${sim.plan.site === 'A' ? 'B' : 'A'}`;
    } else if (!chase && p.alive && m.phase === 'freeze' && !this.buyOpen) hint = 'Press B to open the armory';
    if (this.needLock) hint = 'Click to capture the mouse';
    this.txt('hint', E.hint, hint); this.cls('hint', E.hint, 'on', !!hint);
    const need = p.actionType === 'plant' ? sim.rules.plantTime : sim.rules.cleanseTime;
    E.progress.hidden = !p.actionType;
    if (p.actionType) { this.sty('prog', E.progressBar, 'width', `${Math.min(100, Math.round((p.actionT / need) * 100))}%`); this.txt('pl', E.progressLabel, p.actionType === 'plant' ? 'PLANTING' : 'CLEANSING'); }

    // spectator label
    E.spec.hidden = !chase; if (chase) this.txt('sp', E.spec, p.alive ? `Spectating ${p === sim.player ? 'yourself' : p.name}` : 'No one left to watch');

    this.drawRadar(p);
    if (this.buyOpen) this.refreshBuy();
    if (this.boardOpen) this.refreshBoard();
  }

  // ---------- radar ----------
  buildMapCanvas() {
    const g = this.sim.grid, S = 4 / g.cell, c = document.createElement('canvas');
    c.width = Math.ceil(g.w * g.cell * 4); c.height = Math.ceil(g.h * g.cell * 4);
    const x = c.getContext('2d'); x.fillStyle = '#1c1a26'; x.fillRect(0, 0, c.width, c.height);
    for (let z = 0; z < g.h; z++) for (let i = 0; i < g.w; i++) {
      const h = g.height(i, z);
      x.fillStyle = h >= WALL_H ? '#05040a' : h > 0 ? '#4a3a2c' : '#3a3648';
      x.fillRect(i * g.cell * 4, z * g.cell * 4, g.cell * 4 + 0.5, g.cell * 4 + 0.5);
    }
    x.font = 'bold 28px Georgia'; x.fillStyle = 'rgba(255,70,100,.85)'; x.textAlign = 'center'; x.textBaseline = 'middle';
    for (const id of Object.keys(g.zones)) { const z = g.zoneCenter(id); x.fillText(id, z.x * 4, z.z * 4); }
    this.mapCanvas = c;
  }

  drawRadar(p) {
    if (!this.mapCanvas) this.buildMapCanvas();
    const ctx = this.radar, S = this.el.radar.width, C = S / 2, k = (C / 38), sim = this.sim; // 38 m radius
    ctx.clearRect(0, 0, S, S); ctx.save(); ctx.beginPath(); ctx.arc(C, C, C - 2, 0, 7); ctx.clip();
    ctx.translate(C, C); ctx.rotate(p.yaw); ctx.scale(k / 4, k / 4); ctx.translate(-p.pos.x * 4, -p.pos.z * 4);
    ctx.globalAlpha = 0.85; ctx.drawImage(this.mapCanvas, 0, 0); ctx.restore();
    const cs = Math.cos(p.yaw), sn = Math.sin(p.yaw);
    const pt = (x, z) => [C + k * ((x - p.pos.x) * cs - (z - p.pos.z) * sn), C + k * ((x - p.pos.x) * sn + (z - p.pos.z) * cs)];
    const dot = (x, z, col, r) => { const [px, py] = pt(x, z); if (Math.hypot(px - C, py - C) > C - 6) return; ctx.fillStyle = col; ctx.beginPath(); ctx.arc(px, py, r, 0, 7); ctx.fill(); };
    if (sim.sigil) { const f = (Math.sin(sim.time * 8) + 1) / 2; dot(sim.sigil.x, sim.sigil.z, `rgba(255,40,70,${0.5 + f * 0.5})`, 9); }
    for (const a of sim.actors) {
      if (!a.alive || a === p) continue;
      if (a.team === p.team) dot(a.pos.x, a.pos.z, TEAMS[a.team].css, 6);
      else if (a.spotted[p.team] > 0) dot(a.pos.x, a.pos.z, '#ffffff', 7), dot(a.pos.x, a.pos.z, TEAMS[a.team].css, 5);
    }
    ctx.fillStyle = '#ffcf5a'; ctx.beginPath(); ctx.moveTo(C, C - 10); ctx.lineTo(C - 7, C + 8); ctx.lineTo(C + 7, C + 8); ctx.closePath(); ctx.fill();
  }

  // ---------- buy menu ----------
  openBuy(on) { this.buyOpen = on; this.el.buy.hidden = !on; this.cache.delete('buyKey'); if (on) { this.el.buyMsg.textContent = ''; this.refreshBuy(); } }

  buyList(p) {
    return BUY_CATEGORIES.flatMap((c) => c.items.map((id) => (id === 'armor' ? ARMOR : WEAPONS[id])).filter((w) => w.id === 'armor' || canUse(w, p.team)));
  }

  refreshBuy() {
    const p = this.sim.player; if (!p) return;
    const key = `${p.money}|${p.armor}|${Object.values(p.loadout).join()}|${this.sim.match.buyOpen}`;
    if (this.cache.get('buyKey') === key) return; this.cache.set('buyKey', key);
    this.el.buyMoney.textContent = `$${p.money}${this.sim.match.buyOpen ? '' : '  — buy time is over'}`;
    const frag = []; let n = 0;
    for (const cat of BUY_CATEGORIES) {
      const items = cat.items.map((id) => (id === 'armor' ? ARMOR : WEAPONS[id])).filter((w) => w.id === 'armor' || canUse(w, p.team));
      if (!items.length) continue;
      frag.push(Object.assign(document.createElement('h4'), { textContent: cat.name }));
      for (const w of items) {
        n++;
        const btn = document.createElement('button'); btn.type = 'button'; btn.className = 'item'; btn.dataset.id = w.id;
        const owned = w.id === 'armor' ? p.armor >= ARMOR.value : p.loadout[w.slot] === w.id;
        if (owned) btn.classList.add('owned'); if (p.money < w.price || !this.sim.match.buyOpen) btn.classList.add('no');
        const top = document.createElement('b'); top.append(`${n}. ${w.name}`, Object.assign(document.createElement('span'), { className: 'price', textContent: `$${w.price}` }));
        const info = document.createElement('small'); info.textContent = w.id === 'armor' ? 'Absorbs bullet damage' : `${w.pellets > 1 ? `${w.damage}×${w.pellets}` : w.damage} dmg · ${w.rate}/s · ${w.mag} rds`;
        btn.append(top, info); frag.push(btn);
      }
    }
    this.el.buyGrid.replaceChildren(...frag);
  }

  tryBuy(id) {
    const r = this.sim.buy(this.sim.player, id); this.el.buyMsg.textContent = r.ok ? '' : r.reason; this.cache.delete('buyKey'); return r;
  }
  buyByIndex(i) { const l = this.buyList(this.sim.player); if (l[i]) return this.tryBuy(l[i].id); }

  // ---------- scoreboard ----------
  openBoard(on) { this.boardOpen = on; this.el.board.hidden = !on; if (on) this.refreshBoard(); }

  refreshBoard() {
    const sim = this.sim, p = sim.player, parts = [];
    this.el.boardTitle.textContent = `${sim.map.name} — ${sim.match.score.vampire} : ${sim.match.score.werewolf}`;
    for (const team of ['vampire', 'werewolf']) {
      const tbl = document.createElement('table'), head = tbl.createTHead().insertRow();
      const th0 = document.createElement('th'); th0.className = team === 'vampire' ? 'tv' : 'tw'; th0.textContent = TEAMS[team].name; head.append(th0);
      for (const h of ['Kills', 'Deaths', 'Money']) head.append(Object.assign(document.createElement('th'), { textContent: h }));
      const body = tbl.createTBody();
      for (const a of sim.actors.filter((x) => x.team === team).sort((x, y) => y.kills - x.kills)) {
        const row = body.insertRow(); if (!a.alive) row.className = 'dead'; if (a === p) row.className += ' me';
        const own = !p || p.team === team;
        for (const v of [a === p ? 'You' : a.name, a.kills, a.deaths, own ? `$${a.money}` : '—']) row.insertCell().textContent = v;
      }
      parts.push(tbl);
    }
    this.el.boardBody.replaceChildren(...parts);
  }
}
