#pragma once
#include <windows.h>
#include <obs-module.h>
#include <obs.h>
#include <graphics/graphics.h>
#include "warp-mesh.h"
#include <vector>
#include <string>

struct SliceConfig {
	int slice_x, slice_y, slice_w, slice_h;
	int mesh_cols, mesh_rows;
	int blend_l, blend_r, blend_t, blend_b;
	int brightness, contrast, gamma, opacity;

	WarpMesh mesh;
	bool mesh_dirty;
	gs_vertbuffer_t *vbuf;
	uint32_t num_verts;

	SliceConfig() {
		slice_x = 0; slice_y = 0; slice_w = 1920; slice_h = 1080;
		mesh_cols = 2; mesh_rows = 2;
		blend_l = 0; blend_r = 0; blend_t = 0; blend_b = 0;
		brightness = 0; contrast = 100; gamma = 100; opacity = 100;
		mesh_dirty = true; vbuf = nullptr; num_verts = 0;
		mesh.init_full_rect(2, 2, 0, 0, (float)slice_w, (float)slice_h);
	}
};

struct DisplayConfig {
	int output_type;
	int monitor;
	bool output_active;
	obs_display_t *output_display;
	HWND output_hwnd;
	uint32_t output_cx, output_cy;
	uint32_t canvas_cx, canvas_cy;
	std::vector<SliceConfig> slices;

	DisplayConfig() {
		output_type = 0; monitor = 1;
		output_active = false;
		output_display = nullptr; output_hwnd = nullptr;
		output_cx = 1920; output_cy = 1080;
		canvas_cx = 1920; canvas_cy = 1080;
		slices.push_back(SliceConfig());
	}
};

struct adv_editor {
	HWND hwnd;
	HWND preview_hwnd;
	obs_display_t *preview_display;

	std::vector<DisplayConfig> displays;
	int active_display;
	int active_slice;

	uint32_t canvas_cx, canvas_cy;

	int drag_idx;
	float drag_off_x, drag_off_y;
	int hover_idx;

	bool panning;
	int pan_last_x, pan_last_y;
	float offset_x, offset_y;
	float zoom;

	int view_mode;

	int slice_drag;
	int slice_drag_corner;
	float slice_drag_sx, slice_drag_sy, slice_drag_sw, slice_drag_sh;
	float slice_drag_mx, slice_drag_my;

	int selected_nav;
	int active_slider;
	int slider_xs[16], slider_ys[16], slider_ws[16];
	int *slider_vals[16];
	int slider_lo[16], slider_hi[16];
	int num_sliders;

	int nav_count;
	int nav_y0;
	int nav_items_y[32];
	int nav_items_depth[32];
	wchar_t nav_items_name[32][64];

	int ndisplays;
	int display_slices[8];

	int btn_insp_start[4];
	int btn_save_x, btn_save_w;
	int btn_saveas_x, btn_saveas_w;
	int btn_load_x, btn_load_w;
	int btn_close_x, btn_close_w;

	bool running;

	HDC mem_dc;
	HBITMAP mem_bmp;
	int mem_w, mem_h;

	int left_panel_w, right_panel_w;
	bool left_collapsed, right_collapsed;

	int toolbar_h, status_h;

	SliceConfig* get_active_slice() {
		if (active_display >= 0 && active_display < (int)displays.size()) {
			auto &d = displays[active_display];
			if (active_slice >= 0 && active_slice < (int)d.slices.size())
				return &d.slices[active_slice];
		}
		return nullptr;
	}
};

adv_editor *editor_open();
void editor_close(adv_editor *ed);
