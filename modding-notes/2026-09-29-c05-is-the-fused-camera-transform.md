# 2026-09-29: the busiest upload is the full camera transform, per object

*Dev PC, `/lm`, driven by Claude (menus replayed by Menu-o-matiC, the camera turned with the mouse).*

## The question

The 2026-09-14 run found the camera's projection (the `c0+4` write) and left the busiest upload of all,
`c0+5` (start register 0, five registers, ~10,000 a second), unread.

## What it is

A probe logged bursts of it every two seconds, with the camera held still and then turned three times.
It is **the per-object world-view-projection matrix, fused**: rows are the object's axes, the last two
columns are z and w (nearly equal, as a left-handed projection gives), and the fifth register is
`(1, 0, 0, 0)` in every sample `[verified-live 2026-09-29, n=1 session, 44 bursts]`.

- Still camera: identical blocks, burst after burst.
- After each turn: the rotation part of the same objects' blocks changes (for one object, row 0 went from
  `(0.137, 0.438, 0.972)` to `(-0.003, 0.441, 0.978)` after the first turn).
- The first block of every burst is a fixed diagonal matrix (scale 26,667, depth 1 / -0.3): some
  screen-wide pass, not the camera.

## What it means

- The view is not uploaded on its own for world geometry: each object arrives with world, view and
  projection already multiplied. Per-eye stereo is still simple (a clip-space shift multiplies onto any
  fused matrix). Head tracking cannot be done by rewriting one view register; it has to go in where the
  engine builds the camera, which is what the reader's CameraManager work is for.
- The 2026-09-14 "c0 is a pure projection" is true of the `c0+4` write only. The `c0+4` block's depth
  offset also changed with the turns (373 → 330 → 282), so even that one is worth a second look.

## Also done

- The pause menu's QUIT page has **Restore Checkpoint**: the quick way back to the start spot, without a
  restart. A quit-to-desktop route now closes the game through its own menu
  (`ai-game-control-profiles/routes/prototype/quit_to_desktop.json`).
- The probe is archived (`dev-archive/tools/proxy-d3d9/archive/`), its log kept in
  `dev-archive/recon/2026-09-29-c05-is-the-fused-wvp/`, and the installed build is back to `2ad2663fc18a`
  (rebuilt byte-identical).
