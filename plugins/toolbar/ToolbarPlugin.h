#pragma once

#include <QObject>

#include "PluginInterface.h"

class QToolBar;

// 顶部工具栏插件：线框/面片切换、退出按钮（对应 Python 版 plugin_toolbar）
class ToolbarPlugin : public QObject, public PluginInterface
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID PluginInterface_iid)
    Q_INTERFACES(PluginInterface)

public:
    QString pluginId() const override { return QStringLiteral("view_toolbar"); }
    QString pluginName() const override { return QStringLiteral("view_toolbar"); }
    QString pluginDesc() const override { return QStringLiteral("顶部工具栏插件：线框/面片切换、退出按钮"); }

    void initialize(ViewerContext* ctx) override { m_ctx = ctx; }
    QWidget* createUi() override;
    void onModelLoaded() override { setDisplayMode(m_displayMode); }
    void onDestroy() override;

private:
    void setDisplayMode(int mode);

    ViewerContext* m_ctx = nullptr;
    QToolBar* m_toolbar = nullptr;
    int m_displayMode = 0; // 0=面片 1=线框
};
