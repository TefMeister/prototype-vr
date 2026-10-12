/* headpose_xr.h - the headset's head pose for Prototype through OpenXR (2026-10-12, /pd; see headpose_xr.c). */
#ifndef HEADPOSE_XR_H
#define HEADPOSE_XR_H

#include "headpose_math.h"

/* GAME THREAD, every scene frame (headpose.c). Watches the switch file and starts the headset thread the first
 * time it appears. Returns 1 and the newest headset pose while the headset is the source; 0 = use the keys. */
int headpose_xr_poll(HeadPose *out);

/* Any thread: the next headset pose becomes "straight ahead". */
void headpose_xr_recentre(void);

#endif
