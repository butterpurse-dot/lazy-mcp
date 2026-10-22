# Blood Moon (Unreal Engine 5) - Vampires vs Werewolves

Native version of the tactical shooter for **PC (Windows / Linux / SteamOS) and Mac**, with a path to Steam and consoles.
The browser prototype in `../blood-moon` stays as the "lite" edition for low-end machines.

> **Status: scaffold, not yet compiled.** The authoring environment had no Unreal Engine, no GPU and no display, so
> none of the C++ below has been built or played. Only `BMMatch` (the round/economy rules) is verified, with a plain-C++
> self-test (`Tools/match_selftest.cpp`). Expect a handful of compile errors on first build; paste them back and they get fixed.

## What is in the box

| Area | Where | State |
|---|---|---|
| Round/economy rules (freeze, live, plant, cleanse, loss streak, win-by-7) | `BMMatch.*` | **Tested** (clang self-test) |
| Replicated server-authoritative gameplay (hitscan, damage, armor, reload, abilities, plant/cleanse) | `BMCharacter.*` | Written, untested |
| Game mode: rounds, buy validation, kill rewards, bots fill teams, sigil, spectate | `BMGameMode.*` | Written, untested |
| Bots: perception, aim, strafing, objectives, A* pathing, stuck recovery | `BMBotController.*`, `BMArena.*` | Written, untested |
| Arena "Crimson Cathedral" built at runtime (no .umap needed), Lumen-ready lights | `BMArena.*` | Written, untested |
| Code-only HUD: vitals, ammo, radar, kill feed, armory, scoreboard | `BMHUD.*` | Written, untested |
| Weapon table (8 weapons) | `BMWeapons.*` | Ported from the JS prototype |

Controls are the same as the prototype: WASD, mouse, LMB fire, R reload, Shift walk, Space jump, 1/2/3 slots, **B** armory (digits buy),
**E** hold plant/cleanse, **F** ability, **Tab** scoreboard. Sensitivity: console `bm.Sens <deg per count>`.

## Build and run

Prerequisites: Unreal Engine **5.5** (Epic Games Launcher), plus Visual Studio 2022 with "Game development with C++" (Windows),
or Xcode (Mac), or the Linux toolchain from the UE docs.

```bash
# 1. Right-click BloodMoon.uproject -> "Generate Project Files" (or: <UE>/Engine/Build/BatchFiles/Build.bat ...)
# 2. Open the generated BloodMoon.sln and build "BloodMoonEditor | Development Editor", or just open the .uproject.
# 3. In the editor press Play (the default map is the engine's built-in /Engine/Maps/Entry; the arena is spawned by code).
```
Standalone with options: `UnrealEditor.exe BloodMoon.uproject /Engine/Maps/Entry?Team=werewolf?Diff=hard -game`
(`Team=vampire|werewolf`, `Diff=easy|normal|hard`).

Listen-server multiplayer works by construction (all gameplay is server-authoritative with replicated state):
`...Entry?listen` on the host, `open <ip>` on clients. Untested, and there is no lag compensation yet (see roadmap).

## Package an installable build

```bash
<UE>/Engine/Build/BatchFiles/RunUAT.bat BuildCookRun -project="<path>/BloodMoon.uproject" -platform=Win64 \
  -clientconfig=Shipping -build -cook -stage -pak -archive -archivedirectory=Dist
```
Use `-platform=Linux` for Linux/Steam Deck and `-platform=Mac` on a Mac. Wrap Win64 output with Inno Setup / MSIX for a normal installer.

## Hardware reality

* **Fine:** your GPU laptops (use the Epic/High scalability tiers: Lumen, virtual shadow maps, TSR).
* **Marginal:** old Windows machines with GPUs older than ~GTX 1060: use the Low tier (`Config/DefaultScalability.ini` turns Lumen/VSM off) and expect 720p-1080p at reduced settings.
* **No:** the old iMac. UE5 does not support those Intel Macs well and its editor will not run. Use the browser edition there (`../blood-moon`).

See `docs/ROADMAP.md` for Steam, consoles, graphics quality, no-lag skins/mods, and the Minecraft-style voxel mode.
