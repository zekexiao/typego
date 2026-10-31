#pragma once

#include <QDialog>

class ThemeManager;

// 插件设置弹窗：无边框 + 自绘标题栏，接入系统材质做出磨砂底，
// 外观与主窗口保持一致。
class PluginSettingsDialog final : public QDialog {
    Q_OBJECT
public:
    // content 由调用方创建，这里接管所有权并放进弹窗主体。
    PluginSettingsDialog(const QString &title, QWidget *content, ThemeManager *themeManager,
                         QWidget *parent = nullptr);
};
