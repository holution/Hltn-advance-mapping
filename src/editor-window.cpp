#include "editor-window.h"
#include <commdlg.h>
#include <string>
#include <fstream>

/* forward */
static void rebuild_vbuf(SliceConfig *sc, uint32_t canvas_cx, uint32_t canvas_cy);
static bool start_output(DisplayConfig *d, uint32_t canvas_cx, uint32_t canvas_cy);
static void stop_output(DisplayConfig *d);
static bool get_monitor_rect(int monitor, RECT *out);
static void save_config_default(adv_editor *ed, HWND parent);
static void save_config_as(adv_editor *ed, HWND parent);
static void load_config(adv_editor *ed, HWND parent);

/* ================================================================
   THEME
   ================================================================ */
#define CLR_BG         RGB(26, 26, 30)
#define CLR_PANEL      RGB(30, 30, 36)
#define CLR_SURFACE    RGB(37, 37, 48)
#define CLR_BORDER     RGB(42, 42, 53)
#define CLR_TEXT       RGB(200, 200, 208)
#define CLR_TEXT_DIM   RGB(130, 130, 145)
#define CLR_ACCENT     RGB(83, 230, 192)
#define CLR_ACCENT_BG  RGB(30, 60, 55)
#define CLR_WHITE      RGB(240, 240, 248)
#define CLR_TOOLBAR    RGB(22, 22, 26)
#define CLR_STATUS     RGB(18, 18, 22)

static HBRUSH br_bg, br_panel, br_surface, br_toolbar, br_status, br_accent;
static HFONT hf_normal, hf_small, hf_title;
static HPEN pen_border, pen_accent;

static void init_theme()
{
	br_bg      = CreateSolidBrush(CLR_BG);
	br_panel   = CreateSolidBrush(CLR_PANEL);
	br_surface = CreateSolidBrush(CLR_SURFACE);
	br_toolbar = CreateSolidBrush(CLR_TOOLBAR);
	br_status  = CreateSolidBrush(CLR_STATUS);
	br_accent  = CreateSolidBrush(CLR_ACCENT);
	pen_border = CreatePen(PS_SOLID, 1, CLR_BORDER);
	pen_accent = CreatePen(PS_SOLID, 1, CLR_ACCENT);
	hf_normal = CreateFontW(-13, 0, 0, 0, FW_NORMAL, 0, 0, 0, ANSI_CHARSET,
		OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, VARIABLE_PITCH, L"Segoe UI");
	hf_small  = CreateFontW(-11, 0, 0, 0, FW_NORMAL, 0, 0, 0, ANSI_CHARSET,
		OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, VARIABLE_PITCH, L"Segoe UI");
	hf_title  = CreateFontW(-12, 0, 0, 0, FW_SEMIBOLD, 0, 0, 0, ANSI_CHARSET,
		OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, VARIABLE_PITCH, L"Segoe UI");
}

static void done_theme()
{
	DeleteObject(br_bg); DeleteObject(br_panel); DeleteObject(br_surface);
	DeleteObject(br_toolbar); DeleteObject(br_status); DeleteObject(br_accent);
	DeleteObject(hf_normal); DeleteObject(hf_small); DeleteObject(hf_title);
	DeleteObject(pen_border); DeleteObject(pen_accent);
}

/* ================================================================
   DRAWING HELPERS
   ================================================================ */
static void fill_rect(HDC dc, int x, int y, int w, int h, COLORREF c)
{
	RECT r = {x, y, x + w, y + h};
	HBRUSH br = CreateSolidBrush(c);
	FillRect(dc, &r, br);
	DeleteObject(br);
}

static void draw_border(HDC dc, int x, int y, int w, int h, COLORREF c)
{
	RECT r = {x, y, x + w, y + h};
	HBRUSH br = CreateSolidBrush(c);
	FrameRect(dc, &r, br);
	DeleteObject(br);
}

