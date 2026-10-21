import * as THREE from 'three';

const mat = (color, o = {}) => new THREE.MeshStandardMaterial({ color, roughness: 0.8, ...o });
const box = (w, h, d, m, x = 0, y = 0, z = 0) => { const b = new THREE.Mesh(new THREE.BoxGeometry(w, h, d), m); b.position.set(x, y, z); return b; };
const GUN = mat(0x1a1a20, { metalness: 0.6, roughness: 0.4, emissive: 0x08080c });
const WOOD = mat(0x4a2e1a);
const SILVER = mat(0xc9d2e0, { metalness: 0.9, roughness: 0.25, emissive: 0x181a20 });

/** Weapon mesh built from primitives. Barrel points to -Z; `userData.muzzle` is the muzzle z. */
export function buildGun(cls) {
  const g = new THREE.Group();
  const add = (m) => (g.add(m), m);
  switch (cls) {
    case 'pistol': add(box(0.05, 0.08, 0.26, GUN, 0, 0, -0.1)); add(box(0.045, 0.12, 0.06, WOOD, 0, -0.09, 0)); g.userData.muzzle = -0.24; break;
    case 'smg': add(box(0.06, 0.1, 0.4, GUN, 0, 0, -0.15)); add(box(0.04, 0.2, 0.05, GUN, 0, -0.14, -0.1)); add(box(0.05, 0.1, 0.06, WOOD, 0, -0.08, 0.1)); g.userData.muzzle = -0.36; break;
    case 'rifle': add(box(0.06, 0.1, 0.62, GUN, 0, 0, -0.25)); add(box(0.05, 0.16, 0.06, GUN, 0, -0.13, -0.2)); add(box(0.06, 0.12, 0.22, WOOD, 0, -0.02, 0.2)); add(box(0.03, 0.04, 0.2, SILVER, 0, 0.03, -0.62)); g.userData.muzzle = -0.72; break;
    case 'shotgun': add(box(0.07, 0.08, 0.78, GUN, 0, 0, -0.3)); add(box(0.08, 0.07, 0.24, WOOD, 0, -0.06, -0.35)); add(box(0.07, 0.11, 0.24, WOOD, 0, -0.02, 0.2)); g.userData.muzzle = -0.7; break;
    case 'sniper': {
      add(box(0.06, 0.1, 0.95, GUN, 0, 0, -0.35)); add(box(0.065, 0.12, 0.26, WOOD, 0, -0.02, 0.28));
      const scope = new THREE.Mesh(new THREE.CylinderGeometry(0.035, 0.035, 0.34, 10), GUN); scope.rotation.x = Math.PI / 2; scope.position.set(0, 0.1, -0.25); add(scope);
      g.userData.muzzle = -0.85; break;
    }
    default: add(box(0.03, 0.05, 0.4, SILVER, 0, 0, -0.28)); add(box(0.12, 0.03, 0.03, GUN, 0, 0, -0.06)); add(box(0.04, 0.04, 0.14, WOOD, 0, 0, 0.04)); g.userData.muzzle = -0.5;
  }
  return g;
}

