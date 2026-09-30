/* headpose_selftest.c - numerical test of headpose_slot(), compiled against the shipped source.
 *
 * The engine builds the camera from C x S (C = cam+0x90, S = View+0x2c, row vectors). For random rigid
 * cameras far from the origin and random head poses it checks, independently of the formula:
 *   1. C x S equals H x C (the camera moved by the head pose in its OWN axes), to float rounding;
 *   2. the resulting camera sits at C's position plus the offset along C's own right/up/forward axes;
 *   3. a zero pose gives the identity slot.
 * Build, from proxy-d3d9/: clang -O2 -o hp.exe test/headpose_selftest.c src/headpose_math.c
 * Exit 0 = pass. -DMUTANT=1 must fail.
 */
#include "../src/headpose_math.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

static double frand(void) { return rand() / (double)RAND_MAX; }

static void rigid(double c[16], double x, double y, double z, double yaw, double pitch, double roll) {
    double cr = cos(roll), sr = sin(roll), cp = cos(pitch), sp = sin(pitch), cy = cos(yaw), sy = sin(yaw);
    double r0[3] = {cr, sr, 0}, u0[3] = {-sr, cr, 0}, f0[3] = {0, 0, 1};
    double *in[3] = {r0, u0, f0};
    for (int i = 0; i < 3; i++) {
        double *v = in[i];
        double a[3] = {v[0], v[1] * cp - v[2] * sp, v[1] * sp + v[2] * cp};
        double b[3] = {a[0] * cy + a[2] * sy, a[1], -a[0] * sy + a[2] * cy};
        c[4 * i] = b[0]; c[4 * i + 1] = b[1]; c[4 * i + 2] = b[2]; c[4 * i + 3] = 0;
    }
    c[12] = x; c[13] = y; c[14] = z; c[15] = 1;
}

static void mul4(const double a[16], const double b[16], double o[16]) {
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++) {
            double s = 0;
            for (int k = 0; k < 4; k++) s += a[4 * i + k] * b[4 * k + j];
            o[4 * i + j] = s;
        }
}

int main(void) {
    int fails = 0, checks = 0;
    double worst_rot = 0, worst_pos = 0;
    srand(4242);
    for (int n = 0; n < 2000; n++) {
        double c[16];
        /* Manhattan is a few km across: cameras up to 4 km from the origin */
        rigid(c, (frand() - 0.5) * 8000, frand() * 400, (frand() - 0.5) * 8000, frand() * 6.283,
              (frand() - 0.5) * 1.2, (frand() - 0.5) * 0.2);
        HeadPose h = {(frand() - 0.5) * 3.0, (frand() - 0.5) * 1.5, (frand() - 0.5) * 0.6,
                      (frand() - 0.5) * 0.6, (frand() - 0.5) * 0.6};
        if (n == 0) h = (HeadPose){0, 0, 0, 0, 0};
        float cf[16], s[16];
        for (int i = 0; i < 16; i++) cf[i] = (float)c[i];
        for (int i = 0; i < 16; i++) c[i] = cf[i];                 /* the engine only has the float */
        checks++;
        if (!headpose_slot(cf, &h, s)) { fails++; printf("FAIL: refused a rigid camera\n"); continue; }
        double sd[16], got[16], hm[16], want[16];
        for (int i = 0; i < 16; i++) sd[i] = s[i];
        mul4(c, sd, got);
        headpose_matrix(&h, hm);
        mul4(hm, c, want);
        /* 1. rotation part to 1e-5, position to the float rounding of km-scale numbers (~1 mm) */
        double er = 0, ep = 0;
        for (int i = 0; i < 3; i++)
            for (int j = 0; j < 3; j++) er = fmax(er, fabs(got[4 * i + j] - want[4 * i + j]));
        for (int j = 0; j < 3; j++) ep = fmax(ep, fabs(got[12 + j] - want[12 + j]));
        worst_rot = fmax(worst_rot, er);
        worst_pos = fmax(worst_pos, ep);
        checks++;
        if (er > 1e-5 || ep > 2e-3) {
            fails++;
            if (fails < 6) printf("FAIL #%d: rotation err %g, position err %g m\n", n, er, ep);
        }
        /* 2. independent of the formula: new position = old + x*right + y*up + z*forward of C itself */
        double pos[3];
        for (int j = 0; j < 3; j++) pos[j] = c[12 + j] + h.x_m * c[j] + h.y_m * c[4 + j] + h.z_m * c[8 + j];
        double ea = 0;
        for (int j = 0; j < 3; j++) ea = fmax(ea, fabs(got[12 + j] - pos[j]));
        checks++;
        if (ea > 2e-3) {
            fails++;
            if (fails < 6) printf("FAIL #%d: camera moved to the wrong place, off by %g m\n", n, ea);
        }
        /* 3. zero pose = identity slot */
        if (n == 0) {
            double ei = 0;
            for (int i = 0; i < 16; i++) ei = fmax(ei, fabs(s[i] - (i % 5 == 0 ? 1.0 : 0.0)));
            checks++;
            if (ei > 1e-3) { fails++; printf("FAIL: zero pose is not the identity (err %g)\n", ei); }
        }
    }
    /* a flat (non-invertible) camera must be refused, not written */
    float flat[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 5, 6, 7, 1}, s[16];
    HeadPose h0 = {0.1, 0, 0, 0, 0};
    checks++;
    if (headpose_slot(flat, &h0, s)) { fails++; printf("FAIL: a flat camera was accepted\n"); }
    printf("checks %d, failures %d, worst rotation err %.2e, worst position err %.2e m\n", checks, fails,
           worst_rot, worst_pos);
    printf(fails ? "RESULT: FAIL\n" : "RESULT: PASS\n");
    return fails ? 1 : 0;
}
