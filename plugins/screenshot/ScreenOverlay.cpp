#include "ScreenOverlay.h"

#include "Annotation.h"
#include "ScreenshotSession.h"

#include <QEvent>
#include <QFontMetrics>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QRegion>
#include <QTimer>
#include <QtMath>

#include <cmath>

#ifdef Q_OS_WIN
#define NOMINMAX
#include <windows.h>
#endif

namespace {

const QColor Accent(22, 119, 255);
const QColor HandleFill(255, 255, 255);

QString elide(const QFontMetrics &metrics, const QString &text, int width)
{
    return metrics.elidedText(text, Qt::ElideRight, qMax(20, width));
}

} // namespace

ScreenOverlay::ScreenOverlay(ScreenshotSession *session, const ScreenInfo &info)
    : QWidget(nullptr, Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool)
    , m_session(session)
    , m_info(info)
{
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setCursor(Qt::CrossCursor);
    setWindowTitle(QStringLiteral("TypeGo Screenshot"));
    setGeometry(info.logical);
}

ScreenOverlay::~ScreenOverlay() = default;

void ScreenOverlay::start()
{
    showFullScreen();
    forceTopmost();
}

void ScreenOverlay::forceTopmost()
{
#ifdef Q_OS_WIN
    SetWindowPos(reinterpret_cast<HWND>(winId()), HWND_TOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_SHOWWINDOW);
#endif
}

QPointF ScreenOverlay::toLocal(const QPointF &global) const
{
    return global - QPointF(m_info.logical.topLeft());
}

QRect ScreenOverlay::toLocal(const QRect &global) const
{
    return global.translated(-m_info.logical.topLeft());
}

void ScreenOverlay::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    paintDesktop(painter);

    const QRect selection = m_session->selection();
    if (selection.isEmpty()) {
        painter.fillRect(rect(), QColor(0, 0, 0, 90));
    } else {
        const QRect selectionLocal = toLocal(selection);
        paintAnnotations(painter, selectionLocal);
        paintDim(painter, selectionLocal);
        paintSelection(painter, selectionLocal);
        paintHandles(painter, selectionLocal);
        paintSizeBadge(painter, selectionLocal);
    }
    paintGuides(painter);
    paintHover(painter);
    paintMagnifier(painter);
    paintTextInputOutline(painter);
}

void ScreenOverlay::paintDesktop(QPainter &painter)
{
    const QImage &desktop = m_session->desktopImage();
    if (desktop.isNull()) return;
    painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
    painter.drawImage(rect(), desktop, m_info.source);
}

void ScreenOverlay::paintAnnotations(QPainter &painter, const QRect &selectionLocal)
{
    const QVector<ShotAnnotation> &annotations = m_session->annotations();
    if (annotations.isEmpty() && !m_session->hasPendingAnnotation()) return;
    const QRect clip = selectionLocal.intersected(rect());
    if (clip.isEmpty()) return;
    painter.save();
    painter.setClipRect(clip);
    for (const ShotAnnotation &annotation : annotations) {
        if (annotation.isEffect()) {
            paintEffect(painter, annotation);
            continue;
        }
        painter.save();
        painter.translate(-m_info.logical.topLeft());
        paintShotAnnotation(painter, annotation);
        painter.restore();
    }
    if (m_session->hasPendingAnnotation()) {
        const ShotAnnotation &pending = m_session->pendingAnnotation();
        if (pending.isEffect()) {
            paintEffect(painter, pending);
        } else {
            painter.save();
            painter.translate(-m_info.logical.topLeft());
            paintShotAnnotation(painter, pending);
            painter.restore();
        }
    }
    // 选中标注的虚线框和手柄。
    const int selected = m_session->selectedAnnotation();
    if (selected >= 0 && selected < annotations.size()) {
        painter.save();
        painter.translate(-m_info.logical.topLeft());
        paintShotSelection(painter, annotations.at(selected), 1.0);
        painter.restore();
    }
    painter.restore();
}

