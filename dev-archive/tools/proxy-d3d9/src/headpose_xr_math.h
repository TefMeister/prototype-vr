/* headpose_xr_math.h - an OpenXR head pose -> Prototype's HeadPose (2026-10-12, /pd). Pure C, no Windows.
 *
 * OpenXR's LOCAL space is right-handed: x right, y up, z BACK. Prototype's camera is left-handed: x right, y up,
 * z FORWARD (dossier §6, measured 2026-09-14). So the head is mirrored through z on its way in (R_e = S R S,
 * S = diag(1,1,-1)). The first pose after a recentre is "straight ahead": only its heading is taken out, so
 * looking up or tilting stays real. Head movement is measured from where the head was at the recentre. */
#ifndef HEADPOSE_XR_MATH_H
#define HEADPOSE_XR_MATH_H

#include "headpose_math.h"

/* The next call takes the current heading and position as forward / centre. */
void hx_request_recentre(void);

/* q = OpenXR orientation (x y z w), p = position in metres, both in LOCAL. use_position 0 = turning only.
 * Fills out (has_rot = 1). Returns 1 if this call recentred. Keeps the recentre state: one caller thread only. */
int hx_head_from_xr(const float q[4], const float p[3], int use_position, HeadPose *out);

/* Heading of an OpenXR orientation in degrees, positive = turned left (for the log). */
double hx_yaw_deg(const float q[4]);

#endif
