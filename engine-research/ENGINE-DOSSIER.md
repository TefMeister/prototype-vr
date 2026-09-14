# Engine Dossier — Prototype (Titanium)

> One consolidated, living reference for this game's engine, filled in as the
> `PLAYBOOK.md` phases are worked. Chronological blow-by-blow belongs in the
> `dev-archive/` and `modding-notes/` folders; this file is the *distilled current
> truth*. Update it whenever a fact changes; correct false leads in place.

**Status:** M0, static recon done on both machines (2026-09-13 home, 2026-09-14 dev PC); the game has **not** been launched yet. · **VR-readiness verdict:** promising on shape. All the real code sits in an **unprotected** DLL, and the camera lives in one named class (`proto::CameraManager`) that Lua can reach. Nothing has been run.

## 1. Identity
- Game / build / version: Prototype, Steam build. `prototypef.exe` is a small launcher stub; the game code is in `prototypeenginef.dll`. Both are linked 2025-08-14, so this is a recent rebuild, not the 2009 binary `[inferred-static 2026-09-13]`.
- Platform & store; unofficial port? (extra fragility/legal notes): Steam (PC). Official release, not a fan port.
- Legitimacy: owned copy confirmed.

## 2. Engine lineage
- Family / base engine and how it was modified: Radical Entertainment's Titanium engine `[reported]`. Scaleform GFx and Lua strings are present in the engine DLL `[inferred-static 2026-09-13]`.
- Middleware (animation, audio, physics, megatexture, CUDA, etc.): **Scaleform GFx** for UI — and unusually it is **exported** from the engine DLL rather than hidden: all 77 exports are GFx symbols (`scaleformp3d::FlashFileHandler`, `GImage`, `GFxLoader`, `GSysFile`, `GBufferedFile`) `[inferred-static 2026-09-14]`. ⭐ That means the HUD is a separate, identifiable layer — exactly what has to be pulled out of the eye view later. **Lua** is present as `engine::LuaGOH` (a game-object handle). Bink for video, plus `WINMM`/`DSOUND` for audio.
- Distinctive file formats / build tags / symbol naming: `.rcf` archives (12 of them, ~7.9 GB) and `.p3d` files (Radical's Pure3D format `[reported]`). ⭐ **The two small ones are now OPENED** `[inferred-static 2026-09-14]`: both are `ATG CORE CEMENT LIBRARY` containers (matching `cementfiles.p3d` in the install root). **`shaders.rcf` holds readable HLSL source** (see §6); **`scripts.rcf` holds `init.lua` and a `frevents.lua` as compiled Lua bytecode** (the `ESC Lua` header), not source — readable only via a bytecode decompiler, though the engine carries the VM (`engine::LuaGOH`). Two are small enough to be worth opening first `[inferred-static 2026-09-14]`:
  - **`shaders.rcf` — 10.5 KB.** Far too small to hold a game's shaders, so almost certainly a *list* or index rather than the shaders themselves. Cheap to check.
  - **`scripts.rcf` — 182 KB.** With Lua confirmed in the engine, the most likely home of the game's scripts.
  - Build identity: both binaries linked **2025-08-14**, a recent rebuild rather than the 2009 shipping binary. ⚠️ **Any public research, offsets or trainers for Prototype almost certainly target the old build and will not apply.** Structure carries over; addresses do not.

## 3. Binary & memory
- 32/64-bit, size, module base, ASLR behaviour (stable base? relocations?): **32-bit** `[inferred-static 2026-09-14]`.
  - `prototypef.exe` 2.5 MB — base `0x400000`, ASLR off, relocations stripped. Its `.text` is **7 KB**; the rest is `.rsrc` (2.2 MB) and the Steam DRM `.bind` section. A stub, nothing more.
  - `prototypeenginef.dll` 20.2 MB — base `0x10000000`, ASLR off, **relocations present**. `.text` is **14.2 MB**. Plain sections only: **no wrapper, no packer**.
  - ⭐ **The friendliest protection shape in the 2026-09-13 batch:** the Steam wrapper sits on a 7 KB stub that only starts the game, while **all 14 MB of real code is in a completely unprotected DLL**, readable statically today with no unpacking step and no running process. Dead Space 2's code is behind a wrapper; Prototype's is not.
  - ⚠️ ASLR is off but relocations are present, so Windows *may* still move the DLL. Confirm once rather than assume.
- Renderer API (D3D11/12, DXGI, GL, Vulkan) with evidence: **Direct3D 9, confirmed from the import table** (not just strings): `d3d9.dll → Direct3DCreate9` in `prototypeenginef.dll` `[inferred-static 2026-09-14]`. ⚠️ Note it does **not** import `d3dx9_*`, so unlike Hard Reset and Dead Space 2 there is no `D3DXGetShaderConstantTable` route to named shader constants here. A DirectX redistributable ships in `DirectX\\`.
- Developer console / cvar system present? how opened?: **A console subsystem exists as a class** — `engine::ConsoleManager`, `engine::ConsoleManagerPeer`, `ConsoleManager::Callback` with a callback list, and a `ConsoleAspectRatio` setting `[inferred-static 2026-09-14]`. ⚠️ **Whether a player can open it is a completely separate question and nothing found answers it** — no key binding, no cvar registry, no help text of the kind Hard Reset carries. Also present: `Proto_Profile_BodySurfCheatEnabled`, a cheat flag stored in the player profile.

## 4. DRM / anti-debug & injection foothold
- DRM (CEG/Denuvo/GOG/none); launch-time-debugger behaviour: The launcher stub carries the Steam DRM wrapper section (`.bind`); the engine DLL itself shows no protection `[inferred-static 2026-09-13]`. Not tested live.
- Injection vector: **`d3d9.dll` beside `prototypef.exe`**, available in principle — `prototypeenginef.dll` imports `d3d9.dll` -> `Direct3DCreate9` statically, and DLL search order is a property of the **process**, so the exe being a stub changes nothing `[inferred-static 2026-09-14]`. A proxy is built and deployed; **not yet run**.
- Attach workflow that works: not yet tested.
- Injection vector that works (proxy DLL name / injector / framework): not yet tested.

## 5. Threading & frame structure
- Immediate context only, or deferred contexts + command lists?:
- Which thread(s) do what; render-thread name(s):
- One-frame walkthrough (record → replay → present):

## 6. Camera & projection delivery (the crucial section)

# 🏆 ANSWERED 2026-09-14 `[verified-live 2026-09-14, n=1 session, ~500k uploads]`

Evidence: `dev-archive/recon/2026-09-14-camera-found/`; notes: `modding-notes/2026-09-14b-the-camera-is-found.md`.

```
c0, layout R (register i = row i), written as a dedicated "c0+4"
    [  1.191754    0.000000    0.000000    0.000000 ]
    [  0.000000    2.118673    0.000000    0.000000 ]
    [  0.000000    0.000000    1.000040    1.000000 ]
    [  0.000000    0.000000   -0.300012    0.000000 ]
```

| Property | Value |
| --- | --- |
| Register / width | **c0**, 4 registers (`c0+4`, 12,486 per 5 s) |
| Packing | **layout R** (w-from-z at index 11) |
| Handedness | **LEFT-handed**, `clip.w = +view.z` |
| Aspect | **exactly 1.7778 = 16/9**, matching the 1920x1080 mode |
| X axis | **not mirrored** (`xs` positive) - contrast Dead Space 2 |
| Near / far | **0.300000 / 7500** |
| FOV | **80.00 deg horizontal**, 50.53 vertical |

⭐ **80.00 deg, near 0.3, far 7500 - three round numbers.** Independent corroboration that the matrix
was read correctly, not merely found.

⭐ **The depth term fits the textbook LH form exactly** (`m[10] = 1.000040` gives a clean `zf = 7500`),
unlike Dead Space 2's non-standard one. Per-eye derivations can use the standard formulae here.

**Eight fields of view, all at exactly 16:9** - 80.00 (normal), 60.80, 44.24, 42.73, 35.67, 32.70,
20.51, 19.76 deg. One camera with a zoom ladder, not eight cameras.

## ⚠️ CORRECTION to the static reading below: it is a PURE PROJECTION, not a fused WVP

The shipped shader source (below) declares `p3dWorldViewProjectionMatrix : register( c0 )`, and this
dossier concluded from it that the engine uses a **fused** world-view-projection - then reasoned that
head tracking must therefore go CPU-side.

**The runtime measurement refutes that for the gameplay path.** The matrix actually arriving at `c0`
has a **perfectly diagonal upper-left 2x2 with zero rotation terms**; a fused WVP would carry the
camera's rotation there. `[verified-live 2026-09-14]`

⭐ **Both observations are true and the error was generalising.** This dossier already recorded that
registers are assigned **per shader** and that 10.5 KB is a *sample* of utility shaders - and then
reasoned as though the sample's convention were the engine's. The sampled shader does use a fused WVP;
**the shaders that draw the game do not.**

- The "second independent reason" for promoting the `proto::CameraManager` row is **withdrawn**. That
  row stands on its original merit only.
- ⭐ **Better news, not worse:** a separate projection gives the same clean split as Dead Space 2 -
  **stereo shears the projection at c0, head tracking goes to whatever carries the view.**
- **Where the view lives is the open question.** `c0+5` is the most common upload of all (49,260 per
  5 s against the camera's 12,486) and its contents are unread. That is the next thing to look at.

⚠️ **The detector is NOISY on this engine** - dozens of junk matches where Dead Space 2 produced three.
What separated the camera was the aspect test, so the instrument now **computes the display aspect
itself and marks matching signatures** rather than asking a human to divide.

⚠️ The device is **not** a pure device (`BehaviorFlags=0x44`), so unlike Dead Space 2 a constant
read-back instrument *would* work here if one is ever needed.

⚠️ **n=1, one area (the tutorial). Nothing has been written.**

---

### The static reading that predicted it

⭐ **The game ships readable HLSL SOURCE** in `shaders.rcf`, which declares its constants by name and
register `[inferred-static 2026-09-14]` — the same class of find as Enslaved's shipped `.usf` files.
Notes: `modding-notes/2026-09-14-shader-source-ships-and-the-proxy-is-ported.md`.

```hlsl
const float4x4 p3dWorldViewProjectionMatrix : register( c0 );
...
p3dPositionWorldViewProjection = mul( world_view_proj_matrix, position );
```

- How the world transform reaches the GPU: as a **FUSED world-view-projection matrix**, one 4x4
  carrying all three — **unlike Dead Space 2**, whose projection arrives alone with the view elsewhere.
  ⭐ **Consequence:** per-eye stereo is still easy (the shift a second eye needs is a clip-space shear,
  `clip.x += dx * clip.w`, which pre-multiplies onto a fused matrix just as well). **Head tracking is
  harder** — there is no view register to rotate, so the rotation must be injected CPU-side in
  `proto::CameraManager` before the matrix is built. That makes the existing `CameraManager` `[PD]`
  row the most important on this project.
- Exact constant slot, name, layout: `p3dWorldViewProjectionMatrix` at `c0`, four registers, in the
  sampled shaders. ⚠️ **`c0` is NOT a universal convention here** — `p3dLight_ObjectSpaceLightBounds`
  sits at `c0` in one shader and `c4` in another, so registers are assigned **per shader**. The durable
  finding is the **naming convention and the fused shape**, not the number. ⚠️ And 10.5 KB is a handful
  of utility shaders (a Bink YUV player, a light-bounds helper, a basic transform), **not the game's
  set** — the bulk are elsewhere. This is a sample.
- Other declared constants: `p3dNaN` (`c1`/`c5`, used to cull a vertex), `p3dBuiltIn_Zero` (`c2`),
  `p3dBuiltIn_One` (`c3`), `p3dLight_ObjectSpaceLightBounds` (`c0`/`c4`), samplers `y_tex`/`cb_tex`/
  `cr_tex` (`s0`-`s2`).
- Handedness, row/column convention: **not yet established.** The source shows `mul(matrix, position)`,
  but the register packing the compiler chose is not readable from the source alone. The deployed
  instrument tests both packings and will answer it.
- **Instrument deployed** (`dev-archive/tools/proxy-d3d9/`), read-only, register-agnostic, ported from
  `dead-space-2-vr` complete with that project's all-17-exports and wrap-don't-patch lessons.
  `[compile-verified 2026-09-14]`; detector 15/15 and wrapper 16/16 `[verified-numerically 2026-09-14]`.
  **Not yet run against the game.**
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
- Nothing blocking seen yet. Because the real code sits in an unprotected DLL, static reading should be straightforward — and 2026-09-14 confirmed that from the PE headers rather than assuming it.
- ⭐ **`proto::CameraManager` is the best camera shape in the batch.** The binary is full of bound closures over it — `Closure1<CameraManager, void, float>`, `Closure2<CameraManager, bool, int, math::Vector>`, `Closure5<CameraManager, void, float, float, float, math::Vector, math::Vector>`, `Closure6<CameraManager, bool, int, LuaGOH, const char*, math::Vector, int, bool>` `[inferred-static 2026-09-14]`. A single named class owning the camera and taking `math::Vector` arguments means camera state most likely lives in **one object** rather than smeared across the renderer. It is the opposite of Dead Space 2's bone-bound camera picture.
- ⭐ **`engine::LuaGOH` appears inside `CameraManager` closures** — a game-object handle passed to a camera call is script-facing by construction, so **Lua can probably reach the camera** `[hypothesis]`.
- ⚠️ **Every claim above comes from names, not from disassembly.** Names prove a thing exists and prove nothing about what it does. No code was read; nothing was run.
