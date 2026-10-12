/* headpose_xr.c - the headset drives Prototype's head pose (2026-10-12, /pd). NOT yet run in the game.
 *
 * While prototype_headset.txt exists next to prototypef.exe, a HEADSET THREAD (ours; the only thread that calls xr*):
 *   1. checks the process's msvcp140.dll is new enough (Heavy Rain, 2026-10-12: a game's own 14.16 copy closed it
 *      silently inside xrCreateInstance), then loads the 32-bit OpenXR loader ([vr] LoaderPath, default next to the
 *      exe) for the runtime in [vr] RuntimeJson (this process only; empty = the system's active runtime),
 *   2. makes its OWN D3D11 device on the runtime's adapter and a session on it (the game is D3D9, so the session can
 *      never be the game's; same shape as Alan Wake's and Heavy Rain's),
 *   3. runs empty frames so the runtime keeps the session going, and each frame locates VIEW in LOCAL,
 *   4. turns that into a HeadPose (headpose_xr_math.c) and publishes it; headpose.c writes it into the View's slot
 *      on the game thread, exactly where the numpad pose goes today (seen working live 2026-09-30).
 * The same steps ran as a 32-bit test program with the simulator's 32-bit runtime: 234 frames, 234 head poses in
 * 4 s [measured 2026-10-12, n=1] (heavy-rain-vr/proxy-d3d11/tests/xr_host.c, built for i686).
 * Recentre: numpad 5 (while the headset is the source). The thread stays once started; removing the file only
 * hands the pose back to the keys. */
#define COBJMACROS
#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <stdio.h>
#include <string.h>
#define XR_USE_PLATFORM_WIN32
#define XR_USE_GRAPHICS_API_D3D11
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

#include "headpose_xr.h"
#include "headpose_xr_math.h"
#define JITTERLOG_IMPLEMENTATION
#include "jitterlog.h"           /* BeG0nE jitter recorder: writes only while the session runs */

void log_msg(const char *fmt, ...);     /* proxy.c */

/* ---- Settings ------------------------------------------------------------------------------------- */
#define XR_SWITCH_FILE     "prototype_headset.txt"    /* present = the headset drives the head pose      */
#define XR_INI             "prototype_vr.ini"         /* [vr] RuntimeJson, LoaderPath, Position, AllowOldCppRuntime */
#define XR_INI_SECTION     "vr"
#define XR_LOADER_NAME     "openxr_loader.dll"        /* default: next to the exe (32-bit)               */
#define XR_APP_NAME        "prototype-vr"
#define XR_SWITCH_POLL_MS  250u                       /* how often the game thread looks for the file    */
#define XR_IDLE_MS         10u                        /* headset thread sleep while the session is not running */
#define XR_POSE_LOG_MS     1000u                      /* head pose in the log at most this often         */
#define XR_MIN_CPP_MAJOR   14                         /* runtimes built with VS 2022 17.10+ need 14.40+  */
#define XR_MIN_CPP_MINOR   40
#define XR_RAD2DEG         57.29578f
#define XR_PUBLISH_TRIES   8                          /* reader retries while the headset thread writes  */

enum { XR_OFF = 0, XR_LOADING, XR_RUNNING, XR_FAILED };

/* ---- State ---------------------------------------------------------------------------------------- */
static volatile LONG g_stage, g_want, g_recentre_req, g_seq;
static HeadPose g_pub;                      /* seqlock-published: g_seq odd while being written */
static char g_dir[MAX_PATH];
static int g_position = 1;

static PFN_xrGetInstanceProcAddr p_gipa;
#define XRF(name) static PFN_##name p_##name
XRF(xrCreateInstance); XRF(xrGetSystem); XRF(xrCreateSession); XRF(xrCreateReferenceSpace); XRF(xrPollEvent);
XRF(xrBeginSession); XRF(xrEndSession); XRF(xrWaitFrame); XRF(xrBeginFrame); XRF(xrEndFrame); XRF(xrLocateSpace);
XRF(xrGetD3D11GraphicsRequirementsKHR);

/* headset-thread only */
static XrInstance g_xi;
static XrSystemId g_xsys;
static XrSession g_xs;
static XrSpace g_local, g_view;
static int g_xrun;
static LUID g_xluid;
static D3D_FEATURE_LEVEL g_xfl;
static ID3D11Device *g_xdev;
static ID3D11DeviceContext *g_xctx;

