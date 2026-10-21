export const TEAM_IDS = ['vampire', 'werewolf'];
export const other = (t) => (t === 'vampire' ? 'werewolf' : 'vampire');

// Asymmetric but balanced: Vampires are fast + lifesteal, Werewolves are tanky + can reveal.
export const TEAMS = Object.freeze({
  vampire: {
    id: 'vampire', name: 'Vampires', role: 'attack', css: '#e0284a', color: 0xb0102a,
    maxHp: 100, speed: 5.7, killHeal: 25,
    ability: { id: 'shadowstep', name: 'Shadow Step', cooldown: 12, distance: 6.5, hint: 'Dash forward' },
    names: ['Vlad', 'Selene', 'Lestat', 'Carmilla', 'Nosferatu', 'Alucard', 'Drusilla', 'Lilith'],
  },
  werewolf: {
    id: 'werewolf', name: 'Werewolves', role: 'defend', css: '#4d9bff', color: 0x6b4a2b,
    maxHp: 125, speed: 5.3, killHeal: 0,
    ability: { id: 'howl', name: 'Feral Howl', cooldown: 20, duration: 4, speedMul: 1.25, reveal: 3.5, hint: 'Haste + reveal enemies' },
    names: ['Fenrir', 'Remus', 'Greyback', 'Lupin', 'Akela', 'Fang', 'Garou', 'Ulfric'],
  },
});
