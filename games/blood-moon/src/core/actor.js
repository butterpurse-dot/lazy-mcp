import { WEAPONS, defaultLoadout, ARMOR } from '../config/weapons.js';
import { TEAMS } from '../config/teams.js';

let seq = 0;

export function createActor({ name, team, bot }) {
  const t = TEAMS[team];
  return {
    id: ++seq, name, team, bot, brain: null,
    pos: { x: 0, y: 0, z: 0 }, vel: { x: 0, y: 0, z: 0 }, yaw: 0, pitch: 0, punch: 0,
    hp: t.maxHp, maxHp: t.maxHp, armor: 0, money: 0, kills: 0, deaths: 0, alive: false,
    loadout: defaultLoadout(), ammo: {}, slot: 'secondary',
    reloading: false, reloadT: 0, fireCd: 0, recoil: 0, prevFire: false, scoped: false,
    abilityCd: 0, howlT: 0, spotted: { vampire: 0, werewolf: 0 },
    actionT: 0, actionType: null, stepDist: 0, grounded: true, deathT: 0, hurtT: 0, hurtFrom: null, killer: null,
  };
}

export const weaponOf = (a) => WEAPONS[a.loadout[a.slot]];
export const ammoOf = (a) => a.ammo[a.loadout[a.slot]];

export function giveWeapon(a, id) {
  const w = WEAPONS[id];
  a.loadout[w.slot] = id;
  a.ammo[id] = { mag: w.mag, reserve: w.reserve };
  if (w.slot !== 'melee') a.slot = w.slot;
  a.reloading = false;
}

export function giveArmor(a) { a.armor = ARMOR.value; }

export function resetLoadout(a) {
  a.loadout = defaultLoadout(); a.ammo = {}; a.armor = 0;
  for (const id of Object.values(a.loadout)) if (id) giveWeapon(a, id);
  a.slot = 'secondary';
}

export function refillAmmo(a) {
  for (const id of Object.values(a.loadout)) if (id) a.ammo[id] = { mag: WEAPONS[id].mag, reserve: WEAPONS[id].reserve };
  a.reloading = false;
}
