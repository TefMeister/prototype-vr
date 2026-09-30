/* headpose_math.c - the head-pose maths for Prototype's View+0x2c slot. Pure C, no Windows, so the
 * self-test (test/headpose_selftest.c) runs it exactly as shipped.
 *
 * Measured live 2026-09-29 (modding-notes/2026-09-29b-head-tracking-slot-works.md): SetupCamera builds
 * the view as Invert(C x S), with C = the camera's camera->world at cam+0x90 and S = the View's slot at
 * +0x2c, row vectors (p' = p . M). A head pose H is given in CAMERA space, so the camera->world we want is
 * H x C, and C x S = H x C gives S = inv(C) x H x C. A raw H in the slot orbits the world origin instead
 * (seen live: a 10 degree yaw swung the camera onto another street). */
#include "headpose_math.h"
#include <math.h>
#include <string.h>

#ifndef MUTANT
#define MUTANT 0
#endif

int headpose_invert4(const double a[16], double out[16]) {
    double t[4][8];
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 8; j++) t[i][j] = j < 4 ? a[4 * i + j] : (j - 4 == i);
    for (int c = 0; c < 4; c++) {
        int p = c;
        for (int r = c + 1; r < 4; r++)
            if (fabs(t[r][c]) > fabs(t[p][c])) p = r;
        if (fabs(t[p][c]) < 1e-9) return 0;
        for (int j = 0; j < 8; j++) { double x = t[c][j]; t[c][j] = t[p][j]; t[p][j] = x; }
        double d = t[c][c];
        for (int j = 0; j < 8; j++) t[c][j] /= d;
        for (int r = 0; r < 4; r++) {
            if (r == c) continue;
            double k = t[r][c];
            for (int j = 0; j < 8; j++) t[r][j] -= k * t[c][j];
        }
    }
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++) out[4 * i + j] = t[i][4 + j];
    return 1;
}

void headpose_mul4(const double a[16], const double b[16], double o[16]) {
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++) {
            double s = 0;
            for (int k = 0; k < 4; k++) s += a[4 * i + k] * b[4 * k + j];
            o[4 * i + j] = s;
        }
}

void headpose_matrix(const HeadPose *h, double out[16]) {
    /* pitch about the camera's x, then yaw about its y, then the offset; row vectors. Positive pitch looks UP:
     * the first build had the sign the other way and numpad 8 looked down (seen live 2026-09-30). */
    double cp = cos(h->pitch_rad), sp = sin(h->pitch_rad), cy = cos(h->yaw_rad), sy = sin(h->yaw_rad);
    double rx[16] = {1, 0, 0, 0,  0, cp, -sp, 0,  0, sp, cp, 0,  0, 0, 0, 1};
    double ry[16] = {cy, 0, -sy, 0,  0, 1, 0, 0,  sy, 0, cy, 0,  0, 0, 0, 1};
    headpose_mul4(rx, ry, out);
    out[12] = h->x_m;
    out[13] = h->y_m;
    out[14] = h->z_m;
}

int headpose_slot(const float cam_to_world[16], const HeadPose *h, float slot_out[16]) {
    double c[16], ci[16], hm[16], t[16], s[16];
    for (int i = 0; i < 16; i++) c[i] = cam_to_world[i];
    if (!headpose_invert4(c, ci)) return 0;
    headpose_matrix(h, hm);
#if MUTANT == 1 /* mutant: the raw pose, no conjugation (what orbited the world origin live) */
    memcpy(s, hm, sizeof s);
    (void)t;
#else
    headpose_mul4(ci, hm, t);
    headpose_mul4(t, c, s);
#endif
    for (int i = 0; i < 16; i++) slot_out[i] = (float)s[i];
    return 1;
}
