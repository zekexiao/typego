#include "ScrollResultWindow.h"

#include <QClipboard>
#include <QCloseEvent>
#include <QDateTime>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QMessageBox>
#include <QPixmap>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QStandardPaths>
#include <QVBoxLayout>
#include <QtMath>

ScrollResultWindow::ScrollResultWindow(const QImage &image, const QPoint &anchor, QWidget *parent)
    : QWidget(parent)
    , m_image(image)
{
    setAttribute(Qt::WA_DeleteOnClose, true);
    setWindowTitle(tr("Scrolling screenshot"));

    const qreal dpr = m_image.devicePixelRatio() > 0 ? m_image.devicePixelRatio() : 1.0;
    const QSize logical(qCeil(m_image.width() / dpr), qCeil(m_image.height() / dpr));

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(10, 10, 10, 10);
    root->setSpacing(8);

    auto *bar = new QHBoxLayout;
    auto *title = new QLabel(tr("Scrolling screenshot · %1 × %2").arg(logical.width()).arg(logical.height()), this);
    QFont titleFont = title->font();
    titleFont.setBold(true);
    title->setFont(titleFont);
    bar->addWidget(title);
    m_status = new QLabel(tr("Generated from the selected scrolling area"), this);
    m_status->setObjectName(QStringLiteral("muted"));
    bar->addWidget(m_status, 1);
    auto *copy = new QPushButton(tr("Copy"), this);
    auto *save = new QPushButton(tr("Save as..."), this);
    auto *close = new QPushButton(tr("Close"), this);
    bar->addWidget(copy);
    bar->addWidget(save);
    bar->addWidget(close);
    root->addLayout(bar);

    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(false);
    scroll->setAlignment(Qt::AlignHCenter | Qt::AlignTop);
    scroll->setFrameShape(QFrame::StyledPanel);
    auto *view = new QLabel(scroll);
    QPixmap pixmap = QPixmap::fromImage(m_image);
    pixmap.setDevicePixelRatio(dpr);
    view->setPixmap(pixmap);
    view->setFixedSize(logical);
    view->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    scroll->setWidget(view);
    root->addWidget(scroll, 1);

    connect(copy, &QPushButton::clicked, this, &ScrollResultWindow::copyImage);
    connect(save, &QPushButton::clicked, this, &ScrollResultWindow::saveAs);
    connect(close, &QPushButton::clicked, this, &QWidget::close);

    QScreen *screen = QGuiApplication::screenAt(anchor);
    if (!screen) screen = QGuiApplication::primaryScreen();
    if (screen) {
        const QRect available = screen->availableGeometry();
        const int maxWidth = qMax(420, qMin(900, available.width() - 40));
        const int maxHeight = qMax(360, qMin(900, available.height() - 40));
        resize(qBound(420, logical.width() + 64, maxWidth),
               qBound(360, logical.height() + 110, maxHeight));
        move(available.center() - rect().center());
    } else {
        resize(qBound(420, logical.width() + 64, 900), qBound(360, logical.height() + 110, 900));
    }
}

void ScrollResultWindow::copyImage()
{
    if (QClipboard *clipboard = QGuiApplication::clipboard()) {
        clipboard->setImage(m_image);
        if (m_status) m_status->setText(tr("Copied to clipboard"));
    }
}

void ScrollResultWindow::saveAs()
{
    QString directory = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    if (directory.isEmpty()) directory = QDir::homePath();
    const QString defaultPath = QDir(directory).filePath(
        QStringLiteral("LongScreenshot_%1.png")
            .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss"))));
    QString path = QFileDialog::getSaveFileName(this, tr("Save scrolling screenshot"), defaultPath,
                                                tr("PNG image (*.png);;JPEG image (*.jpg *.jpeg)"));
    if (path.isEmpty()) return;
    if (QFileInfo(path).suffix().isEmpty()) path += QStringLiteral(".png");
    if (!m_image.save(path)) {
        QMessageBox::warning(this, tr("Save failed"), tr("Cannot save the image to %1").arg(path));
        return;
    }
    if (m_status) m_status->setText(tr("Saved to %1").arg(QDir::toNativeSeparators(path)));
}

void ScrollResultWindow::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        close();
        return;
    }
    if (event->key() == Qt::Key_C && (event->modifiers() & Qt::ControlModifier)) {
        copyImage();
        return;
    }
    QWidget::keyPressEvent(event);
}

void ScrollResultWindow::closeEvent(QCloseEvent *event)
{
    event->accept();
    emit closed();
}