static void draw_text(HDC dc, const wchar_t *txt, int x, int y, int w, int h, COLORREF c, HFONT f, UINT align)
{
	SelectObject(dc, f);
	SetTextColor(dc, c);
	SetBkMode(dc, TRANSPARENT);
	RECT r = {x, y, x + w, y + h};
	DrawTextW(dc, txt, -1, &r, align | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
}

static bool btn(HDC dc, const wchar_t *label, int x, int y, int w, int h, bool accent, bool hover)
{
	COLORREF bg = accent ? (hover ? CLR_ACCENT : CLR_ACCENT_BG) : (hover ? CLR_SURFACE : CLR_PANEL);
	COLORREF fg = accent ? CLR_BG : (hover ? CLR_WHITE : CLR_TEXT);
	fill_rect(dc, x, y, w, h, bg);
	draw_border(dc, x, y, w, h, accent ? CLR_ACCENT : CLR_BORDER);
	SelectObject(dc, hf_normal);
	SetTextColor(dc, fg);
	SetBkMode(dc, TRANSPARENT);
	RECT r = {x, y, x + w, y + h};
	DrawTextW(dc, label, -1, &r, DT_CENTER | DT_SINGLELINE | DT_VCENTER);
	return hover;
}

static void slider_int_draw(HDC dc, const wchar_t *label, int x, int y, int w, int *val, int lo, int hi)
{
	draw_text(dc, label, x, y, 60, 18, CLR_TEXT_DIM, hf_small, DT_LEFT);
	wchar_t buf[32]; wsprintfW(buf, L"%d", *val);
	draw_text(dc, buf, x + w - 42, y, 40, 18, CLR_TEXT, hf_small, DT_RIGHT);

	int track_y = y + 16, track_h = 4;
	fill_rect(dc, x, track_y, w, track_h, CLR_SURFACE);
	if (*val > lo) {
		float frac = (float)(*val - lo) / (float)(hi - lo);
		fill_rect(dc, x, track_y, (int)(w * frac), track_h, CLR_ACCENT);
	}
	int knob_x = x + (int)((float)(*val - lo) / (float)(hi - lo) * w) - 4;
	if (knob_x < x) knob_x = x;
	if (knob_x > x + w - 8) knob_x = x + w - 8;
	fill_rect(dc, knob_x, y + 12, 8, 12, CLR_WHITE);
}

static void slider_int_track(HDC dc, const wchar_t *label, int x, int y, int w,
	int *val, int lo, int hi, adv_editor *ed)
{
	slider_int_draw(dc, label, x, y, w, val, lo, hi);
	int n = ed->num_sliders;
	if (n < 16) {
		ed->slider_xs[n] = x; ed->slider_ys[n] = y; ed->slider_ws[n] = w;
		ed->slider_vals[n] = val; ed->slider_lo[n] = lo; ed->slider_hi[n] = hi;
		ed->num_sliders = n + 1;
	}
}

/* ================================================================
   AUTO LAYOUT (OUTPUT mesh positioning)
   ================================================================ */
static void layout_output_slices(DisplayConfig *d, uint32_t canvas_cx, uint32_t canvas_cy)
{
	int n = (int)d->slices.size();
	if (n <= 0) return;
	int cols = 1, rows = 1;
	while (cols * rows < n) {
		if (cols == rows) cols++;
		else rows++;
	}
	float cw = (float)canvas_cx / (float)cols;
	float ch = (float)canvas_cy / (float)rows;
	for (int i = 0; i < n; i++) {
		auto &sc = d->slices[i];
		int col = i % cols;
		int row = i / cols;
		sc.mesh.init_full_rect(sc.mesh_cols, sc.mesh_rows, col * cw, row * ch, cw, ch);
		sc.mesh_dirty = true;
	}
}

/* ================================================================
   REBUILD VBUF
   ================================================================ */
static void rebuild_vbuf(SliceConfig *sc, uint32_t canvas_cx, uint32_t canvas_cy)
{
	if (sc->vbuf) { gs_vertexbuffer_destroy(sc->vbuf); sc->vbuf = nullptr; }
	int cols = sc->mesh_cols, rows = sc->mesh_rows;
	if (sc->mesh.n_columns() != cols || sc->mesh.n_rows() != rows) {
		if (sc->mesh.n_vertices() > 0)
			sc->mesh.resize_preserve(cols, rows);
		else
			sc->mesh.init_full_rect(cols, rows, 0, 0, (float)sc->slice_w, (float)sc->slice_h);
	}
	auto quads = sc->mesh.get_quads();
	if (quads.empty()) return;
	size_t nv = quads.size() * 6;
	struct gs_vb_data *vd = gs_vbdata_create();
	vd->num = (uint32_t)nv;
	vd->points = (vec3 *)bzalloc(nv * sizeof(vec3));
	vd->num_tex = 1;
	vd->tvarray = (struct gs_tvertarray *)bzalloc(sizeof(struct gs_tvertarray));
	vd->tvarray[0].width = 2;
	vd->tvarray[0].array = bzalloc(nv * sizeof(vec2));
	float inv_cols = 1.0f / (float)(cols - 1), inv_rows = 1.0f / (float)(rows - 1);
	float tx0 = (float)sc->slice_x / (float)canvas_cx;
	float ty0 = (float)sc->slice_y / (float)canvas_cy;
	float tx1 = (float)(sc->slice_x + sc->slice_w) / (float)canvas_cx;
	float ty1 = (float)(sc->slice_y + sc->slice_h) / (float)canvas_cy;
	size_t idx = 0;
	vec2 *tverts = (vec2 *)vd->tvarray[0].array;
	for (auto &q : quads) {
		Vec2 verts[4];
		for (int i = 0; i < 4; i++) verts[i] = sc->mesh.get_vertex(q[i]);
		auto setv = [&](int vi) {
			vd->points[idx] = {verts[vi].x, verts[vi].y, 0.0f};
			int ci = q[vi] % cols, ri = q[vi] / cols;
			tverts[idx] = {tx0 + (float)ci * inv_cols * (tx1 - tx0), ty0 + (float)ri * inv_rows * (ty1 - ty0)};
			idx++;
		};
		setv(0); setv(1); setv(2); setv(0); setv(2); setv(3);
	}
	sc->vbuf = gs_vertexbuffer_create(vd, GS_DYNAMIC);
	sc->num_verts = (uint32_t)nv;
	sc->mesh_dirty = false;
}

/* ================================================================
   CONFIG SAVE / LOAD (XML)
   ================================================================ */

static std::wstring get_default_config_path()
{
	wchar_t appdata[MAX_PATH] = {};
	GetEnvironmentVariableW(L"APPDATA", appdata, MAX_PATH);
	std::wstring s(appdata);
	s += L"\\hltn-advanced\\";
	CreateDirectoryW(s.c_str(), nullptr);
	s += L"hltn-advanced.xml";
	return s;
}

static void xml_save(adv_editor *ed, const wchar_t *path)
{
	std::wofstream f(path, std::ios::out | std::ios::trunc);
	if (!f) { MessageBoxW(nullptr, L"Failed to save config file", L"HLTN", MB_OK | MB_ICONERROR); return; }
	f << L"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
	f << L"<hltn_advanced canvas_cx=\"" << ed->canvas_cx << L"\" canvas_cy=\"" << ed->canvas_cy << L"\">\n";
	for (auto &d : ed->displays) {
		f << L"  <display output_type=\"" << d.output_type
		  << L"\" monitor=\"" << d.monitor << L"\">\n";
		for (auto &sc : d.slices) {
			f << L"    <slice x=\"" << sc.slice_x << L"\" y=\"" << sc.slice_y
			  << L"\" w=\"" << sc.slice_w << L"\" h=\"" << sc.slice_h
			  << L"\" mesh_cols=\"" << sc.mesh_cols << L"\" mesh_rows=\"" << sc.mesh_rows
			  << L"\" blend_l=\"" << sc.blend_l << L"\" blend_r=\"" << sc.blend_r
			  << L"\" blend_t=\"" << sc.blend_t << L"\" blend_b=\"" << sc.blend_b
			  << L"\" brightness=\"" << sc.brightness << L"\" contrast=\"" << sc.contrast
			  << L"\" gamma=\"" << sc.gamma << L"\" opacity=\"" << sc.opacity << L"\">\n";
			int nv = sc.mesh.n_vertices();
			f << L"      <mesh cols=\"" << sc.mesh.n_columns() << L"\" rows=\"" << sc.mesh.n_rows() << L"\">\n";
			for (int i = 0; i < nv; i++) {
				Vec2 v = sc.mesh.get_vertex(i);
				f << L"        <v x=\"" << v.x << L"\" y=\"" << v.y << L"\"/>\n";
			}
			f << L"      </mesh>\n";
			f << L"    </slice>\n";
		}
		f << L"  </display>\n";
	}
	f << L"</hltn_advanced>\n";
	f.close();
}

static void xml_load(adv_editor *ed, const wchar_t *path)
{
	std::wifstream f(path);
	if (!f) return;
	std::wstring xml((std::istreambuf_iterator<wchar_t>(f)), std::istreambuf_iterator<wchar_t>());
	f.close();

	obs_enter_graphics();
	for (auto &d : ed->displays)
		for (auto &s : d.slices)
			if (s.vbuf) { gs_vertexbuffer_destroy(s.vbuf); s.vbuf = nullptr; }
	obs_leave_graphics();
	ed->displays.clear();

	auto get_attr = [](const std::wstring &s, const wchar_t *name) -> std::wstring {
		std::wstring key = std::wstring(L" ") + name + L"=\"";
		size_t p = s.find(key);
		if (p == std::wstring::npos) return L"";
		p += key.size();
		size_t e = s.find(L'"', p);
		if (e == std::wstring::npos) return L"";
		return s.substr(p, e - p);
	};
	auto get_int = [&](const std::wstring &s, const wchar_t *name, int def = 0) -> int {
		auto v = get_attr(s, name);
		return v.empty() ? def : _wtoi(v.c_str());
	};
	auto get_float = [&](const std::wstring &s, const wchar_t *name, float def = 0.0f) -> float {
		auto v = get_attr(s, name);
		return v.empty() ? def : (float)_wtof(v.c_str());
	};

	size_t pos = 0;
	size_t root_end = xml.find(L'>', xml.find(L"<hltn_advanced"));
	ed->canvas_cx = get_int(xml.substr(0, root_end + 1), L"canvas_cx", ed->canvas_cx);
	ed->canvas_cy = get_int(xml.substr(0, root_end + 1), L"canvas_cy", ed->canvas_cy);

	while (true) {
		size_t ds = xml.find(L"<display", pos);
		if (ds == std::wstring::npos) break;
		size_t de = xml.find(L"</display>", ds);
		if (de == std::wstring::npos) break;
		pos = de + 10;

		DisplayConfig d;
		std::wstring dtag = xml.substr(ds, xml.find(L'>', ds) - ds + 1);
		d.output_type = get_int(dtag, L"output_type", 0);
		d.monitor = get_int(dtag, L"monitor", 1);

		size_t sp = ds;
		while (true) {
			size_t ss = xml.find(L"<slice", sp);
			if (ss == std::wstring::npos || ss > de) break;
			size_t se = xml.find(L"</slice>", ss);
			if (se == std::wstring::npos || se > de) break;
			sp = se + 8;

			SliceConfig sc;
			std::wstring stag = xml.substr(ss, xml.find(L'>', ss) - ss + 1);
			sc.slice_x = get_int(stag, L"x", 0); sc.slice_y = get_int(stag, L"y", 0);
			sc.slice_w = get_int(stag, L"w", 1920); sc.slice_h = get_int(stag, L"h", 1080);
			sc.mesh_cols = get_int(stag, L"mesh_cols", 2); sc.mesh_rows = get_int(stag, L"mesh_rows", 2);
			sc.blend_l = get_int(stag, L"blend_l"); sc.blend_r = get_int(stag, L"blend_r");
			sc.blend_t = get_int(stag, L"blend_t"); sc.blend_b = get_int(stag, L"blend_b");
			sc.brightness = get_int(stag, L"brightness");
			sc.contrast = get_int(stag, L"contrast", 100);
			sc.gamma = get_int(stag, L"gamma", 100);
			sc.opacity = get_int(stag, L"opacity", 100);

			size_t ms = xml.find(L"<mesh", ss);
			if (ms != std::wstring::npos && ms < se) {
				size_t me_start = xml.find(L'>', ms);
				int mcols = get_int(xml.substr(ms, me_start - ms + 1), L"cols", sc.mesh_cols);
				int mrows = get_int(xml.substr(ms, me_start - ms + 1), L"rows", sc.mesh_rows);
				sc.mesh.resize(mcols, mrows);
				int vi = 0;
				size_t vp = ms;
				while (vi < mcols * mrows) {
					size_t vs = xml.find(L"<v", vp);
					if (vs == std::wstring::npos || vs > se) break;
					size_t ve = xml.find(L"/>", vs);
					if (ve == std::wstring::npos) break;
					vp = ve + 2;
					std::wstring vtag = xml.substr(vs, ve - vs + 2);
					sc.mesh.set_vertex(vi, {get_float(vtag, L"x"), get_float(vtag, L"y")});
					vi++;
				}
			}
			sc.mesh_dirty = true;
			d.slices.push_back(sc);
		}
		if (d.slices.empty()) d.slices.push_back(SliceConfig());
		ed->displays.push_back(d);
	}
	if (ed->displays.empty()) {
		ed->displays.push_back(DisplayConfig());
	}
	ed->active_display = 0;
	ed->active_slice = 0;
}

static void save_config_default(adv_editor *ed, HWND parent)
{
	std::wstring path = get_default_config_path();
	xml_save(ed, path.c_str());
	ed->btn_flash = 8;
	std::wstring msg = L"Saved to: " + path;
	MessageBoxW(parent, msg.c_str(), L"HLTN Save", MB_OK | MB_ICONINFORMATION);
}

static void save_config_as(adv_editor *ed, HWND parent)
{
	wchar_t path[MAX_PATH] = L"hltn-advanced.xml";
	OPENFILENAMEW ofn = {};
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = parent;
	ofn.lpstrFilter = L"XML Files\0*.xml\0All Files\0*.*\0";
	ofn.lpstrFile = path;
	ofn.nMaxFile = MAX_PATH;
	ofn.lpstrDefExt = L"xml";
	ofn.Flags = OFN_OVERWRITEPROMPT | OFN_HIDEREADONLY;
	if (GetSaveFileNameW(&ofn)) {
		xml_save(ed, path);
		ed->btn_flash = 8;
	}
}

static void load_config(adv_editor *ed, HWND parent)
{
	wchar_t path[MAX_PATH] = {};
	OPENFILENAMEW ofn = {};
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = parent;
	ofn.lpstrFilter = L"XML Files\0*.xml\0All Files\0*.*\0";
	ofn.lpstrFile = path;
	ofn.nMaxFile = MAX_PATH;
	ofn.lpstrDefExt = L"xml";
	ofn.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
	if (GetOpenFileNameW(&ofn)) {
		xml_load(ed, path);
		InvalidateRect(ed->hwnd, nullptr, FALSE);
	}
}

/* ================================================================
   NAV
   ================================================================ */
static void rebuild_nav(adv_editor *ed)
{
	int n = 0;
	for (int d = 0; d < (int)ed->displays.size() && n < 30; d++) {
		wsprintfW(ed->nav_items_name[n], L"\u25B6 Display %d", d + 1);
		ed->nav_items_depth[n] = 0; n++;
		int ns = (int)ed->displays[d].slices.size();
		for (int s = 0; s < ns && n < 31; s++) {
			wsprintfW(ed->nav_items_name[n], L"Slice %d", s + 1);
			ed->nav_items_depth[n] = 1; n++;
		}
		if (n < 32) {
			wsprintfW(ed->nav_items_name[n], L"+ Add Slice");
			ed->nav_items_depth[n] = 1; n++;
		}
	}
	ed->nav_count = n;
}

static void draw_nav(HDC dc, adv_editor *ed)
{
	RECT cr; GetClientRect(ed->hwnd, &cr);
	int hh = cr.bottom - ed->toolbar_h - ed->status_h;
	int y = ed->toolbar_h;
	fill_rect(dc, 0, y, ed->left_panel_w, hh, CLR_PANEL);
	draw_border(dc, ed->left_panel_w - 1, y, 1, hh, CLR_BORDER);
	y += 8;
	draw_text(dc, L"NAVIGATOR", 10, y, ed->left_panel_w - 20, 20, CLR_TEXT_DIM, hf_small, DT_LEFT);
	y += 24;
	rebuild_nav(ed);
	ed->nav_y0 = y;
	int cur = 0;
	for (int d = 0; d < (int)ed->displays.size(); d++) {
		for (int s = -1; s < (int)ed->displays[d].slices.size(); s++) {
			if (s >= 0) {
				int col = (ed->active_display == d && ed->active_slice == s) ? CLR_ACCENT :
					CLR_TEXT_DIM;
				fill_rect(dc, 4, y, ed->left_panel_w - 8, 22,
					(ed->active_display == d && ed->active_slice == s) ? CLR_ACCENT_BG : CLR_PANEL);
				draw_text(dc, ed->nav_items_name[cur], 28, y, ed->left_panel_w - 36, 22, col, hf_small, DT_LEFT);
			} else {
				int col = (ed->active_display == d && ed->active_slice < 0) ? CLR_ACCENT : CLR_TEXT;
				fill_rect(dc, 4, y, ed->left_panel_w - 8, 22,
					(ed->active_display == d && ed->active_slice < 0) ? CLR_ACCENT_BG : CLR_PANEL);
				draw_text(dc, ed->nav_items_name[cur], 12, y, ed->left_panel_w - 20, 22, col, hf_title, DT_LEFT);
			}
			ed->nav_items_y[cur] = y;
			cur++;
			y += 22;
		}
		y += 2;
	}
	y += 4;
	btn(dc, L"+ Add Display", 12, y, ed->left_panel_w - 24, 26, true, false);
}

/* ================================================================
   INSPECTOR
   ================================================================ */
static int insp_section(HDC dc, int x, int y, int w, const wchar_t *title)
{
	fill_rect(dc, x, y, w, 22, CLR_SURFACE);
	draw_text(dc, title, x + 8, y, w - 16, 22, CLR_ACCENT, hf_title, DT_LEFT);
	return y + 26;
}

static void draw_inspector(HDC dc, adv_editor *ed)
{
	RECT cr; GetClientRect(ed->hwnd, &cr);
	int hh = cr.bottom - ed->toolbar_h - ed->status_h;
	int y = ed->toolbar_h;
	int x = cr.right - ed->right_panel_w;
	int w = ed->right_panel_w;
	fill_rect(dc, x, y, w, hh, CLR_PANEL);
	draw_border(dc, x, y, 1, hh, CLR_BORDER);
	y += 8;
	draw_text(dc, L"INSPECTOR", x + 10, y, w - 20, 20, CLR_TEXT_DIM, hf_small, DT_LEFT);
	y += 28;
	int pad = 10;
	ed->num_sliders = 0;

	auto sc = ed->get_active_slice();
	auto dc2 = (ed->active_display >= 0 && ed->active_display < (int)ed->displays.size())
		? &ed->displays[ed->active_display] : nullptr;

	if (ed->active_slice < 0 && dc2) {
		/* display-level */
		y = insp_section(dc, x + 4, y, w - 8, L"Output Settings");
		const wchar_t *ot[] = {L"Virtual", L"Spout", L"Display"};
		draw_text(dc, L"Type", x + pad, y, 40, 18, CLR_TEXT_DIM, hf_small, DT_LEFT);
		for (int t = 0; t < 3; t++)
			btn(dc, ot[t], x + 52 + t * 52, y, 48, 20, dc2->output_type == t, false);
		y += 28;
		if (dc2->output_type == 2) {
			draw_text(dc, L"Monitor", x + pad, y, 60, 18, CLR_TEXT_DIM, hf_small, DT_LEFT);
			slider_int_track(dc, L"", x + 70, y, w - 80, &dc2->monitor, 1, 16, ed); y += 28;
		}
	} else if (sc) {
		/* slice-level */
		y = insp_section(dc, x + 4, y, w - 8, L"Slice Region");
		slider_int_track(dc, L"X", x + pad, y, w - pad*2, &sc->slice_x, 0, ed->canvas_cx, ed); y += 22;
		slider_int_track(dc, L"Y", x + pad, y, w - pad*2, &sc->slice_y, 0, ed->canvas_cy, ed); y += 22;
		slider_int_track(dc, L"W", x + pad, y, w - pad*2, &sc->slice_w, 1, ed->canvas_cx, ed); y += 22;
		slider_int_track(dc, L"H", x + pad, y, w - pad*2, &sc->slice_h, 1, ed->canvas_cy, ed); y += 26;

		y = insp_section(dc, x + 4, y, w - 8, L"Warp Mesh");
		slider_int_track(dc, L"Cols", x + pad, y, w - pad*2, &sc->mesh_cols, 2, 32, ed); y += 22;
		slider_int_track(dc, L"Rows", x + pad, y, w - pad*2, &sc->mesh_rows, 2, 32, ed); y += 26;

		y = insp_section(dc, x + 4, y, w - 8, L"Edge Blend");
		slider_int_track(dc, L"Left",   x + pad, y, w - pad*2, &sc->blend_l, 0, 500, ed); y += 22;
		slider_int_track(dc, L"Right",  x + pad, y, w - pad*2, &sc->blend_r, 0, 500, ed); y += 22;
		slider_int_track(dc, L"Top",    x + pad, y, w - pad*2, &sc->blend_t, 0, 500, ed); y += 22;
		slider_int_track(dc, L"Bottom", x + pad, y, w - pad*2, &sc->blend_b, 0, 500, ed); y += 26;

		y = insp_section(dc, x + 4, y, w - 8, L"Color Adjust");
		slider_int_track(dc, L"Brightness", x + pad, y, w - pad*2, &sc->brightness, -100, 100, ed); y += 22;
		slider_int_track(dc, L"Contrast",   x + pad, y, w - pad*2, &sc->contrast, 0, 300, ed); y += 22;
		slider_int_track(dc, L"Gamma",      x + pad, y, w - pad*2, &sc->gamma, 0, 300, ed); y += 22;
		slider_int_track(dc, L"Opacity",    x + pad, y, w - pad*2, &sc->opacity, 0, 100, ed); y += 26;
	}

	y += 4;
	if (dc2) {
		ed->btn_insp_start[0] = x + pad; ed->btn_insp_start[1] = y;
		ed->btn_insp_start[2] = w - 20; ed->btn_insp_start[3] = 28;
		btn(dc, dc2->output_active ? L"Stop Output" : L"Start Output", x + pad, y, w - 20, 28, true, false);
	}
}

/* ================================================================
   TOOLBAR + STATUS
   ================================================================ */
static void draw_toolbar(HDC dc, adv_editor *ed)
{
	int ww; RECT r; GetClientRect(ed->hwnd, &r); ww = r.right;
	int th = 32;
	fill_rect(dc, 0, 0, ww, th, CLR_TOOLBAR);
	draw_border(dc, 0, th - 1, ww, 1, CLR_BORDER);
	int x = 8;
	draw_text(dc, L"HLTN Advanced", x, 0, 100, th, CLR_TEXT_DIM, hf_title, DT_LEFT); x += 104;
	btn(dc, L"INPUT",  x, 4, 52, 24, ed->view_mode == 0, false); x += 56;
	btn(dc, L"OUTPUT", x, 4, 56, 24, ed->view_mode == 1, false); x += 64;
	x += 8; draw_border(dc, x, 6, 1, 20, CLR_BORDER); x += 8;
	btn(dc, L"\u21A9", x, 4, 28, 24, false, false); x += 32;
	btn(dc, L"\u21AA", x, 4, 28, 24, false, false); x += 32;
	x += 8; draw_border(dc, x, 6, 1, 20, CLR_BORDER); x += 8;
	int zm = (int)(ed->zoom * 100);
	wchar_t zbuf[16]; wsprintfW(zbuf, L"%d%%", zm);
	btn(dc, zbuf, x, 4, 50, 24, false, false); x += 54;
	x += 8; draw_border(dc, x, 6, 1, 20, CLR_BORDER); x += 8;

	auto sc = ed->get_active_slice();
	int vw = ed->view_mode == 0 ? (int)ed->canvas_cx : (sc ? sc->slice_w : 1920);
	int vh = ed->view_mode == 0 ? (int)ed->canvas_cy : (sc ? sc->slice_h : 1080);
	wchar_t resbuf[64]; wsprintfW(resbuf, L"%dx%d", vw, vh);
	draw_text(dc, resbuf, ww - 220, 0, 120, th, CLR_TEXT_DIM, hf_small, DT_RIGHT);
	draw_text(dc, ed->view_mode == 0 ? L"Input" : L"Output", ww - 108, 0, 96, th, CLR_ACCENT, hf_small, DT_LEFT);

	auto d2 = (ed->active_display >= 0 && ed->active_display < (int)ed->displays.size())
		? &ed->displays[ed->active_display] : nullptr;
	bool act = d2 ? d2->output_active : false;
	btn(dc, act ? L"Stop" : L"Start", ww - 90, 4, 80, 24, true, false);
}

static void draw_status(HDC dc, adv_editor *ed)
{
	wchar_t buf[256];
	RECT cr; GetClientRect(ed->hwnd, &cr);
	int y = cr.bottom - ed->status_h;
	fill_rect(dc, 0, y, cr.right, ed->status_h, CLR_STATUS);
	draw_border(dc, 0, y, cr.right, 1, CLR_BORDER);

	auto sc = ed->get_active_slice();
	auto d2 = (ed->active_display >= 0 && ed->active_display < (int)ed->displays.size())
		? &ed->displays[ed->active_display] : nullptr;
	bool act = d2 ? d2->output_active : false;

	wsprintfW(buf, L"Output: %s  |  Canvas: %dx%d  |  Display %d",
		act ? L"ON" : L"OFF", ed->canvas_cx, ed->canvas_cy,
		ed->active_display + 1);
	if (sc)
		wsprintfW(buf + lstrlenW(buf), L"  |  Slice %d: %d,%d %dx%d",
			ed->active_slice + 1, sc->slice_x, sc->slice_y, sc->slice_w, sc->slice_h);
	draw_text(dc, buf, 10, y, cr.right - 20, ed->status_h, CLR_TEXT_DIM, hf_small, DT_LEFT);

	wsprintfW(buf, L"Mesh: %dx%d  |  Zoom: %.0f%%",
		sc ? sc->mesh_cols : 0, sc ? sc->mesh_rows : 0, ed->zoom * 100);
	draw_text(dc, buf, 10, y, cr.right - 20, ed->status_h, CLR_TEXT_DIM, hf_small, DT_RIGHT);

	int bx = cr.right - 346, by = y + 3, bw = 78, bh = ed->status_h - 6;
	ed->btn_save_x = bx; ed->btn_save_w = bw;
	btn(dc, L"Save", bx, by, bw, bh, ed->btn_flash > 0, ed->btn_flash > 0);
	bx += bw + 8;
	ed->btn_saveas_x = bx; ed->btn_saveas_w = bw;
	btn(dc, L"Save As", bx, by, bw, bh, false, false);
	bx += bw + 8;
	ed->btn_load_x = bx; ed->btn_load_w = bw;
	btn(dc, L"Load", bx, by, bw, bh, false, false);
	bx += bw + 8;
	ed->btn_close_x = bx; ed->btn_close_w = bw;
	btn(dc, L"Close", bx, by, bw, bh, false, false);
}

static void editor_paint(adv_editor *ed)
{
	if (ed->btn_flash > 0) ed->btn_flash--;
	HDC dc = ed->mem_dc;
	RECT cr; GetClientRect(ed->hwnd, &cr);
	fill_rect(dc, 0, 0, cr.right, cr.bottom, CLR_BG);
	draw_toolbar(dc, ed);
	draw_nav(dc, ed);
	draw_inspector(dc, ed);
	draw_status(dc, ed);
}

/* ================================================================
   PREVIEW DRAW
   ================================================================ */
static void preview_draw(void *data, uint32_t cx, uint32_t cy)
{
	auto *ed = (adv_editor *)data;
	auto sc = ed->get_active_slice();
	gs_texture_t *tex = obs_get_main_texture();
	if (!tex) return;

	float cw = (float)ed->canvas_cx, ch = (float)ed->canvas_cy;
	if (cw == 0) cw = 1920; if (ch == 0) ch = 1080;
	float slx = (sc && sc->slice_w > 0 && sc->slice_h > 0) ? (float)sc->slice_x : 0;
	float sly = (sc && sc->slice_w > 0 && sc->slice_h > 0) ? (float)sc->slice_y : 0;
	float slw = (sc && sc->slice_w > 0 && sc->slice_h > 0) ? (float)sc->slice_w : cw;
	float slh = (sc && sc->slice_w > 0 && sc->slice_h > 0) ? (float)sc->slice_h : ch;

	gs_viewport_push();
	gs_projection_push();
	if (ed->view_mode == 0)
		gs_ortho(ed->offset_x, ed->offset_x + cw / ed->zoom, ed->offset_y, ed->offset_y + ch / ed->zoom, -100, 100);
	else
		gs_ortho(ed->offset_x, ed->offset_x + cw / ed->zoom,
			 ed->offset_y, ed->offset_y + ch / ed->zoom, -100, 100);
	gs_set_viewport(0, 0, (int)cx, (int)cy);

	gs_effect_t *eff = obs_get_base_effect(OBS_EFFECT_DEFAULT);
	if (eff) {
		gs_eparam_t *img = gs_effect_get_param_by_name(eff, "image");
		if (img) gs_effect_set_texture_srgb(img, tex);
		if (ed->view_mode == 0) {
			while (gs_effect_loop(eff, "Draw"))
				gs_draw_sprite(tex, 0, (uint32_t)cw, (uint32_t)ch);
		} else if (ed->active_display >= 0 && ed->active_display < (int)ed->displays.size()) {
			auto &slices = ed->displays[ed->active_display].slices;
			for (int s = 0; s < (int)slices.size(); s++) {
				auto *sl = &slices[s];
				if (sl->slice_w <= 0 || sl->slice_h <= 0) continue;
				if (sl->mesh_dirty || !sl->vbuf)
					rebuild_vbuf(sl, ed->canvas_cx, ed->canvas_cy);
			}
			while (gs_effect_loop(eff, "Draw")) {
				for (int s = 0; s < (int)slices.size(); s++) {
					auto *sl = &slices[s];
					if (!sl->vbuf) continue;
					gs_load_vertexbuffer(sl->vbuf);
					gs_draw(GS_TRIS, 0, sl->num_verts);
				}
			}
		}
	}

	gs_effect_t *solid = obs_get_base_effect(OBS_EFFECT_SOLID);

	/* Draw ALL slices in INPUT mode */
	if (ed->view_mode == 0) {
		for (int d = 0; d < (int)ed->displays.size(); d++) {
			for (int s = 0; s < (int)ed->displays[d].slices.size(); s++) {
				auto *sl = &ed->displays[d].slices[s];
				bool is_active = (d == ed->active_display && s == ed->active_slice);

				float s_slx = (float)sl->slice_x, s_sly = (float)sl->slice_y;
				float s_slw = (float)sl->slice_w, s_slh = (float)sl->slice_h;
				if (!solid) continue;

				gs_technique_t *tech = gs_effect_get_technique(solid, "Mixed");
				if (!tech) tech = gs_effect_get_technique(solid, "Solid");
				if (tech) {
					vec4 col;
					vec4_set(&col, is_active ? 0.0f : 0.2f, is_active ? 0.8f : 0.4f,
						 is_active ? 1.0f : 0.6f, is_active ? 0.25f : 0.15f);
					gs_effect_set_vec4(gs_effect_get_param_by_name(solid, "color"), &col);
					gs_technique_begin(tech); gs_technique_begin_pass(tech, 0);
					gs_matrix_push(); gs_matrix_translate3f(s_slx, s_sly, 0);
					gs_draw_sprite(nullptr, 0, (uint32_t)s_slw, (uint32_t)s_slh);
					gs_matrix_pop();
					gs_technique_end_pass(tech); gs_technique_end(tech);
				}

				/* Handles only for active slice */
				if (!is_active) continue;

				float hs = 10.0f / ed->zoom;
				float hx[8] = {s_slx, s_slx+s_slw, s_slx+s_slw, s_slx, s_slx+s_slw/2, s_slx+s_slw, s_slx+s_slw/2, s_slx};
				float hy[8] = {s_sly, s_sly, s_sly+s_slh, s_sly+s_slh, s_sly, s_sly+s_slh/2, s_sly+s_slh, s_sly+s_slh/2};
				gs_technique_t *ht = gs_effect_get_technique(solid, "Mixed");
				if (!ht) ht = gs_effect_get_technique(solid, "Solid");
				if (ht) {
					vec4 hc; vec4_set(&hc, 1.0f, 1.0f, 1.0f, 0.9f);
					gs_effect_set_vec4(gs_effect_get_param_by_name(solid, "color"), &hc);
					gs_technique_begin(ht); gs_technique_begin_pass(ht, 0);
					for (int hi = 0; hi < 8; hi++) {
						gs_matrix_push(); gs_matrix_translate3f(hx[hi]-hs, hy[hi]-hs, 0);
						gs_draw_sprite(nullptr, 0, (uint32_t)(hs*2), (uint32_t)(hs*2));
						gs_matrix_pop();
					}
					gs_technique_end_pass(ht); gs_technique_end(ht);
				}
			}
		}
	}

	if (ed->view_mode == 1 && sc && sc->mesh.n_vertices() > 0 && solid) {
		int cols = sc->mesh_cols, rows = sc->mesh_rows;
		int nlv = (rows*(cols-1) + cols*(rows-1)) * 2;
		if (nlv > 0) {
			auto &mesh = sc->mesh;
			std::vector<vec3> pts(nlv); int li = 0;
			for (int r = 0; r < rows; r++) for (int c = 0; c < cols-1; c++) {
				Vec2 v0 = mesh.get_vertex_2d(c, r), v1 = mesh.get_vertex_2d(c+1, r);
				pts[li++] = {v0.x,v0.y,0}; pts[li++] = {v1.x,v1.y,0};
			}
			for (int c = 0; c < cols; c++) for (int r = 0; r < rows-1; r++) {
				Vec2 v0 = mesh.get_vertex_2d(c, r), v1 = mesh.get_vertex_2d(c, r+1);
				pts[li++] = {v0.x,v0.y,0}; pts[li++] = {v1.x,v1.y,0};
			}
			struct gs_vb_data *ld = gs_vbdata_create();
			ld->num = nlv; ld->points = (vec3*)bzalloc(nlv*sizeof(vec3));
			ld->colors = (uint32_t*)bzalloc(nlv*sizeof(uint32_t));
			memcpy(ld->points, pts.data(), nlv*sizeof(vec3));
			for (int i = 0; i < nlv; i++) ld->colors[i] = 0x80FFFFFF;
			gs_vertbuffer_t *lb = gs_vertexbuffer_create(ld, GS_DYNAMIC);
			gs_technique_t *t2 = gs_effect_get_technique(solid, "SolidColored");
			if (t2) {
				vec4 wc; vec4_set(&wc,1,1,1,1);
				gs_effect_set_vec4(gs_effect_get_param_by_name(solid,"color"),&wc);
				gs_technique_begin(t2); gs_technique_begin_pass(t2,0);
				gs_load_vertexbuffer(lb); gs_draw(GS_LINES,0,0);
				gs_technique_end_pass(t2); gs_technique_end(t2);
			}
			gs_vertexbuffer_destroy(lb);
		}
		int nv = sc->mesh.n_vertices();
		float vhs = 8.0f / ed->zoom;
		gs_technique_t *vht = gs_effect_get_technique(solid, "Solid");
		if (!vht) vht = gs_effect_get_technique(solid, "Mixed");
		if (vht) {
			vec4 vc; vec4_set(&vc, 0.32f, 0.90f, 0.76f, 1.0f);
			gs_effect_set_vec4(gs_effect_get_param_by_name(solid,"color"),&vc);
			gs_technique_begin(vht); gs_technique_begin_pass(vht,0);
			for (int vi = 0; vi < nv; vi++) {
				Vec2 p = sc->mesh.get_vertex(vi);
				if (ed->drag_idx == vi) { vec4 dc; vec4_set(&dc,1,1,1,1); gs_effect_set_vec4(gs_effect_get_param_by_name(solid,"color"),&dc); }
				gs_matrix_push(); gs_matrix_translate3f(p.x-vhs, p.y-vhs, 0);
				gs_draw_sprite(nullptr,0,(uint32_t)(vhs*2),(uint32_t)(vhs*2));
				gs_matrix_pop();
				if (ed->drag_idx == vi) gs_effect_set_vec4(gs_effect_get_param_by_name(solid,"color"),&vc);
			}
			gs_technique_end_pass(vht); gs_technique_end(vht);
		}
	}
	gs_load_vertexbuffer(nullptr);
	gs_projection_pop(); gs_viewport_pop();
}

/* ================================================================
   PREVIEW MOUSE
   ================================================================ */
static void editor_mouse_to_scene(adv_editor *ed, int mx, int my, float *sx, float *sy)
{
	RECT rc; GetClientRect(ed->preview_hwnd, &rc);
	float ww = (float)(rc.right > 0 ? rc.right : 1);
	float wh = (float)(rc.bottom > 0 ? rc.bottom : 1);

	float cw, ch;
	if (ed->view_mode == 1) {
		cw = (float)ed->canvas_cx;
		ch = (float)ed->canvas_cy;
	} else {
		cw = (float)ed->canvas_cx;
		ch = (float)ed->canvas_cy;
	}
	if (cw == 0) cw = 1920; if (ch == 0) ch = 1080;
	*sx = ed->offset_x + ((float)mx / ww) * (cw / ed->zoom);
	*sy = ed->offset_y + ((float)my / wh) * (ch / ed->zoom);
}
static int editor_find_vertex(adv_editor *ed, float sx, float sy)
{
	auto sc = ed->get_active_slice();
	if (!sc) return -1;
	int nv = sc->mesh.n_vertices();
	float best_d = 144.0f; int best = -1;
	for (int i = 0; i < nv; i++) {
		Vec2 v = sc->mesh.get_vertex(i);
		float dx = sx - v.x, dy = sy - v.y;
		float d = dx*dx + dy*dy;
		if (d < best_d) { best_d = d; best = i; }
	}
	return best;
}
static LRESULT CALLBACK preview_wndproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
	auto *ed = (adv_editor *)GetWindowLongPtrW(GetParent(hwnd), GWLP_USERDATA);
	if (!ed) return DefWindowProcW(hwnd, msg, wp, lp);
	auto sc = ed->get_active_slice();
	float slx = sc ? (float)sc->slice_x : 0, sly = sc ? (float)sc->slice_y : 0;
	float slw = sc ? (float)sc->slice_w : (float)ed->canvas_cx;
	float slh = sc ? (float)sc->slice_h : (float)ed->canvas_cy;
	float out_clamp_w = (float)ed->canvas_cx;
	float out_clamp_h = (float)ed->canvas_cy;
	switch (msg) {
	case WM_LBUTTONDOWN: {
		float sx, sy;
		editor_mouse_to_scene(ed, LOWORD(lp), HIWORD(lp), &sx, &sy);
		if (ed->view_mode == 1 && sc) {
			int idx = editor_find_vertex(ed, sx, sy);
			if (idx >= 0) {
				ed->drag_idx = idx; Vec2 v = sc->mesh.get_vertex(idx);
				ed->drag_off_x = v.x - sx; ed->drag_off_y = v.y - sy;
				SetCapture(hwnd); return 0;
			}
		} else if (sc) {
			float hs = 8.0f / ed->zoom;
			float hx[8] = {slx, slx+slw, slx+slw, slx, slx+slw/2, slx+slw, slx+slw/2, slx};
			float hy[8] = {sly, sly, sly+slh, sly+slh, sly, sly+slh/2, sly+slh, sly+slh/2};
			for (int ci = 0; ci < 8; ci++)
				if (sx >= hx[ci]-hs && sx <= hx[ci]+hs && sy >= hy[ci]-hs && sy <= hy[ci]+hs) {
					ed->slice_drag = 1; ed->slice_drag_corner = ci;
					ed->slice_drag_sx = slx; ed->slice_drag_sy = sly;
					ed->slice_drag_sw = slw; ed->slice_drag_sh = slh;
					ed->slice_drag_mx = sx; ed->slice_drag_my = sy;
					SetCapture(hwnd); return 0;
				}
			if (sx >= slx && sx <= slx+slw && sy >= sly && sy <= sly+slh) {
				ed->slice_drag = 2;
				ed->slice_drag_mx = sx - slx; ed->slice_drag_my = sy - sly;
				SetCapture(hwnd); return 0;
			}
		}
		return 0;
	}
	case WM_LBUTTONUP: {
		if (ed->drag_idx >= 0 && sc) { sc->mesh_dirty = true; ed->drag_idx = -1; }
		if (ed->slice_drag && sc) { sc->mesh_dirty = true; ed->slice_drag = 0; }
		ReleaseCapture(); return 0;
	}
	case WM_MBUTTONDOWN: ed->panning = true; ed->pan_last_x = LOWORD(lp); ed->pan_last_y = HIWORD(lp); SetCapture(hwnd); return 0;
	case WM_MBUTTONUP: ed->panning = false; ReleaseCapture(); return 0;
	case WM_MOUSEMOVE: {
		int mx = LOWORD(lp), my = HIWORD(lp);
		if (ed->panning) {
			float cw = (float)ed->canvas_cx, ch = (float)ed->canvas_cy;
			if (cw==0) cw=1920; if (ch==0) ch=1080;
			float dx = (float)(mx - ed->pan_last_x)*(cw/ed->zoom)/(cw>0?cw:1);
			float dy = (float)(my - ed->pan_last_y)*(ch/ed->zoom)/(ch>0?ch:1);
			ed->offset_x -= dx; ed->offset_y -= dy;
			ed->pan_last_x = mx; ed->pan_last_y = my;
		} else if (ed->slice_drag == 2 && sc) {
			float sx, sy; editor_mouse_to_scene(ed, mx, my, &sx, &sy);
			int sw = sc->slice_w, sh = sc->slice_h;
			int nx = (int)(sx - ed->slice_drag_mx), ny = (int)(sy - ed->slice_drag_my);
			if (nx < 0) nx = 0; if (ny < 0) ny = 0;
			if (nx + sw > (int)ed->canvas_cx) nx = ed->canvas_cx - sw;
			if (ny + sh > (int)ed->canvas_cy) ny = ed->canvas_cy - sh;
			sc->slice_x = nx; sc->slice_y = ny;
		} else if (ed->slice_drag == 1 && sc) {
			float sx, sy; editor_mouse_to_scene(ed, mx, my, &sx, &sy);
			int ci = ed->slice_drag_corner;
			float ox = ed->slice_drag_sx, oy = ed->slice_drag_sy;
			float ow = ed->slice_drag_sw, oh = ed->slice_drag_sh;
			if (ci == 0 || ci == 3 || ci == 7) { int nx=(int)sx; if(nx<0)nx=0; if(nx>(int)(ox+ow-10))nx=(int)(ox+ow-10); sc->slice_x=nx; sc->slice_w=(int)(ox+ow-nx); }
			if (ci == 1 || ci == 2 || ci == 5) { int nw=(int)(sx-ox); if(nw<10)nw=10; if(ox+nw>(int)ed->canvas_cx)nw=ed->canvas_cx-(int)ox; sc->slice_w=nw; }
			if (ci == 0 || ci == 1 || ci == 4) { int ny=(int)sy; if(ny<0)ny=0; if(ny>(int)(oy+oh-10))ny=(int)(oy+oh-10); sc->slice_y=ny; sc->slice_h=(int)(oy+oh-ny); }
			if (ci == 2 || ci == 3 || ci == 6) { int nh=(int)(sy-oy); if(nh<10)nh=10; if(oy+nh>(int)ed->canvas_cy)nh=ed->canvas_cy-(int)oy; sc->slice_h=nh; }
		} else if (ed->drag_idx >= 0 && sc) {
			float sx, sy; editor_mouse_to_scene(ed, mx, my, &sx, &sy);
			float nx = sx + ed->drag_off_x, ny = sy + ed->drag_off_y;
			float bw = (ed->view_mode == 1) ? out_clamp_w : slw;
			float bh = (ed->view_mode == 1) ? out_clamp_h : slh;
			if (nx < 0) nx = 0; if (ny < 0) ny = 0;
			if (nx > bw) nx = bw; if (ny > bh) ny = bh;
			sc->mesh.set_vertex(ed->drag_idx, {nx, ny});
			sc->mesh_dirty = true;
		}
		return 0;
	}
	case WM_MOUSEWHEEL: {
		int delta = GET_WHEEL_DELTA_WPARAM(wp);
		float before = ed->zoom;
		ed->zoom *= (delta > 0) ? 1.1f : (1.0f / 1.1f);
		if (ed->zoom < 0.1f) ed->zoom = 0.1f; if (ed->zoom > 50.0f) ed->zoom = 50.0f;
		POINT pt; pt.x = LOWORD(lp); pt.y = HIWORD(lp); ScreenToClient(hwnd, &pt);
		RECT rc; GetClientRect(hwnd, &rc);
		float ww = (float)(rc.right>0?rc.right:1), wh = (float)(rc.bottom>0?rc.bottom:1);
		float cw = (float)ed->canvas_cx, ch = (float)ed->canvas_cy;
		if (cw==0) cw=1920; if (ch==0) ch=1080;
		float sx = ed->offset_x + ((float)pt.x/ww)*(cw/before);
		float sy = ed->offset_y + ((float)pt.y/wh)*(ch/before);
		ed->offset_x = sx - ((float)pt.x/ww)*(cw/ed->zoom);
		ed->offset_y = sy - ((float)pt.y/wh)*(ch/ed->zoom);
		return 0;
	}
	}
	return DefWindowProcW(hwnd, msg, wp, lp);
}

