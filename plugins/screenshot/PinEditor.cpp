#include "PinEditor.h"

#include "CaptureToolbar.h"
#include "ShotIcons.h"

#include <QClipboard>
#include <QCloseEvent>
#include <QCursor>
#include <QDateTime>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QScreen>
#include <QStandardPaths>
#include <QTimer>
#include <QtMath>

#include <utility>

namespace {

const qreal kMinScale = 0.05;

} // namespace

PinEditor::PinEditor(const QImage &base, const QVector<ShotAnnotation> &annotations, QWidget *parent)
    : QWidget(parent, Qt::Tool | Qt::FramelessWindowHint)
    , m_base(base)
    , m_annotations(annotations)
{
    setAttribute(Qt::WA_DeleteOnClose, true);
    setWindowTitle(tr("Edit pinned screenshot"));
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);

    for (const ShotAnnotation &annotation : std::as_const(m_annotations))
        m_nextNumber = qMax(m_nextNumber, annotation.number + 1);

    // 大图按可用屏幕缩放显示，小图保持原始尺寸。
    const QSizeF logical = imageLogicalSize();
    QScreen *screen = parent ? QGuiApplication::screenAt(parent->frameGeometry().center()) : nullptr;
    if (!screen) screen = QGuiApplication::screenAt(QCursor::pos());
    if (!screen) screen = QGuiApplication::primaryScreen();
    const QRect available = screen ? screen->availableGeometry() : QRect(0, 0, 1280, 800);
    const qreal fitWidth = (available.width() - 80.0) / qMax(1.0, logical.width());
    const qreal fitHeight = (available.height() - 180.0) / qMax(1.0, logical.height());
    m_scale = qBound(kMinScale, qMin<qreal>(1.0, qMin(fitWidth, fitHeight)), 1.0);
    const QSize canvasSize(qMax(1, qRound(logical.width() * m_scale)),
                           qMax(1, qRound(logical.height() * m_scale)));

    m_toolbar = new CaptureToolbar(this, CaptureToolbar::Mode::Pin);
    m_toolbar->adjustSize();
    connect(m_toolbar, &CaptureToolbar::toolChanged, this, &PinEditor::setTool);
    connect(m_toolbar, &CaptureToolbar::colorChanged, this, &PinEditor::applyColorToSelection);
    connect(m_toolbar, &CaptureToolbar::widthChanged, this, &PinEditor::applyWidthToSelection);
    connect(m_toolbar, &CaptureToolbar::undoRequested, this, &PinEditor::undo);
    connect(m_toolbar, &CaptureToolbar::redoRequested, this, &PinEditor::redo);
    connect(m_toolbar, &CaptureToolbar::copyRequested, this, &PinEditor::copyImage);
    connect(m_toolbar, &CaptureToolbar::saveRequested, this, &PinEditor::saveAs);
    connect(m_toolbar, &CaptureToolbar::closeRequested, this, &QWidget::close);

    const int padding = 10;
    const int gap = 8;
    const QSize toolbarSize = m_toolbar->size();
    const int windowWidth = qMax(canvasSize.width(), toolbarSize.width()) + padding * 2;
    const int windowHeight = padding + canvasSize.height() + gap + toolbarSize.height() + padding;
    setFixedSize(windowWidth, windowHeight);
    m_canvasRect = QRect(QPoint((windowWidth - canvasSize.width()) / 2, padding), canvasSize);
    m_toolbar->move(qMax(padding, (windowWidth - toolbarSize.width()) / 2),
                    padding + canvasSize.height() + gap);

    m_composed = composeShotImage(m_base, m_annotations);
    m_history.append(m_annotations);
    refreshToolbarHistory();
    m_toolbar->setTool(m_tool);
    setCursor(Qt::ArrowCursor);
}

QSizeF PinEditor::imageLogicalSize() const
{
    const qreal dpr = m_base.devicePixelRatio() > 0 ? m_base.devicePixelRatio() : 1.0;
    return QSizeF(m_base.width() / dpr, m_base.height() / dpr);
}

