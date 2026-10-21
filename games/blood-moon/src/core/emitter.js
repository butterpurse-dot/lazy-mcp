export class Emitter {
  constructor() { this.h = new Map(); }
  on(type, fn) {
    if (!this.h.has(type)) this.h.set(type, new Set());
    this.h.get(type).add(fn);
    return () => this.h.get(type).delete(fn);
  }
  emit(type, data) { const s = this.h.get(type); if (s) for (const fn of s) fn(data); }
}
