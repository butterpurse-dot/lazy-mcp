import { Sim } from './core/sim.js';
import { Renderer } from './render/renderer.js';
import { UI } from './ui.js';
import { Input } from './input.js';
import { Sfx } from './audio/sfx.js';
import { loadProfile, saveProfile } from './profile.js';
import { weaponOf } from './core/actor.js';
import { TEAMS, TEAM_IDS } from './config/teams.js';

const $ = (id) => document.getElementById(id);
const STEP = 1 / 60;
const params = new URLSearchParams(location.search);
const profile = loadProfile();

// One long-lived world. The menu runs an all-bot "attract mode" match behind it.
const sim = new Sim({ autopilot: true, difficulty: profile.difficulty });
const canvas = $('game');
let renderer;
try { renderer = new Renderer(canvas, sim); } catch (err) {
  document.body.append(Object.assign(document.createElement('p'), { className: 'noscript', textContent: 'Blood Moon needs WebGL. Please enable hardware acceleration or try another browser.' }));
  throw err;
}
const ui = new UI(sim), sfx = new Sfx(), input = new Input(canvas, () => sim);
let state = 'menu'; // menu | playing | paused | over
let acc = 0, last = performance.now(), specIdx = 0, camSwitch = 0, camTarget = null, nextBeep = 0, hadLock = false;

// ---------- menu ----------
const pick = (team) => { profile.team = team; document.querySelectorAll('.teamcard').forEach((c) => c.classList.toggle('sel', c.dataset.team === team)); };
document.querySelectorAll('.teamcard').forEach((c) => c.addEventListener('click', () => { pick(c.dataset.team); sfx.unlock(); sfx.click(); }));
$('optDiff').value = profile.difficulty; $('optSens').value = profile.sens; $('optVol').value = profile.volume;
$('optVol').addEventListener('input', (e) => { profile.volume = +e.target.value; sfx.setVolume(profile.volume); });
pick(profile.team); sfx.setVolume(profile.volume);
const renderLifetime = () => { const s = profile.stats; $('lifetime').textContent = s.matches ? `Career: ${s.wins} wins / ${s.matches} matches · ${s.kills} kills · ${s.deaths} deaths` : ''; };
renderLifetime();

function startGame() {
  profile.difficulty = $('optDiff').value; profile.sens = +$('optSens').value; profile.volume = +$('optVol').value; saveProfile(profile);
  input.sens = profile.sens; sfx.setVolume(profile.volume); sfx.unlock();
  sim.setPlayer(profile.team); sim.setDifficulty(profile.difficulty); sim.restart();
  for (const id of ['menu', 'pause', 'over']) $(id).hidden = true;
  state = 'playing'; input.enabled = true; ui.show(true); specIdx = 0; matchCounted = false;
  if (!params.has('nolock')) { input.lock(); } 
}

let matchCounted = false;
function toMenu() {
  input.enabled = false; input.reset(); input.unlock(); ui.openBuy(false); ui.openBoard(false); ui.show(false);
  sim.setPlayer(null); sim.restart(); state = 'menu';
  for (const id of ['pause', 'over']) $(id).hidden = true; $('menu').hidden = false; renderLifetime();
}

$('play').addEventListener('click', startGame);
$('rematch').addEventListener('click', startGame);
$('toMenu').addEventListener('click', toMenu);
$('quit').addEventListener('click', toMenu);
$('restart').addEventListener('click', () => { sim.restart(); resume(); });
$('resume').addEventListener('click', resume);
function resume() { $('pause').hidden = true; state = 'playing'; input.enabled = true; if (!params.has('nolock')) input.lock(); }
function pause() { if (state !== 'playing') return; state = 'paused'; input.enabled = false; input.reset(); ui.openBuy(false); ui.openBoard(false); $('pause').hidden = false; }

input.onLockChange = (locked) => {
  if (locked) hadLock = true;
  else if (hadLock && state === 'playing' && !ui.buyOpen) pause();
};
document.addEventListener('visibilitychange', () => { if (document.hidden) pause(); });

input.onKey = (code, down) => {
  if (state !== 'playing') return;
  if (code === 'Tab') ui.openBoard(down);
  if (!down) return;
  if (code === 'KeyB') { const open = !ui.buyOpen; ui.openBuy(open); if (open) input.unlock(); else input.lock(); }
  else if (code === 'Escape' && ui.buyOpen) { ui.openBuy(false); input.lock(); }
  else if (ui.buyOpen && /^Digit[1-9]$/.test(code)) { const r = ui.buyByIndex(+code.slice(5) - 1); if (r?.ok) sfx.click(); }
};
input.onClick = () => { if (state === 'playing' && !input.locked && !ui.buyOpen && !params.has('nolock')) input.lock(); if (!sim.player?.alive && state === 'playing') specIdx++; };
$('buyGrid').addEventListener('click', (e) => { const b = e.target.closest('.item'); if (b) { const r = ui.tryBuy(b.dataset.id); if (r.ok) sfx.click(); } });