void ScreenOverlay::paintEffect(QPainter &painter, const ShotAnnotation &annotation)
{
    const QRectF local = annotation.rect().translated(-m_info.logical.topLeft());
    const QRectF visible = local.intersected(QRectF(rect()));
    if (visible.width() < 1 || visible.height() < 1) return;

    // 逻辑坐标按本屏 DPI 映射回虚拟桌面截图的物理像素。
    const qreal dpr = m_info.dpr;
    const int left = qFloor(visible.left() * dpr);
    const int top = qFloor(visible.top() * dpr);
    const int right = qCeil(visible.right() * dpr);
    const int bottom = qCeil(visible.bottom() * dpr);
    QRect physical(left, top, qMax(1, right - left), qMax(1, bottom - top));
    physical.translate(m_info.source.topLeft());
    physical = physical.intersected(m_info.source);
    if (physical.isEmpty()) return;

    QImage patch = m_session->desktopImage().copy(physical);
    if (patch.isNull()) return;
    applyShotEffect(patch, patch.rect(), annotation.tool, qMax(3, qRound(8 * dpr)));

    const bool smooth = painter.renderHints() & QPainter::SmoothPixmapTransform;
    painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
    painter.drawImage(visible, patch);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, smooth);
}

void ScreenOverlay::paintDim(QPainter &painter, const QRect &selectionLocal)
{
    const QRect local = selectionLocal.intersected(rect());
    if (local.isEmpty()) return;
    QRegion outside(rect());
    outside -= local;
    painter.save();
    painter.setClipRegion(outside);
    painter.fillRect(rect(), QColor(0, 0, 0, 118));
    painter.restore();
}

void ScreenOverlay::paintSelection(QPainter &painter, const QRect &selectionLocal)
{
    painter.save();
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(QColor(255, 255, 255, 90), 1));
    painter.drawRect(QRectF(selectionLocal).adjusted(-1.5, -1.5, 1.5, 1.5));
    painter.setPen(QPen(Accent, 1.6));
    painter.drawRect(QRectF(selectionLocal).adjusted(-0.5, -0.5, 0.5, 0.5));
    painter.restore();
}

void ScreenOverlay::paintHandles(QPainter &painter, const QRect &selectionLocal)
{
    if (m_session->tool() != ShotTool::Select || hasTextInput()) return;
    const int size = 7;
    const int half = size / 2;
    const QPoint points[] = {
        selectionLocal.topLeft(), QPoint(selectionLocal.center().x(), selectionLocal.top()),
        selectionLocal.topRight(), QPoint(selectionLocal.right(), selectionLocal.center().y()),
        selectionLocal.bottomRight(), QPoint(selectionLocal.center().x(), selectionLocal.bottom()),
        selectionLocal.bottomLeft(), QPoint(selectionLocal.left(), selectionLocal.center().y()),
    };
    painter.save();
    painter.setPen(QPen(Accent, 1.2));
    painter.setBrush(HandleFill);
    for (const QPoint &point : points) {
        if (!rect().adjusted(-half, -half, half, half).contains(point)) continue;
        painter.drawRect(QRect(point - QPoint(half, half), QSize(size, size)));
    }
    painter.restore();
}

void ScreenOverlay::paintSizeBadge(QPainter &painter, const QRect &selectionLocal)
{
    const QRect selection = m_session->selection();
    if (selection.isEmpty()) return;
    const QString text = QStringLiteral("%1 × %2").arg(selection.width()).arg(selection.height());
    QFont font;
    font.setPixelSize(11);
    const QFontMetrics metrics(font);
    const QSize boxSize(metrics.horizontalAdvance(text) + 14, metrics.height() + 7);
    QPoint position(selectionLocal.left(), selectionLocal.top() - boxSize.height() - 5);
    if (position.y() < 4) position.setY(selectionLocal.top() + 5);
    position.setX(qBound(0, position.x(), qMax(0, width() - boxSize.width() - 4)));
    const QRect box(position, boxSize);
    painter.save();
    painter.setPen(QPen(QColor(255, 255, 255, 45), 1));
    painter.setBrush(QColor(22, 24, 28, 215));
    painter.drawRoundedRect(box, 4, 4);
    painter.setPen(QColor(240, 242, 246));
    painter.setFont(font);
    painter.drawText(box, Qt::AlignCenter, text);
    painter.restore();
}

