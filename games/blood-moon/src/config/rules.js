// Match rules - tweak here, no code changes needed.
export const RULES = Object.freeze({
  teamSize: 5,
  winRounds: 7,        // first to 7 wins (best of 13)
  freezeTime: 7,       // buy phase, nobody moves
  buyWindow: 20,       // seconds after round start in which buying stays open
  roundTime: 100,
  endDelay: 6,
  plantTime: 3,
  cleanseTime: 5,
  sigilFuse: 35,
  sigilRadius: 14,
  startMoney: 800,
  maxMoney: 16000,
  killReward: 300,
  plantReward: 300,
  winReward: 3250,
  lossBase: 1400,
  lossStep: 500,
  lossMaxStreak: 4,
});

export const DIFFICULTY = Object.freeze({
  easy:   { react: 0.6,  turn: 4.5,  extraSpread: 4.0, headAim: 0.08, fov: 100 },
  normal: { react: 0.34, turn: 7.5,  extraSpread: 2.2, headAim: 0.22, fov: 115 },
  hard:   { react: 0.18, turn: 11,   extraSpread: 1.1, headAim: 0.4,  fov: 130 },
});
