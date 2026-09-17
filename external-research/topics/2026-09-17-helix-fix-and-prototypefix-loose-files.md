# Prototype has a HeliX 3D Vision fix, and PrototypeFix lets loose files override the RCF archives

**Status:** 🆕 new · **Priority:** medium.

## What is public

- Prototype had strong **3D Vision** support, limited to low depth by its 2D crosshair and markers;
  **HeliX's fix** pushes HUD elements, enemy markers and health bars to depth `[reported]`.
- **PrototypeFix** (Nexus mod 52; crash, FPS and frame-doubling fixes) also makes **loose or modded
  files take priority over the files inside `.rcf` archives**, so modded files load without unpacking
  `[reported]`.
- Audio tools (EDITP3D, RADPTOOL) handle `00audio.rcf` `[reported]`.

## Why it matters here

1. The board's `[PD]` row "find where the bulk of the shaders live" meets PrototypeFix's loader:
   whatever it hooks to prefer loose files also reveals how the game resolves archive paths
   `[hypothesis]`.
2. HeliX's HUD-depth fix edits the shaders that draw screen-space markers — the same ones that need
   special handling per eye in VR.

## Next step

Read PrototypeFix's description and the HeliX page's file list; check whether either names shader files.

## Sources

- Helix Mod, Prototype — <https://helixmod.blogspot.com/2012/03/prototype.html>
- PrototypeFix — <https://www.nexusmods.com/prototype/mods/52>
- P3D audio tool — <https://www.nexusmods.com/prototype/mods/110>
