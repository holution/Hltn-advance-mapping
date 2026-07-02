# HLTN Advanced Output — Project Context

## Project Vision
Mengembangkan **OBS Studio sebagai creative software untuk New Media Art**.
Dua plugin inti dalam monorepo `Hltn_Map/`:

| Plugin | Type | Path | Fungsi |
|--------|------|------|--------|
| **HLTN Mapping** | Filter | `obs-perspective-mesh/` | Perspective mesh warp filter untuk source video |
| **HLTN Advanced Output** | Output | `obs-hltn-advanced/` | Multi-display output dengan warp mesh projection mapping |

### Roadmap
- **Fase 1 (current):** Plugin plugin sebagai add-on OBS Studio
- **Fase 2:** Tambah plugin plugin lain sesuai kebutuhan New Media Art
- **Fase 3:** Build standalone creative software dengan pondasi source code OBS (`obs-studio/`)
- Monorepo `Hltn_Map/` menyimpan semua plugin + OBS source sebagai dependency

## Overview
Plugin OBS Studio untuk multi-display output dengan warp mesh projection mapping.
Menu di OBS: **Tools → Advance Display Output**

## Repo Structure
```
obs-hltn-advanced/
├── src/
│   ├── plugin-main.cpp      # Entry point, register menu item
│   ├── editor-window.h      # Data structs: SliceConfig, DisplayConfig, adv_editor
│   ├── editor-window.cpp    # Main editor (~1426 lines) — UI, preview, output, nav, inspector
│   ├── warp-mesh.h          # WarpMesh class declaration
│   └── warp-mesh.cpp        # Mesh operations (resize, init_full_rect, resize_preserve, get_quads)
├── data/
│   ├── locale/en-US.ini     # Locale strings
│   └── edge_blend.effect    # Custom OBS effect for edge blending (vertex+grid UV, per-pixel alpha)
├── build_plugin.ps1         # Build script (MinGW g++, links against OBS DLLs, copies effect)
├── CMakeLists.txt           # Alternative CMake build
├── README.md                # User-facing docs + screenshot
└── AGENTS.md                # This file
```

## Architecture

### Data Hierarchy
```
adv_editor
└── displays: vector<DisplayConfig>
    └── slices: vector<SliceConfig>
        └── mesh: WarpMesh (vertices grid cols×rows)
        └── vbuf: gs_vertbuffer_t* (GPU vertex buffer, 2 texcoords: texture UV + grid UV)
```

### Key structs (`editor-window.h`)
- **SliceConfig** — INPUT region (slice_x/y/w/h), mesh settings (cols/rows), blend settings (width + stretch per edge), color adjust, WarpMesh, vertex buffer
- **DisplayConfig** — output_type (0=Virtual,1=Spout,2=Display), monitor index, output window, canvas_cx/cy for rebuild
- **adv_editor** — main editor state, active display/slice, preview, nav, zoom, drag state, UI layout, btn positions

### Two View Modes
| Mode | Preview Shows | Edit Capability |
|------|--------------|-----------------|
| **INPUT** (`view_mode=0`) | Full canvas texture + colored slice overlays | Drag/resize slice regions, handles |
| **OUTPUT** (`view_mode=1`) | All slices rendered through warp mesh | Drag mesh vertices (on active slice only) |

### Rendering Pipeline
1. **preview_draw()** — Editor preview (line ~742)
   - INPUT: `gs_draw_sprite()` full canvas + solid overlays for slices
   - OUTPUT: iterates all slices, rebuilds dirty vbufs, renders non-blend slices with default effect, then blend slices with custom `edge_blend.effect` using "BlendAlpha" technique
   - Draws mesh wireframe + vertex handles for active slice in OUTPUT mode

2. **output_draw()** — External monitor output (line ~1027)
   - Same two-pass approach: default effect for non-blend, blend effect for blend-enabled
   - `gs_projection_push/pop` for proper ViewProj binding
   - `set_blend_uniforms()` called per slice to pass normalized stretch values

3. **rebuild_vbuf()** — Rebuilds GPU vertex buffer (line ~210)
   - Creates 2 texcoord arrays: [0]=texture UV, [1]=grid UV (0..1 based on col/row)
   - Grid UV used by edge_blend.effect for warp-following alpha
   - Calls `resize_preserve()` or `init_full_rect()` based on mesh state

## Coordinate Systems

