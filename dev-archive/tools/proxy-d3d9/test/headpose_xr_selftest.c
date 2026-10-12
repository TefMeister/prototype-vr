/* headpose_xr_selftest.c - OpenXR head pose -> HeadPose, compiled against the shipped headpose_xr_math.c and
 * headpose_math.c (2026-10-12, /pd).
 *
 * The key route (numpad yaw/pitch) was seen working in the game on 2026-09-30 (numpad 6 turned right, numpad 8
 * looked up after the sign fix). So the strongest check here is: a headset turned LEFT by t must give exactly the
 * matrix the keys give for yaw -t; looking UP by t the keys' pitch +t; both together the keys' yaw and pitch
 * together. Then random heads against an independent quaternion route (q v q*, no matrices), the recentre,
 * the position, and tilt (which the keys cannot do). Build + run: see build.sh. Exit 0 = pass.
 * -DPLANT_NO_MIRROR=1 (the classic mistake: forgetting OpenXR's z points back) must fail. */
#include "../src/headpose_math.h"
#include "../src/headpose_xr_math.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef PLANT_NO_MIRROR
#define PLANT_NO_MIRROR 0
#endif
#define D2R 0.017453292519943295
#define TOL 1e-6
#define RANDOM_CASES 20000

static int g_fail, g_pass;
#define CHECK(c, what) do { if (c) g_pass++; else { g_fail++; if (g_fail < 15) printf("FAIL: %s\n", what); } } while (0)

static double frand(double a, double b) { return a + (b - a) * rand() / (double)RAND_MAX; }

static void q_axis(double ax, double ay, double az, double deg, float q[4]) {
    double n = sqrt(ax * ax + ay * ay + az * az), h = deg * D2R / 2;
    q[0] = (float)(ax / n * sin(h)); q[1] = (float)(ay / n * sin(h)); q[2] = (float)(az / n * sin(h)); q[3] = (float)cos(h);
}
static void qm(const float a[4], const float b[4], float o[4]) {
    float r[4] = { a[3] * b[0] + a[0] * b[3] + a[1] * b[2] - a[2] * b[1], a[3] * b[1] - a[0] * b[2] + a[1] * b[3] + a[2] * b[0],
                   a[3] * b[2] + a[0] * b[1] - a[1] * b[0] + a[2] * b[3], a[3] * b[3] - a[0] * b[0] - a[1] * b[1] - a[2] * b[2] };
    memcpy(o, r, sizeof r);
}
static void q_rot(const float q[4], const double v[3], double o[3]) {   /* q v q* */
    float p[4] = { (float)v[0], (float)v[1], (float)v[2], 0 }, c[4] = { -q[0], -q[1], -q[2], q[3] }, t[4];
    qm(q, p, t);
    qm(t, c, t);
    o[0] = t[0]; o[1] = t[1]; o[2] = t[2];
}

/* headset pose through the shipped code, with the planted mistake if asked */
static void from_xr(const float q[4], const float p[3], int pos, double m[16]) {
    HeadPose h;
    hx_head_from_xr(q, p, pos, &h);
#if PLANT_NO_MIRROR
    h.rot[2] = -h.rot[2]; h.rot[5] = -h.rot[5]; h.rot[6] = -h.rot[6]; h.rot[7] = -h.rot[7]; h.z_m = -h.z_m;
#endif
    headpose_matrix(&h, m);
}
static void from_keys(double yaw, double pitch, double m[16]) {
    HeadPose h = { .yaw_rad = yaw, .pitch_rad = pitch };
    headpose_matrix(&h, m);
}
static double diff16(const double a[16], const double b[16]) {
    double w = 0;
    for (int i = 0; i < 16; i++) w = fmax(w, fabs(a[i] - b[i]));
    return w;
}
static void straight_ahead(void) {               /* recentre on a plain start pose */
    float q[4] = { 0, 0, 0, 1 }, p[3] = { 0, 0, 0 };
    HeadPose h;
    hx_request_recentre();
    hx_head_from_xr(q, p, 1, &h);
}

