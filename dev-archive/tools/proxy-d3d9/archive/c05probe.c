/*
 * c05probe.c - read the c0+5 upload. READ-ONLY PROBE (2026-09-29, /lm).
 *
 * The 2026-09-14 run found the most common vertex-constant upload of all is
 * start c0, count 5 (49,260 per 5 s against the camera's 12,486) and never read
 * it. This logs short bursts of it, alongside the latest camera projection (the
 * dedicated c0+4 write), so the log can answer: is c0..c3 of this block a
 * per-object world-view-projection, and does its rotation part follow the
 * camera when the player turns?
 *
 * Probe, not mod: it lives in its own file, changes nothing it forwards, and
 * goes to archive/ once the question is answered.
 */
#include <windows.h>
#include <stdio.h>
#include <string.h>

void log_msg(const char *fmt, ...);     /* proxy.c */

/* ---- Settings ------------------------------------------------------------ */
#define C05_START          0u      /* the upload being read: start register... */
#define C05_COUNT          5u      /* ...and count                              */
#define C05_BURST          6       /* consecutive uploads logged per burst      */
#define C05_BURST_EVERY_MS 2000u   /* one burst every two seconds               */
#define C05_MAX_BURSTS     90      /* three minutes, then the probe goes quiet  */
#define CAM_START          0u      /* the camera's own write (c0+4)             */
#define CAM_COUNT          4u

static float     g_cam[16];
static int       g_have_cam;
static ULONGLONG g_last_burst_ms;
static int       g_in_burst, g_bursts;

void c05_observe(unsigned int start, const float *data, unsigned int count) {
    ULONGLONG now;
    int r;
    if (!data) return;
    if (start == CAM_START && count == CAM_COUNT) {
        memcpy(g_cam, data, sizeof g_cam);
        g_have_cam = 1;
        return;
    }
    if (start != C05_START || count != C05_COUNT || g_bursts >= C05_MAX_BURSTS) return;
    now = GetTickCount64();
    if (!g_in_burst) {
        if (now - g_last_burst_ms < C05_BURST_EVERY_MS) return;
        g_last_burst_ms = now;
        ++g_bursts;
        log_msg("C05 burst %d at %llu ms. camera c0+4 (last seen): diag %.5f %.5f z %.5f %.5f",
                g_bursts, (unsigned long long)now, g_have_cam ? g_cam[0] : 0.0f,
                g_have_cam ? g_cam[5] : 0.0f, g_have_cam ? g_cam[10] : 0.0f,
                g_have_cam ? g_cam[14] : 0.0f);
    }
    log_msg("C05 #%d", g_in_burst);
    for (r = 0; r < (int)C05_COUNT; ++r)
        log_msg("C05   c%d [%12.6f %12.6f %12.6f %12.6f]", r, data[4 * r], data[4 * r + 1],
                data[4 * r + 2], data[4 * r + 3]);
    if (++g_in_burst >= C05_BURST) g_in_burst = 0;
}
