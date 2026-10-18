#pragma once

#include <QObject>
#include <QPointer>

#include "PluginInterface.h"
#include "TextureUtils.h"

class QComboBox;
class QLabel;
class QSlider;
class QCheckBox;
class CorridorWidget;

// Mipmap 纹理过滤插件（实验任务3-1）
//  - 在主视口中切换 6 种 GL 纹理过滤方式，调节纹理平铺次数、按层着色观察 mipmap 选层
//  - 一键加载“演示地面 + 棋盘格”，掠射角下摩尔纹/闪烁最明显
//  - 可弹出任务1的走廊场景窗口，且与面板的过滤方式双向同步
class MipmapPlugin : public QObject, public PluginInterface
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID PluginInterface_iid)
    Q_INTERFACES(PluginInterface)

public:
    QString pluginId() const override { return QStringLiteral("mipmap_filter"); }
    QString pluginName() const override { return QStringLiteral("Mipmap 纹理过滤"); }
    QString pluginDesc() const override { return QStringLiteral("切换 GL_NEAREST ~ GL_LINEAR_MIPMAP_LINEAR 六种过滤方式"); }

    void initialize(ViewerContext* ctx) override { m_ctx = ctx; }
    QWidget* createUi() override;
    void onModelLoaded() override;
    void onDestroy() override;

private:
    void applyFilter(TextureUtils::Filter f);   // 面板 -> 主视口 + 走廊窗口
    void loadDemoGround();
    void openCorridor();

    ViewerContext* m_ctx = nullptr;
    QComboBox* m_filterCombo = nullptr;
    QLabel* m_descLabel = nullptr;
    QSlider* m_tilingSlider = nullptr;
    QLabel* m_tilingLabel = nullptr;
    QCheckBox* m_tintCheck = nullptr;
    QPointer<CorridorWidget> m_corridor;        // 窗口关闭后自动置空
};
