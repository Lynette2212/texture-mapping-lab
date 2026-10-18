#include "Primitives.h"

#include <cmath>

namespace Primitives {

namespace {
constexpr float kPi = 3.14159265358979323846f;

void pushVertex(MeshData& m, float px, float py, float pz, float nx, float ny, float nz, float u, float v)
{
    m.vertices.insert(m.vertices.end(), {px, py, pz, nx, ny, nz, u, v});
}
} // namespace

MeshData makeSphere(float radius, int slices, int stacks)
{
    MeshData m;
    // 纬度 phi 从北极(0)到南极(π)，经度 theta 绕 Y 轴一圈(0~2π)
    for (int i = 0; i <= stacks; ++i) {
        const float phi = kPi * float(i) / float(stacks);
        const float y = std::cos(phi);
        const float r = std::sin(phi);
        for (int j = 0; j <= slices; ++j) {
            const float theta = 2.0f * kPi * float(j) / float(slices);
            const float x = r * std::sin(theta);
            const float z = r * std::cos(theta);
            // 单位球上的点本身就是法线
            pushVertex(m, radius * x, radius * y, radius * z, x, y, z,
                       float(j) / float(slices), 1.0f - float(i) / float(stacks));
        }
    }
    const int ring = slices + 1;
    for (int i = 0; i < stacks; ++i) {
        for (int j = 0; j < slices; ++j) {
            const unsigned a = unsigned(i * ring + j);
            const unsigned b = unsigned((i + 1) * ring + j);
            // 逆时针绕序（从球外看），便于开启背面剔除
            m.indices.insert(m.indices.end(), {a, b, a + 1, a + 1, b, b + 1});
        }
    }
    return m;
}

MeshData makeCube(float h)
{
    MeshData m;
    // 每个面：法线 n，以及面内两个方向 s、t（s × t = n）
    struct Face { float n[3], s[3], t[3]; };
    const Face faces[6] = {
        {{ 1, 0, 0}, { 0, 0,-1}, {0, 1, 0}},
        {{-1, 0, 0}, { 0, 0, 1}, {0, 1, 0}},
        {{ 0, 1, 0}, { 1, 0, 0}, {0, 0,-1}},
        {{ 0,-1, 0}, { 1, 0, 0}, {0, 0, 1}},
        {{ 0, 0, 1}, { 1, 0, 0}, {0, 1, 0}},
        {{ 0, 0,-1}, {-1, 0, 0}, {0, 1, 0}},
    };
    const float corner[4][2] = {{-1, -1}, {1, -1}, {1, 1}, {-1, 1}};
    for (const Face& f : faces) {
        const unsigned base = unsigned(m.vertices.size() / MeshData::kStride);
        for (const auto& c : corner) {
            float p[3];
            for (int k = 0; k < 3; ++k)
                p[k] = h * (f.n[k] + c[0] * f.s[k] + c[1] * f.t[k]);
            pushVertex(m, p[0], p[1], p[2], f.n[0], f.n[1], f.n[2], (c[0] + 1) * 0.5f, (c[1] + 1) * 0.5f);
        }
        m.indices.insert(m.indices.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
    }
    return m;
}

MeshData makeQuad(const float o[3], const float eu[3], const float ev[3], float repeatU, float repeatV)
{
    MeshData m;
    float n[3] = {eu[1] * ev[2] - eu[2] * ev[1], eu[2] * ev[0] - eu[0] * ev[2], eu[0] * ev[1] - eu[1] * ev[0]};
    const float len = std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
    for (float& c : n)
        c /= (len > 0 ? len : 1.0f);

    const float uv[4][2] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
    for (const auto& c : uv) {
        float p[3];
        for (int k = 0; k < 3; ++k)
            p[k] = o[k] + c[0] * eu[k] + c[1] * ev[k];
        pushVertex(m, p[0], p[1], p[2], n[0], n[1], n[2], c[0] * repeatU, c[1] * repeatV);
    }
    m.indices = {0, 1, 2, 0, 2, 3};
    return m;
}

} // namespace Primitives
