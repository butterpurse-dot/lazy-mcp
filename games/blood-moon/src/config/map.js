// Map "Crimson Cathedral": hand-authored from rectangles so it stays easy to edit/extend.
// All coordinates are grid cells (x, z). Vampires attack (plant the Blood Sigil at site A or B),
// Werewolves defend. Props marked `mirror` are duplicated across the vertical axis.
export const MAP = Object.freeze({
  name: 'Crimson Cathedral',
  cell: 1.5, width: 48, height: 40,
  carve: [
    [18, 2, 29, 7],   // werewolf spawn
    [18, 32, 29, 37], // vampire spawn
    [3, 3, 14, 12],   // site A hall
    [33, 3, 44, 12],  // site B hall
    [14, 4, 17, 7],   // A <-> wolf spawn
    [30, 4, 33, 7],   // B <-> wolf spawn
    [21, 8, 26, 31],  // mid nave
    [3, 13, 8, 36],   // left lane
    [39, 13, 44, 36], // right lane
    [9, 33, 17, 36],  // vampire -> left lane hall
    [30, 33, 38, 36], // vampire -> right lane hall
    [9, 20, 20, 23],  // left <-> mid crossing
    [27, 20, 38, 23], // mid <-> right crossing
    [10, 13, 12, 19], // short A
    [35, 13, 37, 19], // short B
  ],
  // [char, x, z, mirror]  p = pillar (full height), c = crate (1.3 m cover)
  props: [
    ['c', 4, 10, 1], ['c', 5, 10, 1], ['c', 12, 5, 1], ['c', 12, 6, 1], ['c', 11, 11, 1], ['c', 8, 3, 1],
    ['c', 5, 17, 1], ['c', 6, 17, 1], ['c', 4, 24, 1], ['c', 5, 24, 1], ['c', 6, 29, 1], ['c', 7, 29, 1],
    ['c', 14, 21, 1], ['c', 14, 22, 1], ['c', 13, 34, 1], ['c', 13, 35, 1],
    ['p', 22, 13, 1], ['p', 22, 27, 1], ['p', 10, 8, 1],
    ['c', 23, 10, 0], ['c', 24, 10, 0], ['c', 23, 16, 0], ['c', 24, 16, 0], ['c', 23, 25, 0], ['c', 24, 25, 0], ['c', 23, 29, 0], ['c', 24, 29, 0],
  ],
  zones: { A: [5, 5, 9, 9], B: [38, 5, 42, 9] },
  holds: { A: [3, 3, 14, 12], B: [33, 3, 44, 12], M: [21, 8, 26, 12] },
  spawns: {
    werewolf: [[20, 3], [23, 3], [26, 3], [20, 5], [23, 5], [26, 5], [28, 4]],
    vampire: [[20, 36], [23, 36], [26, 36], [20, 34], [23, 34], [26, 34], [28, 35]],
  },
  lights: [[7, 7, 0xff9a3c], [40, 7, 0xff9a3c], [23.5, 4, 0x6f8cff], [23.5, 35, 0xff3c3c], [23.5, 20, 0xb06cff], [5, 25, 0xff6a3c], [42, 25, 0xff6a3c]],
});

export function buildRows(map = MAP) {
  const g = Array.from({ length: map.height }, () => Array(map.width).fill('#'));
  for (const [x1, z1, x2, z2] of map.carve)
    for (let z = z1; z <= z2; z++) for (let x = x1; x <= x2; x++) g[z][x] = '.';
  for (const [ch, x, z, mirror] of map.props) {
    g[z][x] = ch;
    if (mirror) g[z][map.width - 1 - x] = ch;
  }
  return g.map((r) => r.join(''));
}
