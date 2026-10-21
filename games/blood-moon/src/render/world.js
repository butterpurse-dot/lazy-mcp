import * as THREE from 'three';
import { WALL_H } from '../core/grid.js';
import { brickTexture, floorTexture, crateTexture, stoneTexture, zoneTexture, glowTexture, moonTexture } from './textures.js';
import { mulberry32 } from '../core/rng.js';

/** Builds all static geometry for a map. Returns { flicker(t) } for animated lights. */
export function buildWorld(scene, grid, map) {
  const c = grid.cell, W = grid.w * c, H = grid.h * c;
  scene.background = new THREE.Color(0x07060f);
  scene.fog = new THREE.Fog(0x0a0714, 18, 95);

  scene.add(new THREE.HemisphereLight(0x8090c8, 0x302030, 1.25));
  const moon = new THREE.DirectionalLight(0xff9a90, 0.7); moon.position.set(-40, 60, 30); scene.add(moon);

  // floor
  const floor = new THREE.Mesh(new THREE.PlaneGeometry(W, H), new THREE.MeshStandardMaterial({ map: floorTexture(W / 6), roughness: 0.95 }));
  floor.rotation.x = -Math.PI / 2; floor.position.set(W / 2, 0, H / 2); scene.add(floor);

  // walls / pillars / crates as instanced boxes; only tiles that touch walkable space are drawn
  const touchesFree = (x, z) => { for (let dz = -1; dz <= 1; dz++) for (let dx = -1; dx <= 1; dx++) if (!grid.solid(x + dx, z + dz)) return true; return false; };
  const lists = { '#': [], p: [], c: [] };
  for (let z = 0; z < grid.h; z++) for (let x = 0; x < grid.w; x++) { const ch = grid.rows[z][x]; if (lists[ch] && touchesFree(x, z)) lists[ch].push([x, z]); }
  const inst = (list, geo, mat, y) => {
    if (!list.length) return;
    const m = new THREE.InstancedMesh(geo, mat, list.length), d = new THREE.Object3D();
    list.forEach(([x, z], i) => { d.position.set(grid.center(x), y, grid.center(z)); d.updateMatrix(); m.setMatrixAt(i, d.matrix); });
    scene.add(m);
  };
  inst(lists['#'], new THREE.BoxGeometry(c, WALL_H, c), new THREE.MeshStandardMaterial({ map: brickTexture(), roughness: 0.9 }), WALL_H / 2);
  inst(lists.p, new THREE.CylinderGeometry(c * 0.5, c * 0.55, WALL_H, 10), new THREE.MeshStandardMaterial({ map: stoneTexture(), roughness: 0.8 }), WALL_H / 2);
  inst(lists.c, new THREE.BoxGeometry(c * 0.96, 1.3, c * 0.96), new THREE.MeshStandardMaterial({ map: crateTexture(), roughness: 0.8 }), 0.65);

  // site markers
  for (const id of Object.keys(map.zones)) {
    const [x1, z1, x2, z2] = map.zones[id], z = grid.zoneCenter(id);
    const m = new THREE.Mesh(new THREE.PlaneGeometry((x2 - x1 + 1) * c, (z2 - z1 + 1) * c),
      new THREE.MeshBasicMaterial({ map: zoneTexture(id, '#ff4466'), transparent: true, depthWrite: false, blending: THREE.AdditiveBlending, fog: false }));
    m.rotation.x = -Math.PI / 2; m.position.set(z.x, 0.03, z.z); scene.add(m);
  }

  // moon, stars
  const moonDisc = new THREE.Sprite(new THREE.SpriteMaterial({ map: moonTexture(), fog: false, depthWrite: false }));
  moonDisc.scale.set(60, 60, 1); moonDisc.position.set(-120, 150, -170); scene.add(moonDisc);
  const halo = new THREE.Sprite(new THREE.SpriteMaterial({ map: glowTexture(), color: 0xff2a1a, fog: false, blending: THREE.AdditiveBlending, depthWrite: false }));
  halo.scale.set(150, 150, 1); halo.position.copy(moonDisc.position).multiplyScalar(0.99); scene.add(halo);
  const r = mulberry32(5), pts = new Float32Array(900);
  for (let i = 0; i < 300; i++) { const u = r() * Math.PI * 2, v = r() * 0.9 + 0.1, R = 420; pts.set([Math.cos(u) * R * v + W / 2, Math.sqrt(1 - v * v) * R + 20, Math.sin(u) * R * v + H / 2], i * 3); }
  const sg = new THREE.BufferGeometry(); sg.setAttribute('position', new THREE.BufferAttribute(pts, 3));
  scene.add(new THREE.Points(sg, new THREE.PointsMaterial({ color: 0xddddff, size: 1.6, sizeAttenuation: false, fog: false })));

  // torches
  const torches = map.lights.map(([cx, cz, color], i) => {
    const light = new THREE.PointLight(color, 38, 26, 1.6); light.position.set(cx * c, 2.4, cz * c); scene.add(light);
    const flame = new THREE.Sprite(new THREE.SpriteMaterial({ map: glowTexture(), color, blending: THREE.AdditiveBlending, depthWrite: false }));
    flame.scale.set(0.9, 0.9, 1); flame.position.copy(light.position); scene.add(flame);
    return { light, flame, base: 38, phase: i * 1.7 };
  });
  return {
    flicker(t) {
      for (const k of torches) { const f = 0.85 + 0.15 * Math.sin(t * 9 + k.phase) * Math.sin(t * 5.3 + k.phase * 2); k.light.intensity = k.base * f; k.flame.scale.setScalar(0.8 + 0.2 * f); }
    },
  };
}
