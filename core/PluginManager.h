#pragma once

#include <memory>
#include <vector>

#include <QString>
#include <QStringList>

#include "viewer_core_global.h"

class MainWindow;
class QPluginLoader;

// 插件管理器：扫描插件目录中的 DLL，按 plugin_config.json 的
// active_plugins 列表注册；配置缺失或非法时加载全部可用插件。
class VIEWER_CORE_EXPORT PluginManager
{
public:
    explicit PluginManager(MainWindow* window);
    ~PluginManager();
    PluginManager(const PluginManager&) = delete;
    PluginManager& operator=(const PluginManager&) = delete;

    // 返回成功加载的插件数
    int loadPlugins(const QString& pluginDir, const QString& configPath);

private:
    QStringList readActiveList(const QString& configPath, bool* ok) const;

    MainWindow* m_window;
    std::vector<std::unique_ptr<QPluginLoader>> m_loaders; // 保持 DLL 常驻
};
