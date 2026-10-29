#pragma once

#include <QImage>
#include <QPoint>
#include <QWidget>

class QLabel;

// 长截图结果窗口：以可滚动方式查看拼接结果，并提供复制/另存。
class ScrollResultWindow final : public QWidget {
    Q_OBJECT
public:
    ScrollResultWindow(const QImage &image, const QPoint &anchor, QWidget *parent = nullptr);

signals:
    void closed();

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

private:
    void copyImage();
    void saveAs();

    QImage m_image;
    QLabel *m_status = nullptr;
};