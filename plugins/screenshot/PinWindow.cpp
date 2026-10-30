#include "PinWindow.h"

#include "PinEditor.h"

#include <QApplication>
#include <QClipboard>
#include <QCloseEvent>
#include <QContextMenuEvent>
#include <QDateTime>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPointer>
#include <QScreen>
#include <QStandardPaths>
#include <QTimer>
#include <QWheelEvent>
#include <QtMath>

namespace {
const qreal kMinZoom = 0.1;
const qreal kMaxZoom = 6.0;
}

PinWindow::PinWindow(const QImage &base, const QVector<ShotAnnotation> &annotations,
                     const QPoint &position, QWidget *parent)
    : QWidget(parent, Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool)
    , m_base(base)
    , m_annotations(annotations)
{
    setAttribute(Qt::WA_DeleteOnClose, true);
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    setFocusPolicy(Qt::StrongFocus);
    setCursor(Qt::OpenHandCursor);
    setWindowTitle(tr("Pinned screenshot"));
    updateWindowSize();

    QScreen *screen = QGuiApplication::screenAt(position);
    if (!screen) screen = QGuiApplication::primaryScreen();
    if (screen) {
        const QRect available = screen->availableGeometry();
        QPoint target = position;
        if (target.x() + width() > available.right()) target.setX(available.right() - width());
        if (target.y() + height() > available.bottom()) target.setY(available.bottom() - height());
        if (target.x() < available.left()) target.setX(available.left());
        if (target.y() < available.top()) target.setY(available.top());
        move(target);
    } else {
        move(position);
    }
}

QSizeF PinWindow::imageLogicalSize() const
{
    const qreal dpr = m_base.devicePixelRatio() > 0 ? m_base.devicePixelRatio() : 1.0;
    return QSizeF(m_base.width() / dpr, m_base.height() / dpr);
}

const QImage &PinWindow::composedImage()
{
    if (m_composedDirty || m_composed.isNull()) {
        m_composed = composeShotImage(m_base, m_annotations);
        m_composedDirty = false;
    }
    return m_composed;
}

void PinWindow::updateWindowSize()
{
    if (m_base.isNull()) return;
    const QSizeF logical = imageLogicalSize();
    resize(qMax(1, qRound(logical.width() * m_zoom)),
           qMax(1, qRound(logical.height() * m_zoom)));
}

void PinWindow::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.drawImage(rect(), composedImage());
    painter.setPen(QPen(QColor(22, 119, 255, 180), 1));
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(rect().adjusted(0, 0, -1, -1));
}

void PinWindow::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) return;
    activateWindow();
    setFocus();
    m_dragging = true;
    m_dragOffset = event->globalPosition().toPoint() - frameGeometry().topLeft();
    setCursor(Qt::ClosedHandCursor);
    event->accept();
}

void PinWindow::mouseMoveEvent(QMouseEvent *event)
{
    if (!m_dragging) return;
    move(event->globalPosition().toPoint() - m_dragOffset);
    event->accept();
}

void PinWindow::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) return;
    m_dragging = false;
    setCursor(Qt::OpenHandCursor);
    QWidget::mouseReleaseEvent(event);
}

void PinWindow::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) return;
    m_dragging = false;
    editAnnotations();
    event->accept();
}

void PinWindow::wheelEvent(QWheelEvent *event)
{
    const int delta = event->angleDelta().y();
    if (delta == 0) return;
    applyZoom(m_zoom * (delta > 0 ? 1.1 : 0.9), event->globalPosition().toPoint());
    event->accept();
}

void PinWindow::applyZoom(qreal zoom, const QPoint &anchorGlobal)
{
    const qreal target = qBound(kMinZoom, zoom, kMaxZoom);
    if (qFuzzyCompare(target, m_zoom)) return;
    const QPoint local = mapFromGlobal(anchorGlobal);
    const qreal factor = target / m_zoom;
    m_zoom = target;
    updateWindowSize();
    // 保持光标下的内容不漂移。
    const QPoint newLocal(qRound(local.x() * factor), qRound(local.y() * factor));
    move(anchorGlobal - newLocal);
}

