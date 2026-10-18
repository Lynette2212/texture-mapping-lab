#pragma once

#include <QObject>
#include <QString>

#include "PluginInterface.h"

class QGridLayout;
class QScrollArea;
class QPushButton;
class QTimer;

// 纹理贴图功能插件（对应 Python 版 plugin_texture）
class TexturePlugin : public QObject, public PluginInterface
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID PluginInterface_iid)
    Q_INTERFACES(PluginInterface)

public:
    TexturePlugin();
    ~TexturePlugin() override;

    QString pluginId() const override { return QStringLiteral("texture_mapping"); }
    QString pluginName() const override { return QStringLiteral("texture_mapping"); }
    QString pluginDesc() const override { return QStringLiteral("纹理贴图功能插件"); }

    void initialize(ViewerContext* ctx) override { m_ctx = ctx; }
    QWidget* createUi() override;
    void onWindowResize() override;

private:
    void loadTextureImages();
    void onTextureClicked(QPushButton* btn, const QString& path);

    ViewerContext* m_ctx = nullptr;
    QScrollArea* m_scroll = nullptr;
    QGridLayout* m_grid = nullptr;
    QTimer* m_reloadTimer = nullptr;
    QPushButton* m_selectedBtn = nullptr;
    QString m_selectedPath;
    QString m_textureDir;
};
