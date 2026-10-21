import { EYE } from './combat.js';

export const emptyIntent = () => ({ mx: 0, mz: 0, walk: false, jump: false, fire: false, reload: false, use: false, ability: false, slot: null, scope: false });
export const eyeOf = (a) => ({ x: a.pos.x, y: a.pos.y + EYE, z: a.pos.z });
/** View direction: yaw about +Y (0 = facing -Z), pitch up positive. */
export const forward = (yaw, pitch) => ({ x: -Math.sin(yaw) * Math.cos(pitch), y: Math.sin(pitch), z: -Math.cos(yaw) * Math.cos(pitch) });