QPointF PinEditor::toLogical(const QPointF &local) const
{
    if (m_scale <= 0) return local;
    return QPointF((local.x() - m_canvasRect.x()) / m_scale,
                   (local.y() - m_canvasRect.y()) / m_scale);
}

QPointF PinEditor::clampToImage(const QPointF &logical) const
{
    const QSizeF size = imageLogicalSize();
    return QPointF(qBound(0.0, logical.x(), size.width()),
                   qBound(0.0, logical.y(), size.height()));
}

void PinEditor::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.fillRect(rect(), QColor(20, 22, 26));
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    // 移动 / 缩放标注时逐帧重新合成，保证拖动过程可见。
    painter.drawImage(m_canvasRect, composedImage());
    painter.setPen(QPen(QColor(22, 119, 255, 200), 1));
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(m_canvasRect.adjusted(0, 0, -1, -1));
    paintPendingAnnotation(painter);
    paintSelection(painter);
}

QImage PinEditor::composedImage() const
{
    if (m_drag == DragMode::Move || m_drag == DragMode::Resize)
        return composeShotImage(m_base, m_annotations);
    return m_composed;
}

void PinEditor::paintSelection(QPainter &painter)
{
    if (m_tool != ShotTool::Select) return;
    if (m_selected < 0 || m_selected >= m_annotations.size()) return;
    painter.save();
    painter.setClipRect(m_canvasRect);
    painter.translate(m_canvasRect.topLeft());
    painter.scale(m_scale, m_scale);
    paintShotSelection(painter, m_annotations.at(m_selected), 1.0 / qMax(m_scale, kMinScale));
    painter.restore();
}

void PinEditor::paintPendingAnnotation(QPainter &painter)
{
    if (!m_hasPending) return;
    painter.save();
    painter.setClipRect(m_canvasRect);
    if (m_pending.isEffect()) {
        const qreal dpr = m_base.devicePixelRatio() > 0 ? m_base.devicePixelRatio() : 1.0;
        const QRectF logical = m_pending.rect();
        const QRect source = QRectF(logical.x() * dpr, logical.y() * dpr,
                                    logical.width() * dpr, logical.height() * dpr)
                                 .toAlignedRect().intersected(m_composed.rect());
        if (!source.isEmpty()) {
            QImage patch = m_composed.copy(source);
            applyShotEffect(patch, patch.rect(), m_pending.tool, qMax(3, qRound(8 * dpr)));
            const QRectF target(m_canvasRect.x() + logical.x() * m_scale,
                                m_canvasRect.y() + logical.y() * m_scale,
                                logical.width() * m_scale, logical.height() * m_scale);
            painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
            painter.drawImage(target, patch);
        }
    } else {
        painter.translate(m_canvasRect.topLeft());
        painter.scale(m_scale, m_scale);
        paintShotAnnotation(painter, m_pending);
    }
    painter.restore();
}

QVector<QPointF> PinEditor::handlePoints(const ShotAnnotation &annotation) const
{
    return shotAnnotationHandles(annotation);
}

int PinEditor::handleAt(const ShotAnnotation &annotation, const QPointF &logical) const
{
    return shotAnnotationHandleAt(annotation, logical, 8.0 / qMax(m_scale, kMinScale));
}

int PinEditor::hitTest(const QPointF &logical) const
{
    return shotAnnotationHitTest(m_annotations, logical, 4.0 / qMax(m_scale, kMinScale));
}

