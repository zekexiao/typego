#pragma once

#include <QColor>
#include <QList>
#include <QWidget>

#include "Annotation.h"

class QButtonGroup;
class QToolButton;

// 截图选区浮动工具栏：工具、颜色、粗细与完成动作。
class CaptureToolbar final : public QWidget {
    Q_OBJECT
public:
    // Pin 模式复用于贴图编辑：隐藏长截图和贴图按钮，关闭按钮表示完成。
    enum class Mode { Capture, Pin };

    explicit CaptureToolbar(QWidget *parent = nullptr, Mode mode = Mode::Capture);
    ~CaptureToolbar() override;

    void setTool(ShotTool tool);
    void setColor(const QColor &color);
    void setWidth(int width);
    void setUndoRedo(bool canUndo, bool canRedo);

signals:
    void toolChanged(ShotTool tool);
    void colorChanged(const QColor &color);
    void widthChanged(int width);
    void undoRequested();
    void redoRequested();
    void scrollRequested();
    void copyRequested();
    void saveRequested();
    void pinRequested();
    void closeRequested();

private:
    QToolButton *makeToolButton(ShotTool tool);
    void selectColor(QToolButton *button);
    void selectWidth(QToolButton *button);

    QButtonGroup *m_toolGroup = nullptr;
    QList<QToolButton *> m_toolButtons;
    QList<QToolButton *> m_colorButtons;
    QList<QToolButton *> m_widthButtons;
    QToolButton *m_undo = nullptr;
    QToolButton *m_redo = nullptr;
    Mode m_mode = Mode::Capture;
};