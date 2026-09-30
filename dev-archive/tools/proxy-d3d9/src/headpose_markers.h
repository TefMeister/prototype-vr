/* headpose_markers.h - HUD markers and scene culling follow the head pose (see headpose_markers.c). */
#ifndef HEADPOSE_MARKERS_H
#define HEADPOSE_MARKERS_H

#include <stdint.h>
#include "headpose_math.h"

/* Where the markers get the current head pose. Returns 1 and fills *h when a pose is active. */
typedef int (*HeadPoseSource)(HeadPose *h);

/* M = inv(C) . inv(H) . C (row vectors): a world point p becomes p . M. Returns 0 if C cannot be inverted. */
int headpose_marker_matrix(const float cam_to_world[16], const HeadPose *h, double out[16]);

/* Install both hooks; `delta` = actual engine base - 0x10000000. Returns how many were written (0-2). */
int headpose_markers_install(uintptr_t delta, HeadPoseSource source);

/* Numpad . : switch the marker/culling adjustment off or on (for A/B tests). */
void headpose_markers_toggle(void);

#endif