/* ================================================================
   OUTPUT START/STOP
   ================================================================ */
static void output_draw(void *data, uint32_t cx, uint32_t cy)
{
	auto *d = (DisplayConfig *)data;
	gs_texture_t *tex = obs_get_main_texture();
	if (!tex) return;
	gs_effect_t *eff = obs_get_base_effect(OBS_EFFECT_DEFAULT);
	if (!eff) return;
	gs_eparam_t *img = gs_effect_get_param_by_name(eff, "image");
	if (img) gs_effect_set_texture_srgb(img, tex);
	gs_viewport_push();
	gs_set_viewport(0, 0, (int)cx, (int)cy);
	while (gs_effect_loop(eff, "Draw")) {
		for (auto &sc : d->slices) {
			if (sc.slice_w <= 0 || sc.slice_h <= 0) continue;
			if (sc.mesh_dirty || !sc.vbuf)
				rebuild_vbuf(&sc, d->canvas_cx, d->canvas_cy);
			if (sc.vbuf) {
				gs_load_vertexbuffer(sc.vbuf);
				gs_draw(GS_TRIS, 0, sc.num_verts);
			}
		}
	}
	gs_load_vertexbuffer(nullptr);
	gs_viewport_pop();
}

static BOOL CALLBACK mon_enum_cb(HMONITOR hm, HDC, LPRECT r, LPARAM lp)
{
	auto *list = (std::vector<RECT> *)lp;
	MONITORINFOEXW mi = {sizeof(mi)};
	if (GetMonitorInfoW(hm, &mi)) list->push_back(mi.rcMonitor);
	return TRUE;
}
static bool get_monitor_rect(int monitor, RECT *out)
{
	std::vector<RECT> list;
	EnumDisplayMonitors(nullptr, nullptr, mon_enum_cb, (LPARAM)&list);
	int idx = monitor - 1;
	if (idx >= 0 && idx < (int)list.size()) { *out = list[idx]; return true; }
	if (!list.empty()) { *out = list[0]; return true; }
	return false;
}