// ---------- sound wiring ----------
sim.bus.on('shot', ({ actor, weapon }) => sfx.shot(weapon.cls, actor === sim.player ? null : actor.pos));
sim.bus.on('hit', ({ attacker, victim, head }) => { if (attacker === sim.player) sfx.hit(head); if (victim === sim.player) sfx.hurt(); });
sim.bus.on('step', ({ actor }) => { if (actor !== sim.player) sfx.step(actor.pos); });
sim.bus.on('reload', ({ actor }) => sfx.reload(actor === sim.player ? null : actor.pos));
sim.bus.on('dry', ({ actor }) => { if (actor === sim.player) sfx.dry(); });
sim.bus.on('ability', ({ actor, id }) => (id === 'howl' ? sfx.howl(actor === sim.player ? null : actor.pos) : sfx.dash(actor === sim.player ? null : actor.pos)));
sim.bus.on('plant', () => sfx.plant());
sim.bus.on('explode', () => sfx.boom());
sim.bus.on('roundEnd', (r) => { if (state === 'playing') sfx.win(!sim.player || r.winner === sim.player.team); });
sim.bus.on('matchOver', ({ winner }) => {
  if (!sim.player) return;
  const p = sim.player, won = winner === p.team;
  state = 'over'; input.enabled = false; input.reset(); input.unlock(); ui.openBuy(false); ui.openBoard(false);
  $('overTitle').textContent = won ? 'VICTORY' : 'DEFEAT';
  $('overBody').textContent = `${TEAMS[winner].name} win ${sim.match.score[winner]} : ${sim.match.score[winner === 'vampire' ? 'werewolf' : 'vampire']}  ·  You: ${p.kills} kills, ${p.deaths} deaths`;
  $('over').hidden = false;
  if (!matchCounted) { matchCounted = true; const s = profile.stats; s.matches++; if (won) s.wins++; s.kills += p.kills; s.deaths += p.deaths; saveProfile(profile); }
});
// attract mode loops forever
sim.bus.on('matchOver', () => { if (!sim.player) setTimeout(() => { if (!sim.player && state === 'menu') sim.restart(); }, 4000); });

// ---------- camera selection ----------
function pickView() {
  const p = sim.player;
  if (p && p.alive) return { actor: p, chase: false };
  const mates = sim.actors.filter((a) => a.alive && (!p || a.team === p.team));
  const pool = mates.length ? mates : sim.actors.filter((a) => a.alive);
  if (p) {
    if (!pool.length) return { actor: p.killer ?? p, chase: true };
    if (!camTarget || !camTarget.alive || specIdx !== camSwitch) { camTarget = specIdx === 0 && p.killer?.alive ? p.killer : pool[specIdx % pool.length]; camSwitch = specIdx; }
    return { actor: camTarget, chase: true };
  }
  // attract mode: follow the action, hop to a new target every few seconds
  if (!camTarget || !camTarget.alive || sim.time > camSwitch) { camTarget = pool[Math.floor(Math.random() * pool.length)] ?? sim.actors[0]; camSwitch = sim.time + 8; }
  return { actor: camTarget, chase: true };
}

// ---------- main loop ----------
function frame(now) {
  requestAnimationFrame(frame);
  const dt = Math.min(0.1, (now - last) / 1000); last = now;
  if (state !== 'paused') {
    acc += dt;
    for (let i = 0; acc >= STEP && i < 6; i++, acc -= STEP) sim.update(STEP);
    if (acc > STEP) acc = 0;
  }
  const view = pickView();
  if (state === 'playing' && sim.player && !sim.player.alive && view.actor !== camTarget) camTarget = view.actor;
  ui.needLock = state === 'playing' && !input.locked && !ui.buyOpen && !params.has('nolock');
  renderer.render(dt, view);
  sfx.listen(view.actor.pos.x, view.actor.pos.z, view.actor.yaw);
  if (state !== 'menu') {
    ui.update(view, view.chase);
    if (sim.sigil && sim.match.phase === 'live' && state === 'playing') {
      const fuse = sim.match.sigil.fuse;
      if ((nextBeep -= dt) <= 0) { nextBeep = 0.12 + (fuse / sim.rules.sigilFuse) ** 1.6 * 1.0; sfx.beep(fuse < 10); }
    }
  }
}

if (import.meta.env.DEV) window.__bm = { sim, ui, input }; // dev-only debug handle (stripped from production builds)
if (params.has('autostart')) {
  const team = TEAM_IDS.includes(params.get('autostart')) ? params.get('autostart') : profile.team;
  pick(team); startGame();
}
requestAnimationFrame(frame);
