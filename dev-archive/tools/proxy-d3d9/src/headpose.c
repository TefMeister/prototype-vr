/* headpose.c - the per-frame head-pose writer (board row 2026-09-29, built 2026-09-30 by /pd, NOT yet run).
 *
 * Every frame pure3d::ViewPass::Render (0x10768670) calls 0x107545a0(View, Camera), a 2-argument stdcall
 * that stores the camera in the View and runs View::SetupCamera, which builds the view from
 * Invert(cam+0x90 x View+0x2c). This file puts a 5-byte jump on 0x107545a0 and, for calls coming from
 * ViewPass::Render with the scene camera (near 0.3, far 7500), writes inv(C) x H x C into View+0x2c just
 * before SetupCamera reads it (headpose_math.c). H comes from the number pad for now; a headset replaces
 * it later.
 *
 * Nothing is written until a head key is first pressed, so the game is untouched by default.
 *
 * Keys (the number pad is unbound in this game; NumLock must be on):
 *   4 / 6  turn left / right  (YAW_STEP_DEG)        8 / 2  look up / down (PITCH_STEP_DEG)
 *   1 / 3  move left / right  (MOVE_STEP_M)         7 / 9  move down / up (MOVE_STEP_M)
 *   5      back to no head pose                     0      which Views get the pose: all, then each alone
 *
 * Which of the three Views is on screen is not known statically; every View seen is numbered in the
 * log the first time it appears, and numpad 0 narrows the pose to one of them to tell them apart.
 */
#include <windows.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include "headpose.h"
#include "headpose_math.h"

void log_msg(const char *fmt, ...);     /* proxy.c */

/* ---- Settings ------------------------------------------------------------------------------------- */
#define ENGINE_DLL          "prototypeenginef.dll"
#define ENGINE_STATIC_BASE  0x10000000u
#define VA_VIEW_SET_CAMERA  0x107545a0u   /* stdcall (View *, Camera *), ret 8                          */
#define VA_RENDER_RETURN    0x10768738u   /* the return address inside pure3d::ViewPass::Render          */
#define OFF_VIEW_POSE       0x2c          /* View: the 4x4 multiplied with cam+0x90 in SetupCamera       */
#define OFF_CAM_NEAR        0x20
#define OFF_CAM_FAR         0x24
#define OFF_CAM_HFOV        0x14
#define OFF_CAM_TO_WORLD    0x90
#define SCENE_NEAR          0.3f
#define SCENE_FAR           7500.0f
#define YAW_STEP_DEG        5.0
#define PITCH_STEP_DEG      5.0
#define MOVE_STEP_M         0.10
#define KEY_POLL_MS         30u
#define MAX_VIEWS           8
#define PATCH_LEN           5
static const uint8_t EXPECTED_PROLOGUE[PATCH_LEN] = {0x8B, 0x44, 0x24, 0x08, 0x56};  /* mov eax,[esp+8]; push esi */

/* ---- State ---------------------------------------------------------------------------------------- */
typedef void (__stdcall *SetCameraFn)(void *view, void *camera);
static SetCameraFn g_trampoline;
static uintptr_t   g_render_return;
static HeadPose    g_pose;
static int         g_active;            /* set by the first head key; nothing is written before it */
static int         g_target;            /* 0 = every View, k = only the k-th View seen             */
static void       *g_views[MAX_VIEWS];
static int         g_view_count;
static unsigned long g_writes, g_skipped;

static int view_number(void *view, void *cam) {
    for (int i = 0; i < g_view_count; i++)
        if (g_views[i] == view) return i + 1;
    if (g_view_count >= MAX_VIEWS) return 0;
    g_views[g_view_count++] = view;
    log_msg("HEADPOSE: scene View #%d = %p (camera %p, hfov %.2f deg)", g_view_count, view, cam,
            *(float *)((char *)cam + OFF_CAM_HFOV) * 57.29578f);
    return g_view_count;
}

static void log_pose(const char *why) {
    log_msg("HEADPOSE: %s -> yaw %+.1f deg, pitch %+.1f deg, offset (%+.2f, %+.2f, %+.2f) m, applied to %s%d "
            "(%d Views seen, %lu writes, %lu skipped so far)", why, g_pose.yaw_rad * 57.29578,
            g_pose.pitch_rad * 57.29578, g_pose.x_m, g_pose.y_m, g_pose.z_m,
            g_target ? "View #" : "every View, count ", g_target ? g_target : g_view_count, g_view_count,
            g_writes, g_skipped);
}

