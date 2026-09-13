# Engine Dossier — Prototype (Titanium)

> One consolidated, living reference for this game's engine, filled in as the
> `PLAYBOOK.md` phases are worked. Chronological blow-by-blow belongs in the
> `dev-archive/` and `modding-notes/` folders; this file is the *distilled current
> truth*. Update it whenever a fact changes; correct false leads in place.

**Status:** M0, first static look (2026-09-13); the game has not been launched yet. · **VR-readiness verdict:** TBD. Nothing seen so far rules it out.

## 1. Identity
- Game / build / version: Prototype, Steam build. `prototypef.exe` is a small launcher stub; the game code is in `prototypeenginef.dll`. Both are linked 2025-08-14, so this is a recent rebuild, not the 2009 binary `[inferred-static 2026-09-13]`.
- Platform & store; unofficial port? (extra fragility/legal notes): Steam (PC). Official release, not a fan port.
- Legitimacy: owned copy confirmed.

## 2. Engine lineage
- Family / base engine and how it was modified: Radical Entertainment's Titanium engine `[reported]`. Scaleform GFx and Lua strings are present in the engine DLL `[inferred-static 2026-09-13]`.
- Middleware (animation, audio, physics, megatexture, CUDA, etc.):
- Distinctive file formats / build tags / symbol naming: `.rcf` archives and `.p3d` files (Radical's Pure3D format `[reported]`), not yet looked at.

## 3. Binary & memory
- 32/64-bit, size, module base, ASLR behaviour (stable base? relocations?): **32-bit**. `prototypef.exe` 2.5 MB: almost all resources plus a `.bind` section. `prototypeenginef.dll` 20.2 MB: plain sections, relocations present `[inferred-static 2026-09-13]`.
- Renderer API (D3D11/12, DXGI, GL, Vulkan) with evidence: Direct3D 9: `d3d9.dll` in the engine DLL's strings; a DirectX redistributable ships in `DirectX\` `[inferred-static 2026-09-13]`.
- Developer console / cvar system present? how opened?: not yet investigated.

## 4. DRM / anti-debug & injection foothold
- DRM (CEG/Denuvo/GOG/none); launch-time-debugger behaviour: The launcher stub carries the Steam DRM wrapper section (`.bind`); the engine DLL itself shows no protection `[inferred-static 2026-09-13]`. Not tested live.
- Attach workflow that works: not yet tested.
- Injection vector that works (proxy DLL name / injector / framework): not yet tested.

## 5. Threading & frame structure
- Immediate context only, or deferred contexts + command lists?:
- Which thread(s) do what; render-thread name(s):
- One-frame walkthrough (record → replay → present):

## 6. Camera & projection delivery (the crucial section)
- How the world transform reaches the GPU (shared VP buffer / per-draw MVP /
  other), with **shader-reflection / disassembly evidence**:
- Exact constant-buffer slot, parameter name(s), byte offset(s), layout,
  handedness, row/column convention:
- Where projection `P` / FOV comes from:
- The per-eye override maths (`K_eye = …`):

## 7. Constant-buffer fill mechanism
- Map/DISCARD ring / UpdateSubresource / D3D11.1 offset / **persistent map +
  memcpy** (trap):
- Can source contents be read cheaply (captured CPU pointer) or need staging
  read-back?:
- The chosen override patch point and why:

## 8. Pass inventory (by render target)
- Main scene (res/formats):
- Shadow passes (depth-only sizes):
- Post / AA chain (SMAA/TAA/motion vectors; downscale sizes):
- UI / HUD (how it's kept separate):

## 9. cvar / console cheat sheet
| command / cvar | effect | use |
|---|---|---|
| | | |

## 10. Autonomous harness recipe (this game)
- Launch to a known scene (commands used):
- In-process input / camera drive method that worked:
- Frame-capture method; where images land:

## 11. Dead ends & false leads (save future time)
- none yet.

## 12. Open risks toward the North Star
- Nothing blocking seen yet. Because the real code sits in an unprotected DLL, static reading should be straightforward.
