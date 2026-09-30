/* headpose_math.h - camera-space head pose -> the value for Prototype's View+0x2c slot. */
#ifndef HEADPOSE_MATH_H
#define HEADPOSE_MATH_H

typedef struct {
    double yaw_rad, pitch_rad;   /* turn about the camera's own up / right axes           */
    double x_m, y_m, z_m;        /* offset in the camera's own axes, metres               */
} HeadPose;

/* The head pose as a row-vector 4x4 (rotation, then the offset in the last row). */
void headpose_matrix(const HeadPose *h, double out[16]);

/* slot = inv(C) x H x C, C = the camera's camera->world (cam+0x90). Returns 0 (slot untouched) if C
 * cannot be inverted. */
int headpose_slot(const float cam_to_world[16], const HeadPose *h, float slot_out[16]);

#endif
