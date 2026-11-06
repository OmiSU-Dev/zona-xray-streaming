# Monolith Reforged — megamap / level-flow touchpoints

ZONA’s **megamap** goal is **one physical `.level` load** (no `ChangeLevel` between stitched overworld regions). Most work stays in **Level Editor + gamedata** (`all.spawn`, `game_graph`, remove `level_changer` instances at internal seams). This note maps **where the engine handles transitions today** so future C++ work starts from the right files.

## 1. `level_changer` world object (player walks into volume)

**File:** `src/xrGame/level_changer.cpp`  
Triggers UI **`CUIGameSP::ChangeLevel`** or, in silent mode, sends **`M_CHANGE_LEVEL`** on the wire.

## 2. UI confirmation → network packet

**File:** `src/xrGame/UIGameSP.cpp` — `CChangeLevelWnd::OnOk()`  
Builds **`M_CHANGE_LEVEL`** with destination game vertex, level vertex, position, angles, then `Level().Send(...)`.

## 3. Client receives transition → reconnect / reload path

**File:** `src/xrGame/Level_network_messages.cpp`  
`case M_CHANGE_LEVEL:` (alongside `M_LOAD_GAME`, `M_RELOAD_GAME`) clears Lua GC state and calls **`MakeReconnect()`** — full level transition pipeline, not an in-place teleport.

## 4. Server relays

**File:** `src/xrGame/xrServer.cpp` — `case M_CHANGE_LEVEL:`  
Forwards to `game->change_level(...)` and may **`SendBroadcast`** so peers follow.

## 5. Related

| Topic | Where to look |
|-------|----------------|
| Map / game sync messages | `src/xrGame/Level_network_messages.cpp`, `Level_network_map_sync.cpp` |
| MP level commands | `src/xrGame/console_commands_mp.cpp` (`sv_changelevel`, etc.) |
| ALife-driven transitions | `src/xrGame/alife_update_manager.cpp` (emits `M_CHANGE_LEVEL` in some paths) |

## Implications for Phase 1 megamap

1. **Content:** Remove or disable **`level_changer`** spawn entries that connect **two regions now inside one `.level`**. Replace with open geometry or scripted teleports that **move the actor** without `M_CHANGE_LEVEL`.
2. **Lua:** `_G.ChangeLevel` and ZONA’s `zz_zona_wilderness_spawn` wrapper still matter for **multi-hop** flows until spawn lives entirely inside one load.
3. **Engine C++ changes** are only needed if you add **new** behavior (e.g. soft streaming, in-level teleport API, or MP semantics). The stock path **always** goes through **`MakeReconnect()`** for `M_CHANGE_LEVEL` on the client.

Baseline for this tree: tag **`2026.9.12`**, branch **`zona/baseline-2026.9.12`**. See `ZONA.md` and `BUILD.md`.
