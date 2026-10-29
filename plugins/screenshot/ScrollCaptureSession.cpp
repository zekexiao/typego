#include "ScrollCaptureSession.h"

#include "WinCapture.h"

#include <QApplication>
#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QScreen>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>
#include <QtMath>

namespace {

// 用带洞的异形窗口画选区外侧边框：选区本身不属于窗口，
// 因此抓取区域完全不会被边框覆盖，鼠标滚轮也能直接传给下面的窗口。
class ScrollFrameWindow final : public QWidget {
public:
    ScrollFrameWindow(const QRect &geometry, const QSize &innerSize)
        : QWidget(nullptr, Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint)
    {
        setAttribute(Qt::WA_NoSystemBackground);
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setAttribute(Qt::WA_ShowWithoutActivating);
        setGeometry(geometry);
        const int margin = 4;
        setMask(QRegion(rect()) - QRegion(QRect(margin, margin, innerSize.width(), innerSize.height())));
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.fillRect(rect(), QColor(22, 119, 255));
        painter.setPen(QPen(QColor(255, 255, 255, 90), 1));
        painter.drawRect(rect().adjusted(0, 0, -1, -1));
    }
};

} // namespace

ScrollCaptureSession::ScrollCaptureSession(const QRect &selection, const ScreenInfo &screen,
                                           QObject *parent)
    : QObject(parent)
    , m_selection(selection.normalized())
    , m_screen(screen)
{
    const QPoint topLeft(
        m_screen.physical.x() + qRound((m_selection.x() - m_screen.logical.x()) * m_screen.dpr),
        m_screen.physical.y() + qRound((m_selection.y() - m_screen.logical.y()) * m_screen.dpr));
    m_physicalRect = QRect(topLeft, QSize(qMax(1, qRound(m_selection.width() * m_screen.dpr)),
                                          qMax(1, qRound(m_selection.height() * m_screen.dpr))))
                         .intersected(m_screen.physical);
}

ScrollCaptureSession::~ScrollCaptureSession()
{
    if (m_timer) m_timer->stop();
    if (m_frame) {
        m_frame->hide();
        delete m_frame;
        m_frame = nullptr;
    }
    if (m_panel) {
        m_panel->hide();
        delete m_panel;
        m_panel = nullptr;
        m_status = nullptr;
    }
}