void PinEditor::updateCursorFor(const QPointF &logical)
{
    if (m_tool != ShotTool::Select) {
        setCursor(Qt::CrossCursor);
        return;
    }
    if (m_selected >= 0 && m_selected < m_annotations.size()) {
        const int handle = handleAt(m_annotations.at(m_selected), logical);
        if (handle >= 0) {
            if (m_annotations.at(m_selected).tool == ShotTool::Line
                || m_annotations.at(m_selected).tool == ShotTool::Arrow) {
                setCursor(Qt::SizeAllCursor);
            } else {
                setCursor(handle == 0 || handle == 3 ? Qt::SizeFDiagCursor : Qt::SizeBDiagCursor);
            }
            return;
        }
    }
    setCursor(hitTest(logical) >= 0 ? Qt::OpenHandCursor : Qt::ArrowCursor);
}

void PinEditor::mousePressEvent(QMouseEvent *event)
{
    if (m_textEdit) {
        commitTextInput();
        return;
    }
    if (event->button() != Qt::LeftButton) return;
    setFocus(Qt::MouseFocusReason);
    if (!m_canvasRect.contains(event->position().toPoint())) return;

    const QPointF point = clampToImage(toLogical(event->position()));

    if (m_tool == ShotTool::Select) {
        // 先命中已选标注的缩放手柄，再按层级命中标注，最后取消选择。
        if (m_selected >= 0 && m_selected < m_annotations.size()) {
            const int handle = handleAt(m_annotations.at(m_selected), point);
            if (handle >= 0) {
                m_selectedHandle = handle;
                m_dragOriginal = m_annotations.at(m_selected);
                m_drag = DragMode::Resize;
                return;
            }
        }
        const int index = hitTest(point);
        if (index >= 0) {
            m_selected = index;
            m_selectedHandle = -1;
            m_dragOriginal = m_annotations.at(index);
            m_dragStart = point;
            m_drag = DragMode::Move;
            update();
        } else if (m_selected >= 0) {
            m_selected = -1;
            update();
        }
        return;
    }

    if (m_tool == ShotTool::Text) {
        beginTextInput(point);
        return;
    }
    if (m_tool == ShotTool::Number) {
        ShotAnnotation annotation;
        annotation.tool = ShotTool::Number;
        annotation.color = m_color;
        annotation.width = m_width;
        annotation.start = point;
        annotation.end = point;
        annotation.number = m_nextNumber++;
        commitAnnotation(annotation);
        return;
    }

    m_pending = ShotAnnotation{};
    m_pending.tool = m_tool;
    m_pending.color = m_color;
    m_pending.width = m_width;
    m_pending.start = point;
    m_pending.end = point;
    if (m_tool == ShotTool::Pen) m_pending.points.append(point);
    m_hasPending = true;
    m_drag = DragMode::Draw;
    update();
}

void PinEditor::applyAnnotationDrag(const QPointF &point)
{
    if (m_selected < 0 || m_selected >= m_annotations.size()) return;
    ShotAnnotation &annotation = m_annotations[m_selected];
    if (m_drag == DragMode::Move) {
        annotation = m_dragOriginal;
        shotAnnotationTranslate(annotation, point - m_dragStart);
    } else if (m_drag == DragMode::Resize) {
        shotAnnotationResize(annotation, m_dragOriginal, m_selectedHandle, point);
    }
}

void PinEditor::mouseMoveEvent(QMouseEvent *event)
{
    if (m_drag == DragMode::Move || m_drag == DragMode::Resize) {
        // 拖动中用未裁剪的坐标，标注才能一直跟在光标下。
        applyAnnotationDrag(toLogical(event->position()));
        update();
        return;
    }

    const QPointF point = clampToImage(toLogical(event->position()));
    if (m_drag == DragMode::Draw && m_hasPending) {
        if (m_pending.tool == ShotTool::Pen) {
            if (m_pending.points.isEmpty()
                || QLineF(m_pending.points.last(), point).length() > 2.0 / qMax(m_scale, kMinScale))
                m_pending.points.append(point);
        } else {
            m_pending.end = point;
        }
        update();
        return;
    }
    updateCursorFor(point);
}

