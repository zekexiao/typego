#include "PinWindow.h"

#include <QApplication>
#include <QClipboard>
#include <QCloseEvent>
#include <QContextMenuEvent>
#include <QDateTime>
#include <QDir>
#include <QFileDialog>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QScreen>
#include <QStandardPaths>
#include <QWheelEvent>
#include <QtMath>

PinWindow::PinWindow(const QImage &image, const QPoint &position, QWidget *parent)
    : QWidget(parent, Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool)
    , m_image(image)
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

void PinWindow::updateWindowSize()
{
    if (m_image.isNull()) return;
    const qreal dpr = m_image.devicePixelRatio() > 0 ? m_image.devicePixelRatio() : 1.0;
    const QSizeF logical(m_image.width() / dpr, m_image.height() / dpr);
    resize(qMax(1, qRound(logical.width() * m_zoom)),
           qMax(1, qRound(logical.height() * m_zoom)));
}

void PinWindow::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.drawImage(rect(), m_image);
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
    event->accept();
}

void PinWindow::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) return;
    close();
}

void PinWindow::wheelEvent(QWheelEvent *event)
{
    const int delta = event->angleDelta().y();
    if (delta == 0) return;
    applyZoom(m_zoom * (delta > 0 ? 1.1 : 0.9));
    event->accept();
}

void PinWindow::applyZoom(qreal zoom)
{
    const qreal target = qBound<qreal>(0.1, zoom, 4.0);
    if (qFuzzyCompare(target, m_zoom)) return;
    const QPoint center = mapToGlobal(rect().center());
    m_zoom = target;
    updateWindowSize();
    move(center - rect().center());
}

void PinWindow::contextMenuEvent(QContextMenuEvent *event)
{
    QMenu menu(this);
    QAction *copy = menu.addAction(tr("Copy"));
    QAction *save = menu.addAction(tr("Save as..."));
    menu.addSeparator();
    QAction *closeAction = menu.addAction(tr("Close"));
    QAction *chosen = menu.exec(event->globalPos());
    if (chosen == copy) {
        QGuiApplication::clipboard()->setImage(m_image);
    } else if (chosen == save) {
        saveAs();
    } else if (chosen == closeAction) {
        close();
    }
}

void PinWindow::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        close();
        return;
    }
    if (event->key() == Qt::Key_C && (event->modifiers() & Qt::ControlModifier)) {
        QGuiApplication::clipboard()->setImage(m_image);
        return;
    }
    QWidget::keyPressEvent(event);
}

void PinWindow::closeEvent(QCloseEvent *event)
{
    event->accept();
    emit closed();
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
    if (!m_image.save(path)) {
        QMessageBox::warning(this, tr("Save failed"), tr("Cannot save the image to %1").arg(path));
    }
}