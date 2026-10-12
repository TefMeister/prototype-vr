/* headpose_xr_live_test.c - runs the SHIPPED headpose_xr.c against a real OpenXR runtime (the simulator on the dev
 * PC), in a 32-bit program that is not the game (2026-10-12, /pd).
 *
 *   headpose_xr_live_test.exe <seconds>
 * Needs prototype_vr.ini ([vr] RuntimeJson + LoaderPath) beside the exe; it creates prototype_headset.txt itself,
 * polls headpose_xr_poll() like the game thread does, and prints the head's forward axis in camera coordinates once
 * a second. Turning the simulator's head while it runs shows which way the pose turns. Exit 0 = poses arrived. */
#include <windows.h>
#include <stdarg.h>
#include <stdio.h>
#include "../src/headpose_xr.h"

#define POLL_MS      10
#define PRINT_EVERY  1000u

void log_msg(const char *fmt, ...) {
    va_list a;
    va_start(a, fmt);
    printf("  log: ");
    vprintf(fmt, a);
    printf("\n");
    fflush(stdout);
    va_end(a);
}

int main(int argc, char **argv) {
    int secs = argc > 1 ? atoi(argv[1]) : 6;
    char dir[MAX_PATH], sw[MAX_PATH];
    GetModuleFileNameA(NULL, dir, MAX_PATH);
    *strrchr(dir, '\\') = 0;
    _snprintf(sw, MAX_PATH, "%s\\prototype_headset.txt", dir);
    CloseHandle(CreateFileA(sw, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL));
    unsigned long got = 0;
    DWORD t0 = GetTickCount(), last = 0;
    while (GetTickCount() - t0 < (DWORD)secs * 1000) {
        HeadPose h;
        if (headpose_xr_poll(&h)) {
            got++;
            if (GetTickCount() - last >= PRINT_EVERY) {
                printf("t=%5.1f s  forward (%+.3f %+.3f %+.3f)  up (%+.3f %+.3f %+.3f)  offset %+.3f %+.3f %+.3f m\n",
                       (GetTickCount() - t0) / 1000.0, h.rot[6], h.rot[7], h.rot[8], h.rot[3], h.rot[4], h.rot[5],
                       h.x_m, h.y_m, h.z_m);
                fflush(stdout);
                last = GetTickCount();
            }
        }
        Sleep(POLL_MS);
    }
    DeleteFileA(sw);
    printf("RESULT: %lu headset poses read in %d s - %s\n", got, secs, got ? "the headset feed works outside the game" : "FAIL");
    fflush(stdout);
    ExitProcess(got ? 0 : 1);   /* the headset thread is left to process exit, as in the game */
}
