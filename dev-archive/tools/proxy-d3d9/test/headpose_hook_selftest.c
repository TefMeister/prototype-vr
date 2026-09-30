/* headpose_hook_selftest.c - prove the 5-byte jump and its trampoline work, WITHOUT the game.
 *
 * A stand-in for 0x107545a0 is assembled into executable memory with the SAME first five bytes
 * (mov eax,[esp+8]; push esi) and the same shape (stdcall View, Camera; ret 8); it stores the camera
 * pointer at View+0, the way the real one hands it to View::SetCamera. A second stub calls it the way
 * ViewPass::Render does, so its return address plays the part of 0x10768738. Then:
 *   1. the jump is installed (and refused on wrong bytes);
 *   2. before any pose is set, a call through the stub leaves the slot alone and the original still runs;
 *   3. with a pose set, the stub's call writes inv(C) H C into View+0x2c and the original still runs;
 *   4. a call from anywhere else, or with a non-scene camera, leaves the slot alone.
 * Build (32-bit, like the game), from proxy-d3d9/:
 *   i686-w64-mingw32-clang -O2 -o hook_st.exe test/headpose_hook_selftest.c src/headpose.c src/headpose_math.c
 * Exit 0 = pass.
 */
#include <windows.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "../src/headpose.h"

void log_msg(const char *fmt, ...) { (void)fmt; }

typedef void (__stdcall *SetCameraFn)(void *view, void *camera);
typedef void (__cdecl *CallerFn)(void *view, void *camera);

static int fails, checks;
static void check(int ok, const char *what) {
    checks++;
    if (!ok) { fails++; printf("FAIL: %s\n", what); } else printf("  ok    %s\n", what);
}

int main(void) {
    static const unsigned char fake[] = {
        0x8B, 0x44, 0x24, 0x08,   /* mov eax, [esp+8]   ; camera          */
        0x56,                     /* push esi                              */
        0x8B, 0x74, 0x24, 0x08,   /* mov esi, [esp+8]   ; view             */
        0x89, 0x06,               /* mov [esi], eax     ; view+0 = camera  */
        0x5E,                     /* pop esi                               */
        0xC2, 0x08, 0x00,         /* ret 8                                 */
    };
    unsigned char *code = VirtualAlloc(NULL, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    unsigned char *target = code, *caller = code + 64;
    memcpy(target, fake, sizeof fake);
    /* caller(view, cam), cdecl: push cam; push view; call target; ret */
    unsigned char stub[] = {0xFF, 0x74, 0x24, 0x08, 0xFF, 0x74, 0x24, 0x08, 0xE8, 0, 0, 0, 0, 0xC3};
    int rel = (int)(target - (caller + 13));
    memcpy(stub + 9, &rel, 4);
    memcpy(caller, stub, sizeof stub);
    uintptr_t render_return = (uintptr_t)(caller + 13);

    float cam[64] = {0}, view[64];
    cam[0x20 / 4] = 0.3f;
    cam[0x24 / 4] = 7500.0f;
    cam[0x14 / 4] = 1.396f;
    /* camera->world: turned 30 degrees, 2 km from the origin */
    float c = cosf(0.5236f), s = sinf(0.5236f);
    float to_world[16] = {c, 0, -s, 0,  0, 1, 0, 0,  s, 0, c, 0,  1834.5f, 22.7f, -1422.25f, 1};
    memcpy(&cam[0x90 / 4], to_world, sizeof to_world);
    float ident[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};

    printf("head-pose hook self-test - no game\n");
    unsigned char wrong[8] = {0x55, 0x8B, 0xEC, 0, 0, 0, 0, 0};
    check(!headpose_install_at(wrong, render_return), "refuses a function with different first bytes");
    check(headpose_install_at(target, render_return), "installs on the expected first bytes");
    check(target[0] == 0xE9, "the jump is in place");

    memset(view, 0, sizeof view);
    memcpy(&view[0x2c / 4], ident, sizeof ident);
    ((CallerFn)caller)(view, cam);
    check(*(void **)view == (void *)cam, "no pose yet: the original still ran (View+0 = camera)");
    check(!memcmp(&view[0x2c / 4], ident, sizeof ident), "no pose yet: the slot is untouched");

    HeadPose h = {0.1745, 0, 0.5, 0, 0};    /* 10 degrees and half a metre, as in the live test */
    float want[16];
    headpose_slot(to_world, &h, want);
    headpose_set_pose(&h);
    memset(view, 0, sizeof view);
    ((CallerFn)caller)(view, cam);
    check(*(void **)view == (void *)cam, "with a pose: the original still ran");
    check(!memcmp(&view[0x2c / 4], want, sizeof want), "with a pose: the slot holds inv(C) H C");

    memset(view, 0, sizeof view);
    ((SetCameraFn)target)(view, cam);          /* called from here, not from "Render" */
    float zero[16] = {0};
    check(*(void **)view == (void *)cam, "other caller: the original still ran");
    check(!memcmp(&view[0x2c / 4], zero, sizeof zero), "other caller: the slot is untouched");

    cam[0x24 / 4] = 60.0f;                       /* a different camera (not near 0.3 / far 7500) */
    memset(view, 0, sizeof view);
    ((CallerFn)caller)(view, cam);
    check(!memcmp(&view[0x2c / 4], zero, sizeof zero), "non-scene camera: the slot is untouched");

    printf("checks %d, failures %d\n%s\n", checks, fails, fails ? "RESULT: FAIL" : "RESULT: PASS");
    return fails ? 1 : 0;
}