static bool start_output(DisplayConfig *d, uint32_t canvas_cx, uint32_t canvas_cy)
{
	if (d->output_active) return true;
	if (d->output_type == 0 || d->output_type == 1) {
		d->output_active = true;
		if (d->output_type == 1) MessageBoxW(nullptr, L"Spout output not yet implemented", L"HLTN", MB_OK);
		return true;
	}
	RECT r;
	if (!get_monitor_rect(d->monitor, &r))
		{r.left=0; r.top=0; r.right=(int)canvas_cx; r.bottom=(int)canvas_cy;}
	int w=r.right-r.left, h=r.bottom-r.top;
	d->output_cx=w; d->output_cy=h;
	HINSTANCE hi = GetModuleHandleW(nullptr);
	d->output_hwnd = CreateWindowExW(WS_EX_APPWINDOW, L"STATIC", L"HLTN Output",
		WS_POPUP, r.left, r.top, w, h, nullptr, nullptr, hi, nullptr);
	if (!d->output_hwnd) return false;
	ShowWindow(d->output_hwnd, SW_SHOW);
	gs_init_data gid={}; gid.cx=w; gid.cy=h; gid.format=GS_BGRA; gid.zsformat=GS_ZS_NONE;
	gid.window.hwnd = d->output_hwnd;
	d->output_display = obs_display_create(&gid, 0);
	if (!d->output_display) { DestroyWindow(d->output_hwnd); d->output_hwnd=nullptr; return false; }
	d->canvas_cx = canvas_cx; d->canvas_cy = canvas_cy;
	for (auto &sc : d->slices)
		sc.mesh_dirty = true;
	obs_display_add_draw_callback(d->output_display, output_draw, d);
	d->output_active = true;
	return true;
}
static void stop_output(DisplayConfig *d)
{
	if (!d->output_active) return;
	if (d->output_display) {
		obs_display_remove_draw_callback(d->output_display, output_draw, d);
		obs_display_destroy(d->output_display); d->output_display = nullptr;
	}
	if (d->output_hwnd) { DestroyWindow(d->output_hwnd); d->output_hwnd = nullptr; }
	d->output_active = false;
}

