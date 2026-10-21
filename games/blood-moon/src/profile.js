// Player settings + lifetime stats, persisted locally. Everything read back is validated.
const KEY = 'bloodmoon.profile.v1';
const clamp = (v, a, b, d) => (Number.isFinite(+v) ? Math.min(b, Math.max(a, +v)) : d);
const DEFAULT = { team: 'vampire', difficulty: 'normal', sens: 1, volume: 0.6, stats: { matches: 0, wins: 0, kills: 0, deaths: 0 } };

export function loadProfile() {
  try {
    const raw = JSON.parse(localStorage.getItem(KEY) || '{}'), s = raw.stats || {};
    return {
      team: raw.team === 'werewolf' ? 'werewolf' : 'vampire',
      difficulty: ['easy', 'normal', 'hard'].includes(raw.difficulty) ? raw.difficulty : DEFAULT.difficulty,
      sens: clamp(raw.sens, 0.2, 4, 1), volume: clamp(raw.volume, 0, 1, 0.6),
      stats: { matches: clamp(s.matches, 0, 1e9, 0), wins: clamp(s.wins, 0, 1e9, 0), kills: clamp(s.kills, 0, 1e9, 0), deaths: clamp(s.deaths, 0, 1e9, 0) },
    };
  } catch { return structuredClone(DEFAULT); }
}

export function saveProfile(p) {
  try { localStorage.setItem(KEY, JSON.stringify(p)); } catch { /* storage unavailable (private mode) - non-fatal */ }
}