static void ini_path(char *out) { _snprintf(out, MAX_PATH, "%s\\%s", g_dir, XR_INI); out[MAX_PATH - 1] = 0; }

static int ini_str(const char *key, char *out, DWORD len) {
    char ini[MAX_PATH];
    ini_path(ini);
    out[0] = 0;
    GetPrivateProfileStringA(XR_INI_SECTION, key, "", out, len, ini);
    return out[0] != 0;
}

static int ini_int(const char *key, int fallback) {
    char ini[MAX_PATH];
    ini_path(ini);
    return (int)GetPrivateProfileIntA(XR_INI_SECTION, key, fallback, ini);
}

/* ---- Start-up, on the headset thread -------------------------------------------------------------- */
static int cpp_runtime_ok(void) {
    HMODULE m = GetModuleHandleA("msvcp140.dll");
    char path[MAX_PATH], buf[4096];
    VS_FIXEDFILEINFO *fi = NULL;
    UINT len = 0;
    DWORD h, n;
    if (!m) { log_msg("XR: no msvcp140.dll loaded yet - the runtime will bring its own"); return 1; }
    GetModuleFileNameA(m, path, MAX_PATH);
    n = GetFileVersionInfoSizeA(path, &h);
    if (!n || n > sizeof buf || !GetFileVersionInfoA(path, 0, n, buf) || !VerQueryValueA(buf, "\\", (void **)&fi, &len) || !fi) {
        log_msg("XR: msvcp140.dll in use: %s (version unreadable) - trying anyway", path);
        return 1;
    }
    unsigned maj = HIWORD(fi->dwFileVersionMS), min = LOWORD(fi->dwFileVersionMS);
    log_msg("XR: msvcp140.dll in use: %s, version %u.%u", path, maj, min);
    if (maj > XR_MIN_CPP_MAJOR || (maj == XR_MIN_CPP_MAJOR && min >= XR_MIN_CPP_MINOR)) return 1;
    if (ini_int("AllowOldCppRuntime", 0)) { log_msg("XR: older than %d.%d, AllowOldCppRuntime=1: trying anyway", XR_MIN_CPP_MAJOR, XR_MIN_CPP_MINOR); return 1; }
    log_msg("XR: STOPPED: that copy is older than %d.%d and would close the game inside the OpenXR runtime. Fix: move the "
            "game's own msvcp140.dll and vcruntime140.dll out of its folder", XR_MIN_CPP_MAJOR, XR_MIN_CPP_MINOR);
    return 0;
}

static int xr_fn(XrInstance inst, const char *name, PFN_xrVoidFunction *out) {
    return XR_SUCCEEDED(p_gipa(inst, name, out)) && *out;
}
#define XRLOAD(inst, name) xr_fn(inst, #name, (PFN_xrVoidFunction *)&p_##name)

