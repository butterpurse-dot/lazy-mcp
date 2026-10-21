// Data-driven arsenal. Add an entry here and it shows up in the buy menu, bots and HUD.
// damage/headMul: dmg per pellet; pen: fraction of damage that bypasses armor;
// spread/moveSpread/kick in degrees; falloff: dmg multiplier per 10 m.
const base = { teams: null, headMul: 3, pen: 0.6, auto: false, pellets: 1, falloff: 0.97, range: 120,
  moveMul: 1, moveSpread: 3, kick: 0.6, scoped: false, killReward: 300, price: 0, reserve: 0 };
const def = (o) => Object.freeze({ ...base, ...o });

export const WEAPONS = Object.freeze({
  dagger:    def({ id: 'dagger', name: 'Silver Dagger', slot: 'melee', cls: 'melee', melee: true, damage: 40, headMul: 1, pen: 0.7,
                   rate: 1.8, mag: 1, range: 2.4, moveMul: 1.08, spread: 0, moveSpread: 0, kick: 0, killReward: 1500 }),
  nightfang: def({ id: 'nightfang', name: 'Nightfang .45', slot: 'secondary', cls: 'pistol', damage: 26, rate: 7, mag: 12, reserve: 48,
                   reload: 2.0, spread: 0.5, kick: 0.9, pen: 0.5 }),
  hexmagnum: def({ id: 'hexmagnum', name: 'Hex Magnum', slot: 'secondary', cls: 'pistol', price: 700, damage: 58, headMul: 3.4, rate: 2.2,
                   mag: 7, reserve: 28, reload: 2.4, spread: 0.4, kick: 2.4, pen: 0.8 }),
  cryptsmg:  def({ id: 'cryptsmg', name: 'Crypt SMG', slot: 'primary', cls: 'smg', price: 1200, damage: 21, headMul: 2.6, rate: 14, auto: true,
                   mag: 30, reserve: 120, reload: 2.2, spread: 0.8, moveSpread: 1.8, kick: 0.5, pen: 0.5, moveMul: 1.02, killReward: 600, falloff: 0.94 }),
  gravegun:  def({ id: 'gravegun', name: 'Grave Shotgun', slot: 'primary', cls: 'shotgun', price: 1800, damage: 11, pellets: 9, headMul: 2, rate: 1.3,
                   mag: 8, reserve: 32, reload: 3.0, spread: 3.2, moveSpread: 2, kick: 4, pen: 0.4, range: 40, falloff: 0.88, killReward: 900 }),
  bloodcarbine: def({ id: 'bloodcarbine', name: 'Blood Carbine', slot: 'primary', cls: 'rifle', teams: ['vampire'], price: 2700, damage: 33, headMul: 3.6,
                   rate: 10, auto: true, mag: 30, reserve: 90, reload: 2.5, spread: 0.35, moveSpread: 4.5, kick: 1.15, pen: 0.77, moveMul: 0.93, falloff: 0.98 }),
  moonrifle: def({ id: 'moonrifle', name: 'Moon Rifle', slot: 'primary', cls: 'rifle', teams: ['werewolf'], price: 3100, damage: 29, headMul: 3.6,
                   rate: 11, auto: true, mag: 30, reserve: 90, reload: 2.3, spread: 0.3, moveSpread: 4, kick: 0.85, pen: 0.8, moveMul: 0.93, falloff: 0.985 }),
  silverbolt: def({ id: 'silverbolt', name: 'Silver Sniper', slot: 'primary', cls: 'sniper', price: 4750, damage: 135, headMul: 2, rate: 0.75,
                   mag: 5, reserve: 20, reload: 3.4, spread: 0.05, moveSpread: 9, kick: 3, pen: 0.95, moveMul: 0.82, scoped: true, falloff: 1, killReward: 100 }),
});

export const ARMOR = Object.freeze({ id: 'armor', name: 'Silver Plate', price: 650, value: 100 });

export const BUY_CATEGORIES = Object.freeze([
  { name: 'Sidearms', items: ['hexmagnum'] },
  { name: 'SMG / Shotgun', items: ['cryptsmg', 'gravegun'] },
  { name: 'Rifles', items: ['bloodcarbine', 'moonrifle', 'silverbolt'] },
  { name: 'Gear', items: ['armor'] },
]);

export const SLOTS = Object.freeze(['primary', 'secondary', 'melee']);
export const defaultLoadout = () => ({ primary: null, secondary: 'nightfang', melee: 'dagger' });
export const canUse = (w, team) => !w.teams || w.teams.includes(team);