void PinEditor::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton || m_drag == DragMode::None) return;
    const QPointF point = clampToImage(toLogical(event->position()));

    if (m_drag == DragMode::Move || m_drag == DragMode::Resize) {
        // 松手位置可能比最后一次移动事件更接近光标，用它补齐避免落后一段。
        applyAnnotationDrag(toLogical(event->position()));
        m_drag = DragMode::None;
        m_selectedHandle = -1;
        // 位置没有变化时不产生空的历史记录。
        if (m_selected >= 0 && m_selected < m_annotations.size()) {
            const ShotAnnotation &annotation = m_annotations.at(m_selected);
            const bool changed = annotation.start != m_dragOriginal.start
                || annotation.end != m_dragOriginal.end
                || annotation.points != m_dragOriginal.points;
            if (changed) {
                pushHistory();
                notifyChanged();
            } else {
                update();
            }
        }
        return;
    }

    if (m_drag == DragMode::Draw && m_hasPending) {
        if (m_pending.tool == ShotTool::Pen) m_pending.points.append(point);
        else m_pending.end = point;

        bool keep = true;
        if (m_pending.tool == ShotTool::Pen) keep = m_pending.points.size() > 1;
        else keep = QLineF(m_pending.start, m_pending.end).length() > 2.0;
        const ShotAnnotation annotation = m_pending;
        m_hasPending = false;
        m_drag = DragMode::None;
        if (keep) commitAnnotation(annotation);
        else update();
    }
}

void PinEditor::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton || m_tool != ShotTool::Select) return;
    const QPointF point = clampToImage(toLogical(event->position()));
    const int index = hitTest(point);
    if (index < 0 || m_annotations.at(index).tool != ShotTool::Text) return;
    m_selected = index;
    m_drag = DragMode::None;
    beginTextInput(m_annotations.at(index).start, m_annotations.at(index).text, index);
    update();
    event->accept();
}

void PinEditor::commitAnnotation(const ShotAnnotation &annotation)
{
    m_annotations.append(annotation);
    pushHistory();
    notifyChanged();
}

void PinEditor::deleteSelected()
{
    if (m_selected < 0 || m_selected >= m_annotations.size()) return;
    m_annotations.removeAt(m_selected);
    m_selected = -1;
    m_selectedHandle = -1;
    pushHistory();
    notifyChanged();
}

void PinEditor::applyColorToSelection(const QColor &color)
{
    m_color = color;
    if (m_selected < 0 || m_selected >= m_annotations.size()) return;
    m_annotations[m_selected].color = color;
    pushHistory();
    notifyChanged();
}

void PinEditor::applyWidthToSelection(int width)
{
    m_width = width;
    if (m_selected < 0 || m_selected >= m_annotations.size()) return;
    m_annotations[m_selected].width = width;
    pushHistory();
    notifyChanged();
}

void PinEditor::notifyChanged()
{
    m_composed = composeShotImage(m_base, m_annotations);
    refreshToolbarHistory();
    update();
    emit annotationsChanged(m_annotations);
}

void PinEditor::pushHistory()
{
    if (m_historyIndex + 1 < m_history.size()) m_history.resize(m_historyIndex + 1);
    m_history.append(m_annotations);
    // 历史只存矢量数据；超过上限时丢弃最旧的一条。
    if (m_history.size() > 100) m_history.removeFirst();
    m_historyIndex = m_history.size() - 1;
    refreshToolbarHistory();
}

void PinEditor::restoreHistory(int index)
{
    if (index < 0 || index >= m_history.size() || index == m_historyIndex) return;
    m_historyIndex = index;
    m_annotations = m_history.at(index);
    m_selected = -1;
    m_selectedHandle = -1;
    m_hasPending = false;
    m_drag = DragMode::None;
    notifyChanged();
}

void PinEditor::refreshToolbarHistory()
{
    if (!m_toolbar) return;
    m_toolbar->setUndoRedo(m_historyIndex > 0, m_historyIndex + 1 < m_history.size());
}

void PinEditor::undo()
{
    if (m_historyIndex <= 0) return;
    restoreHistory(m_historyIndex - 1);
}

