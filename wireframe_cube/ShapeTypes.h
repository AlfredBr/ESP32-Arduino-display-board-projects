#pragma once
#include <stdint.h>

namespace Cube
{
	constexpr int MAX_VERTICES = 60;
	constexpr int MAX_EDGES = 90;
	enum class Shape
	{
		Cube,
		Tetrahedron,
		Octahedron,
		Dodecahedron,
		Icosahedron,
		TruncatedIcosahedron,
		Count
	};
	inline const char* shapeName(Shape shape) {
		switch (shape) {
			case Shape::Cube: return "Cube";
			case Shape::Tetrahedron: return "Tetrahedron";
			case Shape::Octahedron: return "Octahedron";
			case Shape::Dodecahedron: return "Dodecahedron";
			case Shape::Icosahedron: return "Icosahedron";
			case Shape::TruncatedIcosahedron: return "Truncated Icosahedron";
			default: return "";
		}
	}
	struct Vertex
	{
		float x, y, z;
	};
	struct Point
	{
		float x, y, z;
	};
	struct Color
	{
		float r, g, b;
	};
	struct Mesh
	{
		const Vertex *vertices;
		int vertexCount;
		const uint8_t (*edges)[2];
		int edgeCount;
		Color color, depthColor;
	};
}
