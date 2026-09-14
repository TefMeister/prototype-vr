# 2026-09-14 (b) — 🏆 Prototype's camera found: c0, left-handed, near 0.3, far 7500, 80° exactly

**Tefa played the tutorial; Claude read the log.**
`[verified-live 2026-09-14, n=1 session, ~8 minutes, 500k+ constant uploads]`
Evidence: `dev-archive/recon/2026-09-14-camera-found/proto_proxy-tutorial-playthrough.log`.

## The answer

```
c0, layout R (register i = row i), written as a dedicated "c0+4"

    [  1.191754    0.000000    0.000000    0.000000 ]
    [  0.000000    2.118673    0.000000    0.000000 ]
    [  0.000000    0.000000    1.000040    1.000000 ]
    [  0.000000    0.000000   -0.300012    0.000000 ]
```

| Property | Value | How it is known |
| --- | --- | --- |
| Register | **c0**, four registers | a dedicated `c0+4` write, 12,486 in one 5 s period |
| Packing | **layout R** — register *i* is row *i* | the w-from-z term sits at index 11 |
| Handedness | **LEFT-handed**, `clip.w = +view.z` | `m[11] = +1`, every sighting |
| Aspect | **exactly 1.7778 = 16/9** | `ys/xs`, matching the 1920×1080 fullscreen mode |
| X axis | **not mirrored** (`xs` positive) | contrast with Dead Space 2, whose `xs` is negative |
| Near plane | **0.300000** | `−m[14]/m[10]` |
| Far plane | **7500** | `m[10]·zn/(m[10]−1)` |
| FOV | **80.00° horizontal**, 50.53° vertical | `2·atan(1/1.191754)` |

⭐ **80.00° to two decimal places, near 0.3, far 7500.** Three round numbers a designer would type.
That the arithmetic lands on them is independent corroboration that the matrix has been read correctly
rather than merely found.

⭐ **The depth term fits the textbook left-handed form exactly** — unlike Dead Space 2, whose
`m[10] = 0.990097` had to be flagged as non-standard. Here `m[10] = 1.000040` gives a clean
`zf = 7500`. Any per-eye derivation can use the standard formulae without re-deriving from scratch.

## The zoom ladder — eight fields of view, all at exactly 16:9

| h-FOV | sightings | likely |
| --- | --- | --- |
| **80.00°** | 34 | ⭐ normal gameplay |
| 60.80° | 2 | a moderate zoom or cutscene |
| 44.24°, 42.73° | 3 | closer |
| 35.67°, 32.70° | 2 | closer still |
| 20.51°, 19.76° | 2 | long zoom — a scoped or targeting view |

Every one carries `ys/xs = 1.7778` to four decimals. **One camera, eight fields** — not eight cameras.

## ⚠️ CORRECTION — the matrix is a PURE PROJECTION, not the fused one I predicted

Earlier today I read the shipped shader source in `shaders.rcf`, saw
`const float4x4 p3dWorldViewProjectionMatrix : register( c0 );`, and concluded the game uses a
**fused** world-view-projection. I then reasoned from that: "head tracking is harder, there is no view
register to rotate, so it must go in CPU-side — that promotes the `CameraManager` row for a second
reason."

**The runtime measurement says otherwise.** The matrix actually arriving at `c0` during gameplay has a
**perfectly diagonal upper-left 2×2 with zero rotation terms**. A fused world-view-projection carries
the camera's rotation there. **This is a projection alone.**

⭐ **Both observations are true, and the mistake was mine in generalising.** I had already written
that registers are assigned **per shader** in this engine and that 10.5 KB is a *sample* of utility
shaders — and then reasoned as though the sample's convention were the engine's. The sampled shader
really does use a fused WVP at c0; **the shaders that draw the game do not.**

**What the correction changes:**

- The "second independent reason" for promoting `proto::CameraManager` **is withdrawn.** It remains a
  good row on its original merit — a single named class owning the camera is the friendliest shape for
  head tracking — but not because of matrix fusion.
- ⭐ **This is better news, not worse.** A separate projection means the same clean split Dead Space 2
  has: **per-eye stereo shears the projection at c0; head tracking goes to whatever carries the view.**
- **Where the view lives is now the open question**, exactly as it is on Dead Space 2.

## ⚠️ The detector is noisy on this engine, and the aspect ratio did the real work

Dead Space 2 produced three candidate signatures, one obviously right. **Prototype produced dozens**,
most of them junk that happens to have a `±1` in the w slot — `[c9 … 17.2844]`, `[c25 … −0.5589]`,
`[c8 392.918060 29.281885 … 0.0745]`, a `c1` whose ratio wanders between 2.8 and 18.9 between periods.

**What separated the camera from all of it was one test: `ys/xs` equals the display aspect ratio.** The
camera sat at 1.7778 in every period while the noise wandered. That heuristic was carried over from
Alan Wake as a line of log text; on this engine it is the whole instrument.

⭐ **Improvement earned by this run, now implemented:** the wrapper already sees the back-buffer
dimensions in `CreateDevice`, so **the instrument now computes the display aspect itself and marks
matching signatures with `<<< MATCHES DISPLAY ASPECT`** instead of asking a human to divide. On a noisy
engine that is the difference between an answer and a list.

⚠️ Deliberately **not** done: filtering out non-matching signatures. A square (1.0) frustum is a
legitimate shadow pass and worth seeing, and an engine that renders at a non-display aspect would be
hidden by such a filter.

## What is NOT established

- **One session, one area** (the tutorial). `n=1`. The register could differ elsewhere.
- **Where the view/world transform lives.** `c0+5` is the single most common upload (49,260 per 5 s
  against the camera's 12,486) and its contents have not been read. That is the next thing to look at.
- **Whether `c0` holds the projection in every shader.** It does not in the sampled utility shader, so
  "c0 is the projection" is true of the gameplay path observed, not of the engine universally.
- **Nothing has been written.** The instrument is read-only; whether editing c0 moves the picture is
  untested.
- The device is **not** a pure device (`BehaviorFlags=0x44`), so unlike Dead Space 2 a read-back
  instrument *would* work here if one is ever needed.

## The next step, and it needs no game

Log the contents of the `c0+5` upload for a few frames. If it carries rotation that changes as the
player turns, that is the view transform, and the two halves of the job are separated on this project
too.
