// 任务1 独立演示程序：Mipmap 纹理过滤 —— 走廊
#include <QApplication>
#include <QSurfaceFormat>

#include "CorridorWidget.h"

int main(int argc, char* argv[])
{
    // 必须在 QApplication 构造前设置默认 OpenGL 格式：3.3 核心模式 + 24 位深度缓冲
    QSurfaceFormat format;
    format.setVersion(3, 3);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setDepthBufferSize(24);
    QSurfaceFormat::setDefaultFormat(format);

    QApplication app(argc, argv);

    CorridorWidget w;
    w.resize(1280, 800);
    w.show();
    return QApplication::exec();
}
