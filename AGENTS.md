# HLTN Advanced Output — Project Context

## Project Vision
Mengembangkan **OBS Studio sebagai creative software untuk New Media Art**.
Dua plugin inti dalam monorepo `Hltn_Map/`:

| Plugin | Type | Path | Fungsi |
|--------|------|------|--------|
| **HLTN Mapping** | Filter | `obs-perspective-mesh/` | Perspective mesh warp filter untuk source video |
| **HLTN Advanced Output** | Output | `obs-hltn-advanced/` | Multi-display output dengan warp mesh projection mapping |

Repositori ini fokus pada **HLTN Advanced Output**.

## Overview
Plugin OBS Studio untuk multi-display output dengan warp mesh projection mapping.
Menu di OBS: **Tools → Advance Display Output**

## Repo Structure
```
obs-hltn-advanced/
├── src/
│   ├── plugin-main.cpp      # Entry point, register menu item
│   ├── editor-window.h      # Data structs: SliceConfig, DisplayConfig, adv_editor
│   ├── editor-window.cpp    # Main editor (~1066 lines) — UI, preview, output, nav, inspector
│   ├── warp-mesh.h          # WarpMesh class declaration
│   └── warp-mesh.cpp        # Mesh operations (resize, init_full_rect, resize_preserve, get_quads)
├── data/locale/en-US.ini    # Locale strings
├── build_plugin.ps1         # Build script (MinGW g++, links against OBS DLLs)
├── CMakeLists.txt           # Alternative CMake build
└── README.md                # User-facing docs + screenshot
```

## Architecture

### Data Hierarchy
```
adv_editor
└── displays: vector<DisplayConfig>
    └── slices: vector<SliceConfig>
        └── mesh: WarpMesh (vertices grid cols×rows)
        └── vbuf: gs_vertbuffer_t* (GPU vertex buffer)
```

### Key structs (`editor-window.h`)
- **SliceConfig** — INPUT region (slice_x/y/w/h), mesh settings, blend, color adjust, WarpMesh, vertex buffer
- **DisplayConfig** — output_type (0=Virtual,1=Spout,2=Display), monitor index, output window, canvas_cx/cy for rebuild
- **adv_editor** — main editor state, active display/slice, preview, nav, zoom, drag state, UI layout

### Two View Modes
| Mode | Preview Shows | Edit Capability |
|------|--------------|-----------------|
| **INPUT** (`view_mode=0`) | Full canvas texture + colored slice overlays | Drag/resize slice regions, handles |
| **OUTPUT** (`view_mode=1`) | All slices rendered through warp mesh | Drag mesh vertices (on active slice only) |

### Rendering Pipeline
1. **preview_draw()** (line ~401) — Called per frame for editor preview
   - INPUT: `gs_draw_sprite()` full canvas + solid overlays for slices
   - OUTPUT: iterates all slices in active display, rebuilds dirty vbufs, renders via `gs_draw(GS_TRIS)` in single `gs_effect_loop`
   - Draws mesh wireframe + vertex handles for active slice in OUTPUT mode

2. **output_draw()** (line ~692) — Called per frame for external monitor output
   - Iterates all slices, checks `mesh_dirty`, auto-rebuilds vbuf when needed
   - Uses single `gs_effect_loop` to render all slices

3. **rebuild_vbuf()** (line ~129) — Rebuilds GPU vertex buffer from mesh data
   - Calculates UVs from slice_x/y/w/h relative to canvas
   - If cols/rows changed and mesh has vertices → calls `resize_preserve()` (bilinear interpolation)
   - If mesh has no vertices → calls `init_full_rect()` (flat rectangle)
   - Sets `mesh_dirty = false`

## Coordinate Systems (Important!)

### INPUT mode
- Uses canvas dimensions (canvas_cx, canvas_cy)
- Slice regions in canvas pixel coords

### OUTPUT mode
- **Projection** uses `canvas_cx, canvas_cy` (NOT slice_w/h) — fixed 2025-07-01
- **Mouse mapping** uses `canvas_cx, canvas_cy` — consistent with projection
- **Vertex clamping** uses `canvas_cx, canvas_cy` in OUTPUT mode, `slice_w/h` in INPUT mode
- Mesh vertices positioned by `layout_output_slices()` in canvas coordinate space

### layout_output_slices()
- Does NOT touch slice INPUT region (slice_x/y/w/h) — user controls INPUT via inspector
- Only sets mesh vertex positions via `init_full_rect()` to grid cells
- Called on Add Slice and Remove Slice

## Key Functions Map

| Function | Line | Purpose |
|----------|------|---------|
| `rebuild_vbuf()` | ~129 | GPU vertex buffer from mesh |
| `layout_output_slices()` | ~129 | Auto-grid mesh positions |
| `draw_nav()` | ~190 | Navigator panel render |
| `draw_inspector()` | ~237 | Right panel (slice settings) |
| `preview_draw()` | ~401 | Main preview render callback |
| `editor_mouse_to_scene()` | ~556 | Screen → scene coords |
| `preview_wndproc()` | ~588 | Preview mouse handling |
| `output_draw()` | ~692 | External output render callback |
| `start_output()` | ~728 | Create output window/monitor |
| `stop_output()` | ~758 | Destroy output window |
| `editor_wndproc()` | ~775 | Main window message handler |
| `editor_open()` | ~986 | Create editor window |
| `editor_close()` | ~1042 | Destroy editor, cleanup |

## Completed Features
- [x] Multi-display, multi-slice architecture
- [x] Warp mesh with interactive vertex editing
- [x] INPUT mode: slice region drag/resize with 8 handles
- [x] OUTPUT mode: renders ALL slices via warp mesh (not just active)
- [x] Display-level selection shows all slices (no raw canvas)
- [x] Auto-layout grid for OUTPUT mesh positions on add/remove slice
- [x] Live output update when editing warp while output running
- [x] `resize_preserve()` — bilinear interpolation preserves warp when changing cols/rows
- [x] Default mesh 2x2 (reduced from 4x4 for less clutter)
- [x] Save & Close buttons in status bar
- [x] Display output type (monitor) with auto-detection

## Known Issues / Pending
- [ ] "+ Add Slice" not rendered in Navigator (dead nav_items_y entries)
- [ ] "Duplicate" right-click menu item has no handler
- [ ] No "Remove Display" functionality
- [ ] Edge Blend & Color Adjust sliders exist but not applied in rendering shader
- [ ] Spout output type (type=1) marked "not yet implemented"
- [ ] No config persistence (settings lost on OBS restart)
- [ ] Undo/Redo buttons in toolbar are non-functional placeholders
- [ ] No keyboard shortcuts

## Build & Deploy
```powershell
.\build_plugin.ps1
# Copy to OBS (requires admin):
# build\hltn-advanced.dll → C:\Program Files\obs-studio\obs-plugins\64bit\
```

Requires: OBS Studio installed, OBS source at `../obs-studio/`, MSYS2 MinGW64.

## Git
Repo: https://github.com/holution/Hltn-advance-mapping
Branch: master

## Current Session State (2025-07-01)
Last changes pushed at commit `8fa84ed`. Working on OUTPUT mode fixes (coordinate system, auto-layout, live update, mesh init bug). Next priorities: Navigator UX bugs, Edge Blend rendering, config persistence.