static int start_runtime(void) {
    char rt[MAX_PATH], loader[MAX_PATH];
    XrResult r;
    if (!cpp_runtime_ok()) return 0;
    if (ini_str("RuntimeJson", rt, sizeof rt)) {
        SetEnvironmentVariableA("XR_RUNTIME_JSON", rt);             /* this process only */
        log_msg("XR: step 1: runtime for this game only: %s", rt);
    } else {
        log_msg("XR: step 1: no [vr] RuntimeJson in %s - the system's active OpenXR runtime", XR_INI);
    }
    if (!ini_str("LoaderPath", loader, sizeof loader)) { _snprintf(loader, MAX_PATH, "%s\\%s", g_dir, XR_LOADER_NAME); loader[MAX_PATH - 1] = 0; }
    HMODULE lib = LoadLibraryA(loader);
    if (!lib) { log_msg("XR: the 32-bit OpenXR loader was not found at %s (error %lu)", loader, GetLastError()); return 0; }
    p_gipa = (PFN_xrGetInstanceProcAddr)(void *)GetProcAddress(lib, "xrGetInstanceProcAddr");
    if (!p_gipa || !XRLOAD(XR_NULL_HANDLE, xrCreateInstance)) { log_msg("XR: the loader has no entry point"); return 0; }
    {
        const char *exts[1] = { XR_KHR_D3D11_ENABLE_EXTENSION_NAME };
        XrInstanceCreateInfo ici;
        memset(&ici, 0, sizeof ici);
        ici.type = XR_TYPE_INSTANCE_CREATE_INFO;
        strcpy(ici.applicationInfo.applicationName, XR_APP_NAME);
        ici.applicationInfo.apiVersion = XR_CURRENT_API_VERSION;
        ici.enabledExtensionCount = 1;
        ici.enabledExtensionNames = exts;
        log_msg("XR: step 2: xrCreateInstance ...");
        r = p_xrCreateInstance(&ici, &g_xi);
        if (XR_FAILED(r)) { log_msg("XR: xrCreateInstance failed (XrResult %d): no 32-bit D3D11 OpenXR runtime", (int)r); return 0; }
    }
    if (!(XRLOAD(g_xi, xrGetSystem) && XRLOAD(g_xi, xrCreateSession) && XRLOAD(g_xi, xrCreateReferenceSpace) &&
          XRLOAD(g_xi, xrPollEvent) && XRLOAD(g_xi, xrBeginSession) && XRLOAD(g_xi, xrEndSession) &&
          XRLOAD(g_xi, xrWaitFrame) && XRLOAD(g_xi, xrBeginFrame) && XRLOAD(g_xi, xrEndFrame) &&
          XRLOAD(g_xi, xrLocateSpace) && XRLOAD(g_xi, xrGetD3D11GraphicsRequirementsKHR))) {
        log_msg("XR: a required OpenXR function is missing");
        return 0;
    }
    {
        XrSystemGetInfo gi = { XR_TYPE_SYSTEM_GET_INFO };
        XrGraphicsRequirementsD3D11KHR req = { XR_TYPE_GRAPHICS_REQUIREMENTS_D3D11_KHR };
        gi.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
        r = p_xrGetSystem(g_xi, &gi, &g_xsys);
        if (XR_FAILED(r)) { log_msg("XR: no headset system (XrResult %d)", (int)r); return 0; }
        r = p_xrGetD3D11GraphicsRequirementsKHR(g_xi, g_xsys, &req);
        if (XR_FAILED(r)) { log_msg("XR: graphics requirements failed (XrResult %d)", (int)r); return 0; }
        g_xluid = req.adapterLuid;
        g_xfl = req.minFeatureLevel;
    }
    log_msg("XR: steps 2-4 ok: instance, headset system, D3D11 requirements");
    return 1;
}

static int make_device(void) {
    HMODULE d3d = LoadLibraryA("d3d11.dll"), dxgi = LoadLibraryA("dxgi.dll");   /* this proxy is d3d9, so no clash */
    PFN_D3D11_CREATE_DEVICE p_dev = d3d ? (PFN_D3D11_CREATE_DEVICE)(void *)GetProcAddress(d3d, "D3D11CreateDevice") : NULL;
    HRESULT (WINAPI *p_fac)(REFIID, void **) =
        dxgi ? (HRESULT (WINAPI *)(REFIID, void **))(void *)GetProcAddress(dxgi, "CreateDXGIFactory1") : NULL;
    IDXGIFactory1 *fac = NULL;
    IDXGIAdapter1 *ad = NULL, *chosen = NULL;
    D3D_FEATURE_LEVEL fl = g_xfl > D3D_FEATURE_LEVEL_11_0 ? g_xfl : D3D_FEATURE_LEVEL_11_0;
    if (!p_dev) { log_msg("XR: no D3D11CreateDevice"); return 0; }
    if (p_fac && SUCCEEDED(p_fac(&IID_IDXGIFactory1, (void **)&fac))) {
        for (UINT i = 0; IDXGIFactory1_EnumAdapters1(fac, i, &ad) != DXGI_ERROR_NOT_FOUND; ++i) {
            DXGI_ADAPTER_DESC1 d;
            IDXGIAdapter1_GetDesc1(ad, &d);
            if (!chosen && !memcmp(&d.AdapterLuid, &g_xluid, sizeof(LUID))) chosen = ad;
            else IDXGIAdapter1_Release(ad);
        }
        IDXGIFactory1_Release(fac);
    }
    HRESULT hr = p_dev((IDXGIAdapter *)chosen, chosen ? D3D_DRIVER_TYPE_UNKNOWN : D3D_DRIVER_TYPE_HARDWARE, NULL, 0,
                       &fl, 1, D3D11_SDK_VERSION, &g_xdev, NULL, &g_xctx);
    if (SUCCEEDED(hr)) log_msg("XR: step 5: own D3D11 device ok (adapter %s)", chosen ? "matched" : "default");
    else log_msg("XR: could not make the headset thread's D3D11 device (hr=0x%08lX)", (unsigned long)hr);
    if (chosen) IDXGIAdapter1_Release(chosen);
    return SUCCEEDED(hr);
}