void ScrollCaptureSession::start()
{
    if (m_stopping) return;
    const int margin = 4;
    m_frame = new ScrollFrameWindow(m_selection.adjusted(-margin, -margin, margin, margin),
                                    m_selection.size());
    m_frame->show();
    m_frame->raise();
    ShotWin::forceTopmost(static_cast<quintptr>(m_frame->winId()));

    m_panel = new QWidget(nullptr, Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    m_panel->setObjectName(QStringLiteral("scrollPanel"));
    m_panel->setAttribute(Qt::WA_StyledBackground, true);
    m_panel->setAttribute(Qt::WA_ShowWithoutActivating, true);
    m_panel->setWindowTitle(tr("Scrolling capture"));
    m_panel->setStyleSheet(QStringLiteral(
        "QWidget#scrollPanel { background: rgba(26, 28, 32, 240); border: 1px solid rgba(255,255,255,45); border-radius: 10px; }"
        "QLabel { color: #eef1f5; }"
        "QLabel#scrollHint { color: #c8ccd4; font-size: 11px; }"
        "QPushButton { color: #e8eaee; background: rgba(255,255,255,26); border: 1px solid rgba(255,255,255,40);"
        "  border-radius: 6px; padding: 4px 14px; }"
        "QPushButton:hover { background: rgba(255,255,255,45); }"
        "QPushButton#scrollDone { background: #1677ff; border-color: #1677ff; }"
        "QPushButton#scrollDone:hover { background: #3a8dff; }"));

    auto *layout = new QVBoxLayout(m_panel);
    layout->setContentsMargins(12, 10, 12, 10);
    layout->setSpacing(6);
    auto *title = new QLabel(tr("Scrolling capture"), m_panel);
    QFont titleFont = title->font();
    titleFont.setBold(true);
    title->setFont(titleFont);
    layout->addWidget(title);
    auto *hint = new QLabel(tr("Scroll the mouse wheel inside the selection, then click Done."), m_panel);
    hint->setObjectName(QStringLiteral("scrollHint"));
    hint->setWordWrap(true);
    hint->setMaximumWidth(320);
    layout->addWidget(hint);
    m_status = new QLabel(tr("Waiting for scrolling..."), m_panel);
    m_status->setObjectName(QStringLiteral("scrollHint"));
    layout->addWidget(m_status);

    auto *buttons = new QHBoxLayout;
    buttons->addStretch();
    auto *cancelButton = new QPushButton(tr("Cancel"), m_panel);
    auto *doneButton = new QPushButton(tr("Done"), m_panel);
    doneButton->setObjectName(QStringLiteral("scrollDone"));
    cancelButton->setFocusPolicy(Qt::NoFocus);
    doneButton->setFocusPolicy(Qt::NoFocus);
    buttons->addWidget(cancelButton);
    buttons->addWidget(doneButton);
    layout->addLayout(buttons);

    connect(cancelButton, &QPushButton::clicked, this, &ScrollCaptureSession::cancel);
    connect(doneButton, &QPushButton::clicked, this, &ScrollCaptureSession::finishNow);

    m_panel->adjustSize();
    m_panel->show();
    m_panel->raise();
    positionPanel();
    ShotWin::excludeFromCapture(static_cast<quintptr>(m_panel->winId()));
    ShotWin::forceTopmost(static_cast<quintptr>(m_panel->winId()));

    // 先让桌面完成一次重绘，再抓第一帧，避免把刚隐藏的覆盖层拍进去。
    m_timer = new QTimer(this);
    m_timer->setInterval(160);
    connect(m_timer, &QTimer::timeout, this, &ScrollCaptureSession::captureFrame);
    m_timer->start();
}

void ScrollCaptureSession::positionPanel()
{
    if (!m_panel) return;
    m_panel->adjustSize();
    QScreen *screen = m_screen.screen ? m_screen.screen : QGuiApplication::primaryScreen();
    const QRect available = screen ? screen->availableGeometry()
                                   : QRect(0, 0, 1920, 1080);
    const QSize size = m_panel->size();
    int x = m_selection.right() - size.width() + 1;
    int y = m_selection.bottom() + 12;
    if (y + size.height() > available.bottom() - 8)
        y = m_selection.top() - size.height() - 12;
    if (y < available.top() + 8)
        y = m_selection.bottom() - size.height() - 12;
    x = qBound(available.left() + 8, x, qMax(available.left() + 8, available.right() - size.width() - 8));
    y = qBound(available.top() + 8, y, qMax(available.top() + 8, available.bottom() - size.height() - 8));
    m_panel->move(x, y);
}

void ScrollCaptureSession::captureFrame()
{
    if (m_stopping) return;
    QString error;
    const QImage frame = ShotWin::captureRect(m_physicalRect, &error);
    if (frame.isNull()) {
        if (++m_captureErrors >= 5) {
            if (m_stopping) return;
            m_stopping = true;
            if (m_timer) m_timer->stop();
            destroyWindows();
            emit failed(error.isEmpty() ? tr("Scrolling capture failed") : error);
        }
        return;
    }
    m_captureErrors = 0;

    ScrollMatch match;
    const ScrollAddResult result = m_stitcher.addFrame(frame, &match);
    switch (result) {
    case ScrollAddResult::FirstFrame:
        updateStatus(tr("Waiting for scrolling..."));
        break;
    case ScrollAddResult::Stitched:
        m_noOverlap = 0;
        updateStatus(tr("Stitched %1 px").arg(m_stitcher.stitchedLength()));
        emit progress(m_stitcher.stitchedLength());
        break;
    case ScrollAddResult::Unchanged:
        break;
    case ScrollAddResult::NoOverlap:
        if (++m_noOverlap >= 3) updateStatus(tr("Scrolling too fast, scroll back a little"));
        break;
    case ScrollAddResult::LimitReached:
        finishNow();
        break;
    case ScrollAddResult::Invalid:
        if (m_stopping) return;
        m_stopping = true;
        if (m_timer) m_timer->stop();
        destroyWindows();
        emit failed(tr("Scrolling capture failed"));
        break;
    }
}

void ScrollCaptureSession::updateStatus(const QString &text)
{
    if (!m_status) return;
    if (m_status->text() == text) return;
    m_status->setText(text);
    positionPanel();
}

void ScrollCaptureSession::finishNow()
{
    if (m_stopping) return;
    m_stopping = true;
    if (m_timer) m_timer->stop();
    QImage image = m_stitcher.image();
    destroyWindows();
    if (image.isNull()) {
        emit failed(tr("Nothing was captured"));
        return;
    }
    image.setDevicePixelRatio(m_screen.dpr > 0 ? m_screen.dpr : 1.0);
    emit finished(image);
}

void ScrollCaptureSession::cancel()
{
    if (m_stopping) return;
    m_stopping = true;
    if (m_timer) m_timer->stop();
    destroyWindows();
    emit cancelled();
}

void ScrollCaptureSession::destroyWindows()
{
    if (m_frame) {
        m_frame->hide();
        m_frame->deleteLater();
        m_frame = nullptr;
    }
    if (m_panel) {
        m_panel->hide();
        m_panel->deleteLater();
        m_panel = nullptr;
        m_status = nullptr;
    }
}