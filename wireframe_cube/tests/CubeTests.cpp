#include "../CubeAnimation.h"
#include "../ShapeVisibility.h"
#include <assert.h>
#include <stdio.h>
#include <vector>
#include <limits>

static uint64_t hash(const uint16_t* frame, size_t count) {
  uint64_t result = 1469598103934665603ULL;
  for (size_t i = 0; i < count; ++i) result = (result ^ frame[i]) * 1099511628211ULL;
  return result;
}
static void preview(const char* path, const uint16_t* frame) {
  FILE* file = fopen(path, "wb");
  assert(file);
  const int w = CubeConfig::WIDTH, h = CubeConfig::HEIGHT;
  const uint32_t size = 54 + w * h * 3;
  uint8_t header[54] = {};
  header[0] = 'B'; header[1] = 'M'; header[10] = 54; header[14] = 40;
  header[26] = 1; header[28] = 24;
  for (int i = 0; i < 4; ++i) {
    header[2+i] = static_cast<uint8_t>(size >> (i*8));
    header[18+i] = static_cast<uint8_t>(w >> (i*8));
    header[22+i] = static_cast<uint8_t>(h >> (i*8));
  }
  fwrite(header, 1, sizeof(header), file);
  for (int y = h-1; y >= 0; --y) for (int x = 0; x < w; ++x) {
    const uint16_t p = frame[y*w+x];
    const uint8_t rgb[3] = {static_cast<uint8_t>((p&31)*255/31),
      static_cast<uint8_t>(((p>>5)&63)*255/63), static_cast<uint8_t>((p>>11)*255/31)};
    fwrite(rgb, 1, 3, file);
  }
  assert(fclose(file) == 0);
}
int main(int argc, char** argv) {
  const Cube::Mesh meshes[] = {Cube::CubeShape::MESH, Cube::TetrahedronShape::MESH,
    Cube::OctahedronShape::MESH, Cube::DodecahedronShape::MESH, Cube::IcosahedronShape::MESH,
    Cube::TruncatedIcosahedronShape::MESH};
  const int expectedEdges[] = {12, 6, 12, 30, 30, 90};
  const int expectedDegrees[] = {3, 3, 4, 3, 5, 3};
  for (int shape = 0; shape < static_cast<int>(Cube::Shape::Count); ++shape) {
    const auto& mesh = meshes[shape];
    const auto faces = Cube::makeEdgeFaces(mesh);
    if (shape == static_cast<int>(Cube::Shape::TruncatedIcosahedron)) {
      std::vector<Cube::Vertex> normals;
      int pentagons = 0, hexagons = 0;
      for (int edge = 0; edge < mesh.edgeCount; ++edge) {
        for (const auto& plane : faces.faces[edge]) {
          const auto n = plane.normal;
          bool seen = false;
          for (const auto& old : normals)
            if (fabsf(n.x-old.x)+fabsf(n.y-old.y)+fabsf(n.z-old.z) < 0.0001f) seen = true;
          if (seen) continue;
          normals.push_back(n);
          int vertices = 0;
          for (int i = 0; i < mesh.vertexCount; ++i) {
            const auto& v = mesh.vertices[i];
            if (fabsf(n.x*v.x+n.y*v.y+n.z*v.z-1) < 0.0001f) ++vertices;
          }
          assert(vertices == 5 || vertices == 6);
          pentagons += vertices == 5;
          hexagons += vertices == 6;
        }
      }
      assert(pentagons == 12 && hexagons == 20);
    }
    for (int edge = 0; edge < mesh.edgeCount; ++edge) {
      for (const auto& face : faces.faces[edge]) {
        assert(face.offset == 1);
        for (int vertex = 0; vertex < mesh.vertexCount; ++vertex) {
          const auto& v = mesh.vertices[vertex];
          assert(face.normal.x*v.x + face.normal.y*v.y + face.normal.z*v.z < 1.0001f);
        }
        for (int endpoint : mesh.edges[edge]) {
          const auto& v = mesh.vertices[endpoint];
          assert(fabsf(face.normal.x*v.x + face.normal.y*v.y + face.normal.z*v.z - 1) < 0.0001f);
        }
      }
    }
    assert(mesh.edgeCount == expectedEdges[shape]);
    int degrees[Cube::MAX_VERTICES] = {};
    float edgeLength2 = 0;
    for (int i = 0; i < mesh.edgeCount; ++i) {
      const int a = mesh.edges[i][0], b = mesh.edges[i][1];
      assert(a >= 0 && a < mesh.vertexCount && b > a && b < mesh.vertexCount);
      ++degrees[a]; ++degrees[b];
      const auto& u = mesh.vertices[a]; const auto& v = mesh.vertices[b];
      const float length2 = (u.x-v.x)*(u.x-v.x) + (u.y-v.y)*(u.y-v.y) + (u.z-v.z)*(u.z-v.z);
      if (i == 0) edgeLength2 = length2;
      assert(fabsf(length2 - edgeLength2) < 0.0001f);
      for (int j = 0; j < i; ++j)
        assert(mesh.edges[j][0] != a || mesh.edges[j][1] != b);
    }
    for (int i = 0; i < mesh.vertexCount; ++i) {
      assert(degrees[i] == expectedDegrees[shape]);
      const auto& v = mesh.vertices[i];
      assert(fabsf(v.x*v.x + v.y*v.y + v.z*v.z - 3) < 0.0001f);
    }
  }
  const auto cubeFaces = Cube::makeEdgeFaces(Cube::CubeShape::MESH);
  int frontEdges = 0, angledEdges = 0;
  for (int edge = 0; edge < 12; ++edge) {
    frontEdges += Cube::edgeVisible(cubeFaces, edge, {0,0,-5.5f});
    angledEdges += Cube::edgeVisible(cubeFaces, edge, {5,4,-6});
  }
  assert(frontEdges == 4 && angledEdges == 9);
  constexpr size_t count = CubeConfig::WIDTH * CubeConfig::HEIGHT;
  std::vector<uint16_t> guarded(count + 2, 0xA55A), duplicate(count);
  uint16_t* frame = guarded.data() + 1;
  Cube::Animation timing;
  assert(timing.shape() == Cube::Shape::Cube);
  timing.update(19.75f);
  assert(timing.shape() == Cube::Shape::Cube);
  timing.update(0.25f);
  assert(timing.shape() == Cube::Shape::Tetrahedron && timing.vertexCount() == 4);
  timing.draw(frame);
  uint16_t gold = 0;
  for (size_t i = 0; i < count; ++i) if (frame[i] > gold) gold = frame[i];
  assert((gold >> 11) > 15 && ((gold >> 5) & 63) > 20 && (gold & 31) < 5);
  timing.update(20);
  assert(timing.shape() == Cube::Shape::Octahedron && timing.vertexCount() == 6);
  timing.draw(frame);
  uint16_t magenta = 0;
  for (size_t i = 0; i < count; ++i) if (frame[i] > magenta) magenta = frame[i];
  assert((magenta >> 11) > 15 && (magenta & 31) > 15 && ((magenta >> 5) & 63) < 15);
  timing.update(20);
  assert(timing.shape() == Cube::Shape::Dodecahedron && timing.vertexCount() == 20);
  timing.draw(frame);
  uint16_t green = 0;
  for (size_t i = 0; i < count; ++i) if (frame[i] > green) green = frame[i];
  assert(((green >> 5) & 63) > 30 && (green >> 11) < 8 && (green & 31) < 8);
  timing.update(20);
  assert(timing.shape() == Cube::Shape::Icosahedron && timing.vertexCount() == 12);
  timing.draw(frame);
  uint16_t pink = 0;
  for (size_t i = 0; i < count; ++i) if (frame[i] > pink) pink = frame[i];
  assert((pink >> 11) > 15 && (pink >> 11) > (pink & 31));
  assert((pink & 31) > 10 && ((pink >> 5) & 63) < 2 * (pink >> 11));
  timing.update(20);
  assert(timing.shape() == Cube::Shape::TruncatedIcosahedron && timing.vertexCount() == 60);
  timing.draw(frame);
  timing.update(20);
  assert(timing.shape() == Cube::Shape::Cube && timing.vertexCount() == 8);
  Cube::Animation cube, replay;
  cube.draw(frame);
  const uint64_t initial = hash(frame, count);
  if (argc > 1) preview(argv[1], frame);
  int lit = 0, intermediate = 0;
  for (size_t i = 0; i < count; ++i) {
    if (frame[i]) ++lit;
    if ((frame[i]&31) > 0 && (frame[i]&31) < 15) ++intermediate;
  }
  assert(lit > 1000 && lit < 10000 && intermediate > 100);
  cube.update(std::numeric_limits<float>::quiet_NaN());
  cube.update(-1);
  cube.draw(frame);
  assert(hash(frame, count) == initial);
  cube.draw(nullptr);
  for (int step = 0; step < 36000; ++step) {
    cube.update(1.0f/60);
    replay.update(1.0f/60);
    Cube::Point points[Cube::MAX_VERTICES];
    cube.project(points);
    for (int vertex = 0; vertex < cube.vertexCount(); ++vertex) {
      const auto& p = points[vertex];
      assert(isfinite(p.x) && isfinite(p.y) && isfinite(p.z));
      assert(p.x > 4 && p.x < CubeConfig::WIDTH-5);
      assert(p.y > 4 && p.y < CubeConfig::HEIGHT-5);
      const float scale = CubeConfig::FOCAL_LENGTH / (CubeConfig::CAMERA_DISTANCE + p.z);
      const float x = (p.x - (CubeConfig::WIDTH-1)*0.5f) / scale;
      const float y = (p.y - (CubeConfig::HEIGHT-1)*0.5f) / scale;
      assert(fabsf(x*x + y*y + p.z*p.z - 3) < 0.0001f);
    }
    if (step < 180 || step % 60 == 0) {
      cube.draw(frame);
      replay.draw(duplicate.data());
      assert(hash(frame,count) == hash(duplicate.data(),count));
      assert(guarded.front() == 0xA55A && guarded.back() == 0xA55A);
      for (int x = 0; x < CubeConfig::WIDTH; ++x) {
        assert(frame[x] == 0 && frame[(CubeConfig::HEIGHT-1)*CubeConfig::WIDTH+x] == 0);
      }
    }
    if (step == 0) assert(hash(frame,count) != initial);
  }
  puts("PASS: six-shape geometry, face planes, cycle timing, colors, antialias coverage, motion, guarded framebuffer, projection bounds, rigid rotation, 10-minute simulation and deterministic replay");
}

