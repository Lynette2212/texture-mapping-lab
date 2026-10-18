#pragma once

#include <QtPlugin>
#include <QString>

class QWidget;
class ViewerContext;

// 3D 查看器插件统一接口（对应 Python 版 Base3DPlugin）。
// 新插件：继承 QObject + PluginInterface，
// 加 Q_PLUGIN_METADATA(IID PluginInterface_iid) / Q_INTERFACES(PluginInterface)，
// 编译成 DLL 放入程序 plugins/ 目录即可被自动扫描。
class PluginInterface
{
public:
    virtual ~PluginInterface() = default;

    // plugin_config.json 中的唯一标识（对应 Python 版 plugin_cls_name）
    virtual QString pluginId() const = 0;
    virtual QString pluginName() const = 0;
    virtual QString pluginDesc() const = 0;

    // 注册时由内核调用，传入全局上下文
    virtual void initialize(ViewerContext* ctx) = 0;

    // 创建插件专属 UI 面板，返回的 QWidget 挂载到右侧扩展区；返回 nullptr 表示无面板
    virtual QWidget* createUi() = 0;

    // 【钩子】模型加载完成后触发（模型数据从 ctx->currentMesh 获取）
    virtual void onModelLoaded() {}

    // 【钩子】窗口缩放时触发
    virtual void onWindowResize() {}

    // 【钩子】程序关闭时释放资源
    virtual void onDestroy() {}
};

#define PluginInterface_iid "com.cg.viewer3d.PluginInterface/1.0"
Q_DECLARE_INTERFACE(PluginInterface, PluginInterface_iid)
