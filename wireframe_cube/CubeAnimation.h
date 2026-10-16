#pragma once
#include <math.h>
#include <stdint.h>
#include <string.h>
#include "CubeConfig.h"
#include "CubeShape.h"
#include "TetrahedronShape.h"
#include "OctahedronShape.h"
#include "DodecahedronShape.h"
#include "IcosahedronShape.h"
#include "TruncatedIcosahedronShape.h"
#if HIDDEN_LINE_REMOVAL
#include "ShapeVisibility.h"
#endif

namespace Cube {
constexpr float TAU = 6.28318530718f;
#if HIDDEN_LINE_REMOVAL
constexpr EdgeFaces SHAPE_FACES[] = {
  makeEdgeFaces(CubeShape::MESH), makeEdgeFaces(TetrahedronShape::MESH),
  makeEdgeFaces(OctahedronShape::MESH), makeEdgeFaces(DodecahedronShape::MESH),
  makeEdgeFaces(IcosahedronShape::MESH), makeEdgeFaces(TruncatedIcosahedronShape::MESH)
};
#endif
class Animation {
  float angleX = 0.35f, angleY = 0.55f, angleZ = 0.12f;
  double shapeSeconds = 0;
  Mesh mesh() const {
    switch (shape()) {
      case Shape::Tetrahedron: return TetrahedronShape::MESH;
      case Shape::Octahedron: return OctahedronShape::MESH;
      case Shape::Dodecahedron: return DodecahedronShape::MESH;
      case Shape::Icosahedron: return IcosahedronShape::MESH;
      case Shape::TruncatedIcosahedron: return TruncatedIcosahedronShape::MESH;
      default: return CubeShape::MESH;
    }
  }
  static float clamp(float v, float low, float high) {
    return fmaxf(low, fminf(high, v));
  }
  void pixel(uint16_t* frame, int x, int y, float light, float depth) const {
    if (x < 0 || x >= CubeConfig::WIDTH || y < 0 || y >= CubeConfig::HEIGHT) return;
    const Mesh current = mesh();
    const float red = current.color.r + current.depthColor.r * depth;
    const float green = current.color.g + current.depthColor.g * depth;
    const float blue = current.color.b + current.depthColor.b * depth;
    const int r = static_cast<int>(red * light) >> 3;
    const int g = static_cast<int>(green * light) >> 2;
    const int b = static_cast<int>(blue * light) >> 3;
    uint16_t& previous = frame[y * CubeConfig::WIDTH + x];
    // Maximum blending keeps shared vertices from flashing as edges overlap.
    const int oldR = previous >> 11, oldG = (previous >> 5) & 63, oldB = previous & 31;
    previous = static_cast<uint16_t>((((r > oldR) ? r : oldR) << 11) |
      (((g > oldG) ? g : oldG) << 5) | ((b > oldB) ? b : oldB));
  }
  void line(uint16_t* frame, Point a, Point b) const {
    const float dx = b.x - a.x, dy = b.y - a.y;
    const float length2 = dx * dx + dy * dy;
    if (length2 < 0.0001f) return;
    const bool horizontal = fabsf(dx) >= fabsf(dy);
    const float majorA = horizontal ? a.x : a.y, majorB = horizontal ? b.x : b.y;
    const float minorA = horizontal ? a.y : a.x;
    const float majorDelta = horizontal ? dx : dy, minorDelta = horizontal ? dy : dx;
    const int limit = horizontal ? CubeConfig::WIDTH : CubeConfig::HEIGHT;
    const int first = static_cast<int>(fmaxf(0, floorf(fminf(majorA, majorB) - 4)));
    const int last = static_cast<int>(fminf(limit - 1, ceilf(fmaxf(majorA, majorB) + 4)));
    const float depth = clamp(0.5f - (a.z + b.z) * 0.14f, 0, 1);
    const float brightness = 0.48f + 0.52f * depth;
    for (int major = first; major <= last; ++major) {
      const float t = clamp((major - majorA) / majorDelta, 0, 1);
      const int center = static_cast<int>(floorf(minorA + minorDelta * t));
      for (int minor = center - 4; minor <= center + 4; ++minor) {
        const int x = horizontal ? major : minor, y = horizontal ? minor : major;
        const float along = clamp(((x - a.x) * dx + (y - a.y) * dy) / length2, 0, 1);
        const float ox = x - a.x - along * dx, oy = y - a.y - along * dy;
        const float distance2 = ox * ox + oy * oy;
        if (distance2 >= 12.25f) continue;
        const float distance = sqrtf(distance2);
        const float core = clamp(1.65f - distance, 0, 1);
        const float halo = 0.13f * clamp(1 - distance / 3.5f, 0, 1);
        pixel(frame, x, y, brightness * fmaxf(core, halo), depth);
      }
    }
  }
public:
  Shape shape() const {
    return static_cast<Shape>(static_cast<int>(shapeSeconds / CubeConfig::SHAPE_SECONDS));
  }
  int vertexCount() const { return mesh().vertexCount; }
  void update(float seconds) {
    if (!isfinite(seconds) || seconds <= 0) return;
    // Keep the cycle tied to elapsed time even after a delayed frame.
    shapeSeconds = fmod(shapeSeconds + seconds, static_cast<int>(Shape::Count) * static_cast<double>(CubeConfig::SHAPE_SECONDS));
    seconds = fminf(seconds, 0.1f);
    angleX = fmodf(angleX + seconds * CubeConfig::SPEED_X, TAU);
    angleY = fmodf(angleY + seconds * CubeConfig::SPEED_Y, TAU);
    angleZ = fmodf(angleZ + seconds * CubeConfig::SPEED_Z, TAU);
  }
  void project(Point (&points)[MAX_VERTICES]) const {
    const float cx = cosf(angleX), sx = sinf(angleX);
    const float cy = cosf(angleY), sy = sinf(angleY);
    const float cz = cosf(angleZ), sz = sinf(angleZ);
    const Mesh current = mesh();
    for (int i = 0; i < current.vertexCount; ++i) {
      const Vertex& v = current.vertices[i];
      const float y = v.y * cx - v.z * sx, z = v.y * sx + v.z * cx;
      const float x = v.x * cy + z * sy, depth = -v.x * sy + z * cy;
      const float scale = CubeConfig::FOCAL_LENGTH / (CubeConfig::CAMERA_DISTANCE + depth);
      points[i] = {(CubeConfig::WIDTH - 1) * 0.5f + (x * cz - y * sz) * scale,
                   (CubeConfig::HEIGHT - 1) * 0.5f + (x * sz + y * cz) * scale, depth};
    }
  }
  void draw(uint16_t* frame) const {
    if (!frame) return;
    memset(frame, 0, CubeConfig::WIDTH * CubeConfig::HEIGHT * sizeof(uint16_t));
    Point points[MAX_VERTICES];
    project(points);
    const Mesh current = mesh();
#if HIDDEN_LINE_REMOVAL
    // Inverse-rotate the perspective camera (0,0,-distance) into model space.
    // Z rotation leaves that camera position unchanged.
    const float distance = CubeConfig::CAMERA_DISTANCE;
    const Vertex camera{distance * sinf(angleY),
      -distance * cosf(angleY) * sinf(angleX),
      -distance * cosf(angleY) * cosf(angleX)};
    const EdgeFaces& faces = SHAPE_FACES[static_cast<int>(shape())];
#endif
    for (int i = 0; i < current.edgeCount; ++i) {
#if HIDDEN_LINE_REMOVAL
      if (!edgeVisible(faces, i, camera)) continue;
#endif
      const auto& edge = current.edges[i];
      line(frame, points[edge[0]], points[edge[1]]);
    }
  }
};
}