/* ================================================================
   MAIN WINDOW PROC
   ================================================================ */
static const wchar_t *WND_CLASS = L"HltnAdvEditor2";
static LRESULT CALLBACK editor_wndproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
	auto *ed = (adv_editor *)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
	if (!ed && msg != WM_CREATE) return DefWindowProcW(hwnd, msg, wp, lp);
	switch (msg) {
	case WM_CREATE: { auto *cs = (CREATESTRUCTW *)lp; SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)cs->lpCreateParams); return 0; }
	case WM_CLOSE:
		for (auto &d : ed->displays) stop_output(&d);
		DestroyWindow(hwnd); return 0;
	case WM_DESTROY: return 0;
	case WM_ERASEBKGND: return 1;
	case WM_PAINT: {
		PAINTSTRUCT ps; BeginPaint(hwnd, &ps);
		editor_paint(ed);
		BitBlt(ps.hdc, 0, 0, ed->mem_w, ed->mem_h, ed->mem_dc, 0, 0, SRCCOPY);
		EndPaint(hwnd, &ps); return 0;
	}
	case WM_SIZE: {
		RECT rc; GetClientRect(hwnd, &rc);
		int ww=rc.right, wh=rc.bottom;
		if (ed->mem_dc) SelectObject(ed->mem_dc, GetStockObject(NULL_BRUSH));
		if (ed->mem_bmp) DeleteObject(ed->mem_bmp);
		if (ed->mem_dc) DeleteDC(ed->mem_dc);
		HDC hdc2 = GetDC(hwnd);
		ed->mem_dc = CreateCompatibleDC(hdc2);
		ed->mem_bmp = CreateCompatibleBitmap(hdc2, ww, wh);
		SelectObject(ed->mem_dc, ed->mem_bmp);
		ed->mem_w=ww; ed->mem_h=wh; ReleaseDC(hwnd, hdc2);
		int cw = ww - ed->left_panel_w - ed->right_panel_w;
		if (cw < 200) { ed->left_panel_w=180; ed->right_panel_w=220; cw=ww-400; }
		if (ed->preview_hwnd)
			SetWindowPos(ed->preview_hwnd, nullptr, ed->left_panel_w, ed->toolbar_h, cw, wh-ed->toolbar_h-ed->status_h, SWP_NOZORDER);
		InvalidateRect(hwnd, nullptr, FALSE); return 0;
	}
	case WM_LBUTTONDOWN: {
		int mx=LOWORD(lp), my=HIWORD(lp);
		if (my < ed->toolbar_h) {
			if (mx>=112 && mx<168) { ed->view_mode=0; InvalidateRect(hwnd,nullptr,FALSE); }
			else if (mx>=168 && mx<232) {
				ed->view_mode=1;
				if (ed->active_slice < 0 && ed->active_display >= 0 &&
				    ed->active_display < (int)ed->displays.size() &&
				    !ed->displays[ed->active_display].slices.empty())
					ed->active_slice = 0;
				InvalidateRect(hwnd,nullptr,FALSE);
			}
			else if (mx>=308 && mx<358) {
				float zs[]={0.25f,0.5f,1.0f,2.0f,4.0f}; int zi=0;
				for (int i=0;i<5;i++) if (ed->zoom>=zs[i]*0.9f) zi=i;
				ed->zoom=zs[(zi+1)%5]; InvalidateRect(hwnd,nullptr,FALSE);
			} else {
				RECT cr2; GetClientRect(hwnd,&cr2);
				if (mx>=cr2.right-90 && mx<=cr2.right-10) {
					auto dc2 = (ed->active_display >= 0 && ed->active_display < (int)ed->displays.size())
						? &ed->displays[ed->active_display] : nullptr;
					if (dc2) {
						if (dc2->output_active) stop_output(dc2); else start_output(dc2, ed->canvas_cx, ed->canvas_cy);
						InvalidateRect(hwnd,nullptr,FALSE);
					}
				}
		}
		return 0;
		}
		/* Status bar buttons */
		{ RECT cr; GetClientRect(hwnd, &cr);
		  int sy = cr.bottom - ed->status_h + 3;
		  int sh = ed->status_h - 6;
		  if (my >= sy && my < sy + sh) {
			if (mx >= ed->btn_save_x && mx < ed->btn_save_x + ed->btn_save_w) {
				save_config_default(ed, hwnd); return 0;
			}
			if (mx >= ed->btn_saveas_x && mx < ed->btn_saveas_x + ed->btn_saveas_w) {
				save_config_as(ed, hwnd); return 0;
			}
			if (mx >= ed->btn_load_x && mx < ed->btn_load_x + ed->btn_load_w) {
				load_config(ed, hwnd); return 0;
			}
			if (mx >= ed->btn_close_x && mx < ed->btn_close_x + ed->btn_close_w) {
				SendMessageW(hwnd, WM_CLOSE, 0, 0); return 0;
			}
		  }
		}
		/* Navigator click */
		if (mx < ed->left_panel_w) {
			for (int i = 0; i < ed->nav_count; i++) {
				if (my >= ed->nav_items_y[i] && my < ed->nav_items_y[i]+22) {
					const wchar_t *name = ed->nav_items_name[i];
					if (wcsstr(name, L"Add Slice")) {
						int dd=-1, acc=0;
						for (int d=0;d<(int)ed->displays.size();d++) {
							int sec=1+(int)ed->displays[d].slices.size()+1;
							if (i<acc+sec){dd=d;break;} acc+=sec;
						}
					if (dd>=0) { ed->displays[dd].slices.push_back(SliceConfig()); 
				ed->active_display=dd; ed->active_slice=(int)ed->displays[dd].slices.size()-1;
				layout_output_slices(&ed->displays[dd], ed->canvas_cx, ed->canvas_cy); }
						InvalidateRect(hwnd,nullptr,FALSE); return 0;
					}
					/* Find which display/slice this item is */
					int cur=0;
					for (int d=0;d<(int)ed->displays.size();d++) {
						for (int s=-1;s<(int)ed->displays[d].slices.size();s++) {
							if (cur == i) { ed->active_display=d; ed->active_slice=s; InvalidateRect(hwnd,nullptr,FALSE); return 0; }
							cur++;
						}
						cur++; /* skip add slice */
					}
					break;
				}
			}
			/* Add Display button */
			int by = ed->nav_y0 + ed->nav_count*22 + 4;
			if (my >= by && my < by+26 && ed->displays.size() < 8) {
				ed->displays.push_back(DisplayConfig());
				ed->active_display = (int)ed->displays.size()-1;
				ed->active_slice = -1;
				InvalidateRect(hwnd,nullptr,FALSE); return 0;
			}
		}
		/* Resize handle */
		if (mx>=ed->left_panel_w && mx<ed->left_panel_w+4) {
			ed->left_collapsed = !ed->left_collapsed;
			ed->left_panel_w = ed->left_collapsed ? 0 : 220;
			SendMessageW(hwnd, WM_SIZE, 0, 0); return 0;
		}
		/* Inspector START button */
		int *bs = ed->btn_insp_start;
		if (mx>=bs[0] && mx<=bs[0]+bs[2] && my>=bs[1] && my<=bs[1]+bs[3]) {
			auto dc2 = (ed->active_display >= 0) ? &ed->displays[ed->active_display] : nullptr;
			if (dc2) { if (dc2->output_active) stop_output(dc2); else start_output(dc2, ed->canvas_cx, ed->canvas_cy); }
			InvalidateRect(hwnd,nullptr,FALSE); return 0;
		}
		/* Output type buttons */
		{	RECT cr3; GetClientRect(hwnd,&cr3);
			int ipx = cr3.right - ed->right_panel_w;
			int oty = ed->toolbar_h + 62;
			if (my>=oty && my<oty+20 && mx>ipx+50) {
				int t=(mx-ipx-52)/52;
				if (t>=0 && t<3) {
					auto dc3 = (ed->active_display >= 0 && ed->active_display < (int)ed->displays.size())
						? &ed->displays[ed->active_display] : nullptr;
					if (dc3) { dc3->output_type = t; ed->active_slider = -1; }
					InvalidateRect(hwnd,nullptr,FALSE); return 0;
				}
			}
		}
		/* Slider drag */
		for (int i=0; i<ed->num_sliders; i++) {
			int sy2 = ed->slider_ys[i]+16, sx2=ed->slider_xs[i], sw2=ed->slider_ws[i];
			if (my>=sy2-6 && my<=sy2+10 && mx>=sx2 && mx<=sx2+sw2) {
				ed->active_slider=i; SetCapture(hwnd);
				float frac=(float)(mx-sx2)/(float)sw2;
				if (frac<0)frac=0; if (frac>1)frac=1;
				int v=ed->slider_lo[i]+(int)(frac*(ed->slider_hi[i]-ed->slider_lo[i])+0.5f);
				if (v<ed->slider_lo[i])v=ed->slider_lo[i];
				if (v>ed->slider_hi[i])v=ed->slider_hi[i];
				*ed->slider_vals[i]=v;
				if (ed->get_active_slice()) ed->get_active_slice()->mesh_dirty=true;
				InvalidateRect(hwnd,nullptr,FALSE); return 0;
			}
		}
		return 0;
	}
	case WM_MOUSEMOVE: {
		int mx=LOWORD(lp);
		if (ed->active_slider>=0) {
			int i=ed->active_slider, sx2=ed->slider_xs[i], sw2=ed->slider_ws[i];
			float frac=(float)(mx-sx2)/(float)sw2;
			if (frac<0)frac=0; if (frac>1)frac=1;
			int v=ed->slider_lo[i]+(int)(frac*(ed->slider_hi[i]-ed->slider_lo[i])+0.5f);
			if (v<ed->slider_lo[i])v=ed->slider_lo[i];
			if (v>ed->slider_hi[i])v=ed->slider_hi[i];
			*ed->slider_vals[i]=v;
			if (ed->get_active_slice()) ed->get_active_slice()->mesh_dirty=true;
			InvalidateRect(hwnd,nullptr,FALSE);
		}
		return 0;
	}
	case WM_LBUTTONUP: if (ed->active_slider>=0) { ed->active_slider=-1; ReleaseCapture(); } return 0;
	case WM_RBUTTONDOWN: {
		int mx=LOWORD(lp), my=HIWORD(lp);
		if (mx<ed->left_panel_w && my>ed->toolbar_h && my<ed->toolbar_h+300) {
			HMENU ctx=CreatePopupMenu();
			AppendMenuW(ctx,MF_STRING,1001,L"Add Slice");
			AppendMenuW(ctx,MF_STRING,1002,L"Duplicate");
			AppendMenuW(ctx,MF_STRING,1003,L"Remove");
			POINT pt; GetCursorPos(&pt);
			int cmd=TrackPopupMenu(ctx,TPM_RETURNCMD|TPM_NONOTIFY,pt.x,pt.y,0,hwnd,nullptr);
			DestroyMenu(ctx);
			if (cmd==1001 && ed->active_display>=0 && ed->active_display<(int)ed->displays.size()) {
				auto &d=ed->displays[ed->active_display];
				d.slices.push_back(SliceConfig());
				ed->active_slice=(int)d.slices.size()-1;
				layout_output_slices(&d, ed->canvas_cx, ed->canvas_cy);
				InvalidateRect(hwnd,nullptr,FALSE);
			}
			if (cmd==1003 && ed->active_display>=0 && ed->active_slice>=0) {
				auto &d=ed->displays[ed->active_display];
				if (ed->active_slice<(int)d.slices.size()) {
					d.slices.erase(d.slices.begin()+ed->active_slice);
					if (d.slices.empty()) d.slices.push_back(SliceConfig());
					else layout_output_slices(&d, ed->canvas_cx, ed->canvas_cy);
					ed->active_slice=-1;
					InvalidateRect(hwnd,nullptr,FALSE);
				}
			}
		}
		return 0;
	}
	}
	return DefWindowProcW(hwnd, msg, wp, lp);
}

