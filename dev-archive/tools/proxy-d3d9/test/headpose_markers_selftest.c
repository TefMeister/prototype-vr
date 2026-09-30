/* headpose_markers_selftest.c - numerical test of headpose_marker_matrix(), compiled against the shipped source.
 *
 * The engine draws the scene with view V_scene = inv(C . S), S = headpose_slot(C, H), and places markers with the
 * camera's own cam+0x50 = inv(C). For random cameras far from the origin, random head poses and random points it
 * checks that p . M . inv(C) (what the marker code now computes) equals p . V_scene (where the scene draws p).
 * Build, from proxy-d3d9/ (32-bit, like the game, since headpose_markers.c includes the hooks):
 *   i686-w64-mingw32-clang -O2 -o mk.exe test/headpose_markers_selftest.c src/headpose_markers.c src/headpose_math.c
 * Exit 0 = pass.
 */
#include "../src/headpose_markers.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

void log_msg(const char *fmt, ...) { (void)fmt; }

static double frand(void) { return rand() / (double)RAND_MAX; }

static void mul4(const double a[16], const double b[16], double o[16]) {
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++) {
            double s = 0;
            for (int k = 0; k < 4; k++) s += a[4 * i + k] * b[4 * k + j];
            o[4 * i + j] = s;
        }
}

static int inv4(const double a[16], double out[16]) {
    double t[4][8];
    for (int i = 0; i < 4; i++) for (int j = 0; j < 8; j++) t[i][j] = j < 4 ? a[4 * i + j] : (j - 4 == i);
    for (int c = 0; c < 4; c++) {
        int p = c;
        for (int r = c + 1; r < 4; r++) if (fabs(t[r][c]) > fabs(t[p][c])) p = r;
        if (fabs(t[p][c]) < 1e-12) return 0;
        for (int j = 0; j < 8; j++) { double x = t[c][j]; t[c][j] = t[p][j]; t[p][j] = x; }
        double d = t[c][c];
        for (int j = 0; j < 8; j++) t[c][j] /= d;
        for (int r = 0; r < 4; r++) if (r != c) { double k = t[r][c]; for (int j = 0; j < 8; j++) t[r][j] -= k * t[c][j]; }
    }
    for (int i = 0; i < 4; i++) for (int j = 0; j < 4; j++) out[4 * i + j] = t[i][4 + j];
    return 1;
}

static void xf(const double p[3], const double m[16], double o[3]) {
    for (int j = 0; j < 3; j++) o[j] = p[0] * m[j] + p[1] * m[4 + j] + p[2] * m[8 + j] + m[12 + j];
}

int main(void) {
    int fails = 0, checks = 0;
    double worst = 0;
    srand(777);
    for (int n = 0; n < 3000; n++) {
        HeadPose cam = {frand() * 6.283, (frand() - 0.5) * 1.0, 0, 0, 0};
        double c[16];
        headpose_matrix(&cam, c);                       /* a rigid camera->world, placed far away */
        c[12] = (frand() - 0.5) * 6000; c[13] = frand() * 300; c[14] = (frand() - 0.5) * 6000;
        float cf[16], sf[16];
        for (int i = 0; i < 16; i++) { cf[i] = (float)c[i]; c[i] = cf[i]; }
        HeadPose h = {(frand() - 0.5) * 2, (frand() - 0.5) * 1, (frand() - 0.5) * 0.4, (frand() - 0.5) * 0.4,
                      (frand() - 0.5) * 0.4};
        double m[16], s[16], cs[16], vscene[16], ci[16];
        if (!headpose_slot(cf, &h, sf) || !headpose_marker_matrix(cf, &h, m)) { fails++; continue; }
        for (int i = 0; i < 16; i++) s[i] = sf[i];
        mul4(c, s, cs);
        inv4(cs, vscene);
        inv4(c, ci);
        double p[3], a[3], b[3], q[3];
        for (int j = 0; j < 3; j++) p[j] = c[12 + j] + (frand() - 0.5) * 200;
        xf(p, m, q);
        xf(q, ci, a);          /* marker path: p . M . cam+0x50 */
        xf(p, vscene, b);      /* scene path */
        double e = fmax(fmax(fabs(a[0] - b[0]), fabs(a[1] - b[1])), fabs(a[2] - b[2]));
        if (e > worst) worst = e;
        checks++;
        if (e > 5e-3) { fails++; if (fails < 5) printf("FAIL #%d: off by %g m\n", n, e); }
    }
    printf("checks %d, failures %d, worst %.2e m\n%s\n", checks, fails, worst, fails ? "RESULT: FAIL" : "RESULT: PASS");
    return fails ? 1 : 0;
}
