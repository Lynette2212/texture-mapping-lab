#include <QApplication>
#include <QSurfaceFormat>

#include "MainWindow.h"
#include "PluginManager.h"

int main(int argc, char* argv[])
{
    // 必须在 QApplication 构造前设置默认 OpenGL 格式
    QSurfaceFormat format;
    format.setVersion(3, 3);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setDepthBufferSize(24);
    QSurfaceFormat::setDefaultFormat(format);

    QApplication app(argc, argv);

    MainWindow window;

    PluginManager pluginManager(&window);
    const QString appDir = QCoreApplication::applicationDirPath();
    pluginManager.loadPlugins(appDir + QStringLiteral("/plugins"),
                              appDir + QStringLiteral("/plugin_config.json"));

    window.show();

    // 支持命令行直接打开模型：viewer3d.exe model.obj
    const QStringList args = QCoreApplication::arguments();
    if (args.size() > 1 && args[1].endsWith(QStringLiteral(".obj"), Qt::CaseInsensitive))
        window.loadModel(args[1]);

    return QApplication::exec();
}