void ScreenOverlay::paintGuides(QPainter &painter)
{
    if (!m_session->hasCursor() || m_session->tool() != ShotTool::Select) return;
    if (!m_session->selection().isEmpty() && !m_session->draggingSelection()) return;
    const QPointF cursor = toLocal(m_session->cursor());
    if (!rect().contains(cursor.toPoint())) return;
    painter.save();
    painter.setPen(QPen(QColor(255, 255, 255, 70), 1, Qt::DashLine));
    painter.drawLine(QPointF(0, cursor.y()), QPointF(width(), cursor.y()));
    painter.drawLine(QPointF(cursor.x(), 0), QPointF(cursor.x(), height()));
    painter.restore();
}

void ScreenOverlay::paintHover(QPainter &painter)
{
    if (!m_session->selection().isEmpty() || m_session->hoverWindow().isEmpty()) return;
    const QRect local = toLocal(m_session->hoverWindow());
    if (!local.intersects(rect())) return;
    painter.save();
    painter.setPen(QPen(Accent, 2));
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(local.adjusted(1, 1, -1, -1));

    QFont font;
    font.setPixelSize(12);
    const QFontMetrics metrics(font);
    const QString size = QStringLiteral("%1 × %2").arg(local.width()).arg(local.height());
    const QString title = m_session->hoverTitle().isEmpty() ? size
        : elide(metrics, m_session->hoverTitle(), 360) + QStringLiteral("  ") + size;
    const QSize boxSize(metrics.horizontalAdvance(title) + 16, metrics.height() + 8);
    QPoint position(local.left(), local.top() - boxSize.height() - 4);
    if (position.y() < 4) position.setY(local.top() + 4);
    position.setX(qBound(0, position.x(), qMax(0, width() - boxSize.width() - 4)));
    const QRect box(position, boxSize);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(22, 24, 28, 225));
    painter.drawRoundedRect(box, 4, 4);
    painter.setPen(QColor(240, 242, 246));
    painter.setFont(font);
    painter.drawText(box.adjusted(8, 0, -8, 0), Qt::AlignVCenter | Qt::AlignLeft, title);
    painter.restore();
}

