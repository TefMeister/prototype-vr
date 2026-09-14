# 2026-09-14 — Prototype ships readable shader SOURCE, and the proven proxy is ported

**The game was not launched. Nothing here has been run against it.** Everything is file inspection,
compile-verification, or a self-test in a process of our own.

Worked while a `/pd` session held `dead-space-2-vr`; that game was checked as `FRESH` and left alone.

## ⭐ The find: `shaders.rcf` is HLSL source, and it names the camera matrix

The board asked whether `shaders.rcf` (10.5 KB — far too small to hold a game's shaders) was an index.
It is not an index. **It is readable HLSL source text** `[inferred-static 2026-09-14]`.

Both `shaders.rcf` and `scripts.rcf` are containers whose header reads `ATG CORE CEMENT LIBRARY` —
Radical's own archive format, matching the `cementfiles.p3d` in the install root.

The shader source declares its constants **by name and by register**:

```hlsl
const float4x4 p3dWorldViewProjectionMatrix : register( c0 );
...
p3dPositionWorldViewProjection = mul( world_view_proj_matrix, position );
```

⭐ **This is the most valuable thing in the static toolbox** — the same class of find as Enslaved's
shipped `.usf` files, and it means the engine's register convention is readable off disk rather than
inferred from a capture.

Every declared constant in the sample:

| Constant | Register | What it is |
| --- | --- | --- |
| `p3dWorldViewProjectionMatrix` | `c0` (4 registers) | ⭐ the camera transform |
| `p3dLight_ObjectSpaceLightBounds` | `c0` in one shader, `c4` in another | object-space light bounds |
| `p3dNaN` | `c1` / `c5` | a NaN constant, used to cull a vertex |
| `p3dBuiltIn_Zero`, `p3dBuiltIn_One` | `c2`, `c3` | literal constants supplied by the engine |
| `y_tex`, `cb_tex`, `cr_tex` | `s0`, `s1`, `s2` | a YUV video shader — Bink playback |

## ⚠️ What this does and does not establish

- ⚠️ **`c0` is not a universal convention.** `p3dLight_ObjectSpaceLightBounds` is at `c0` in one
  shader and `c4` in another, so registers are assigned **per shader**. The durable finding is the
  **naming convention and the fused shape**, not the number.
- ⚠️ **10.5 KB is a handful of shaders, not the game's set.** These look like utility shaders (a video
  player, a light-bounds helper, a basic transform). The bulk are elsewhere — in the large `.rcf`
  archives, or generated at runtime. **This is a sample.**
- ⚠️ Nothing here is a measurement of what a running frame does.

## ⭐ The structural finding: a FUSED world-view-projection

`p3dWorldViewProjectionMatrix` is a single 4×4 carrying world, view and projection together —
**unlike Dead Space 2**, whose projection arrives alone at `c4` with the view elsewhere.

**What that costs, and what it does not:**

- ⭐ **Per-eye stereo is still straightforward.** The horizontal shift a second eye needs is a shear
  in *clip* space: `clip.x += dx · clip.w`. That is a matrix pre-multiplied onto whatever produced
  the clip position, so it works on a fused matrix exactly as well as on a separate projection.
- ⚠️ **Head tracking is harder.** With view and projection fused there is no "view matrix" register to
  rotate. The rotation has to be injected either by decomposing the fused matrix per draw — expensive
  and fragile — or, far better, **at the CPU side, in `proto::CameraManager`**, before the matrix is
  ever built.

⭐ **That makes the existing `[PD]` row on `proto::CameraManager` the most important one on this
project**, and it was already queued from the morning's static pass on the grounds that a single named
class owning the camera is the friendliest possible shape. This is the second, independent reason.

## `scripts.rcf`

Same `ATG CORE CEMENT LIBRARY` container. Holds **`init.lua`** and **`scripts\frevents.lua`**, both as
**compiled Lua bytecode** (`\x1bLua` header), not source `[inferred-static 2026-09-14]`. So the
scripts are not directly readable, though Lua bytecode is a well-understood format and the engine
carries the VM (`engine::LuaGOH` was found in the morning pass).

## The proxy is ported, built and deployed

The `d3d9.dll` proxy and camera instrument from `dead-space-2-vr` are copied here, adapted, and
**arrive already carrying that project's two expensive lessons** so Prototype does not pay for either:

1. **Export all seventeen d3d9 functions**, not just `Direct3DCreate9`. A one-export proxy stopped
   Dead Space 2 launching; the game called `D3DPERF_GetStatus` six seconds in, the lookup returned
   NULL, and calling NULL gave a DEP kill with no owning module. It took an A/B against a genuine
   Microsoft DLL to prove it was ours and not the DRM.
2. **Wrap `IDirect3D9`, do not patch its vtable.** Patching slot 16 stood down on every launch because
   something (almost certainly the Steam overlay) already held it — and standing down happens before
   a device exists, so the instrument could never install.

⚠️ **Neither lesson has been re-verified on Prototype.** They are carried because the mechanisms are
generic — any Steam D3D9 game can carry an overlay hook, any game can call any d3d9 export — not
because this game has been observed needing them.

**The injection point is available:** `prototypeenginef.dll` imports `d3d9.dll → Direct3DCreate9`
statically. The exe being a stub changes nothing — DLL search order is a property of the **process**,
so a `d3d9.dll` beside `prototypef.exe` is still found first `[inferred-static 2026-09-14]`.

### Verified

| Claim | Evidence |
| --- | --- |
| Builds clean | `-Wall -Wextra`, zero warnings `[compile-verified 2026-09-14]` |
| Detector correct | **15/15** against matrices built from the documented D3D formulae `[verified-numerically 2026-09-14]` |
| Thunks forward, stack balanced | self-test passes |
| Wrapper forwards correctly | **16/16** against the real `IDirect3D9`, and a real device is created through it `[verified-numerically 2026-09-14]` |
| Build reproducible | byte-identical over two builds, `sha256 45e40c6b4022…` |
| Deployed | `Prototype\d3d9.dll`, 215,040 bytes, recorded with `deployed.sh`, **nothing overwritten** — the folder had no `d3d9.dll` |

## ⚠️ The honest risk on the first launch, and how to resolve it in one step

**Prototype has never been run on this machine.** So if it fails to start with the proxy in place,
that alone cannot distinguish *our proxy* from *the game not running here at all* — which is exactly
the confound that cost `dead-space-2-vr` two rounds earlier today.

**The resolution is built into the instruction rather than a second unknown:** if it does not start,
delete `Prototype\d3d9.dll` and launch again. Runs → our proxy. Still fails → the game, and the proxy
was never the issue.

## What is NOT established

- Nothing has been run against the game.
- Whether Prototype's transform travels through `SetVertexShaderConstantF` at all. The shipped source
  says it is a shader constant, which is strong, but the sample is small and says nothing about the
  path the engine actually uses at runtime.
- Whether the fused matrix really lands at `c0` in the shaders that matter — the instrument is
  register-agnostic precisely so it can answer that independently rather than trusting the sample.
- Whether the Steam overlay is present in this game at all (it is assumed, not observed).
