#pragma once
#include <Canis/Math.hpp>
#include <vector>
#include <string>
#include <cstdint>

namespace Canis {
// Queries describe both eyes for single-pass VR. A primitive survives if either eye sees it.
struct VisibilityView {
    Matrix4 clip[2]{Matrix4(1),Matrix4(1)};
    Vector3 eye[2]{};
    int count=1;
};
struct BakedVisibility {
    struct Bounds { Vector3 minimum{},maximum{}; };
    std::vector<Bounds> bounds;
    std::vector<std::vector<unsigned>> hidden;
    Vector3 minimum{};
    glm::ivec3 dimensions{};
    float cellSize=0;
    bool Load(const std::string& path, uint32_t crc, size_t primitiveCount);
    int Cell(const Vector3& eye) const;
    void Query(const VisibilityView& view, const Matrix4& model, std::vector<unsigned char>& rejected) const;
    bool Visible(size_t primitive, const VisibilityView& view, const Matrix4& model) const;
};
}
