#include "ViewerContext.h"

#include <QCoreApplication>
#include <QPlainTextEdit>

#include "MainWindow.h"

void ViewerContext::appendInfo(const QString& text)
{
    if (infoDisplay)
        infoDisplay->appendPlainText(text);
}

bool ViewerContext::loadModel(const QString& path)
{
    // 主窗口负责持有 Mesh 并分发 onModelLoaded，插件只需给出路径
    if (auto* win = qobject_cast<MainWindow*>(mainWindow))
        return win->loadModel(path);
    return false;
}

QString ViewerContext::dataPath(const QString& relative)
{
    // 构建时 data/ 被拷贝到 exe 同级目录
    return QCoreApplication::applicationDirPath() + QStringLiteral("/data/") + relative;
}
