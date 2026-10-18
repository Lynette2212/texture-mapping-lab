#include "PluginManager.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLibrary>
#include <QPluginLoader>

#include <algorithm>
#include <vector>

#include "MainWindow.h"
#include "PluginInterface.h"

PluginManager::PluginManager(MainWindow* window) : m_window(window) {}

PluginManager::~PluginManager() = default;

QStringList PluginManager::readActiveList(const QString& configPath, bool* ok) const
{
    *ok = false;
    QFile file(configPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning().noquote() << QStringLiteral("提示：未找到 plugin_config.json，自动加载所有可用插件");
        return {};
    }
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject()) {
        qWarning().noquote() << QStringLiteral("配置读取异常，自动加载所有插件");
        return {};
    }
    QStringList list;
    const QJsonArray arr = doc.object().value(QStringLiteral("active_plugins")).toArray();
    for (const QJsonValue& v : arr)
        list.append(v.toString());
    *ok = true;
    return list;
}

int PluginManager::loadPlugins(const QString& pluginDir, const QString& configPath)
{
    bool configOk = false;
    const QStringList wanted = readActiveList(configPath, &configOk);

    // 扫描 DLL，收集有效插件候选
    struct Candidate
    {
        QString id;
        std::unique_ptr<QPluginLoader> loader;
    };
    std::vector<Candidate> candidates;

    const QDir dir(pluginDir);
    // Windows 下插件是 *.dll，Linux 下是 lib*.so，macOS 下是 *.dylib；
    // 再用 QLibrary::isLibrary 过滤掉同目录下的 .lib/.exp 等非动态库文件
    const QStringList files = dir.entryList({QStringLiteral("*.dll"), QStringLiteral("*.so"), QStringLiteral("*.dylib")},
                                            QDir::Files, QDir::Name);
    QStringList dlls;
    for (const QString& f : files)
        if (QLibrary::isLibrary(f))
            dlls.append(f);
    for (const QString& fileName : dlls) {
        auto loader = std::make_unique<QPluginLoader>(dir.absoluteFilePath(fileName));
        QObject* instance = loader->instance();
        if (!instance) {
            qWarning().noquote() << QStringLiteral("插件加载失败: %1\n%2").arg(fileName, loader->errorString());
            continue;
        }
        auto* plugin = qobject_cast<PluginInterface*>(instance);
        if (!plugin) {
            qWarning().noquote() << QStringLiteral("%1 不是有效的 Viewer 插件").arg(fileName);
            loader->unload();
            continue;
        }
        candidates.push_back({plugin->pluginId(), std::move(loader)});
    }

    // 按配置顺序注册；无配置则按插件标识字典序
    QStringList order;
    if (configOk) {
        order = wanted;
    } else {
        for (const Candidate& c : candidates)
            order.append(c.id);
        order.sort();
    }

    int loaded = 0;
    for (const QString& id : order) {
        auto it = std::find_if(candidates.begin(), candidates.end(),
                               [&](const Candidate& c) { return c.id == id; });
        if (it == candidates.end()) {
            qWarning().noquote() << QStringLiteral("警告：不存在标识为 [%1] 的插件，跳过").arg(id);
            continue;
        }
        auto* plugin = qobject_cast<PluginInterface*>(it->loader->instance());
        m_window->registerPlugin(plugin);
        m_loaders.push_back(std::move(it->loader));
        ++loaded;
        qInfo().noquote() << QStringLiteral("已加载插件: %1").arg(id);
    }
    return loaded;
}
