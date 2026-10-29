#pragma once

#include <QString>
#include <QWidget>

#include <functional>

class QCheckBox;
class QLabel;
class QLineEdit;
class QSpinBox;

// 截图插件设置页：保存目录、历史条数与放大镜。
class ScreenshotSettingsWidget final : public QWidget {
    Q_OBJECT
public:
    struct Values {
        QString saveDir;
        int historyLimit = 30;
        bool magnifier = true;
        int magnifierZoom = 8;
        int historyCount = 0;
    };

    ScreenshotSettingsWidget(const Values &values, QWidget *parent,
                             std::function<void(const Values &)> changed,
                             std::function<int()> clearHistory);

private:
    void notify();
    void updateCount(int count);
    void browse();

    Values m_values;
    std::function<void(const Values &)> m_changed;
    std::function<int()> m_clearHistory;
    QLineEdit *m_saveDir = nullptr;
    QSpinBox *m_historyLimit = nullptr;
    QCheckBox *m_magnifier = nullptr;
    QSpinBox *m_magnifierZoom = nullptr;
    QLabel *m_count = nullptr;
};