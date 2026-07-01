#include "warp-mesh.h"
#include <cassert>

WarpMesh::WarpMesh() { resize(2, 2); }
WarpMesh::WarpMesh(int columns, int rows) { resize(columns, rows); }

void WarpMesh::resize(int columns, int rows)
{
	assert(columns >= 2 && rows >= 2);
	m_columns = columns;
	m_rows = rows;
	m_vertices.resize(columns * rows);
	m_index_2d.resize(columns * rows);
	build();
}

void WarpMesh::build()
{
	rebuild_index();
	reorder_vertices();
}

void WarpMesh::rebuild_index()
{
	m_index_2d.resize(m_columns * m_rows);
	for (int y = 0; y < m_rows; y++) {
		for (int x = 0; x < m_columns; x++) {
			m_index_2d[idx(x, y)] = y * m_columns + x;
		}
	}
}

void WarpMesh::reorder_vertices()
{
	std::vector<Vec2> new_verts(m_vertices.size());
	for (int y = 0; y < m_rows; y++) {
		for (int x = 0; x < m_columns; x++) {
			new_verts[y * m_columns + x] =
				m_vertices[m_index_2d[idx(x, y)]];
		}
	}
	m_vertices = new_verts;
	rebuild_index();
}

void WarpMesh::resize_preserve(int new_columns, int new_rows)
{
	if (new_columns == m_columns && new_rows == m_rows) return;

	int old_cols = m_columns, old_rows = m_rows;
	std::vector<Vec2> old_verts = m_vertices;

	resize(new_columns, new_rows);

	for (int y = 0; y < new_rows; y++) {
		for (int x = 0; x < new_columns; x++) {
			float u = (float)x * (float)(old_cols - 1) / (float)(new_columns - 1);
			float v = (float)y * (float)(old_rows - 1) / (float)(new_rows - 1);

			int ix = (int)u;
			int iy = (int)v;
			float fx = u - (float)ix;
			float fy = v - (float)iy;

			if (ix >= old_cols - 1) { ix = old_cols - 2; fx = 1.0f; }
			if (iy >= old_rows - 1) { iy = old_rows - 2; fy = 1.0f; }
			if (ix < 0) { ix = 0; fx = 0.0f; }
			if (iy < 0) { iy = 0; fy = 0.0f; }

			Vec2 v00 = old_verts[iy * old_cols + ix];
			Vec2 v10 = old_verts[iy * old_cols + ix + 1];
			Vec2 v01 = old_verts[(iy + 1) * old_cols + ix];
			Vec2 v11 = old_verts[(iy + 1) * old_cols + ix + 1];

			Vec2 result = {
				v00.x + fx * (v10.x - v00.x) + fy * (v01.x - v00.x) + fx * fy * (v11.x - v10.x - v01.x + v00.x),
				v00.y + fx * (v10.y - v00.y) + fy * (v01.y - v00.y) + fx * fy * (v11.y - v10.y - v01.y + v00.y)};
			set_vertex_2d(x, y, result);
		}
	}
}

void WarpMesh::init_full_rect(int columns, int rows,
			      float x0, float y0, float w, float h)
{
	resize(columns, rows);
	for (int y = 0; y < rows; y++) {
		for (int x = 0; x < columns; x++) {
			float fx = (float)x / (float)(columns - 1);
			float fy = (float)y / (float)(rows - 1);
			m_vertices[y * columns + x] = {
				x0 + fx * w,
				y0 + fy * h};
		}
	}
}

Vec2 WarpMesh::get_vertex_2d(int x, int y) const
{
	return m_vertices[m_index_2d[idx(x, y)]];
}

void WarpMesh::set_vertex_2d(int x, int y, Vec2 v)
{
	m_vertices[m_index_2d[idx(x, y)]] = v;
}

std::vector<std::array<int, 4>> WarpMesh::get_quads() const
{
	std::vector<std::array<int, 4>> quads;
	int hq = (m_columns > 1) ? (m_columns - 1) : 0;
	int vq = (m_rows > 1) ? (m_rows - 1) : 0;
	if (hq < 1 || vq < 1)
		return quads;

	quads.reserve(hq * vq);
	for (int y = 0; y < vq; y++) {
		for (int x = 0; x < hq; x++) {
			int i0 = m_index_2d[idx(x, y)];
			int i1 = m_index_2d[idx(x + 1, y)];
			int i2 = m_index_2d[idx(x + 1, y + 1)];
			int i3 = m_index_2d[idx(x, y + 1)];
			quads.push_back({i0, i1, i2, i3});
		}
	}
	return quads;
}
