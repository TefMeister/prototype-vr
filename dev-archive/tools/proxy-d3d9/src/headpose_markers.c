/* headpose_markers.c - make Prototype's world-anchored HUD markers (and the scene camera's sphere culling)
 * follow the head pose that headpose.c writes into View+0x2c.
 *
 * Why (live 2026-09-30): the objective arrow stayed at its screen spot while the head-posed scene turned. The
 * marker code projects with pure3d::Camera's own world->camera matrix (cam+0x50), never with the View:
 *   0x106dcf50  Camera::WorldToScreen   thiscall(Camera*) (const Vec3 *world, Vec3 *out), ret 8
 *   0x106dcd70  Camera::SphereVisible   thiscall(Camera*) (const Vec3 *world, float radius) -> al, ret 8
 * (reader's static trace, prologues checked on disk: 83 EC 0C | 56 | 8B F1). The scene's final view is
 * inv(C.S) = cam+0x50 . inv(H) with S = inv(C).H.C, so handing the engine p' = p.inv(C).inv(H).C puts a marker
 * exactly where the head-posed scene draws that point (5,000 random checks, 0 failures; reader,
 * staging/prototype-vr/reader-2026-09-30/markers/).
 *
 * SphereVisible is general culling (16 callers), so hooking it also makes the scene camera's culling follow the
 * head: the intended effect, and a behaviour change that the log and numpad . can switch off for a test.
 * The combined matrix is cached and rebuilt only when C or the pose changes, since culling runs per object. */
#include <windows.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include "headpose_markers.h"

void log_msg(const char *fmt, ...);     /* proxy.c */

/* ---- Settings ------------------------------------------------------------------------------------- */
#define VA_CAM_WORLD_TO_SCREEN  0x106dcf50u
#define VA_CAM_SPHERE_VISIBLE   0x106dcd70u
#define OFF_CAM_NEAR            0x20
#define OFF_CAM_FAR             0x24
#define OFF_CAM_TO_WORLD        0x90
#define SCENE_NEAR              0.3f
#define SCENE_FAR               7500.0f
#define MARKER_PATCH_LEN        6       /* three whole instructions: 83 EC 0C | 56 | 8B F1 */
#define OPCODE_JMP_REL32        0xE9
#define OPCODE_NOP              0x90
#define JMP_LEN                 5
static const uint8_t MARKER_PROLOGUE[MARKER_PATCH_LEN] = {0x83, 0xEC, 0x0C, 0x56, 0x8B, 0xF1};

/* ---- maths ---------------------------------------------------------------------------------------- */
int headpose_marker_matrix(const float cam_to_world[16], const HeadPose *h, double out[16]) {
    double c[16], ci[16], hm[16], hi[16], t[16];
    for (int i = 0; i < 16; i++) c[i] = cam_to_world[i];
    if (!headpose_invert4(c, ci)) return 0;
    headpose_matrix(h, hm);
    if (!headpose_invert4(hm, hi)) return 0;
    headpose_mul4(ci, hi, t);
    headpose_mul4(t, c, out);
    return 1;
}

/* ---- hooks ---------------------------------------------------------------------------------------- */
typedef void *(__fastcall *WorldToScreenFn)(void *cam, void *edx, const float *world, float *out);
typedef int   (__fastcall *SphereVisibleFn)(void *cam, void *edx, const float *world, float radius);
/* __fastcall with a dummy EDX matches thiscall: ECX = this, stack args popped by the callee (ret 8). */

static HeadPoseSource  g_source;
static WorldToScreenFn g_w2s_tramp;
static SphereVisibleFn g_vis_tramp;
static int             g_enabled = 1;
static unsigned long   g_adjusted_w2s, g_adjusted_vis;

/* cache: the combined matrix for the last (C, pose) seen */
static float    g_cached_c[16];
static HeadPose g_cached_h;
static double   g_cached_m[16];
static int      g_cached_ok;

static int is_scene_camera(const void *cam) {
    float n = *(const float *)((const char *)cam + OFF_CAM_NEAR);
    float f = *(const float *)((const char *)cam + OFF_CAM_FAR);
    return fabsf(n - SCENE_NEAR) < 1e-3f && fabsf(f - SCENE_FAR) < 1.0f;
}

