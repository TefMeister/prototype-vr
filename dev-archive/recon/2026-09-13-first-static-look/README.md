# First static look (2026-09-13)

Read from the installed Steam copy on the home PC, without launching the game. Every claim
below is `[inferred-static 2026-09-13]` unless tagged otherwise: it comes from reading file headers
and strings, not from running anything.

- **Install:** `Prototype`, 7.9 GB.
- **Identity:** Prototype, Steam build. `prototypef.exe` is a small launcher stub; the game code is in `prototypeenginef.dll`. Both are linked 2025-08-14, so this is a recent rebuild, not the 2009 binary `[inferred-static 2026-09-13]`.
- **Engine:** Radical Entertainment's Titanium engine `[reported]`. Scaleform GFx and Lua strings are present in the engine DLL `[inferred-static 2026-09-13]`.
- **Binary:** **32-bit**. `prototypef.exe` 2.5 MB: almost all resources plus a `.bind` section. `prototypeenginef.dll` 20.2 MB: plain sections, relocations present `[inferred-static 2026-09-13]`.
- **Renderer:** Direct3D 9: `d3d9.dll` in the engine DLL's strings; a DirectX redistributable ships in `DirectX\` `[inferred-static 2026-09-13]`.
- **Protection:** The launcher stub carries the Steam DRM wrapper section (`.bind`); the engine DLL itself shows no protection `[inferred-static 2026-09-13]`. Not tested live.
- **Other files:** `.rcf` archives and `.p3d` files (Radical's Pure3D format `[reported]`), not yet looked at.

## Method

PE headers read with a short script: machine type, link timestamp, section names and sizes.
Then a case-insensitive search of each binary for renderer DLL names (`d3d9`, `d3d11`, `d3d12`,
`dxgi`, `vulkan-1`, `opengl32`), protection markers (`denuvo`, `securom`, `.bind`) and middleware
names. A string match shows a name is present in the file, not that the code path is used.

## Risks noted

- Nothing blocking seen yet. Because the real code sits in an unprotected DLL, static reading should be straightforward.