void PinEditor::redo()
{
    if (m_historyIndex + 1 >= m_history.size()) return;
    restoreHistory(m_historyIndex + 1);
}

void PinEditor::setTool(ShotTool tool)
{
    m_tool = tool;
    if (tool != ShotTool::Select) m_selected = -1;
    m_toolbar->setTool(tool);
    setCursor(tool == ShotTool::Select ? Qt::ArrowCursor : Qt::CrossCursor);
    update();
}

void PinEditor::beginTextInput(const QPointF &logicalPosition, const QString &initial, int editIndex)
{
    cancelTextInput();
    m_textOrigin = logicalPosition;
    m_textEditIndex = editIndex;

    ShotAnnotation probe;
    probe.tool = ShotTool::Text;
    probe.width = m_width;
    QColor color = m_color;
    if (editIndex >= 0 && editIndex < m_annotations.size()) {
        probe.width = m_annotations.at(editIndex).width;
        color = m_annotations.at(editIndex).color;
    }
    QFont font = shotAnnotationFont(probe);
    const int pixelSize = qMax(8, qRound(font.pixelSize() * m_scale));
    font.setPixelSize(pixelSize);

    m_textEdit = new QLineEdit(this);
    m_textEdit->setFrame(false);
    m_textEdit->setContentsMargins(0, 0, 0, 0);
    m_textEdit->setTextMargins(0, 0, 0, 0);
    // 字体必须写进样式表，设置样式表会重新应用界面默认字体。
    m_textEdit->setStyleSheet(QStringLiteral(
        "QLineEdit { background: rgba(20, 22, 26, 200); color: %1; border: none; padding: 0;"
        " font-size: %2px; font-weight: bold; }")
        .arg(color.name()).arg(pixelSize));
    m_textEdit->setFont(font);
    const QFontMetrics metrics(font);
    m_textEdit->setFixedHeight(qMax(1, metrics.ascent() + metrics.descent()));
    m_textEdit->resize(qMax(120, qRound(240 * m_scale)), m_textEdit->height());
    m_textEdit->move(m_canvasRect.topLeft()
                     + QPoint(qRound(logicalPosition.x() * m_scale), qRound(logicalPosition.y() * m_scale)));
    if (!initial.isEmpty()) {
        m_textEdit->setText(initial);
        m_textEdit->selectAll();
    }
    m_textEdit->show();
    m_textEdit->setFocus(Qt::MouseFocusReason);
    installEventFilter(m_textEdit);
    update();
}

void PinEditor::cancelTextInput()
{
    if (!m_textEdit) return;
    QLineEdit *edit = m_textEdit;
    m_textEdit = nullptr;
    m_textEditIndex = -1;
    removeEventFilter(edit);
    edit->hide();
    edit->deleteLater();
    setFocus(Qt::OtherFocusReason);
    update();
}

void PinEditor::commitTextInput()
{
    if (!m_textEdit) return;
    QLineEdit *edit = m_textEdit;
    m_textEdit = nullptr;
    const int editIndex = m_textEditIndex;
    m_textEditIndex = -1;
    removeEventFilter(edit);
    const QString text = edit->text();
    edit->hide();
    edit->deleteLater();
    setFocus(Qt::OtherFocusReason);
    update();

    if (editIndex >= 0 && editIndex < m_annotations.size()) {
        // 修改已有文字：内容清空视为删除该标注。
        if (text.trimmed().isEmpty()) {
            m_annotations.removeAt(editIndex);
            m_selected = -1;
        } else {
            m_annotations[editIndex].text = text;
        }
        pushHistory();
        notifyChanged();
        return;
    }
    if (text.trimmed().isEmpty()) return;

    ShotAnnotation annotation;
    annotation.tool = ShotTool::Text;
    annotation.color = m_color;
    annotation.width = m_width;
    annotation.start = m_textOrigin;
    annotation.end = m_textOrigin;
    annotation.text = text;
    commitAnnotation(annotation);
}

