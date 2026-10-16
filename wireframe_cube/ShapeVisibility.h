#pragma once
#include "ShapeTypes.h"

namespace Cube {
struct FacePlane { Vertex normal; float offset; };
struct EdgeFaces { FacePlane faces[30][2] = {}; };

// Find the two supporting face planes at each edge of a convex solid.
// Evaluated at compile time; no face discovery is needed while animating.
constexpr EdgeFaces makeEdgeFaces(const Mesh& mesh) {
  EdgeFaces result{};
  for (int e = 0; e < mesh.edgeCount; ++e) {
    const Vertex a = mesh.vertices[mesh.edges[e][0]];
    const Vertex b = mesh.vertices[mesh.edges[e][1]];
    int found = 0;
    for (int k = 0; k < mesh.vertexCount && found < 2; ++k) {
      const Vertex c = mesh.vertices[k];
      Vertex n{(b.y-a.y)*(c.z-a.z)-(b.z-a.z)*(c.y-a.y),
               (b.z-a.z)*(c.x-a.x)-(b.x-a.x)*(c.z-a.z),
               (b.x-a.x)*(c.y-a.y)-(b.y-a.y)*(c.x-a.x)};
      float offset = n.x*a.x + n.y*a.y + n.z*a.z;
      if (offset < 0) { n = {-n.x,-n.y,-n.z}; offset = -offset; }
      if (offset < 0.0001f) continue;
      bool supporting = true;
      for (int j = 0; j < mesh.vertexCount; ++j) {
        const Vertex v = mesh.vertices[j];
        if (n.x*v.x + n.y*v.y + n.z*v.z > offset + 0.0001f)
          supporting = false;
      }
      if (!supporting) continue;
      // Scaling to offset=1 also makes coplanar candidates easy to deduplicate.
      n = {n.x/offset, n.y/offset, n.z/offset};
      if (found == 1) {
        const Vertex old = result.faces[e][0].normal;
        const float dx = n.x-old.x, dy = n.y-old.y, dz = n.z-old.z;
        if (dx*dx + dy*dy + dz*dz < 0.0001f) continue;
      }
      result.faces[e][found++] = {n, 1};
    }
  }
  return result;
}

inline bool edgeVisible(const EdgeFaces& faces, int edge, Vertex camera) {
  for (const auto& face : faces.faces[edge]) {
    if (face.normal.x*camera.x + face.normal.y*camera.y +
        face.normal.z*camera.z >= face.offset - 0.00001f) return true;
  }
  return false;
}
}