void ScreenOverlay::paintMagnifier(QPainter &painter)
{
    if (!m_session->magnifierEnabled() || !m_session->hasCursor()) return;
    if (m_session->tool() != ShotTool::Select) return;
    if (!m_session->selection().isEmpty() && !m_session->draggingSelection()) return;

    const QPointF global = m_session->cursor();
    const QPointF local = toLocal(global);
    if (!rect().contains(local.toPoint())) return;
    const ScreenInfo *screen = m_session->screenAt(global);
    if (!screen || screen->logical != m_info.logical) return;

    const int panel = 132;
    const int zoom = qBound(2, m_session->magnifierZoom(), 20);
    const int sample = qMax(4, panel / zoom);
    const QPoint localPhysical(qRound((global.x() - m_info.logical.x()) * m_info.dpr),
                               qRound((global.y() - m_info.logical.y()) * m_info.dpr));
    QPoint imagePos = m_info.source.topLeft() + localPhysical;
    QRect source(imagePos.x() - sample / 2, imagePos.y() - sample / 2, sample, sample);
    source.moveLeft(qBound(m_info.source.left(), source.left(), m_info.source.right() - sample + 1));
    source.moveTop(qBound(m_info.source.top(), source.top(), m_info.source.bottom() - sample + 1));

    const QImage &desktop = m_session->desktopImage();
    QImage zoomed = desktop.copy(source).scaled(panel, panel, Qt::IgnoreAspectRatio, Qt::FastTransformation);
    if (zoomed.isNull()) return;

    QPoint panelPos = local.toPoint() + QPoint(26, 26);
    if (panelPos.x() + panel + 8 > width()) panelPos.setX(local.toPoint().x() - panel - 26);
    if (panelPos.y() + panel + 56 > height()) panelPos.setY(local.toPoint().y() - panel - 56);
    panelPos.setX(qBound(8, panelPos.x(), qMax(8, width() - panel - 8)));
    panelPos.setY(qBound(8, panelPos.y(), qMax(8, height() - panel - 56)));

    const QRgb pixel = desktop.pixel(imagePos);
    const QColor color(pixel);
    const QString firstLine = color.name().toUpper();
    const QString secondLine = QStringLiteral("X %1  Y %2").arg(qRound(global.x())).arg(qRound(global.y()));
    const QRect selection = m_session->selection();
    const QString thirdLine = selection.isEmpty() ? QString()
        : QStringLiteral("%1 × %2").arg(selection.width()).arg(selection.height());

    painter.save();
    painter.setPen(QPen(QColor(255, 255, 255, 120), 1));
    painter.setBrush(QColor(16, 18, 22, 230));
    painter.drawRoundedRect(QRect(panelPos, QSize(panel, panel + 52)), 6, 6);
    painter.drawImage(QRect(panelPos + QPoint(2, 2), QSize(panel - 4, panel - 4)), zoomed);

    // 放大镜中心十字线。
    const QPoint center = panelPos + QPoint(panel / 2, panel / 2);
    painter.setPen(QPen(QColor(255, 255, 255, 170), 1));
    painter.drawLine(panelPos + QPoint(panel / 2, 10), panelPos + QPoint(panel / 2, panel - 10));
    painter.drawLine(panelPos + QPoint(10, panel / 2), panelPos + QPoint(panel - 10, panel / 2));
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(QColor(0, 0, 0, 160), 1));
    painter.drawRect(QRect(center - QPoint(6, 6), QSize(12, 12)));

    QFont font;
    font.setPixelSize(11);
    painter.setFont(font);
    painter.setPen(QColor(240, 242, 246));
    painter.drawText(QRect(panelPos + QPoint(8, panel + 2), QSize(panel - 16, 16)),
                     Qt::AlignLeft | Qt::AlignVCenter, firstLine);
    painter.setPen(QColor(190, 196, 206));
    painter.drawText(QRect(panelPos + QPoint(8, panel + 18), QSize(panel - 16, 16)),
                     Qt::AlignLeft | Qt::AlignVCenter, secondLine);
    if (!thirdLine.isEmpty()) {
        painter.drawText(QRect(panelPos + QPoint(8, panel + 34), QSize(panel - 16, 16)),
                         Qt::AlignLeft | Qt::AlignVCenter, thirdLine);
    }
    painter.setPen(QPen(QColor(255, 255, 255, 120), 1));
    painter.setBrush(color);
    painter.drawRect(QRect(panelPos + QPoint(panel - 26, panel + 2), QSize(16, 16)));
    painter.restore();
}

void ScreenOverlay::mousePressEvent(QMouseEvent *event)
{
    if (m_textEdit) commitTextInput();
    m_session->onPress(this, event->globalPosition(), event->button());
    updateCursorShape();
    event->accept();
}

void ScreenOverlay::mouseMoveEvent(QMouseEvent *event)
{
    m_session->onMove(this, event->globalPosition());
    updateCursorShape();
    event->accept();
}

void ScreenOverlay::mouseReleaseEvent(QMouseEvent *event)
{
    m_session->onRelease(this, event->globalPosition());
    updateCursorShape();
    event->accept();
}