void PinWindow::contextMenuEvent(QContextMenuEvent *event)
{
    QMenu menu(this);
    QAction *edit = menu.addAction(tr("Edit annotations"));
    QAction *copy = menu.addAction(tr("Copy"));
    QAction *save = menu.addAction(tr("Save as..."));
    menu.addSeparator();
    QAction *closeAction = menu.addAction(tr("Close"));

    QAction *chosen = menu.exec(event->globalPos());
    if (chosen == edit) editAnnotations();
    else if (chosen == copy) copyImage();
    else if (chosen == save) saveAs();
    else if (chosen == closeAction) close();
}

void PinWindow::keyPressEvent(QKeyEvent *event)
{
    const int key = event->key();
    const bool control = event->modifiers() & Qt::ControlModifier;
    if (key == Qt::Key_Escape || key == Qt::Key_Delete) {
        close();
        return;
    }
    if (control) {
        if (key == Qt::Key_C) {
            copyImage();
            return;
        }
        if (key == Qt::Key_S) {
            saveAs();
            return;
        }
        if (key == Qt::Key_E) {
            editAnnotations();
            return;
        }
        if (key == Qt::Key_W) {
            close();
            return;
        }
        if (key == Qt::Key_0) {
            applyZoom(1.0, mapToGlobal(rect().center()));
            return;
        }
    }
    QWidget::keyPressEvent(event);
}

void PinWindow::closeEvent(QCloseEvent *event)
{
    event->accept();
    emit closed();
}

void PinWindow::hideEvent(QHideEvent *event)
{
    // 截图前宿主会隐藏所有顶层窗口；编辑窗口随贴图一起关闭，避免恢复时错位。
    if (PinEditor *editor = findChild<PinEditor *>(QString(), Qt::FindDirectChildrenOnly))
        editor->close();
    QWidget::hideEvent(event);
}

void PinWindow::editAnnotations()
{
    if (QPointer<PinEditor> existing = findChild<PinEditor *>(QString(), Qt::FindDirectChildrenOnly)) {
        existing->raise();
        existing->activateWindow();
        return;
    }
    auto *editor = new PinEditor(m_base, m_annotations, this);
    connect(editor, &PinEditor::annotationsChanged, this, &PinWindow::setAnnotations);
    QScreen *screen = QGuiApplication::screenAt(frameGeometry().center());
    if (!screen) screen = QGuiApplication::primaryScreen();
    QPoint target = frameGeometry().center() - QPoint(editor->width() / 2, editor->height() / 2);
    if (screen) {
        const QRect available = screen->availableGeometry();
        target.setX(qBound(available.left(), target.x(),
                           qMax(available.left(), available.right() - editor->width())));
        target.setY(qBound(available.top(), target.y(),
                           qMax(available.top(), available.bottom() - editor->height())));
    }
    editor->move(target);
    editor->show();
    editor->raise();
    editor->activateWindow();
}

void PinWindow::setAnnotations(const QVector<ShotAnnotation> &annotations)
{
    m_annotations = annotations;
    m_composedDirty = true;
    update();
}

void PinWindow::copyImage()
{
    QGuiApplication::clipboard()->setImage(composedImage());
}

void PinWindow::saveAs()
{
    QString directory = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    if (directory.isEmpty()) directory = QDir::homePath();
    const QString defaultPath = QDir(directory).filePath(
        QStringLiteral("Screenshot_%1.png").arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss"))));
    QString path = QFileDialog::getSaveFileName(this, tr("Save screenshot"), defaultPath,
                                                tr("PNG image (*.png);;JPEG image (*.jpg *.jpeg)"));
    if (path.isEmpty()) return;
    if (!path.contains(QLatin1Char('.'))) path += QStringLiteral(".png");
    const QString suffix = QFileInfo(path).suffix().toLower();
    const QByteArray format = suffix == QLatin1String("jpg") || suffix == QLatin1String("jpeg")
        ? QByteArrayLiteral("jpg") : QByteArrayLiteral("png");
    if (!composedImage().save(path, format.constData()))
        QMessageBox::warning(this, tr("Save failed"), tr("Cannot save the image to %1").arg(path));
}