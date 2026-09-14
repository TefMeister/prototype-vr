# 2026-09-14 — Prototype, dev-PC static pass (NO LAUNCH)

**Machine:** dev PC `DESKTOP-V8GTSIR`. **Install:** `D:\Program Files (x86)\Steam\steamapps\common\Prototype`,
Steam app 10150, `StateFlags=4` (fully installed) `[inferred-static 2026-09-14]` — the home PC's
2026-09-13 note flagged "installed on the home PC only, as far as this session knows"; **it is
installed here too**.

**The game was not launched.** Everything here is PE headers and strings read off disk.

## Files here

| File | What it is |
| --- | --- |
| `pe-imports-exports.txt` | PE headers, sections, imports and exports for both `prototypef.exe` and `prototypeenginef.dll` |
| `engine-vocabulary.txt` | the engine's own class names for camera, console, Lua and Scaleform |
| `install-listing.txt` | install root contents |

## The split confirmed: a stub launcher and an unprotected engine

`[inferred-static 2026-09-14]`

| | `prototypef.exe` | `prototypeenginef.dll` |
| --- | --- | --- |
| Size | 2.5 MB | 20.2 MB |
| `.text` | **7 KB** | **14.2 MB** |
| Sections | `.rsrc` (2.2 MB) + **`.bind`** (Steam DRM wrapper) | plain `.text/.rdata/.data/.idata/.rsrc/.reloc` — **no wrapper, no packer** |
| Base | `0x400000`, ASLR off, relocs stripped | `0x10000000`, ASLR off, **relocations present** |
| Renderer | — | `d3d9.dll → Direct3DCreate9`, from the import table |
| Exports | none | **77**, all Scaleform GFx (`scaleformp3d`, `GImage`, `GFxLoader`, `GSysFile`) |

⭐ **This is the friendliest protection shape in the batch.** The Steam wrapper is on a 7 KB stub that
does nothing but start the game; **all 14 MB of real code sits in a completely unprotected DLL** that
can be read statically, today, with no unpacking step and no running process. Dead Space 2's code is
behind a wrapper; Prototype's is not.

⚠️ **ASLR is off but relocations are present in the DLL**, which means Windows *may* still move it.
Fixed-base assumptions are safer here than on Portal, but should be confirmed once rather than
assumed.

## The engine's own vocabulary

Read straight out of the mangled C++ names `[inferred-static 2026-09-14]`:

- **`engine::ConsoleManager`** and `engine::ConsoleManagerPeer`, with a callback list
  (`ConsoleManager::Callback`, `SListLite<ConsoleManager::Callback>`). A console subsystem exists as
  a class. ⚠️ **Whether a player can open it is a completely separate question** and nothing found
  answers it — no key binding, no cvar registry, no help text of the kind Hard Reset carries.
- **`proto::CameraManager`** — and the interesting part is its *shape*. The binary is full of bound
  closures over it (`Closure1<CameraManager, void, float>`,
  `Closure2<CameraManager, bool, int, math::Vector>`,
  `Closure5<CameraManager, void, float, float, float, math::Vector, math::Vector>`,
  `Closure6<CameraManager, bool, int, LuaGOH, const char*, math::Vector, int, bool>` …).
  ⭐ **A single named class owning the camera, taking `math::Vector` arguments, is the best possible
  shape for this work** — it means camera state is likely to live in one object rather than being
  smeared across the renderer. It is the opposite of Dead Space 2's bone-bound camera picture.
- **`engine::LuaGOH`** appearing *inside* `CameraManager` closures — **Lua can reach the camera**.
  A game-object handle passed to a camera call is script-facing by construction.
- **Scaleform GFx** for the UI, exported rather than hidden — so the HUD is a separate, identifiable
  layer, which is exactly what has to be pulled out of the eye view later.
- `Proto_Profile_BodySurfCheatEnabled` — a cheat flag stored in the player profile.

## Data layout

The install is 12 `.rcf` archives (~7.9 GB) plus `.p3d` files — Radical's Pure3D format `[reported]`.
Two are small enough to be worth opening before anything else:

- **`shaders.rcf` — 10.5 KB.** Ten kilobytes is far too small to hold a game's shaders, so it is
  almost certainly a *list* or an index rather than the shaders themselves. Cheap to check.
- **`scripts.rcf` — 182 KB.** With Lua confirmed present in the engine, this is the most likely place
  the game's scripts live.

⚠️ Both were **listed, not opened.** `.rcf` is a Radical container format; whether we can read it is
unknown, and `/gr` may find that someone has already documented it.

## Build identity

Both binaries are linked **2025-08-14** `[inferred-static 2026-09-14]` — this is a recent rebuild, not
the 2009 shipping binary. ⚠️ **That cuts both ways and should be said out loud:** any public research,
offsets or trainers written for Prototype almost certainly target the old build and will not apply.
Structure will carry over; addresses will not.

## What this does NOT establish

- Nothing has been run. The console's reachability, the camera class's actual behaviour, and whether
  Lua is exposed to the player are all open.
- No disassembly was done — every claim above comes from *names*, which prove a thing exists and
  prove nothing about what it does.
- The renderer's camera delivery (dossier §6) is untouched.
