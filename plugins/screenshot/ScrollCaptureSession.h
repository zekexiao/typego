#pragma once

#include <QImage>
#include <QObject>
#include <QRect>

#include "ScreenInfo.h"
#include "ScrollStitcher.h"

class QLabel;
class QTimer;
class QWidget;

// 滚动长截图会话：保留一圈边框提示，定时抓取选区并拼接，用户手动滚动目标窗口。
class ScrollCaptureSession final : public QObject {
    Q_OBJECT
public:
    ScrollCaptureSession(const QRect &selection, const ScreenInfo &screen, QObject *parent = nullptr);
    ~ScrollCaptureSession() override;

    void start();
    void cancel();

signals:
    void progress(int length);
    void finished(const QImage &image);
    void cancelled();
    void failed(const QString &message);

private:
    void captureFrame();
    void finishNow();
    void updateStatus(const QString &text);
    void positionPanel();
    void destroyWindows();

    QRect m_selection;
    ScreenInfo m_screen;
    QRect m_physicalRect;
    ScrollStitcher m_stitcher;
    QTimer *m_timer = nullptr;
    QWidget *m_frame = nullptr;
    QWidget *m_panel = nullptr;
    QLabel *m_status = nullptr;
    bool m_stopping = false;
    int m_noOverlap = 0;
    int m_captureErrors = 0;
};