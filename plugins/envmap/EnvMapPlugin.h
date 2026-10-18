#pragma once

#include <QObject>
#include <QPointer>
#include <QStringList>

#include "PluginInterface.h"

class QCheckBox;
class QComboBox;
class QLabel;
class QSlider;
class EnvMapWidget;

// 环境映射插件（实验任务3-2）
//  - 选择 data/env 下的等距柱状全景图，内核把它转换成立方体贴图（cube map）
//  - 主视口把全景画成天空盒，并让模型表面按 reflect(I, N) 反射环境
//  - 可调反射率；可一键加载光滑球体；可弹出任务2的环境映射演示窗口
class EnvMapPlugin : public QObject, public PluginInterface
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID PluginInterface_iid)
    Q_INTERFACES(PluginInterface)

public:
    QString pluginId() const override { return QStringLiteral("environment_mapping"); }
    QString pluginName() const override { return QStringLiteral("环境映射"); }
    QString pluginDesc() const override { return QStringLiteral("天空盒 + 立方体贴图反射"); }

    void initialize(ViewerContext* ctx) override { m_ctx = ctx; }
    QWidget* createUi() override;
    void onDestroy() override;

private:
    void applyEnvironment();     // 根据“启用”勾选与全景选择，更新主视口
    void loadDemoSphere();
    void openEnvDemo();

    ViewerContext* m_ctx = nullptr;
    QCheckBox* m_enableCheck = nullptr;
    QComboBox* m_panoCombo = nullptr;
    QCheckBox* m_skyCheck = nullptr;
    QSlider* m_reflSlider = nullptr;
    QLabel* m_reflLabel = nullptr;
    QStringList m_panoPaths;
    QPointer<EnvMapWidget> m_demo;
};