void ScreenOverlay::keyPressEvent(QKeyEvent *event)
{
    if (m_textEdit) {
        QWidget::keyPressEvent(event);
        return;
    }
    m_session->onKeyPress(event);
}

void ScreenOverlay::leaveEvent(QEvent *event)
{
    m_session->onLeave(this);
    QWidget::leaveEvent(event);
}

void ScreenOverlay::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
#ifdef Q_OS_WIN
    // 常驻置顶的悬浮窗（如便签、助手面板）可能压住新建的覆盖层。
    forceTopmost();
#endif
    updateCursorShape();
}

bool ScreenOverlay::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_textEdit) {
        if (event->type() == QEvent::KeyPress) {
            auto *key = static_cast<QKeyEvent *>(event);
            if (key->key() == Qt::Key_Escape) {
                cancelTextInput();
                return true;
            }
            if (key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter) {
                commitTextInput();
                return true;
            }
        } else if (event->type() == QEvent::FocusOut) {
            // 点击别处时提交已输入的文字，避免编辑内容随焦点丢失。
            QTimer::singleShot(0, this, [this]() {
                if (m_textEdit && !m_textEdit->hasFocus()) commitTextInput();
            });
        }
    }
    return QWidget::eventFilter(watched, event);
}

void ScreenOverlay::beginTextInput(const QPointF &globalPosition)
{
    cancelTextInput();
    m_textOrigin = globalPosition;
    const QPointF local = toLocal(globalPosition);

    QFont font;
    font.setPixelSize(qBound(12, 12 + m_session->width() * 3, 48));
    font.setBold(true);

    m_textEdit = new QLineEdit(this);
    m_textEdit->setFrame(false);
    m_textEdit->setContentsMargins(0, 0, 0, 0);
    m_textEdit->setTextMargins(0, 0, 0, 0);
    const QString color = m_session->color().name();
    // 字体必须写进样式表：设置样式表会重新应用界面默认字体，只调 setFont 会被覆盖。
    m_textEdit->setStyleSheet(QStringLiteral(
        "QLineEdit { background: rgba(20, 22, 26, 200); color: %1; border: none; padding: 0;"
        " font-size: %2px; font-weight: bold; }")
        .arg(color).arg(font.pixelSize()));
    m_textEdit->setFont(font);

    // 高度取字体紧致行高，编辑框基线才能与最终 drawText 的基线一致。
    const QFontMetrics metrics(font);
    m_textEdit->setFixedHeight(qMax(1, metrics.ascent() + metrics.descent()));
    m_textEdit->resize(240, m_textEdit->height());
    m_textEdit->move(local.toPoint());
    m_textEdit->show();
    m_textEdit->setFocus(Qt::MouseFocusReason);
    installEventFilter(m_textEdit);
    update();
}

void ScreenOverlay::paintTextInputOutline(QPainter &painter)
{
    if (!m_textEdit) return;
    painter.save();
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(QColor(255, 255, 255, 170), 1, Qt::DashLine));
    painter.drawRect(m_textEdit->geometry().adjusted(-3, -3, 3, 3));
    painter.restore();
}

void ScreenOverlay::cancelTextInput()
{
    if (!m_textEdit) return;
    QLineEdit *edit = m_textEdit;
    m_textEdit = nullptr;
    removeEventFilter(edit);
    edit->hide();
    edit->deleteLater();
    setFocus(Qt::OtherFocusReason);
    update();
}

void ScreenOverlay::commitTextInput()
{
    if (!m_textEdit) return;
    QLineEdit *edit = m_textEdit;
    m_textEdit = nullptr;
    removeEventFilter(edit);
    const QString text = edit->text();
    edit->hide();
    edit->deleteLater();
    setFocus(Qt::OtherFocusReason);
    update();
    if (!text.trimmed().isEmpty()) m_session->commitText(m_textOrigin, text);
}

void ScreenOverlay::updateCursorShape()
{
    if (m_textEdit) return;
    setCursor(m_session->cursorFor(m_session->cursor()));
}