static void poll_keys(void) {
    static DWORD last;
    static SHORT was[10];
    static const struct { int vk; const char *what; } keys[10] = {
        {VK_NUMPAD0, "numpad 0: next View choice"}, {VK_NUMPAD1, "numpad 1: move left"},
        {VK_NUMPAD2, "numpad 2: look down"},       {VK_NUMPAD3, "numpad 3: move right"},
        {VK_NUMPAD4, "numpad 4: turn left"},       {VK_NUMPAD5, "numpad 5: reset"},
        {VK_NUMPAD6, "numpad 6: turn right"},      {VK_NUMPAD7, "numpad 7: move down"},
        {VK_NUMPAD8, "numpad 8: look up"},         {VK_NUMPAD9, "numpad 9: move up"},
    };
    DWORD now = GetTickCount();
    if (now - last < KEY_POLL_MS) return;
    last = now;
    for (int k = 0; k < 10; k++) {
        SHORT down = (GetAsyncKeyState(keys[k].vk) & 0x8000) != 0;
        if (down && !was[k]) {
            double yaw = YAW_STEP_DEG / 57.29578, pitch = PITCH_STEP_DEG / 57.29578;
            switch (k) {
            case 0: g_target = g_target >= g_view_count ? 0 : g_target + 1; break;
            case 1: g_pose.x_m -= MOVE_STEP_M; break;
            case 2: g_pose.pitch_rad -= pitch; break;
            case 3: g_pose.x_m += MOVE_STEP_M; break;
            case 4: g_pose.yaw_rad -= yaw; break;
            case 5: memset(&g_pose, 0, sizeof g_pose); break;
            case 6: g_pose.yaw_rad += yaw; break;
            case 7: g_pose.y_m -= MOVE_STEP_M; break;
            case 8: g_pose.pitch_rad += pitch; break;
            case 9: g_pose.y_m += MOVE_STEP_M; break;
            }
            g_active = 1;
            log_pose(keys[k].what);
        }
        was[k] = down;
    }
}

static int is_scene_camera(void *cam) {
    float n = *(float *)((char *)cam + OFF_CAM_NEAR), f = *(float *)((char *)cam + OFF_CAM_FAR);
    return fabsf(n - SCENE_NEAR) < 1e-3f && fabsf(f - SCENE_FAR) < 1.0f;
}

static void __stdcall hook_set_camera(void *view, void *cam) {
    if ((uintptr_t)__builtin_return_address(0) == g_render_return && view && cam && is_scene_camera(cam)) {
        poll_keys();
        int number = view_number(view, cam);
        if (g_active) {
            static const HeadPose none;
            const HeadPose *h = g_target == 0 || g_target == number ? &g_pose : &none;
            float slot[16];
            if (headpose_slot((const float *)((char *)cam + OFF_CAM_TO_WORLD), h, slot)) {
                memcpy((char *)view + OFF_VIEW_POSE, slot, sizeof slot);
                g_writes++;
            } else {
                g_skipped++;
            }
        }
    }
    g_trampoline(view, cam);
}

static void write_jump(uint8_t *at, const void *to) {
    int32_t rel = (int32_t)((uintptr_t)to - ((uintptr_t)at + 5));
    at[0] = 0xE9;
    memcpy(at + 1, &rel, 4);
}

void headpose_set_pose(const HeadPose *h) {
    g_pose = *h;
    g_active = 1;
}

int headpose_install_at(void *at, uintptr_t render_return) {
    uint8_t *target = at;
    g_render_return = render_return;
    if (memcmp(target, EXPECTED_PROLOGUE, PATCH_LEN) != 0) {
        log_msg("HEADPOSE: REFUSING - the bytes at %p are %02X %02X %02X %02X %02X, not the expected prologue "
                "(a different game build, or someone else hooked it). The game runs unchanged.", target,
                target[0], target[1], target[2], target[3], target[4]);
        return 0;
    }
    uint8_t *tramp = VirtualAlloc(NULL, 16, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!tramp) { log_msg("HEADPOSE: VirtualAlloc failed (%lu) - not installed", GetLastError()); return 0; }
    memcpy(tramp, target, PATCH_LEN);            /* the two whole instructions we overwrite */
    write_jump(tramp + PATCH_LEN, target + PATCH_LEN);
    g_trampoline = (SetCameraFn)tramp;
    DWORD old;
    if (!VirtualProtect(target, PATCH_LEN, PAGE_EXECUTE_READWRITE, &old)) {
        log_msg("HEADPOSE: VirtualProtect failed (%lu) - not installed", GetLastError());
        return 0;
    }
    write_jump(target, (const void *)hook_set_camera);
    VirtualProtect(target, PATCH_LEN, old, &old);
    FlushInstructionCache(GetCurrentProcess(), target, PATCH_LEN);
    return 1;
}

void headpose_install(void) {
    static int tried;
    if (tried) return;
    tried = 1;
    HMODULE engine = GetModuleHandleA(ENGINE_DLL);
    if (!engine) { log_msg("HEADPOSE: %s is not loaded - not installed", ENGINE_DLL); return; }
    uintptr_t delta = (uintptr_t)engine - ENGINE_STATIC_BASE;
    if (headpose_install_at((void *)(VA_VIEW_SET_CAMERA + delta), VA_RENDER_RETURN + delta))
        log_msg("HEADPOSE: installed on %p (engine at %p). Numpad 4/6 turn, 8/2 look, 1/3 and 7/9 move, 5 reset, "
                "0 picks which View. Nothing is written until the first key.",
                (void *)(VA_VIEW_SET_CAMERA + delta), (void *)engine);
}
