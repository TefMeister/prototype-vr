# 2026-09-29: the head-pose slot works (View+0x2c)

Half-size pictures of our own live test, dev PC, Alex standing in the alley, taken with `../../tools/proto_cam.py`.

- `ht-base.png`: untouched.
- `ht-0xd965c50.png`: 1 m written into the slot of the view at 0xd965c50 (the on-screen scene view; the other two
  views, 0xd965300 and 0xd98e8b0, changed nothing visible). Near things move most, far things least: a real camera move.
- `yaw-10.png`: a raw 10 degree yaw in the slot: the camera swings about the WORLD origin, onto another street.
- `h-yaw10.png`: the same yaw conjugated (slot = inv(C) . H . C, C = cam+0x90): the view turns on the spot.
- `h-right50.png`: 0.5 m to the right, conjugated.
