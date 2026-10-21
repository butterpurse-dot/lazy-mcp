import * as THREE from 'three';
import { mulberry32 } from '../core/rng.js';

function canvas(size, draw) {
  const c = document.createElement('canvas');
  c.width = c.height = size;
  draw(c.getContext('2d'), size, mulberry32(size * 31 + 7));
  return c;
}

function texture(c, rx = 1, ry = 1) {
  const t = new THREE.CanvasTexture(c);
  t.colorSpace = THREE.SRGBColorSpace; t.wrapS = t.wrapT = THREE.RepeatWrapping; t.repeat.set(rx, ry); t.anisotropy = 4;
  return t;
}

const shade = (r, base, spread) => { const v = Math.floor((r() - 0.5) * spread); return `rgb(${base[0] + v},${base[1] + v * 0.8},${base[2] + v * 0.8})`; };

export const brickTexture = () => texture(canvas(256, (g, s, r) => {
  g.fillStyle = '#1b1216'; g.fillRect(0, 0, s, s);
  const rows = 8, h = s / rows, w = s / 4;
  for (let y = 0; y < rows; y++) for (let x = -1; x < 5; x++) {
    g.fillStyle = shade(r, [92, 52, 56], 40);
    g.fillRect(x * w + (y % 2 ? w / 2 : 0) + 2, y * h + 2, w - 4, h - 4);
  }
  for (let i = 0; i < 1200; i++) { g.fillStyle = `rgba(0,0,0,${r() * 0.25})`; g.fillRect(r() * s, r() * s, 2, 2); }
}), 1, 2.4);

export const floorTexture = (tiles) => texture(canvas(256, (g, s, r) => {
  const n = 4, t = s / n;
  for (let y = 0; y < n; y++) for (let x = 0; x < n; x++) { g.fillStyle = shade(r, [58, 56, 66], 28); g.fillRect(x * t + 2, y * t + 2, t - 4, t - 4); }
  g.strokeStyle = 'rgba(0,0,0,.5)'; g.lineWidth = 1;
  for (let i = 0; i < 14; i++) { g.beginPath(); g.moveTo(r() * s, r() * s); g.lineTo(r() * s, r() * s); g.stroke(); }
}), tiles, tiles);

export const crateTexture = () => texture(canvas(128, (g, s, r) => {
  g.fillStyle = '#4a3320'; g.fillRect(0, 0, s, s);
  for (let i = 0; i < 8; i++) { g.fillStyle = shade(r, [88, 62, 38], 24); g.fillRect(0, i * (s / 8) + 1, s, s / 8 - 2); }
  g.strokeStyle = '#2a1c10'; g.lineWidth = 6; g.strokeRect(3, 3, s - 6, s - 6);
  g.beginPath(); g.moveTo(0, 0); g.lineTo(s, s); g.moveTo(s, 0); g.lineTo(0, s); g.stroke();
}));

export const stoneTexture = () => texture(canvas(128, (g, s, r) => {
  g.fillStyle = '#2c2a33'; g.fillRect(0, 0, s, s);
  for (let i = 0; i < 900; i++) { g.fillStyle = shade(r, [70, 68, 80], 40); g.fillRect(r() * s, r() * s, 3, 3); }
}), 1, 2.4);

/** Glowing site letter / rune disc on the floor. */
export const zoneTexture = (letter, css) => {
  const c = canvas(256, (g, s) => {
    const grad = g.createRadialGradient(s / 2, s / 2, 10, s / 2, s / 2, s / 2);
    grad.addColorStop(0, 'rgba(255,40,70,.35)'); grad.addColorStop(0.8, 'rgba(255,40,70,.18)'); grad.addColorStop(1, 'rgba(255,40,70,0)');
    g.fillStyle = grad; g.fillRect(0, 0, s, s);
    g.strokeStyle = css; g.lineWidth = 5; g.beginPath(); g.arc(s / 2, s / 2, s * 0.4, 0, 7); g.stroke();
    g.fillStyle = css; g.font = 'bold 130px Georgia, serif'; g.textAlign = 'center'; g.textBaseline = 'middle'; g.fillText(letter, s / 2, s / 2 + 8);
  });
  const t = new THREE.CanvasTexture(c); t.colorSpace = THREE.SRGBColorSpace; return t;
};

export const sigilTexture = () => {
  const t = new THREE.CanvasTexture(canvas(256, (g, s) => {
    g.translate(s / 2, s / 2); g.strokeStyle = '#ff2244'; g.lineWidth = 6; g.shadowColor = '#ff2244'; g.shadowBlur = 14;
    g.beginPath(); g.arc(0, 0, s * 0.45, 0, 7); g.stroke(); g.beginPath(); g.arc(0, 0, s * 0.38, 0, 7); g.stroke();
    g.beginPath();
    for (let i = 0; i < 6; i++) { const a = (i * 2 * Math.PI * 2) / 5 - Math.PI / 2; const x = Math.cos(a) * s * 0.38, y = Math.sin(a) * s * 0.38; i ? g.lineTo(x, y) : g.moveTo(x, y); }
    g.stroke();
  }));
  t.colorSpace = THREE.SRGBColorSpace; return t;
};

export const glowTexture = () => {
  const t = new THREE.CanvasTexture(canvas(64, (g, s) => {
    const gr = g.createRadialGradient(s / 2, s / 2, 0, s / 2, s / 2, s / 2);
    gr.addColorStop(0, 'rgba(255,240,200,1)'); gr.addColorStop(0.3, 'rgba(255,170,60,.8)'); gr.addColorStop(1, 'rgba(255,100,0,0)');
    g.fillStyle = gr; g.fillRect(0, 0, s, s);
  }));
  t.colorSpace = THREE.SRGBColorSpace; return t;
};

export function nameTexture(name, css) {
  const c = document.createElement('canvas'); c.width = 256; c.height = 64;
  const g = c.getContext('2d'); g.font = 'bold 34px system-ui, sans-serif'; g.textAlign = 'center'; g.textBaseline = 'middle';
  g.lineWidth = 6; g.strokeStyle = 'rgba(0,0,0,.8)'; g.strokeText(name, 128, 32); g.fillStyle = css; g.fillText(name, 128, 32);
  const t = new THREE.CanvasTexture(c); t.colorSpace = THREE.SRGBColorSpace; return t;
}

export const moonTexture = () => {
  const t = new THREE.CanvasTexture(canvas(256, (g, s, r) => {
    const gr = g.createRadialGradient(s * 0.42, s * 0.4, s * 0.05, s / 2, s / 2, s * 0.5);
    gr.addColorStop(0, '#ff7b5a'); gr.addColorStop(1, '#a3110c');
    g.fillStyle = gr; g.beginPath(); g.arc(s / 2, s / 2, s * 0.48, 0, 7); g.fill();
    for (let i = 0; i < 14; i++) { g.fillStyle = `rgba(60,0,0,${0.15 + r() * 0.2})`; g.beginPath(); g.arc(s * (0.2 + r() * 0.6), s * (0.2 + r() * 0.6), 6 + r() * 16, 0, 7); g.fill(); }
  }));
  t.colorSpace = THREE.SRGBColorSpace; return t;
};
