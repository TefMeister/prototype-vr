/* headpose_xr_math.c - OpenXR head pose -> HeadPose for Prototype (2026-10-12, /pd). See headpose_xr_math.h.
 * Checked by test/headpose_xr_selftest.c against the shipped headpose_matrix(). */
#include "headpose_xr_math.h"
#include <math.h>
#include <string.h>

#define HX_HALF_TURN 0.5           /* a quaternion holds HALF its rotation angle */
#define HX_RAD2DEG   57.29577951308232

static int g_recentre = 1;
static double g_yaw0, g_p0[3];

void hx_request_recentre(void) { g_recentre = 1; }

/* Column-convention rotation matrix of a unit quaternion (x y z w): v' = R v. */
static void q_to_m(const double q[4], double R[9]) {
    double x = q[0], y = q[1], z = q[2], w = q[3];
    double n = x * x + y * y + z * z + w * w, s = n > 0 ? 2.0 / n : 0.0;
    R[0] = 1 - s * (y * y + z * z); R[1] = s * (x * y - z * w);     R[2] = s * (x * z + y * w);
    R[3] = s * (x * y + z * w);     R[4] = 1 - s * (x * x + z * z); R[5] = s * (y * z - x * w);
    R[6] = s * (x * z - y * w);     R[7] = s * (y * z + x * w);     R[8] = 1 - s * (x * x + y * y);
}

static double yaw_of(const double q[4]) {
    double R[9];
    q_to_m(q, R);
    return atan2(R[2], R[8]);            /* forward = R (0,0,-1) = (-R2, -R5, -R8); heading = atan2(-fx, -fz) */
}

double hx_yaw_deg(const float qf[4]) {
    double q[4] = { qf[0], qf[1], qf[2], qf[3] };
    return yaw_of(q) * HX_RAD2DEG;
}

int hx_head_from_xr(const float qf[4], const float pf[3], int use_position, HeadPose *out) {
    double q[4] = { qf[0], qf[1], qf[2], qf[3] }, p[3] = { pf[0], pf[1], pf[2] };
    int rc = 0;
    if (g_recentre) {
        g_recentre = 0;
        g_yaw0 = yaw_of(q);
        memcpy(g_p0, p, sizeof g_p0);
        rc = 1;
    }
    /* take the start heading out: R_rel = Ry(-yaw0) R, d = Ry(-yaw0) (p - p0) */
    double R[9], Ry[9], Rr[9], d[3] = { 0, 0, 0 };
    double h = -HX_HALF_TURN * g_yaw0, qy[4] = { 0, sin(h), 0, cos(h) };
    q_to_m(q, R);
    q_to_m(qy, Ry);
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++) Rr[3 * i + j] = Ry[3 * i] * R[j] + Ry[3 * i + 1] * R[3 + j] + Ry[3 * i + 2] * R[6 + j];
    if (use_position)
        for (int i = 0; i < 3; i++)
            d[i] = Ry[3 * i] * (p[0] - g_p0[0]) + Ry[3 * i + 1] * (p[1] - g_p0[1]) + Ry[3 * i + 2] * (p[2] - g_p0[2]);
    /* into the game's axes: R_e = S Rr S, d_e = S d. HeadPose rows = the head's axes in camera coords =
     * the COLUMNS of R_e, so rot = R_e transposed. */
    double Re[9];
    memcpy(Re, Rr, sizeof Re);
    Re[2] = -Re[2]; Re[5] = -Re[5]; Re[6] = -Re[6]; Re[7] = -Re[7];
    memset(out, 0, sizeof *out);
    out->has_rot = 1;
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++) out->rot[3 * i + j] = Re[3 * j + i];
    out->x_m = d[0];
    out->y_m = d[1];
    out->z_m = -d[2];
    return rc;
}