/* ================================================================
   OPEN / CLOSE
   ================================================================ */
adv_editor *editor_open()
{
	auto *ed = (adv_editor *)bzalloc(sizeof(adv_editor));
	ed->active_display = 0; ed->active_slice = 0;
	ed->displays.push_back(DisplayConfig());
	ed->drag_idx = -1; ed->hover_idx = -1; ed->panning = false;
	ed->offset_x = 0; ed->offset_y = 0; ed->zoom = 1.0f;
	ed->view_mode = 0; ed->active_slider = -1; ed->num_sliders = 0;
	ed->slice_drag = 0; ed->slice_drag_corner = -1;
	ed->running = true;
	ed->left_panel_w = 220; ed->right_panel_w = 280;
	ed->toolbar_h = 32; ed->status_h = 30;
	ed->mem_dc = nullptr; ed->mem_bmp = nullptr;

	obs_video_info ovi;
	if (obs_get_video_info(&ovi)) { ed->canvas_cx = ovi.base_width; ed->canvas_cy = ovi.base_height; }
	else { ed->canvas_cx = 1920; ed->canvas_cy = 1080; }

	if (!ed->displays.empty() && !ed->displays[0].slices.empty()) {
		auto *s0 = &ed->displays[0].slices[0];
		s0->slice_w = ed->canvas_cx; s0->slice_h = ed->canvas_cy;
	}

	init_theme();
	HINSTANCE hi = GetModuleHandleW(nullptr);
	WNDCLASSW wc = {};
	if (!GetClassInfoW(hi, WND_CLASS, &wc)) {
		wc.lpfnWndProc = editor_wndproc; wc.hInstance = hi;
		wc.hCursor = LoadCursorW(nullptr, (LPCWSTR)IDC_ARROW);
		wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
		wc.lpszClassName = WND_CLASS; RegisterClassW(&wc);
	}

	int ww = ed->canvas_cx + ed->left_panel_w + ed->right_panel_w + 20;
	int wh = ed->canvas_cy + ed->toolbar_h + ed->status_h + 40;
	if (ww < 1024) ww = 1024; if (wh < 600) wh = 600;

	ed->hwnd = CreateWindowExW(0, WND_CLASS, L"HLTN Advanced Output",
		WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
		CW_USEDEFAULT, CW_USEDEFAULT, ww, wh, nullptr, nullptr, hi, ed);
	if (!ed->hwnd) { bfree(ed); return nullptr; }

	WNDCLASSW pc = {};
	if (!GetClassInfoW(hi, L"HltnAdvPreview", &pc)) {
		pc.lpfnWndProc = preview_wndproc; pc.hInstance = hi;
		pc.hCursor = LoadCursorW(nullptr, (LPCWSTR)IDC_CROSS);
		pc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
		pc.lpszClassName = L"HltnAdvPreview"; RegisterClassW(&pc);
	}

	RECT cr; GetClientRect(ed->hwnd, &cr);
	int cw = cr.right - ed->left_panel_w - ed->right_panel_w;
	int ch = cr.bottom - ed->toolbar_h - ed->status_h;
	ed->preview_hwnd = CreateWindowExW(0, L"HltnAdvPreview", nullptr,
		WS_CHILD | WS_VISIBLE, ed->left_panel_w, ed->toolbar_h, cw, ch, ed->hwnd, nullptr, hi, nullptr);

	if (ed->preview_hwnd) {
		RECT pr; GetClientRect(ed->preview_hwnd, &pr);
		gs_init_data gid = {};
		gid.cx = pr.right; gid.cy = pr.bottom;
		gid.format = GS_BGRA; gid.zsformat = GS_ZS_NONE;
		gid.window.hwnd = ed->preview_hwnd;
		ed->preview_display = obs_display_create(&gid, 0);
		if (ed->preview_display) obs_display_add_draw_callback(ed->preview_display, preview_draw, ed);
	}
	ShowWindow(ed->hwnd, SW_SHOW); UpdateWindow(ed->hwnd);
	{
		std::wstring def = get_default_config_path();
		xml_load(ed, def.c_str());
	}
	InvalidateRect(ed->hwnd, nullptr, FALSE);
	return ed;
}

void editor_close(adv_editor *ed)
{
	if (!ed) return;
	for (auto &d : ed->displays) stop_output(&d);
	if (ed->preview_display) { obs_display_remove_draw_callback(ed->preview_display, preview_draw, ed); obs_display_destroy(ed->preview_display); }
	if (ed->hwnd) DestroyWindow(ed->hwnd);
	if (ed->mem_dc) SelectObject(ed->mem_dc, GetStockObject(NULL_BRUSH));
	if (ed->mem_bmp) DeleteObject(ed->mem_bmp);
	if (ed->mem_dc) DeleteDC(ed->mem_dc);
	obs_enter_graphics();
	for (auto &d : ed->displays)
		for (auto &s : d.slices)
			if (s.vbuf) { gs_vertexbuffer_destroy(s.vbuf); s.vbuf = nullptr; }
	obs_leave_graphics();
	done_theme();
	bfree(ed);
}
