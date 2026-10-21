import { RULES } from '../config/rules.js';
import { other } from '../config/teams.js';

/** Pure round/economy state machine (no world knowledge) - easy to unit-test and to run server-side later. */
export class Match {
  constructor(rules = RULES) { this.rules = rules; this.reset(); }

  reset() {
    this.round = 0;
    this.score = { vampire: 0, werewolf: 0 };
    this.streak = { vampire: 0, werewolf: 0 };
    this.phase = 'idle'; // idle | freeze | live | ended | matchover
    this.timer = 0; this.elapsed = 0;
    this.sigil = null; this.result = null; this.winner = null;
  }

  startRound() {
    this.round++; this.phase = 'freeze'; this.timer = this.rules.freezeTime;
    this.elapsed = 0; this.sigil = null; this.result = null;
  }

  get buyOpen() { return this.phase === 'freeze' || (this.phase === 'live' && this.elapsed < this.rules.buyWindow); }

  plant(site) {
    if (this.phase !== 'live' || this.sigil) return false;
    this.sigil = { site, fuse: this.rules.sigilFuse };
    return true;
  }

  cleanse() {
    if (this.phase !== 'live' || !this.sigil) return false;
    this.end('werewolf', 'cleansed');
    return true;
  }

  end(winner, reason) {
    const r = this.rules, loser = other(winner);
    const loss = Math.min(r.maxMoney, r.lossBase + r.lossStep * Math.min(this.streak[loser], r.lossMaxStreak));
    this.score[winner]++; this.streak[winner] = 0; this.streak[loser]++;
    this.result = { winner, reason, round: this.round, rewards: { [winner]: r.winReward, [loser]: loss } };
    this.phase = 'ended'; this.timer = r.endDelay;
    if (this.score[winner] >= r.winRounds) this.winner = winner;
  }

  /** Advance the clock. Returns 'live' | 'ended' | 'next' | 'matchOver' | null. */
  tick(dt, alive) {
    switch (this.phase) {
      case 'freeze':
        this.timer -= dt;
        if (this.timer <= 0) { this.phase = 'live'; this.timer = this.rules.roundTime; this.elapsed = 0; return 'live'; }
        return null;
      case 'live':
        this.elapsed += dt;
        if (this.sigil) {
          this.sigil.fuse -= dt;
          if (this.sigil.fuse <= 0) { this.sigil.fuse = 0; this.end('vampire', 'detonated'); return 'ended'; }
        } else {
          this.timer -= dt;
          if (this.timer <= 0) { this.timer = 0; this.end('werewolf', 'time'); return 'ended'; }
        }
        if (alive.werewolf <= 0) { this.end('vampire', 'elimination'); return 'ended'; }
        if (alive.vampire <= 0 && !this.sigil) { this.end('werewolf', 'elimination'); return 'ended'; }
        return null;
      case 'ended':
        this.timer -= dt;
        if (this.timer > 0) return null;
        if (this.winner) { this.phase = 'matchover'; return 'matchOver'; }
        return 'next';
      default:
        return null;
    }
  }
}
