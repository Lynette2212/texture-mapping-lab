#pragma once

#include <vector>

#include <QString>
#include <QVector3D>

#include "viewer_core_global.h"

// 网格数据：从 OBJ 文件加载，向渲染后端提供位置/法线/纹理坐标/索引。
// OBJ 缺少法线时自动按面累积计算，缺少纹理坐标时自动生成柱面坐标。
class VIEWER_CORE_EXPORT Mesh
{
public:
    Mesh() = default;

    bool loadFromObj(const QString& path, QString* errorMessage = nullptr);

    const std::vector<float>& positions() const { return m_positions; }
    const std::vector<float>& normals()   const { return m_normals; }
    const std::vector<float>& texCoords() const { return m_texCoords; }
    const std::vector<unsigned int>& indices() const { return m_indices; }

    int vertexCount() const { return static_cast<int>(m_positions.size() / 3); }
    int triangleCount() const { return static_cast<int>(m_indices.size() / 3); }

    QVector3D center() const { return m_center; }
    float boundingRadius() const { return m_radius; }

private:
    void computeNormals();
    void generateCylindricalTexCoords();
    void computeBounds();

    std::vector<float> m_positions;
    std::vector<float> m_normals;
    std::vector<float> m_texCoords;
    std::vector<unsigned int> m_indices;
    QVector3D m_center{0.0f, 0.0f, 0.0f};
    float m_radius = 1.0f;
};