/** Procedural low-poly Vampire / Werewolf. Facing -Z, 1.85 m tall to match the hit cylinder. */
export function buildCharacter(team) {
  const root = new THREE.Group(), body = new THREE.Group(); root.add(body);
  const vamp = team === 'vampire';
  const glow = vamp ? 0x2a0a12 : 0x2a1a0a;
  const skin = mat(vamp ? 0xe6dcdc : 0x4d3220, { emissive: glow }), cloth = mat(vamp ? 0x241a30 : 0x4a331f, { emissive: glow }), accent = mat(vamp ? 0x7a0e22 : 0x6e4a2c);
  const eye = new THREE.MeshBasicMaterial({ color: vamp ? 0xff2030 : 0xffd23a });
  const sw = vamp ? 1 : 1.35;

  const mkLeg = (x) => { const p = new THREE.Group(); p.position.set(x, 0.88, 0); p.add(box(0.17 * sw, 0.88, 0.2 * sw, cloth, 0, -0.44, 0)); body.add(p); return p; };
  const legL = mkLeg(-0.13 * sw), legR = mkLeg(0.13 * sw);
  const torso = vamp ? new THREE.Mesh(new THREE.CylinderGeometry(0.24, 0.32, 0.95, 8), cloth) : box(0.7, 0.75, 0.42, skin, 0, 0, 0);
  torso.position.set(0, vamp ? 1.3 : 1.28, 0); body.add(torso);
  if (vamp) {
    const cape = new THREE.Mesh(new THREE.PlaneGeometry(0.75, 1.15), new THREE.MeshStandardMaterial({ color: 0x6d0c1e, side: THREE.DoubleSide, roughness: 0.9 }));
    cape.position.set(0, 1.18, 0.22); cape.rotation.x = 0.12; body.add(cape);
    body.add(box(0.6, 0.12, 0.3, accent, 0, 1.72, 0));
  } else body.add(box(0.8, 0.28, 0.5, skin, 0, 1.62, 0.04), box(0.3, 0.2, 0.5, accent, 0, 0.92, 0.02));
  const head = new THREE.Group(); head.position.set(0, 1.7, -0.02); body.add(head);
  if (vamp) {
    head.add(new THREE.Mesh(new THREE.SphereGeometry(0.16, 10, 8), skin), box(0.34, 0.1, 0.34, mat(0x0a0a10), 0, 0.1, 0.02));
    head.add(box(0.05, 0.03, 0.02, eye, -0.06, 0.02, -0.15), box(0.05, 0.03, 0.02, eye, 0.06, 0.02, -0.15), box(0.02, 0.05, 0.02, mat(0xffffff), -0.03, -0.09, -0.15), box(0.02, 0.05, 0.02, mat(0xffffff), 0.03, -0.09, -0.15));
  } else {
    head.add(new THREE.Mesh(new THREE.SphereGeometry(0.21, 10, 8), skin), box(0.16, 0.13, 0.3, skin, 0, -0.06, -0.26), box(0.05, 0.04, 0.04, mat(0x050505), 0, -0.02, -0.42));
    for (const s of [-1, 1]) {
      const ear = new THREE.Mesh(new THREE.ConeGeometry(0.07, 0.2, 5), skin); ear.position.set(s * 0.14, 0.22, 0.02); head.add(ear);
      head.add(box(0.06, 0.03, 0.02, eye, s * 0.09, 0.05, -0.2));
    }
    head.add(box(0.14, 0.03, 0.2, mat(0xffffff), 0, -0.13, -0.28));
  }
  const arm = (x) => {
    const p = new THREE.Group(); p.position.set(x * sw, 1.55, 0);
    p.add(box(0.1 * sw, 0.62, 0.1 * sw, vamp ? cloth : skin, 0, -0.31, 0)); body.add(p); return p;
  };
  const armL = arm(-0.3), armR = arm(0.3);
  armR.rotation.x = armL.rotation.x = -1.35;
  const gun = new THREE.Group(); gun.position.set(0.18, 1.32, -0.35); body.add(gun);
  if (!vamp) body.scale.set(1, 1.04, 1);
  return { root, body, legL, legR, armL, armR, gun, head, mats: [skin, cloth], glow: new THREE.Color(glow), gunCls: null };
}

/** First-person arms + weapon, parented to the camera. */
export function buildViewmodel(team) {
  const root = new THREE.Group(), holder = new THREE.Group(); root.add(holder);
  const skin = mat(team === 'vampire' ? 0xe0d4d4 : 0x6a4426, { emissive: 0x201010, depthTest: false }), sleeve = mat(team === 'vampire' ? 0x1b1424 : 0x4a331f, { emissive: 0x0a0608, depthTest: false });
  const forearm = box(0.08, 0.08, 0.4, sleeve, 0.03, -0.12, 0.34); forearm.rotation.x = 0.3;
  const hand = box(0.075, 0.075, 0.1, skin, 0, -0.045, 0.06);
  const arm2 = box(0.075, 0.075, 0.34, sleeve, -0.1, -0.1, 0.12); arm2.rotation.y = 0.35;
  holder.add(forearm, hand, arm2);
  root.userData = { holder, gun: null, cls: null };
  return root;
}

export function setViewGun(vm, cls) {
  if (vm.userData.cls === cls) return;
  const { holder } = vm.userData;
  if (vm.userData.gun) holder.remove(vm.userData.gun);
  const g = buildGun(cls);
  g.traverse((o) => { if (o.material) { o.material = o.material.clone(); o.material.depthTest = false; } });
  g.position.set(0, 0, 0); holder.add(g); vm.userData.gun = g; vm.userData.cls = cls;
  holder.position.set(0, 0, cls === 'sniper' ? 0.1 : 0);
}