bool PinEditor::eventFilter(QObject *watched, QEvent *event)
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
            QTimer::singleShot(0, this, [this]() {
                if (m_textEdit && !m_textEdit->hasFocus()) commitTextInput();
            });
        }
    }
    return QWidget::eventFilter(watched, event);
}

void PinEditor::keyPressEvent(QKeyEvent *event)
{
    if (m_textEdit) {
        QWidget::keyPressEvent(event);
        return;
    }
    const int key = event->key();
    const Qt::KeyboardModifiers modifiers = event->modifiers();
    if (key == Qt::Key_Escape) {
        if (m_selected >= 0) {
            m_selected = -1;
            update();
        } else if (m_tool != ShotTool::Select) {
            setTool(ShotTool::Select);
        } else {
            close();
        }
        event->accept();
        return;
    }
    if (key == Qt::Key_Delete || key == Qt::Key_Backspace) {
        if (m_selected >= 0) {
            deleteSelected();
            event->accept();
            return;
        }
    }
    if (key == Qt::Key_Return || key == Qt::Key_Enter) {
        if (!m_hasPending) close();
        event->accept();
        return;
    }
    if (modifiers & Qt::ControlModifier) {
        if (key == Qt::Key_Z) {
            if (modifiers & Qt::ShiftModifier) redo();
            else undo();
            event->accept();
            return;
        }
        if (key == Qt::Key_Y) {
            redo();
            event->accept();
            return;
        }
        if (key == Qt::Key_C) {
            copyImage();
            event->accept();
            return;
        }
        if (key == Qt::Key_S) {
            saveAs();
            event->accept();
            return;
        }
    }
    ShotTool tool = m_tool;
    switch (key) {
    case Qt::Key_V: tool = ShotTool::Select; break;
    case Qt::Key_R: tool = ShotTool::Rect; break;
    case Qt::Key_O: tool = ShotTool::Ellipse; break;
    case Qt::Key_A: tool = ShotTool::Arrow; break;
    case Qt::Key_L: tool = ShotTool::Line; break;
    case Qt::Key_P: tool = ShotTool::Pen; break;
    case Qt::Key_T: tool = ShotTool::Text; break;
    case Qt::Key_H: tool = ShotTool::Highlight; break;
    case Qt::Key_M: tool = ShotTool::Mosaic; break;
    case Qt::Key_B: tool = ShotTool::Blur; break;
    case Qt::Key_N: tool = ShotTool::Number; break;
    default: break;
    }
    if (tool != m_tool) {
        setTool(tool);
        event->accept();
        return;
    }
    QWidget::keyPressEvent(event);
}

void PinEditor::closeEvent(QCloseEvent *event)
{
    cancelTextInput();
    event->accept();
    emit closed();
}

void PinEditor::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    setFocus(Qt::OtherFocusReason);
}

void PinEditor::copyImage()
{
    QGuiApplication::clipboard()->setImage(m_composed);
}

void PinEditor::saveAs()
{
    QString directory = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    if (directory.isEmpty()) directory = QDir::homePath();
    const QString defaultPath = QDir(directory).filePath(
        QStringLiteral("Screenshot_%1.png")
            .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss"))));
    QString path = QFileDialog::getSaveFileName(this, tr("Save screenshot"), defaultPath,
                                                tr("PNG image (*.png);;JPEG image (*.jpg *.jpeg)"));
    if (path.isEmpty()) return;
    if (QFileInfo(path).suffix().isEmpty()) path += QStringLiteral(".png");
    const QString suffix = QFileInfo(path).suffix().toLower();
    const QByteArray format = suffix == QLatin1String("jpg") || suffix == QLatin1String("jpeg")
        ? QByteArrayLiteral("jpg") : QByteArrayLiteral("png");
    if (!m_composed.save(path, format.constData()))
        QMessageBox::warning(this, tr("Save failed"), tr("Cannot save the image to %1").arg(path));
}