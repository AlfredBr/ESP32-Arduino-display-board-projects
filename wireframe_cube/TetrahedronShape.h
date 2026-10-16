#pragma once
#include "ShapeTypes.h"

namespace Cube
{
	namespace TetrahedronShape
	{
		constexpr Vertex VERTICES[4] = {
			{1.0000000000f, 1.0000000000f, 1.0000000000f},
			{1.0000000000f, -1.0000000000f, -1.0000000000f},
			{-1.0000000000f, 1.0000000000f, -1.0000000000f},
			{-1.0000000000f, -1.0000000000f, 1.0000000000f}};
		constexpr uint8_t EDGES[6][2] = {
			{0, 1},
			{0, 2},
			{0, 3},
			{1, 2},
			{1, 3},
			{2, 3}};
		constexpr Mesh MESH = {VERTICES, 4, EDGES, 6, {255, 175, 15}, {0, 40, 20}};
	}
}
