#pragma once

#include <vector>

// 简单几何体生成（任务1/任务2共用）
// 顶点交错存放：位置(3) + 法线(3) + 纹理坐标(2)，每个顶点 8 个 float
namespace Primitives {

struct MeshData {
    std::vector<float> vertices;        // 交错顶点数组
    std::vector<unsigned int> indices;  // 三角形索引
    static constexpr int kStride = 8;   // 每顶点 float 个数
};

// 经纬球：slices 为经线方向分段数，stacks 为纬线方向分段数
MeshData makeSphere(float radius, int slices = 64, int stacks = 32);

// 立方体：每个面 4 个独立顶点（法线不共享，保证棱角分明）
MeshData makeCube(float halfSize);

// 轴对齐矩形面片：由起点 origin 和两条边向量 edgeU、edgeV 张成，
// 纹理坐标从 (0,0) 到 (repeatU, repeatV)，法线 = normalize(edgeU × edgeV)
MeshData makeQuad(const float origin[3], const float edgeU[3], const float edgeV[3],
                  float repeatU, float repeatV);

} // namespace Primitives