static int create_session(void) {
    XrGraphicsBindingD3D11KHR bind = { XR_TYPE_GRAPHICS_BINDING_D3D11_KHR };
    XrSessionCreateInfo sci = { XR_TYPE_SESSION_CREATE_INFO };
    XrReferenceSpaceCreateInfo rs = { XR_TYPE_REFERENCE_SPACE_CREATE_INFO };
    XrResult r;
    bind.device = g_xdev;
    sci.next = &bind;
    sci.systemId = g_xsys;
    log_msg("XR: step 6: xrCreateSession ...");
    r = p_xrCreateSession(g_xi, &sci, &g_xs);
    if (XR_FAILED(r)) { log_msg("XR: xrCreateSession failed (XrResult %d)", (int)r); return 0; }
    rs.poseInReferenceSpace.orientation.w = 1.0f;
    rs.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
    if (XR_FAILED(p_xrCreateReferenceSpace(g_xs, &rs, &g_local))) { log_msg("XR: no LOCAL space"); return 0; }
    rs.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;
    if (XR_FAILED(p_xrCreateReferenceSpace(g_xs, &rs, &g_view))) { log_msg("XR: no VIEW space"); return 0; }
    log_msg("XR: session created; waiting for the runtime to start it");
    return 1;
}

static void poll_events(void) {
    XrEventDataBuffer ev;
    memset(&ev, 0, sizeof ev);
    ev.type = XR_TYPE_EVENT_DATA_BUFFER;
    while (p_xrPollEvent(g_xi, &ev) == XR_SUCCESS) {
        if (ev.type == XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED) {
            const XrEventDataSessionStateChanged *sc = (const XrEventDataSessionStateChanged *)&ev;
            if (sc->state == XR_SESSION_STATE_READY) {
                XrSessionBeginInfo bi = { XR_TYPE_SESSION_BEGIN_INFO };
                bi.primaryViewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
                if (XR_SUCCEEDED(p_xrBeginSession(g_xs, &bi))) { g_xrun = 1; jl_vr_on(); log_msg("XR: session running"); }
            } else if (sc->state == XR_SESSION_STATE_STOPPING) {
                p_xrEndSession(g_xs);
                g_xrun = 0;
                jl_vr_off();
                log_msg("XR: session stopped by the runtime");
            } else if (sc->state == XR_SESSION_STATE_EXITING || sc->state == XR_SESSION_STATE_LOSS_PENDING) {
                g_xrun = 0;
                jl_vr_off();
                InterlockedExchange(&g_stage, XR_FAILED);
                log_msg("XR: the runtime ended the session - the keys drive the head pose again");
            }
        }
        memset(&ev, 0, sizeof ev);
        ev.type = XR_TYPE_EVENT_DATA_BUFFER;
    }
}

static void publish(const HeadPose *h) {
    InterlockedIncrement(&g_seq);
    MemoryBarrier();
    g_pub = *h;
    MemoryBarrier();
    InterlockedIncrement(&g_seq);
}