### INPUT mode: canvas coords (canvas_cx, canvas_cy)
### OUTPUT mode: canvas coords for projection + mouse mapping, slice-local for mesh vertices

## Edge Blending System

### Data
- `blend_enabled` (bool) — checkbox toggle per slice
- `blend_l/r/t/b` (width, 0-500px) — blend zone width per edge
- `blend_ls/rs/ts/bs` (stretch, 1-500px) — gradient fade distance per edge

### Shader (`edge_blend.effect`)
- Grid UV (TEXCOORD1) interpolated by GPU → follows warp mesh perspective
- Alpha = `grid_uv.x / stretch` for left edge, `(1-grid_uv.x) / stretch` for right
- Same pattern for top/bottom with grid_uv.y
- Stretch values normalized by mesh dimensions in `set_blend_uniforms()`

### Rendering
- Default pass: slices with `blend_enabled=false`
- Blend pass: slices with `blend_enabled=true`, using `edge_blend.effect`
- Both passes in a single frame for proper layering

## Key Functions Map

| Function | Line | Purpose |
|----------|------|---------|
| `rebuild_vbuf()` | ~210 | GPU vertex buffer (2 texcoords, grid UV for blend) |
| `layout_output_slices()` | ~173 | Auto-grid mesh positions (no overlap margin) |
| `set_blend_uniforms()` | ~70 | Set normalized stretch to shader |
| `load_blend_effect()` | ~36 | Load edge_blend.effect (with DLL-dir fallback) |
| `draw_nav()` | ~190 | Navigator panel render |
| `draw_inspector()` | ~530 | Right panel (slice settings, blend checkbox + W/St sliders) |
| `preview_draw()` | ~742 | Main preview render callback |
| `editor_mouse_to_scene()` | ~900 | Screen → scene coords |
| `preview_wndproc()` | ~930 | Preview mouse handling |
| `output_draw()` | ~1027 | External output render callback (2-pass) |
| `start_output()` | ~1085 | Create output window/monitor |
| `stop_output()` | ~1115 | Destroy output window (no vbuf cleanup — deferred to editor_close) |
| `editor_wndproc()` | ~1125 | Main window message handler |
| `editor_open()` | ~1340 | Create editor window, auto-load config |
| `editor_close()` | ~1410 | Destroy editor, cleanup |

## Config Save/Load

### XML format (`hltn-advanced.xml` in `%APPDATA%\hltn-advanced\`)
- Stores all displays, slices, mesh vertices, blend settings
- Auto-load on editor open, auto-save via Save button
- Save As / Load buttons with file dialogs

### Status bar buttons: Save, Save As, Load, Close

## Completed Features
- [x] Multi-display, multi-slice architecture
- [x] Warp mesh with interactive vertex editing
- [x] INPUT mode: slice region drag/resize with 8 handles
- [x] OUTPUT mode: renders ALL slices via warp mesh
- [x] Display-level selection shows all slices
- [x] Auto-layout grid for OUTPUT mesh positions
- [x] Live output update when editing warp
- [x] `resize_preserve()` — bilinear interpolation preserves warp
- [x] Default mesh 2x2
- [x] XML config save/load with auto-persist
- [x] Edge blending with per-pixel shader (grid UV, follows warp perspective)
- [x] Blend width + stretch sub-sliders per edge

## Known Issues / Pending
- [ ] "+ Add Slice" not rendered in Navigator
- [ ] "Duplicate" right-click menu item has no handler
- [ ] No "Remove Display" functionality
- [ ] Spout output type not implemented
- [ ] Undo/Redo buttons non-functional
- [ ] No keyboard shortcuts
- [ ] Canvas resize not auto-detected
- [ ] No scroll in navigator panel
- [ ] Color Adjust sliders not connected to rendering

## Build & Deploy
```powershell
.\build_plugin.ps1
# Copy DLL + effect to OBS (requires admin):
# hltn-advanced.dll → C:\Program Files\obs-studio\obs-plugins\64bit\
# edge_blend.effect → C:\Program Files\obs-studio\obs-plugins\64bit\data\
```

## Git
Repo: https://github.com/holution/Hltn-advance-mapping
Branch: master

## Session State (2025-07-02)
Last commit: `c880811` — edge blend warp fix (grid UV interpolation)
Working: edge blending with manual sliders (width + stretch per edge), per-pixel shader following mesh perspective
