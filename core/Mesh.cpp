#include "Mesh.h"

#include <QFile>
#include <QHash>
#include <QVector2D>
#include <array>
#include <cmath>

namespace {

// OBJ 面片角点索引三元组（已解析为 0 基，-1 表示缺失）
struct ObjIndex
{
    int v = -1, t = -1, n = -1;
    bool operator==(const ObjIndex& o) const { return v == o.v && t == o.t && n == o.n; }
};

size_t qHash(const ObjIndex& k, size_t seed = 0) noexcept
{
    return qHashMulti(seed, k.v, k.t, k.n);
}

int resolveIndex(const QByteArray& s, int count, bool* ok)
{
    if (s.isEmpty())
        return -1;
    int idx = s.toInt(ok);
    if (!*ok || idx == 0) {
        *ok = false;
        return -1;
    }
    // OBJ 索引 1 基，负数表示相对末尾
    idx = (idx > 0) ? idx - 1 : count + idx;
    if (idx < 0 || idx >= count) {
        *ok = false;
        return -1;
    }
    return idx;
}

ObjIndex parseFaceToken(const QByteArray& token, int nv, int nt, int nn, bool* ok)
{
    ObjIndex out;
    const QList<QByteArray> parts = token.split('/');
    if (parts.size() >= 1) out.v = resolveIndex(parts[0], nv, ok);
    if (*ok && parts.size() >= 2) out.t = resolveIndex(parts[1], nt, ok);
    if (*ok && parts.size() >= 3) out.n = resolveIndex(parts[2], nn, ok);
    return out;
}

} // namespace

bool Mesh::loadFromObj(const QString& path, QString* errorMessage)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (errorMessage) *errorMessage = QStringLiteral("无法打开文件: %1").arg(path);
        return false;
    }

    std::vector<QVector3D> rawVerts;
    std::vector<QVector2D> rawUvs;
    std::vector<QVector3D> rawNormals;
    std::vector<std::array<ObjIndex, 3>> triangles; // 已三角化

    bool needNormals = false;   // 存在缺法线索引的面
    bool needTexCoords = false; // 存在缺纹理坐标的面

    QByteArray line;
    while (!(line = file.readLine()).isNull()) {
        const QByteArray trimmed = line.trimmed();
        if (trimmed.isEmpty() || trimmed.startsWith('#'))
            continue;
        const QList<QByteArray> tokens = trimmed.simplified().split(' ');
        const QByteArray& tag = tokens.first();

        if (tag == "v" && tokens.size() >= 4) {
            rawVerts.emplace_back(tokens[1].toFloat(), tokens[2].toFloat(), tokens[3].toFloat());
        } else if (tag == "vt" && tokens.size() >= 3) {
            rawUvs.emplace_back(tokens[1].toFloat(), tokens[2].toFloat());
        } else if (tag == "vn" && tokens.size() >= 4) {
            rawNormals.emplace_back(tokens[1].toFloat(), tokens[2].toFloat(), tokens[3].toFloat());
        } else if (tag == "f" && tokens.size() >= 4) {
            const int cornerCount = tokens.size() - 1;
            std::vector<ObjIndex> corners(cornerCount);
            for (int i = 0; i < cornerCount; ++i) {
                bool ok = true;
                corners[i] = parseFaceToken(tokens[i + 1],
                                            static_cast<int>(rawVerts.size()),
                                            static_cast<int>(rawUvs.size()),
                                            static_cast<int>(rawNormals.size()), &ok);
                if (!ok) {
                    if (errorMessage) *errorMessage = QStringLiteral("面片索引非法: %1").arg(QString::fromUtf8(trimmed));
                    return false;
                }
                if (corners[i].n < 0) needNormals = true;
                if (corners[i].t < 0) needTexCoords = true;
            }
            // 扇形三角化
            for (int i = 1; i + 1 < cornerCount; ++i)
                triangles.push_back({corners[0], corners[i], corners[i + 1]});
        }
    }

    if (triangles.empty()) {
        if (errorMessage) *errorMessage = QStringLiteral("文件不含任何三角面片");
        return false;
    }

    // 顶点去重：(v, vt, vn) 三元组唯一 -> 输出顶点。
    // 若需重算法线/纹理坐标，则该项与位置绑定，键中按常量处理。
    QHash<ObjIndex, unsigned int> remap;
    remap.reserve(triangles.size());
    m_positions.clear();
    m_normals.clear();
    m_texCoords.clear();
    m_indices.clear();
    m_indices.reserve(triangles.size() * 3);

    auto emitVertex = [&](const ObjIndex& src) -> unsigned int {
        const ObjIndex key{src.v, needTexCoords ? 0 : src.t, needNormals ? 0 : src.n};
        auto it = remap.constFind(key);
        if (it != remap.constEnd())
            return it.value();

        const unsigned int newIndex = static_cast<unsigned int>(m_positions.size() / 3);
        const QVector3D& p = rawVerts[static_cast<size_t>(src.v)];
        m_positions.insert(m_positions.end(), {p.x(), p.y(), p.z()});

        if (!needNormals && src.n >= 0) {
            const QVector3D& n = rawNormals[static_cast<size_t>(src.n)];
            m_normals.insert(m_normals.end(), {n.x(), n.y(), n.z()});
        }
        if (!needTexCoords && src.t >= 0) {
            const QVector2D& t = rawUvs[static_cast<size_t>(src.t)];
            m_texCoords.insert(m_texCoords.end(), {t.x(), t.y()});
        }
        remap.insert(key, newIndex);
        return newIndex;
    };

    for (const auto& tri : triangles)
        for (const ObjIndex& corner : tri)
            m_indices.push_back(emitVertex(corner));

    if (needNormals)
        computeNormals();
    if (needTexCoords)
        generateCylindricalTexCoords();
    computeBounds();
    return true;
}

