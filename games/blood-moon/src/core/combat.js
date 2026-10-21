// Pure combat math: hit detection against actors and damage calculation.
export const EYE = 1.65, HEIGHT = 1.85, HEAD_Y = 1.42, RADIUS = 0.42;

/** Ray (o + t*d, d normalised) vs. an upright cylinder. Returns {t, head} or null. */
export function rayVsActor(o, d, a, maxT) {
  const A = d.x * d.x + d.z * d.z;
  if (A < 1e-9) return null;
  const px = o.x - a.pos.x, pz = o.z - a.pos.z;
  const B = px * d.x + pz * d.z, C = px * px + pz * pz - RADIUS * RADIUS, disc = B * B - A * C;
  if (disc < 0) return null;
  const sq = Math.sqrt(disc);
  let t = (-B - sq) / A;
  if (t < 0) t = (-B + sq) / A;
  if (t < 0 || t > maxT) return null;
  const rel = o.y + d.y * t - a.pos.y;
  if (rel < 0 || rel > HEIGHT) return null;
  return { t, head: rel >= HEAD_Y };
}

export function calcDamage(w, dist, head) {
  const d = w.damage * Math.pow(w.falloff, dist / 10);
  return head ? d * w.headMul : d;
}

/** Armor soaks half of the "non-penetrating" share of damage. Returns HP actually lost. */
export function applyDamage(victim, dmg, pen) {
  let hp = dmg;
  if (victim.armor > 0) {
    const used = Math.min(victim.armor, dmg * (1 - pen) * 0.5);
    victim.armor -= used;
    hp = dmg - used * 2;
  }
  victim.hp -= hp;
  return hp;
}
