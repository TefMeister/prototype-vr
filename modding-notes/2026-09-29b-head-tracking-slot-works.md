# 2026-09-29: head tracking works through the View's own pose slot

*Dev PC, `/lm`, driven by Claude, still camera. No rebuild: a memory tool wrote into the running game.*

## What was done

The static reader traced how the camera reaches the renderer (dossier §6) and pointed at a 4×4 in each
`pure3d::View`, at +0x2c, that is identity by default and multiplied into the view every frame. A small tool
(`dev-archive/tools/proto_cam.py`) finds it without a debugger: scan for ViewPass objects, follow to their View and
Camera, keep the one with near 0.3 / far 7500.

## What was seen

- **The chain is real:** three ViewPasses, all rendering the same camera (80°, near 0.3, far 7500), each View's slot
  at identity `[verified-live 2026-09-29, n=1]`.
- **One of the three is the on-screen scene.** Writing 1 m into its slot moved the whole picture as a camera step:
  Alex (near) moved most, the wall less, the far gate hardly at all; the HUD stayed put `[verified-live 2026-09-29, n=1]`.
  The other two views showed no change.
- **The slot works in WORLD space, after camera-to-world.** A raw 10° yaw swung the camera around the world origin
  onto another street. Wrapped as `slot = inv(C) · H · C` (C = the camera's camera-to-world at +0x90, H = the head
  pose in camera space), the same 10° turned the view on the spot, and 0.5 m to the side moved it sideways with
  correct depth `[verified-live 2026-09-29, n=1 each]`. Reset put it back exactly.

## What it means

Head tracking has a home that needs no shader work: every frame, write `inv(C) · H · C` into the scene View's slot.
C changes as Alex moves, so it has to be recomputed each frame from inside the game (a hook near `SetupCamera`, or
the reader's detour point `0x107545a0`), not from an outside tool. That is the next build.

Not yet checked: shadows and reflections under a moved camera, and which View is the scene one without trial
(this run found it by trying all three).