/* One empty frame; the head at its predicted display time becomes the published HeadPose. */
static void frame(void) {
    static DWORD last_log;
    static unsigned long frames, poses;
    XrFrameWaitInfo fwi = { XR_TYPE_FRAME_WAIT_INFO };
    XrFrameState fs = { XR_TYPE_FRAME_STATE };
    XrFrameBeginInfo fbi = { XR_TYPE_FRAME_BEGIN_INFO };
    XrFrameEndInfo fei = { XR_TYPE_FRAME_END_INFO };
    XrSpaceLocation loc = { XR_TYPE_SPACE_LOCATION };
    if (XR_FAILED(p_xrWaitFrame(g_xs, &fwi, &fs))) { Sleep(XR_IDLE_MS); return; }
    p_xrBeginFrame(g_xs, &fbi);
    frames++;
    if (XR_SUCCEEDED(p_xrLocateSpace(g_view, g_local, fs.predictedDisplayTime, &loc)) &&
        (loc.locationFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT)) {
        float q[4] = { loc.pose.orientation.x, loc.pose.orientation.y, loc.pose.orientation.z, loc.pose.orientation.w };
        float p[3] = { loc.pose.position.x, loc.pose.position.y, loc.pose.position.z };
        int pos_ok = (loc.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT) != 0;
        HeadPose h;
        if (!pos_ok) p[0] = p[1] = p[2] = 0.0f;
        jl_pose_q(q[0], q[1], q[2], q[3], p[0], p[1], p[2]);
        if (InterlockedExchange(&g_recentre_req, 0)) hx_request_recentre();
        if (hx_head_from_xr(q, p, g_position && pos_ok, &h))
            log_msg("XR: RECENTRED: heading %.1f deg is now forward", hx_yaw_deg(q));
        publish(&h);
        poses++;
        if (GetTickCount() - last_log >= XR_POSE_LOG_MS) {
            log_msg("XR: head forward (%.3f %.3f %.3f) in camera axes, offset %+.3f %+.3f %+.3f m | frames %lu, poses %lu",
                    h.rot[6], h.rot[7], h.rot[8], h.x_m, h.y_m, h.z_m, frames, poses);
            last_log = GetTickCount();
        }
    }
    fei.displayTime = fs.predictedDisplayTime;
    fei.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
    p_xrEndFrame(g_xs, &fei);
    jl_present();
}

static DWORD WINAPI xr_thread(LPVOID unused) {
    (void)unused;
    g_position = ini_int("Position", 1) != 0;
    jl_init("prototype-vr", "openxr");
    log_msg("XR: headset thread started; head movement %s", g_position ? "moves the camera" : "ignored (turning only)");
    if (!start_runtime() || !make_device() || !create_session()) {
        InterlockedExchange(&g_stage, XR_FAILED);
        log_msg("XR: headset pose unavailable this run; the number pad still works");
        return 0;
    }
    InterlockedExchange(&g_stage, XR_RUNNING);
    while (g_stage == XR_RUNNING) {
        poll_events();
        if (!g_xrun) { Sleep(XR_IDLE_MS); continue; }
        frame();
    }
    log_msg("XR: headset thread stopped");
    return 0;   /* resources are left to process exit */
}

/* ---- Game thread ----------------------------------------------------------------------------------- */
void headpose_xr_recentre(void) { InterlockedExchange(&g_recentre_req, 1); }

/* The switch file appeared: face forward, and start the headset thread the first time. */
static void switched_on(void) {
    InterlockedExchange(&g_recentre_req, 1);                              /* every switch-on starts facing forward */
    if (InterlockedCompareExchange(&g_stage, XR_LOADING, XR_OFF) != XR_OFF) return;
    HANDLE t = CreateThread(NULL, 0, xr_thread, NULL, 0, NULL);
    if (t) { CloseHandle(t); return; }
    InterlockedExchange(&g_stage, XR_FAILED);
    log_msg("XR: could not start the headset thread");
}

/* At most every XR_SWITCH_POLL_MS: has the switch file come or gone? */
static void watch_switch_file(void) {
    static DWORD last;
    char path[MAX_PATH];
    DWORD now = GetTickCount();
    if (now - last < XR_SWITCH_POLL_MS) return;
    last = now;
    if (!g_dir[0]) {
        GetModuleFileNameA(NULL, g_dir, MAX_PATH);
        char *slash = strrchr(g_dir, '\\');
        if (slash) *slash = 0;
    }
    _snprintf(path, MAX_PATH, "%s\\%s", g_dir, XR_SWITCH_FILE);
    path[MAX_PATH - 1] = 0;
    LONG want = GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES;
    if (want == g_want) return;
    InterlockedExchange(&g_want, want);
    log_msg("XR: %s %s", XR_SWITCH_FILE, want ? "found - the headset drives the head pose" : "removed - the number pad drives it again");
    if (want) switched_on();
}

int headpose_xr_poll(HeadPose *out) {
    watch_switch_file();
    if (!g_want || g_stage != XR_RUNNING) return 0;
    for (int i = 0; i < XR_PUBLISH_TRIES; i++) {
        LONG s = g_seq;
        if (s == 0) return 0;
        if (s & 1) continue;
        MemoryBarrier();
        *out = g_pub;
        MemoryBarrier();
        if (g_seq == s) return 1;
    }
    return 0;
}
