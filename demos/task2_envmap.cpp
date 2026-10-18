// 任务2 独立演示程序：环境映射 —— 天空盒 + 反射球 / 立方体
#include <QApplication>
#include <QSurfaceFormat>

#include "EnvMapWidget.h"

int main(int argc, char* argv[])
{
    QSurfaceFormat format;
    format.setVersion(3, 3);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setDepthBufferSize(24);
    QSurfaceFormat::setDefaultFormat(format);

    QApplication app(argc, argv);

    EnvMapWidget w;
    w.resize(1280, 800);
    w.show();
    return QApplication::exec();
}
