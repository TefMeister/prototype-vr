# 2026-09-30 (`/lm`, dev PC, driven by Claude): the head pose works every frame

*Still camera, mouse turns and one short walk. The game was launched, driven and closed by Claude.*

## What was tried

The per-frame head-pose writer built earlier today by `/pd` (`dev-archive/tools/proxy-d3d9/src/headpose.c`) went into
the game. It hooks the call where each ViewPass hands its camera to its View, and writes `inv(C)·H·C` into the View's
`+0x2c` slot, with H set from the number pad.

## What was seen

- **It installs and finds the scene.** The log shows the hook going in on `0x107545a0` and three scene Views with
  the same 80° camera, as the memory scan found yesterday `[verified-live 2026-09-30, n=1]`.
- **Turning works and stays on top of the game camera.** Two presses of numpad 6 (10°) turned the view on the spot:
  the gate moved about 135 pixels, and the maths predicts 134 for 10° at 80° across 1280 pixels. The HUD did not
  move. After a mouse turn, and again after walking forward 1.2 s, switching the pose on and off still showed the
  same turn on top of the new camera `[verified-live 2026-09-30, n=1 each]`.
- **The on-screen View is the second one seen.** Applying the pose to View #1 or #3 alone changed nothing; #2 alone
  looked the same as all three. The reader's static work names #2 "View Pass" (all world drawing), #1 "Prototype
  Visibility" (culling) and #3 the radial-blur pass `[verified-live 2026-09-30, n=1]`.
- **Pitch was upside down in the first build.** Numpad 8 looked down. The sign in `headpose_math.c` was flipped,
  both tests re-run (6,002 and 10 checks, pass), the DLL swapped (`094c473eaf62`), and after a relaunch numpad 8
  looked up `[verified-live 2026-09-30, n=1]`.
- **The blue objective arrow does not follow the head.** It stayed at the same screen spot while the scene turned,
  in both the yaw and the pitch tests `[verified-live 2026-09-30, n=2]`. The reader traced why: markers are placed
  with the game camera's own matrix (`cam+0x50`), which our slot never touches (dossier §6).
- **Shadows** followed the turned scene in every picture pair; no reflective surface was in view, so reflections are
  still unchecked.

## Also done

- **Music is now off**, through the pause menu's audio page, saved to `profile.bin` (backed up first). The slider's
  leftmost lit step is not zero; one more Left is.
- **Menu trap:** after quitting, the main menu opens with QUIT highlighted, so the recorded launch route stops at its
  "NEW highlighted" checkpoint instead of pressing on. Up ×3 to CONTINUE, then `run --from 9`, reached gameplay.

## Not established

- Anything about a real headset: H came from keys, one pose at a time.
- Culling uses the body camera, so a head turned far past the flat view may show pop-in at the edges `[hypothesis]`.
- Reflections under a moved head.

## Next

Make the markers follow the head (hook `0x106dcf50` / `0x106dcd70`, reader's design), then feed H from the headset.
