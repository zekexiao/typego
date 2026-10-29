#pragma once

#include <QWidget>
#include <functional>

class QCheckBox;
class QLabel;
class QSpinBox;

// 插件设置页由宿主设置对话框承载：改动立即写回插件配置，没有独立保存按钮。
class ClipboardSettingsWidget final : public QWidget {
    Q_OBJECT
public:
    struct Values {
        int maxItems = 50;
        bool captureText = true;
        bool captureImages = true;
        bool captureFiles = true;
        int count = 0;
    };

    ClipboardSettingsWidget(const Values &values, QWidget *parent,
                            std::function<void(const Values &)> changed,
                            std::function<int()> clearHistory);

private:
    void notify();
    void updateCount(int count);

    QSpinBox *m_maxItems = nullptr;
    QCheckBox *m_captureText = nullptr;
    QCheckBox *m_captureImages = nullptr;
    QCheckBox *m_captureFiles = nullptr;
    QLabel *m_count = nullptr;
    Values m_values;
    std::function<void(const Values &)> m_changed;
    std::function<int()> m_clearHistory;
};