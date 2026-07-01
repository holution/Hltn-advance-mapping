# HLTN Advanced Output

Plugin OBS Studio untuk **multi-display output** dengan warp mesh, edge blending, dan auto-layout grid.

![HLTN Advanced Output](hltn%20map.jpeg)

## Fitur

- **Multi-Display Output** — Beberapa output display dalam satu project, masing-masing dengan kumpulan slice
- **Warp Mesh** — Edit vertex mesh secara interaktif untuk perspective mapping / projection mapping
- **Edge Blend** — Atur blend kiri/kanan/atas/bawah per slice untuk seamless multi-proyektor
- **Color Adjust** — Brightness, contrast, gamma, opacity per slice
- **Auto-Layout Grid** — Slice baru otomatis di-layout dalam grid di output display
- **Live Output Update** — Edit mesh saat output berjalan, langsung terlihat di layar output
- **Resize Mesh Preserve** — Ubah resolusi mesh (cols/rows) tanpa kehilangan edit warp (bilinear interpolation)
- **Multiple Output Types** — Virtual, Spout (coming soon), Display (monitor)

## Build

**Requirements:**
- Windows + MSYS2 MinGW64
- OBS Studio terinstall di `C:\Program Files\obs-studio\bin\64bit\`
- Source OBS Studio (untuk headers) di `../obs-studio`

```powershell
.\build_plugin.ps1
```

Output: `build\hltn-advanced.dll`

## Install

Copy `build\hltn-advanced.dll` + runtime DLLs ke folder plugin OBS:
```
C:\Program Files\obs-studio\obs-plugins\64bit\
```

## Usage

1. Buka OBS Studio
2. **Tools → Advance Display Output**
3. Tambah Display dan Slice di Navigator
4. Atur slice region dan warp mesh di Inspector
5. Switch ke mode **OUTPUT** untuk melihat dan mengedit hasil mapping
6. Klik **Start Output** untuk menampilkan ke monitor

## License

Proprietary — Holution/Syntetika Project