void Mesh::computeNormals()
{
    m_normals.assign(m_positions.size(), 0.0f);
    for (size_t i = 0; i + 2 < m_indices.size(); i += 3) {
        const unsigned int ia = m_indices[i], ib = m_indices[i + 1], ic = m_indices[i + 2];
        const QVector3D a(m_positions[ia * 3], m_positions[ia * 3 + 1], m_positions[ia * 3 + 2]);
        const QVector3D b(m_positions[ib * 3], m_positions[ib * 3 + 1], m_positions[ib * 3 + 2]);
        const QVector3D c(m_positions[ic * 3], m_positions[ic * 3 + 1], m_positions[ic * 3 + 2]);
        const QVector3D n = QVector3D::crossProduct(b - a, c - a); // 面积加权
        for (unsigned int idx : {ia, ib, ic}) {
            m_normals[idx * 3]     += n.x();
            m_normals[idx * 3 + 1] += n.y();
            m_normals[idx * 3 + 2] += n.z();
        }
    }
    for (size_t i = 0; i < m_normals.size(); i += 3) {
        QVector3D n(m_normals[i], m_normals[i + 1], m_normals[i + 2]);
        n.normalize();
        m_normals[i] = n.x(); m_normals[i + 1] = n.y(); m_normals[i + 2] = n.z();
    }
}

void Mesh::generateCylindricalTexCoords()
{
    // 绕 Y 轴的柱面投影（对应 VTK 版 vtkTextureMapToCylinder）
    QVector3D minP, maxP;
    for (size_t i = 0; i < m_positions.size(); i += 3) {
        const QVector3D p(m_positions[i], m_positions[i + 1], m_positions[i + 2]);
        if (i == 0) { minP = maxP = p; continue; }
        minP.setX(std::min(minP.x(), p.x())); maxP.setX(std::max(maxP.x(), p.x()));
        minP.setY(std::min(minP.y(), p.y())); maxP.setY(std::max(maxP.y(), p.y()));
        minP.setZ(std::min(minP.z(), p.z())); maxP.setZ(std::max(maxP.z(), p.z()));
    }
    const QVector3D c = (minP + maxP) * 0.5f;
    const float height = std::max(maxP.y() - minP.y(), 1e-6f);
    constexpr float twoPi = 6.283185307179586f;

    m_texCoords.assign(m_positions.size() / 3 * 2, 0.0f);
    for (size_t i = 0, v = 0; i < m_positions.size(); i += 3, v += 2) {
        const float u = std::atan2(m_positions[i + 2] - c.z(), m_positions[i] - c.x()) / twoPi + 0.5f;
        m_texCoords[v]     = u;
        m_texCoords[v + 1] = (m_positions[i + 1] - minP.y()) / height;
    }
}

void Mesh::computeBounds()
{
    if (m_positions.empty())
        return;
    QVector3D minP, maxP;
    for (size_t i = 0; i < m_positions.size(); i += 3) {
        const QVector3D p(m_positions[i], m_positions[i + 1], m_positions[i + 2]);
        if (i == 0) { minP = maxP = p; continue; }
        minP.setX(std::min(minP.x(), p.x())); maxP.setX(std::max(maxP.x(), p.x()));
        minP.setY(std::min(minP.y(), p.y())); maxP.setY(std::max(maxP.y(), p.y()));
        minP.setZ(std::min(minP.z(), p.z())); maxP.setZ(std::max(maxP.z(), p.z()));
    }
    m_center = (minP + maxP) * 0.5f;
    m_radius = 0.0f;
    for (size_t i = 0; i < m_positions.size(); i += 3) {
        const QVector3D p(m_positions[i], m_positions[i + 1], m_positions[i + 2]);
        m_radius = std::max(m_radius, (p - m_center).length());
    }
    m_radius = std::max(m_radius, 1e-3f);
}
