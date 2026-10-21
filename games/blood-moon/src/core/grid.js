// Tile grid: collision, ray casting (with per-tile height), line of sight and A* pathfinding.
export const WALL_H = 3.6;
const HEIGHT = { '#': WALL_H, p: WALL_H, c: 1.3 };
const clamp = (v, a, b) => (v < a ? a : v > b ? b : v);
const DIRS = [[1, 0], [-1, 0], [0, 1], [0, -1], [1, 1], [1, -1], [-1, 1], [-1, -1]];

export class Grid {
  constructor(rows, cell, zones = {}) {
    this.rows = rows; this.cell = cell; this.zones = zones;
    this.w = rows[0].length; this.h = rows.length;
    this.hgt = new Float32Array(this.w * this.h);
    this.near = new Uint8Array(this.w * this.h); // free tile touching a solid one (path penalty)
    rows.forEach((r, z) => { for (let x = 0; x < this.w; x++) this.hgt[z * this.w + x] = HEIGHT[r[x]] ?? 0; });
    for (let z = 0; z < this.h; z++) for (let x = 0; x < this.w; x++)
      if (!this.solid(x, z) && DIRS.some(([dx, dz]) => this.solid(x + dx, z + dz))) this.near[z * this.w + x] = 1;
  }

  height(cx, cz) { return cx < 0 || cz < 0 || cx >= this.w || cz >= this.h ? WALL_H : this.hgt[cz * this.w + cx]; }
  solid(cx, cz) { return this.height(cx, cz) > 0; }
  cellOf(v) { return Math.floor(v / this.cell); }
  center(c) { return (c + 0.5) * this.cell; }

  zoneAt(x, z) {
    const cx = this.cellOf(x), cz = this.cellOf(z);
    for (const id in this.zones) {
      const [x1, z1, x2, z2] = this.zones[id];
      if (cx >= x1 && cx <= x2 && cz >= z1 && cz <= z2) return id;
    }
    return null;
  }

  zoneCenter(id) {
    const [x1, z1, x2, z2] = this.zones[id];
    return { x: this.center((x1 + x2) / 2 - 0.5), z: this.center((z1 + z2) / 2 - 0.5) };
  }

  /** Does a circle (x,z,r) overlap any solid tile? */
  hitsWall(x, z, r) {
    const c = this.cell;
    for (let cz = this.cellOf(z - r); cz <= this.cellOf(z + r); cz++)
      for (let cx = this.cellOf(x - r); cx <= this.cellOf(x + r); cx++) {
        if (!this.solid(cx, cz)) continue;
        const nx = clamp(x, cx * c, cx * c + c), nz = clamp(z, cz * c, cz * c + c);
        if ((x - nx) ** 2 + (z - nz) ** 2 < r * r) return true;
      }
    return false;
  }

  /** Push a circle out of any overlapping solid tile (in place). */
  resolve(p, r) {
    const c = this.cell;
    for (let cz = this.cellOf(p.z - r); cz <= this.cellOf(p.z + r); cz++)
      for (let cx = this.cellOf(p.x - r); cx <= this.cellOf(p.x + r); cx++) {
        if (!this.solid(cx, cz)) continue;
        const minX = cx * c, maxX = minX + c, minZ = cz * c, maxZ = minZ + c;
        const dx = p.x - clamp(p.x, minX, maxX), dz = p.z - clamp(p.z, minZ, maxZ);
        const d2 = dx * dx + dz * dz;
        if (d2 >= r * r) continue;
        if (d2 > 1e-9) { const k = (r - Math.sqrt(d2)) / Math.sqrt(d2); p.x += dx * k; p.z += dz * k; }
        else {
          const l = p.x - minX, rt = maxX - p.x, t = p.z - minZ, b = maxZ - p.z, m = Math.min(l, rt, t, b);
          if (m === l) p.x = minX - r; else if (m === rt) p.x = maxX + r; else if (m === t) p.z = minZ - r; else p.z = maxZ + r;
        }
      }
  }

  /** Move with sub-stepping (no tunnelling) and wall sliding. Returns distance actually travelled. */
  move(p, dx, dz, r) {
    const sx = p.x, sz = p.z, steps = Math.max(1, Math.ceil(Math.hypot(dx, dz) / 0.25));
    for (let i = 0; i < steps; i++) { p.x += dx / steps; p.z += dz / steps; this.resolve(p, r); this.resolve(p, r); }
    return Math.hypot(p.x - sx, p.z - sz);
  }

