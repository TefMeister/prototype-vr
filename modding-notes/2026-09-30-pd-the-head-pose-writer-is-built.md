# 2026-09-30 (`/pd`, dev PC): the per-frame head-pose writer is built

**The game was not launched, and nothing here has been run inside it.** The code is built, tested outside the
game, and installed on the dev PC.

## What it does

On 2026-09-29 a memory tool proved that writing `inv(C)·H·C` into the scene View's `+0x2c` slot moves the camera
like a head would (C = the camera's camera-to-world at `cam+0x90`). C changes every frame as Alex moves, so the
write has to happen inside the game, every frame. That is what this build adds (`dev-archive/tools/proxy-d3d9/src/headpose.c`).

- **Where:** a 5-byte jump on `0x107545a0`, the two-argument function `pure3d::ViewPass::Render` calls with
  (View, Camera) just before `View::SetupCamera` reads the slot. Checked on disk: it is `mov eax,[esp+8]; push esi;
  mov esi,[esp+8]; ... call SetCamera; ... call SetupCamera; ret 8`, and it has three callers; only the one
  returning to `0x10768738` (Render) is acted on `[inferred-static 2026-09-30]`.
- **When:** only for the scene camera (near 0.3, far 7500), and **only after the first head key** - until then the
  game is left exactly as it was. The first five bytes are checked before patching; a different build is refused
  and logged.
- **Keys (number pad, NumLock on):** 4/6 turn 5°, 8/2 look up/down 5°, 1/3 move sideways 0.1 m, 7/9 move
  down/up 0.1 m, 5 reset, 0 chooses which View gets the pose (all, then each one alone). Every change is logged
  in `proto_proxy.log` with the pose and how many Views have been seen.
- **Which View is on screen:** still not known statically. The live test found three, all with the same camera,
  and only one changed the picture; the other two showed nothing. So the default writes the pose to **all
  of them**, which cannot hurt the picture, and each View is numbered in the log when first seen; numpad 0 then
  tells them apart in one sitting `[hypothesis]` that writing to all is harmless: it is based on that one poke.

## How it was checked

- **The maths** (`headpose_math.c`, `test/headpose_selftest.c`): 2,000 random cameras up to 4 km from the origin
  and random head poses; the camera the engine builds (`C·slot`) equals the head-moved camera (`H·C`), and its
  position equals C's plus the offset along C's own axes, to 0.4 mm; a zero pose gives the identity slot; a flat
  camera is refused. **6,002 checks, 0 failures**; the "no conjugation" mutant (the raw pose that orbited the
  world origin live) fails 3,998 `[verified-numerically 2026-09-30, n=6002]`.
- **The jump itself** (`test/headpose_hook_selftest.c`, 32-bit like the game): a stand-in function with the same
  first five bytes and the same shape, called the way Render calls it. The jump installs, the original still runs
  every time, the slot gets `inv(C)·H·C` only from the Render call with the scene camera, and wrong first bytes
  are refused. **10 of 10 checks pass** `[verified-numerically 2026-09-30, n=10]`.
- **The whole DLL** outside the game (`test/wrap_selftest.c`): passes, and the head code says
  "prototypeenginef.dll is not loaded - not installed" and stands down, as it should.
- Installed: `d3d9.dll` sha256 `f5ed03d26c2f...` on the dev PC; the previous build (`2ad2663fc18a`) kept as
  `d3d9.dll.backup-2026-09-30-before-headpose`.

## Not established

- That nothing else writes `View+0x2c` between our write and SetupCamera (the live poke held, so probably not).
- What the two other Views draw, and whether writing the pose into them is harmless in every scene.
- The pitch sign (numpad 8 is meant to look up; if it looks down, swap in `headpose_math.c`).

## The one test (flat, still camera)

Load the save, stand still, press numpad 6 twice: the view should turn 10° on the spot while Alex and the HUD
behave as in the 2026-09-29 poke. Walk forward a few steps: the 10° should stay with the camera. Then numpad 0
through the Views to learn which is on screen, and numpad 5 to reset. Log lines start `HEADPOSE:`.
