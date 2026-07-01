#pragma once
#include <vector>
#include <cstdint>
#include <array>

struct Vec2 {
	float x, y;
};

class WarpMesh {
public:
	WarpMesh();
	WarpMesh(int columns, int rows);

	void resize(int columns, int rows);
	void resize_preserve(int new_columns, int new_rows);
	void build();

	void init_full_rect(int columns, int rows,
			    float x0, float y0, float w, float h);

	int n_columns() const { return m_columns; }
	int n_rows() const { return m_rows; }
	int n_vertices() const { return (int)m_vertices.size(); }

	Vec2 get_vertex(int i) const { return m_vertices[i]; }
	void set_vertex(int i, Vec2 v) { m_vertices[i] = v; }

	Vec2 get_vertex_2d(int x, int y) const;
	void set_vertex_2d(int x, int y, Vec2 v);

	std::vector<std::array<int, 4>> get_quads() const;

private:
	int m_columns = 2;
	int m_rows = 2;
	std::vector<Vec2> m_vertices;
	std::vector<int> m_index_2d;

	int idx(int x, int y) const { return x * m_rows + y; }
	void rebuild_index();
	void reorder_vertices();
};
