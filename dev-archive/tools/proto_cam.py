"""proto_cam.py - find Prototype's rendered camera in memory and poke its head-pose slot. No debugger.

The path (static reader, 2026-09-29, ENGINE-DOSSIER section 6): every pure3d::ViewPass (vtable 0x10e5f6bc) holds a
View at +0xbc (vtable 0x10e5da10); the View holds the Camera it last rendered at +0x18 (vtable 0x10e54e94). The
camera's lens: +0x14 hFOV rad, +0x18 vFOV, +0x1c aspect, +0x20 near, +0x24 far; +0x90 camera->world. The View's own
4x4 at +0x2c is identity by default and is multiplied into the view every frame: the head-pose slot under test.

usage:
    proto_cam.py find                         list ViewPass -> View -> Camera chains (near/far/FOV, View+0x2c)
    proto_cam.py poke VIEW_HEX x y z [yawdeg] write a translation (and optional yaw) into View+0x2c
    proto_cam.py reset VIEW_HEX               put View+0x2c back to identity
"""
import ctypes
import ctypes.wintypes as w
import math
import struct
import subprocess
import sys

# ---- Settings -----------------------------------------------------------------
ENGINE_DLL = "prototypeenginef.dll"
STATIC_BASE = 0x10000000
VT_VIEWPASS, VT_VIEW, VT_CAMERA = 0x10E5F6BC, 0x10E5DA10, 0x10E54E94
OFF_PASS_VIEW, OFF_VIEW_CAM, OFF_VIEW_POSE = 0xBC, 0x18, 0x2C
OFF_CAM_FOV, OFF_CAM_NEAR, OFF_CAM_FAR, OFF_CAM_TOWORLD = 0x14, 0x20, 0x24, 0x90
SCAN_CHUNK = 1 << 20

k32, psapi = ctypes.windll.kernel32, ctypes.windll.psapi
k32.OpenProcess.restype = w.HANDLE
k32.VirtualQueryEx.argtypes = [w.HANDLE, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_size_t]
k32.ReadProcessMemory.argtypes = [w.HANDLE, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_size_t, ctypes.c_void_p]
k32.WriteProcessMemory.argtypes = [w.HANDLE, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_size_t, ctypes.c_void_p]


class MBI(ctypes.Structure):
    _fields_ = [("BaseAddress", ctypes.c_void_p), ("AllocationBase", ctypes.c_void_p),
                ("AllocationProtect", w.DWORD), ("PartitionId", w.WORD), ("RegionSize", ctypes.c_size_t),
                ("State", w.DWORD), ("Protect", w.DWORD), ("Type", w.DWORD)]


