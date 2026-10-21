# Blood Moon - Vampires vs Werewolves

A Counter-Strike style 5v5 tactical shooter that runs in the browser (Three.js, no install, no assets to download).
You play one human slot; the other nine are AI bots with pathfinding, hearing, objective logic and an economy.

## Play

```bash
cd games/blood-moon
npm ci
npm run dev          # http://localhost:5173
# or: npm run build && npm run preview
```

Pick a team, press **PLAY**. Click to capture the mouse (Esc pauses).

| Key | Action | Key | Action |
|---|---|---|---|
| W A S D | move | R | reload |
| Mouse / LMB / RMB | aim / fire / scope (sniper) | 1 2 3 | primary / sidearm / dagger |
| Shift | walk (quiet, accurate) | B | armory (buy menu; digits buy too) |
| Space | jump | E (hold) | plant / cleanse the Sigil |
| F | team ability | Tab | scoreboard |

## Rules (CS-style bomb defusal, re-skinned)

* **Vampires attack:** carry out the plant - hold `E` in site **A** or **B** for 3s to place the **Blood Sigil**; it detonates after 35s.
* **Werewolves defend:** stop the plant, or hold `E` next to the Sigil for 5s to cleanse it. If time runs out with no plant, they win.
* Wipe the other team to win the round. If every Vampire dies *after* planting, the Sigil keeps counting down - Werewolves must still cleanse it.
* First to **7** rounds wins. Freeze/buy phase each round, kill rewards, win/loss bonuses with loss streak, survivors keep gear.
* **Vampires:** 100 HP, faster, heal 25 HP per kill, **Shadow Step** dash. **Werewolves:** 125 HP, **Feral Howl** (haste + reveal all enemies on radar), exclusive Moon Rifle (Vampires get the Blood Carbine).

Map: *Crimson Cathedral* - two sites, three lanes, mid nave, shortcuts, crates (shoot over them, not through) and pillars.

## Architecture

```
src/config/   rules, teams, weapons, map          <- all balance/content is data; no code change to tweak or add
src/core/     sim, match, grid, combat, bot, ...  <- pure JS, no DOM/WebGL: deterministic with a seeded RNG, unit-tested
src/render/   three.js world, models, fx          <- reads sim state, never mutates it
src/ui.js     HUD, radar, armory, scoreboard      <- DOM only, textContent only (no innerHTML)
src/audio/    synthesised WebAudio SFX            <- zero audio files
test/         vitest: map validity, grid/raycast/A*, combat, rules/economy, player actions, full bot-vs-bot matches
```

* `Sim` is authoritative and emits events on `sim.bus` (`shot`, `hit`, `kill`, `plant`, `roundEnd`, ...). Renderer, HUD and audio only subscribe. That is the seam for networked multiplayer: run `Sim` on a Node server, send intents, stream snapshots.
* **Add a weapon:** one entry in `config/weapons.js` (+ a `cls` for model/sound). It appears in the armory and bot buy logic automatically (bots buy by price/team).
* **Add a map:** a new object like `config/map.js` (rectangles to carve, props, zones, spawns, lights). `test/core.test.js` verifies spawn -> site connectivity.
* **Tune balance:** `config/rules.js` (economy, timers, bot difficulty) and `config/teams.js` (HP, speed, abilities). A headless bot-vs-bot run is ~1s per match for balance sweeps.

## Security & quality

* Static client, no backend, no secrets, no third-party network calls. Production build ships a strict **CSP** (`default-src 'self'`, no inline scripts/styles, no eval).
* No `innerHTML`; bot names and all text are written via `textContent`. Saved settings are validated/clamped on load (`profile.js`).
* Dependencies pinned exactly; `npm audit` is clean and CI fails on production-dependency advisories.
* Fixed 60 Hz simulation step independent of frame rate; one `Sim` instance and one GL context; pooled FX (no per-shot allocations of meshes).

## Monetisation paths (not implemented, designed for)

* **Cosmetics:** team skins/weapon skins are just material parameters in `render/models.js`; add a `skin` id to the profile and an entitlement check.
* **Content packs:** extra maps and weapons are pure data files - sell as DLC or unlock via season pass.
* **Multiplayer tier:** host the authoritative `Sim` on a server (rooms/matchmaking) and charge for ranked/private servers.
* **Web distribution:** the build is a ~150 kB gz static bundle, deployable to any CDN/itch.io/Steam (Electron/Tauri wrapper) or ad-supported portals.