  /** 3D ray vs. tile heights + floor. Direction must be normalised. */
  raycast(ox, oy, oz, dx, dy, dz, maxDist) {
    let tMax = maxDist, kind = null;
    if (dy < 0) { const t = -oy / dy; if (t >= 0 && t < tMax) { tMax = t; kind = 'floor'; } }
    const hl = Math.hypot(dx, dz);
    if (hl > 1e-6) {
      const c = this.cell, ux = dx / hl, uz = dz / hl;
      let cx = this.cellOf(ox), cz = this.cellOf(oz);
      const sX = ux > 0 ? 1 : -1, sZ = uz > 0 ? 1 : -1;
      const tDx = ux ? Math.abs(c / ux) : Infinity, tDz = uz ? Math.abs(c / uz) : Infinity;
      let tx = ux ? ((ux > 0 ? (cx + 1) * c - ox : ox - cx * c) / Math.abs(ux)) : Infinity;
      let tz = uz ? ((uz > 0 ? (cz + 1) * c - oz : oz - cz * c) / Math.abs(uz)) : Infinity;
      const sMax = tMax * hl;
      for (let i = 0; i < 400; i++) {
        let s;
        if (tx < tz) { s = tx; cx += sX; tx += tDx; } else { s = tz; cz += sZ; tz += tDz; }
        if (s > sMax) break;
        const h = this.height(cx, cz);
        if (h > 0 && oy + dy * (s / hl) < h) { tMax = s / hl; kind = 'wall'; break; }
      }
    }
    return kind ? { dist: tMax, kind, x: ox + dx * tMax, y: oy + dy * tMax, z: oz + dz * tMax } : null;
  }

  /** Unobstructed line between two 3D points? */
  visible(a, b) {
    const dx = b.x - a.x, dy = b.y - a.y, dz = b.z - a.z, len = Math.hypot(dx, dy, dz);
    if (len < 1e-6) return true;
    return !this.raycast(a.x, a.y, a.z, dx / len, dy / len, dz / len, len - 0.05);
  }

  walkable(ax, az, bx, bz, r) {
    const len = Math.hypot(bx - ax, bz - az), n = Math.max(1, Math.ceil(len / 0.35));
    for (let i = 1; i <= n; i++) if (this.hitsWall(ax + ((bx - ax) * i) / n, az + ((bz - az) * i) / n, r)) return false;
    return true;
  }

  nearestFree(cx, cz) {
    if (!this.solid(cx, cz)) return { cx, cz };
    for (let r = 1; r < 8; r++)
      for (let dz = -r; dz <= r; dz++) for (let dx = -r; dx <= r; dx++)
        if (Math.max(Math.abs(dx), Math.abs(dz)) === r && !this.solid(cx + dx, cz + dz)) return { cx: cx + dx, cz: cz + dz };
    return null;
  }

  /** A* over free tiles (8-way, no corner cutting, wall-hugging penalty), then string-pulled. */
  findPath(sx, sz, ex, ez, radius = 0.5) {
    const s = this.nearestFree(this.cellOf(sx), this.cellOf(sz)), g = this.nearestFree(this.cellOf(ex), this.cellOf(ez));
    if (!s || !g) return null;
    const W = this.w, N = W * this.h, goal = g.cz * W + g.cx, start = s.cz * W + s.cx;
    const gs = new Float32Array(N).fill(Infinity), from = new Int32Array(N).fill(-1), closed = new Uint8Array(N);
    const heap = []; const f = new Float32Array(N);
    const push = (i) => { heap.push(i); let k = heap.length - 1; while (k > 0) { const p = (k - 1) >> 1; if (f[heap[p]] <= f[heap[k]]) break; [heap[p], heap[k]] = [heap[k], heap[p]]; k = p; } };
    const pop = () => { const top = heap[0], last = heap.pop(); if (heap.length) { heap[0] = last; let k = 0; for (;;) { const l = 2 * k + 1, r = l + 1; let m = k; if (l < heap.length && f[heap[l]] < f[heap[m]]) m = l; if (r < heap.length && f[heap[r]] < f[heap[m]]) m = r; if (m === k) break; [heap[m], heap[k]] = [heap[k], heap[m]]; k = m; } } return top; };
    const hf = (i) => { const dx = Math.abs((i % W) - g.cx), dz = Math.abs(((i / W) | 0) - g.cz); return Math.max(dx, dz) + 0.414 * Math.min(dx, dz); };
    gs[start] = 0; f[start] = hf(start); push(start);
    while (heap.length) {
      const cur = pop();
      if (closed[cur]) continue;
      closed[cur] = 1;
      if (cur === goal) break;
      const cx = cur % W, cz = (cur / W) | 0;
      for (const [dx, dz] of DIRS) {
        const nx = cx + dx, nz = cz + dz;
        if (this.solid(nx, nz)) continue;
        if (dx && dz && (this.solid(cx + dx, cz) || this.solid(cx, cz + dz))) continue;
        const ni = nz * W + nx;
        if (closed[ni]) continue;
        const cost = gs[cur] + (dx && dz ? 1.414 : 1) + (this.near[ni] ? 0.6 : 0);
        if (cost < gs[ni]) { gs[ni] = cost; from[ni] = cur; f[ni] = cost + hf(ni); push(ni); }
      }
    }
    if (!closed[goal]) return null;
    const pts = [];
    for (let i = goal; i !== start && i !== -1; i = from[i]) pts.push({ x: this.center(i % W), z: this.center((i / W) | 0) });
    pts.reverse();
    const out = []; let cur = { x: sx, z: sz }, i = 0;
    while (i < pts.length) {
      let j = pts.length - 1;
      while (j > i && !this.walkable(cur.x, cur.z, pts[j].x, pts[j].z, radius)) j--;
      out.push(pts[j]); cur = pts[j]; i = j + 1;
    }
    return out;
  }
}