def open_game():
    out = subprocess.check_output(["tasklist", "/FI", "IMAGENAME eq prototypef.exe", "/FO", "CSV", "/NH"], text=True)
    pid = int(out.strip().split(",")[1].strip('"'))
    h = k32.OpenProcess(0x0010 | 0x0020 | 0x0008 | 0x0400, False, pid)
    mods, need = (w.HMODULE * 1024)(), w.DWORD()
    psapi.EnumProcessModulesEx(h, mods, ctypes.sizeof(mods), ctypes.byref(need), 0x01)
    base = None
    for m in mods[: need.value // ctypes.sizeof(w.HMODULE)]:
        name = ctypes.create_unicode_buffer(260)
        psapi.GetModuleFileNameExW(h, m, name, 260)
        if name.value.lower().endswith(ENGINE_DLL):
            base = m
    return h, base


def rd(h, addr, n):
    buf = ctypes.create_string_buffer(n)
    got = ctypes.c_size_t()
    if not k32.ReadProcessMemory(h, ctypes.c_void_p(addr), buf, n, ctypes.byref(got)) or got.value != n:
        return None
    return buf.raw


def u32(h, a):
    b = rd(h, a, 4)
    return struct.unpack("<I", b)[0] if b else None


def f32s(h, a, n):
    b = rd(h, a, 4 * n)
    return list(struct.unpack("<%df" % n, b)) if b else None


def scan_dword(h, value):
    """Every 4-aligned address in private committed memory holding `value`."""
    hits, addr, mbi, pat = [], 0, MBI(), struct.pack("<I", value)
    while addr < 0x7FFF0000 and k32.VirtualQueryEx(h, ctypes.c_void_p(addr), ctypes.byref(mbi), ctypes.sizeof(mbi)):
        size = mbi.RegionSize or 0x1000
        if mbi.State == 0x1000 and mbi.Type == 0x20000 and mbi.Protect in (0x04, 0x40):   # committed, private, RW
            off = 0
            while off < size:
                n = min(SCAN_CHUNK, size - off)
                data = rd(h, (mbi.BaseAddress or 0) + off, n)
                if data:
                    i = data.find(pat)
                    while i != -1:
                        if i % 4 == 0:
                            hits.append((mbi.BaseAddress or 0) + off + i)
                        i = data.find(pat, i + 1)
                off += n
        addr = (mbi.BaseAddress or 0) + size
    return hits


def find(h, base):
    d = base - STATIC_BASE
    rows = []
    for vp in scan_dword(h, VT_VIEWPASS + d):
        view = u32(h, vp + OFF_PASS_VIEW)
        if not view or u32(h, view) != VT_VIEW + d:
            continue
        cam = u32(h, view + OFF_VIEW_CAM)
        if not cam or u32(h, cam) != VT_CAMERA + d:
            continue
        fov, near, far = f32s(h, cam + OFF_CAM_FOV, 1)[0], f32s(h, cam + OFF_CAM_NEAR, 1)[0], f32s(h, cam + OFF_CAM_FAR, 1)[0]
        pose = f32s(h, view + OFF_VIEW_POSE, 16)
        rows.append(dict(viewpass=hex(vp), view=hex(view), camera=hex(cam), hfov_deg=round(math.degrees(fov), 2),
                         near=round(near, 3), far=round(far, 1),
                         scene_camera=abs(near - 0.3) < 1e-3 and abs(far - 7500) < 1,
                         view_pose_identity=all(abs(pose[i] - (1.0 if i % 5 == 0 else 0.0)) < 1e-5 for i in range(16)),
                         cam_pos=[round(x, 2) for x in f32s(h, cam + OFF_CAM_TOWORLD + 48, 3)]))
    return rows


def write_pose(h, view, m):
    buf = struct.pack("<16f", *m)
    ok = k32.WriteProcessMemory(h, ctypes.c_void_p(view + OFF_VIEW_POSE), buf, len(buf), None)
    return bool(ok), f32s(h, view + OFF_VIEW_POSE, 16)


def main():
    h, base = open_game()
    if not base:
        print("engine DLL not found"); sys.exit(1)
    print("engine base", hex(base))
    cmd = sys.argv[1] if len(sys.argv) > 1 else "find"
    if cmd == "find":
        for r in find(h, base):
            print(r)
    elif cmd == "head":
        # The slot applies in WORLD space AFTER camera->world (measured 2026-09-29: a raw 10 deg yaw swung the camera
        # about the world origin, onto another street). So a head pose H given in CAMERA space is conjugated:
        # slot = inv(C) . H . C, with C = cam+0x90, row-vector convention (p' = p . M).
        import numpy as np
        view = int(sys.argv[2], 16)
        x, y, z = (float(v) for v in sys.argv[3:6])
        yaw = math.radians(float(sys.argv[6])) if len(sys.argv) > 6 else 0.0
        cam = u32(h, view + OFF_VIEW_CAM)
        C = np.array(f32s(h, cam + OFF_CAM_TOWORLD, 16), dtype=float).reshape(4, 4)
        c, s = math.cos(yaw), math.sin(yaw)
        H = np.array([[c, 0, -s, 0], [0, 1, 0, 0], [s, 0, c, 0], [x, y, z, 1]], dtype=float)
        M = np.linalg.inv(C) @ H @ C
        ok, now = write_pose(h, view, [float(v) for v in M.reshape(16)])
        print("wrote" if ok else "WRITE FAILED", "camera-space head pose; slot =", [round(v, 3) for v in now])
    elif cmd in ("poke", "reset"):
        view = int(sys.argv[2], 16)
        m = [1.0, 0, 0, 0, 0, 1.0, 0, 0, 0, 0, 1.0, 0, 0, 0, 0, 1.0]
        if cmd == "poke":
            x, y, z = (float(v) for v in sys.argv[3:6])
            yaw = math.radians(float(sys.argv[6])) if len(sys.argv) > 6 else 0.0
            c, s = math.cos(yaw), math.sin(yaw)
            m[0], m[2], m[8], m[10] = c, -s, s, c           # rotation about Y, row-vector convention
            m[12], m[13], m[14] = x, y, z                   # translation row
        ok, now = write_pose(h, view, m)
        print("wrote" if ok else "WRITE FAILED", [round(v, 3) for v in now])


if __name__ == "__main__":
    main()