int main(void) {
    float q[4], qa[4], qb[4], p[3] = { 0, 0, 0 };
    double m[16], k[16];

    /* 1. the headset agrees with the keys that were seen working */
    straight_ahead();
    q_axis(0, 1, 0, 30, q);                       /* head turned 30 deg LEFT */
    from_xr(q, p, 1, m); from_keys(-30 * D2R, 0, k);
    CHECK(diff16(m, k) < TOL, "turn left 30 = keys yaw -30");
    q_axis(1, 0, 0, 20, q);                       /* looking 20 deg UP */
    from_xr(q, p, 1, m); from_keys(0, 20 * D2R, k);
    CHECK(diff16(m, k) < TOL, "look up 20 = keys pitch +20");
    q_axis(0, 1, 0, -40, qa); q_axis(1, 0, 0, -15, qb); qm(qa, qb, q);   /* turned 40 right, then looking 15 down */
    from_xr(q, p, 1, m); from_keys(40 * D2R, -15 * D2R, k);
    CHECK(diff16(m, k) < TOL, "turn right 40 + look down 15 = keys yaw +40 pitch -15");

    /* 2. tilt (keys cannot): left ear down = the head's up leans to the camera's left */
    q_axis(0, 0, 1, 25, q);
    from_xr(q, p, 1, m);
    CHECK(fabs(m[4] + sin(25 * D2R)) < TOL && fabs(m[5] - cos(25 * D2R)) < TOL && fabs(m[6]) < TOL,
          "tilt left 25: the head's up row is (-sin, cos, 0)");

    /* 3. position: 10 cm right -> x +0.1; 10 cm back (OpenXR +z) -> camera z -0.1; off -> nothing */
    q_axis(0, 1, 0, 0, q);
    p[0] = 0.1f; from_xr(q, p, 1, m);
    CHECK(fabs(m[12] - 0.1) < 1e-6 && fabs(m[14]) < 1e-6, "step right 10 cm");
    p[0] = 0; p[2] = 0.1f; from_xr(q, p, 1, m);
    CHECK(fabs(m[14] + 0.1) < 1e-6, "step back 10 cm = camera z -0.1");
    from_xr(q, p, 0, m);
    CHECK(fabs(m[12]) + fabs(m[13]) + fabs(m[14]) < 1e-9, "position off: no offset");
    p[2] = 0;

    /* 4. recentre: started turned 70 left and standing at (0.4, 1.7, -0.3); looking 10 up from there = keys pitch +10 */
    {
        float start[3] = { 0.4f, 1.7f, -0.3f };
        HeadPose h;
        q_axis(0, 1, 0, 70, qa);
        CHECK(fabs(hx_yaw_deg(qa) - 70) < 1e-3, "heading reads 70");
        hx_request_recentre();
        CHECK(hx_head_from_xr(qa, start, 1, &h) == 1, "recentre reported");
        q_axis(1, 0, 0, 10, qb); qm(qa, qb, q);
        from_xr(q, start, 1, m); from_keys(0, 10 * D2R, k);
        CHECK(diff16(m, k) < TOL, "after recentring: forward = start heading, 10 up stays");
    }

    /* 5. random heads vs the quaternion route: the head's right/up/forward in camera axes, and its offset */
    srand(77);
    for (int n = 0; n < RANDOM_CASES; n++) {
        float q0[4], turn[4], start[3] = { (float)frand(-1, 1), (float)frand(1, 2), (float)frand(-1, 1) }, now[3];
        q_axis(0, 1, 0, frand(-180, 180), q0);
        hx_request_recentre();
        { HeadPose h; hx_head_from_xr(q0, start, 1, &h); }
        q_axis(frand(-1, 1), frand(-1, 1), frand(-1, 1) + 1e-3, frand(-120, 120), turn);
        qm(turn, q0, q);
        for (int i = 0; i < 3; i++) now[i] = start[i] + (float)frand(-0.4, 0.4);
        from_xr(q, now, 1, m);
        float qy[4] = { -q0[0], -q0[1], -q0[2], q0[3] }, rel[4];
        qm(qy, q, rel);                           /* heading taken out */
        static const double ax[3][3] = { { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, -1 } };   /* right, up, forward (-z) */
        double worst = 0, v[3], dp[3], d[3];
        for (int r = 0; r < 3; r++) {
            q_rot(rel, ax[r], v);
            v[2] = -v[2];                         /* into the game's axes */
            for (int j = 0; j < 3; j++) worst = fmax(worst, fabs(m[4 * r + j] - v[j]));
        }
        for (int i = 0; i < 3; i++) dp[i] = now[i] - start[i];
        q_rot(qy, dp, d);
        d[2] = -d[2];
        for (int j = 0; j < 3; j++) worst = fmax(worst, fabs(m[12 + j] - d[j]));
        CHECK(worst < 2e-5, "random head agrees with the quaternion route");
    }

    printf("headpose xr selftest%s: %d passed, %d failed\n", PLANT_NO_MIRROR ? " (PLANTED: no z mirror)" : "", g_pass, g_fail);
    return g_fail != 0;
}