/* Returns `world` unchanged, or `scratch` holding p' = p . M. */
static const float *adjust(const void *cam, const float *world, float scratch[3]) {
    HeadPose h;
    if (!g_enabled || !cam || !world || !g_source || !is_scene_camera(cam) || !g_source(&h)) return world;
    const float *c = (const float *)((const char *)cam + OFF_CAM_TO_WORLD);
    if (!g_cached_ok || memcmp(c, g_cached_c, sizeof g_cached_c) || memcmp(&h, &g_cached_h, sizeof h)) {
        memcpy(g_cached_c, c, sizeof g_cached_c);
        g_cached_h = h;
        g_cached_ok = headpose_marker_matrix(c, &h, g_cached_m);
    }
    if (!g_cached_ok) return world;
    const double *m = g_cached_m;
    for (int j = 0; j < 3; j++)
        scratch[j] = (float)(world[0] * m[j] + world[1] * m[4 + j] + world[2] * m[8 + j] + m[12 + j]);
    return scratch;
}

static void *__fastcall hook_w2s(void *cam, void *edx, const float *world, float *out) {
    float p[3];
    const float *use = adjust(cam, world, p);
    if (use != world) g_adjusted_w2s++;
    return g_w2s_tramp(cam, edx, use, out);
}

static int __fastcall hook_vis(void *cam, void *edx, const float *world, float radius) {
    float p[3];
    const float *use = adjust(cam, world, p);
    if (use != world) g_adjusted_vis++;
    return g_vis_tramp(cam, edx, use, radius);
}

static void *install_one(uint8_t *target, const void *hook, const char *name) {
    if (memcmp(target, MARKER_PROLOGUE, MARKER_PATCH_LEN) != 0) {
        log_msg("HEADPOSE markers: REFUSING %s at %p - unexpected first bytes %02X %02X %02X %02X %02X %02X", name,
                target, target[0], target[1], target[2], target[3], target[4], target[5]);
        return NULL;
    }
    uint8_t *tramp = VirtualAlloc(NULL, 16, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!tramp) return NULL;
    memcpy(tramp, target, MARKER_PATCH_LEN);
    tramp[MARKER_PATCH_LEN] = OPCODE_JMP_REL32;
    int32_t back = (int32_t)((uintptr_t)(target + MARKER_PATCH_LEN) - ((uintptr_t)tramp + MARKER_PATCH_LEN + JMP_LEN));
    memcpy(tramp + MARKER_PATCH_LEN + 1, &back, 4);
    DWORD old;
    if (!VirtualProtect(target, MARKER_PATCH_LEN, PAGE_EXECUTE_READWRITE, &old)) return NULL;
    int32_t rel = (int32_t)((uintptr_t)hook - ((uintptr_t)target + JMP_LEN));
    target[0] = OPCODE_JMP_REL32;
    memcpy(target + 1, &rel, 4);
    target[JMP_LEN] = OPCODE_NOP;
    VirtualProtect(target, MARKER_PATCH_LEN, old, &old);
    FlushInstructionCache(GetCurrentProcess(), target, MARKER_PATCH_LEN);
    return tramp;
}

int headpose_markers_install(uintptr_t delta, HeadPoseSource source) {
    int n = 0;
    g_source = source;
    if ((g_w2s_tramp = (WorldToScreenFn)install_one((uint8_t *)(VA_CAM_WORLD_TO_SCREEN + delta), (const void *)hook_w2s,
                                                    "WorldToScreen"))) n++;
    if ((g_vis_tramp = (SphereVisibleFn)install_one((uint8_t *)(VA_CAM_SPHERE_VISIBLE + delta), (const void *)hook_vis,
                                                    "SphereVisible"))) n++;
    log_msg("HEADPOSE markers: %d of 2 hooks in (WorldToScreen, SphereVisible). Numpad . switches them off/on.", n);
    return n;
}

void headpose_markers_toggle(void) {
    g_enabled = !g_enabled;
    log_msg("HEADPOSE markers: %s (so far %lu marker points and %lu culling tests moved to the head)",
            g_enabled ? "ON" : "OFF", g_adjusted_w2s, g_adjusted_vis);
}
