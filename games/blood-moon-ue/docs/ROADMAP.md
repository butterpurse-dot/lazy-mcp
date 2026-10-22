# Roadmap

## 0. Make it compile and play (next step, needs a machine with UE 5.5)
Build, fix compile errors, play a full match vs bots, then profile. Everything else builds on this.

## 1. Modern look and feel (CoD / CS2 class): build on Lyra - no primitive geometry
The current cubes/cylinders are placeholders for testing the rules. The target is a modern shooter look and feel, and the fastest
credible route is **Epic's free Lyra Starter Game** (UE 5.5): production-quality animated characters, first-person weapons with
recoil/ADS/reload animations, hit reactions, Enhanced Input with gamepad support and rebinding, CommonUI menus, scalable settings
and a networked GAS-based shooter framework. Plan:
1. Create the project from Lyra; add Blood Moon as a **Game Feature plugin** (`Plugins/GameFeatures/BloodMoon`). Port: `BMMatch`
   (as-is), economy and buy menu, plant/cleanse Sigil, team abilities (as GAS abilities), bots' objective logic (on Lyra's bots).
2. Art: vampire and werewolf characters retargeted onto Lyra's skeleton (Fab/marketplace or commissioned), Megascans gothic
   cathedral kit for the map, Niagara VFX (muzzle, blood, sigil, howl, dash), MetaSounds audio.
3. Keep the C++ in this folder as the reference implementation and test bed until the Lyra port reaches parity.
Licensing: Lyra and Fab content are licensed for use in Unreal Engine projects - keep that content out of public repos.

## 1b. Graphics quality details
The code uses engine cube/cylinder primitives as placeholders. Engine settings are already modern (Lumen, virtual shadow maps, TSR,
fog, atmosphere in `DefaultEngine.ini` and `BMArena`), but pretty comes from content:
* Environment: Quixel Megascans / Fab Nanite kits (gothic stone, brick, rubble), trim sheets, decals. Replace `BMArena` ISM cubes with a hand-authored level (`.umap`) built from modular kit pieces, keeping the same grid for bot pathing or switching to a NavMesh.
* Characters: skeletal-mesh vampire/werewolf (MetaHuman base or purchased/commissioned), Motion Matching or standard locomotion, IK, first-person arms + weapon models with proper animations.
* VFX: Niagara muzzle flash, impacts (blood, sparks, dust), sigil, howl, dash; post-process (bloom, film grain, color grade).
* Audio: MetaSounds, real recorded weapon sounds, positional footsteps with surface types.
Budget it: a stylized look with strong lighting beats scattered realism on a small team.

## 2. Performance budget - "no lag from skins and mods"
* Frame budget 16.6 ms (60 fps) low tier / 8.3 ms (120 fps) high tier. Add `stat unit` checks to CI-style perf captures.
* **Skins are data, not code:** each skin = a `UPrimaryDataAsset` (mesh variant + material instance params + icon). All skins of a character share one master material and texture atlas, so thousands of skins cost zero extra draw calls; load asynchronously via `UAssetManager`, never on the game thread at spawn.
* **Mods are content-only paks:** ship a documented modding profile (meshes, textures, sounds, data tables). No arbitrary native code or Blueprint scripting in user content -> no malware surface and no unbounded per-frame cost. Validate size/triangle/texture limits at load and reject over-budget packs.
* Instancing for static geometry (already used for walls/crates), Nanite for meshes, texture streaming pool sized per tier, pooled Niagara systems and tracers (no per-shot spawns - `ABMTracer` is the first thing to pool), fixed server tick (60 Hz) with client prediction.
* Quality tiers in `Config/DefaultScalability.ini` + an in-game menu (resolution scale, TSR/DLSS/FSR, shadows, GI).

## 3. Steam
1. Steamworks partner account ($100 app fee) -> App ID.
2. Enable the *Online Subsystem Steam* plugin, add Steamworks SDK, set `DefaultPlatformService=Steam`, `steam_appid.txt` for testing.
3. Sessions/matchmaking via Online Subsystem; Steam Input, achievements, cloud saves, rich presence.
4. Build with `RunUAT BuildCookRun` (Shipping, pak), upload depots with `steamcmd`, store page + trailer + review.
5. Anti-cheat: keep everything server-authoritative (already so); add EasyAntiCheat (free via Epic Online Services) before a public multiplayer launch.

## 4. Consoles
Xbox, PlayStation and Switch require being an approved developer (ID@Xbox, PlayStation Partners, Nintendo Developer Portal), a devkit and
platform-holder certification; Unreal's platform code is distributed to approved developers. Plan on: gamepad input and aim assist (the code polls keys,
so add an input abstraction first), console-safe UI (the HUD is canvas-based and scales), 30/60 fps targets, TRC/XR compliance, and a publisher or porting partner.

## 5. Gameplay still to build
Known limitations of the current scaffold (all found in code review, to be fixed or superseded by the Lyra port):
no in-game key rebinding UI or pause menu yet (Enhanced Input is in place; Lyra's settings screens cover both); Shadow Step/haste are not movement-predicted
(possible client correction); late joiners wait for the next round and teams can exceed 5; arena walls are per-instance physics bodies.
Lag compensation / server-side rewind for hitscan, proper movement-speed prediction (custom `UCharacterMovementComponent` instead of setting
`MaxWalkSpeed`), grenades/utility, weapon drops, bomb carrier, menus (team select, settings, server browser), spectator modes, replays, ranked, more maps.

## 6. Voxel / Minecraft-style mode (phase 2)
Concept: **day = build and gather, night = Blood Moon siege.** Players dig and place blocks to fortify a keep; each night vampires and werewolves
attack from opposite sides (PvE waves, or the competitive 5v5 using player-built arenas).
* Tech: chunked voxel world (16^3 or 32^3 chunks) meshed on worker threads into `UProceduralMeshComponent`/`UDynamicMesh` (greedy meshing, ambient occlusion baked into vertex colors, texture array atlas), Mass or custom ECS for mobs, replicated block deltas (never whole chunks), save as run-length-encoded chunks.
* Reuse: the round/economy state machine, weapons, abilities, HUD and bot AI (pathing moves from the tile grid to a voxel nav grid).
* Appeal beyond FPS players: crafting, base building, co-op survival, creative mode and map sharing (content-only pak mods, section 2).
