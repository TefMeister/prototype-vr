# 2026-10-12 (dev PC, `/pd`, no game launch): the headset feeds the head pose

**The game was not launched.** Everything below ran in small 32-bit test programs, not in Prototype.

## Built

- `headpose_xr.c`: while `prototype_headset.txt` sits next to `prototypef.exe`, a headset thread of ours loads the
  32-bit OpenXR loader for the runtime named in `prototype_vr.ini [vr] RuntimeJson` (this process only), makes its
  own D3D11 device (the game is D3D9, so the session can never be the game's), runs empty frames, and publishes the
  head (VIEW in LOCAL) each frame. It first checks the process's `msvcp140.dll` is 14.40 or newer (the Heavy Rain
  lesson of the same day) and stops with a log line rather than lose the game.
- `headpose_xr_math.c`: OpenXR is right-handed with z back, Prototype's camera left-handed with z forward, so the head
  is mirrored through z. The first pose (and numpad 5) is the recentre: heading only. `Position=1` moves the camera
  with the head, 1 m = 1 world unit (`[hypothesis]`; the 0.5 m key step looked like half a metre on 2026-09-29).
- `HeadPose` can now carry a full rotation (`has_rot`, `rot[9]`), so tilt works; the keys' yaw/pitch path is unchanged.
- The BeG0nE jitter recorder (`jitterlog.h`) is wired in: pose at each headset read, camera at each slot write, a
  present at each headset frame; it writes only while the session runs. A 5 s test-program run wrote one file (kept in
  the recon folder, moved out of BeG0nE's folder so it is not counted as a game session).
- `headpose.c`: the headset pose goes exactly where the numpad pose goes (View+0x2c, every scene frame), and to the
  HUD markers. Removing the switch file hands the pose back to the keys.

## Verified

- Self-test `headpose_xr_selftest.c`: 20,010 checks `[verified-numerically 2026-10-12]`. The strongest: a headset
  turned left by t gives exactly the keys' yaw -t, looking up the keys' pitch +t, and both together, against the key
  route that was seen working in the game (2026-09-30). Plus 20,000 random heads against an independent quaternion
  route, tilt, position, recentre. With the z mirror planted wrong, it fails. The old tests still pass (6,002 maths
  checks, the hook test, the markers test): `test/run_tests.sh`.
- The OpenXR steps in a 32-bit program with the simulator's 32-bit runtime: 234 frames and head poses in 4 s
  `[measured 2026-10-12, n=1]`.
- **The shipped `headpose_xr.c` itself**, in a 32-bit program (`test/headpose_xr_live_test.c`) against the simulator,
  25 s: 2,270 poses; turning the simulator's head 30 deg (yaw) gave a forward axis 30 deg to the LEFT in camera axes;
  pitch 20 + roll 15 gave forward 20 deg up and the head's up leaning left `[measured 2026-10-12, n=1]`.
  Evidence: `dev-archive/recon/2026-10-12-headset-feed-outside-the-game/`.

## Installed (not run in the game)

`d3d9.dll` `3ecec32d9332` (rebuilds to the same hash), the 32-bit `openxr_loader.dll` (Khronos, from the Alan Wake
staging copy) and `prototype_vr.ini`. No switch file, so the game behaves as before. The old dll is kept on E:.

## Not established

- That the game takes it as the keys did (the same slot, so it should).
- Units, comfort, and how the film-style cut-scene cameras behave with a head pose on top.
- The simulator's "yaw +30" being a turn to the left is OpenXR's convention, which the maths assumes; a real headset
  (home PC) is the final word.

## Next live test (`/lm`, still camera)

Launch to the alley (Menu-o-matiC route), put `prototype_headset.txt` next to the exe, turn the simulator's head
30 deg (`set_head_pose yaw 30`):
- the view turns LEFT 30 deg and holds; the log shows steps 1-6, `session running`, `RECENTRED`, head-forward lines;
- the view turns right: the mirror is wrong for this game after all (the test says otherwise; check the handedness);
- nothing moves but the log has poses: the pose is not reaching the View (check `HEADPOSE:` lines, numpad 0 choice);
- the log stops at a step: it names the step (a `STOPPED` line means an old C++ runtime copy).
