/* headpose.h - per-frame head pose for Prototype through View+0x2c (see headpose.c). */
#ifndef HEADPOSE_H
#define HEADPOSE_H

#include <stdint.h>
#include "headpose_math.h"

/* Hook pure3d's View/Camera setup. Call once the engine DLL is loaded (from CreateDevice). */
void headpose_install(void);

/* The same on any function with the expected prologue; calls returning to `render_return` get the pose.
 * Used by test/headpose_hook_selftest.c on a stand-in function. Returns 1 if the jump was written. */
int headpose_install_at(void *target, uintptr_t render_return);

/* Set the head pose from code (a headset feed, or the test) and start writing it. */
void headpose_set_pose(const HeadPose *h);

#endif
