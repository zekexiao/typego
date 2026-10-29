#include <QtTest>
#include <QImage>
#include <QPainter>

#include "ScrollStitcher.h"

namespace {

QImage makePage(int width, int height)
{
    QImage image(width, height, QImage::Format_RGB32);
    for (int y = 0; y < height; ++y) {
        auto *line = reinterpret_cast<QRgb *>(image.scanLine(y));
        for (int x = 0; x < width; ++x) {
            // 使用伪随机散列而不是线性渐变，避免内容出现周期性而干扰位移判断。
            quint32 hash = static_cast<quint32>(x) * 0x9E3779B1u
                ^ static_cast<quint32>(y) * 0x85EBCA77u;
            hash ^= hash >> 15;
            hash *= 0x2545F491u;
            hash ^= hash >> 13;
            line[x] = qRgb(hash & 0xFF, (hash >> 8) & 0xFF, (hash >> 16) & 0xFF);
        }
    }
    return image;
}

QImage makeBand(int width, int height, QRgb color)
{
    QImage image(width, height, QImage::Format_RGB32);
    image.fill(color);
    return image;
}

QImage verticalFrame(const QImage &page, int offset, int contentHeight,
                     const QImage &header = {}, const QImage &footer = {})
{
    const int headerHeight = header.isNull() ? 0 : header.height();
    const int footerHeight = footer.isNull() ? 0 : footer.height();
    QImage frame(page.width(), headerHeight + contentHeight + footerHeight, QImage::Format_RGB32);
    frame.fill(Qt::black);
    QPainter painter(&frame);
    if (!header.isNull()) painter.drawImage(0, 0, header);
    painter.drawImage(QRect(0, headerHeight, page.width(), contentHeight), page,
                      QRect(0, offset, page.width(), contentHeight));
    if (!footer.isNull()) painter.drawImage(0, headerHeight + contentHeight, footer);
    return frame;
}

QImage horizontalFrame(const QImage &page, int offset, int contentWidth)
{
    QImage frame(contentWidth, page.height(), QImage::Format_RGB32);
    frame.fill(Qt::black);
    QPainter painter(&frame);
    painter.drawImage(QRect(0, 0, contentWidth, page.height()), page,
                      QRect(offset, 0, contentWidth, page.height()));
    return frame;
}

} // namespace

// 只验证离线拼接算法；不访问屏幕、窗口或用户目录。
class TstScrollStitcher : public QObject {
    Q_OBJECT
private slots:
    void verticalDown();
    void verticalUp();
    void duplicateFrameIsIgnored();
    void noOverlapKeepsPrevious();
    void fixedHeaderAndFooter();
    void horizontalRight();
};

void TstScrollStitcher::verticalDown()
{
    const QImage page = makePage(200, 2000);
    ScrollStitcher stitcher;
    QCOMPARE(stitcher.addFrame(verticalFrame(page, 0, 300)), ScrollAddResult::FirstFrame);
    QCOMPARE(stitcher.addFrame(verticalFrame(page, 100, 300)), ScrollAddResult::Stitched);
    QCOMPARE(stitcher.addFrame(verticalFrame(page, 200, 300)), ScrollAddResult::Stitched);
    QCOMPARE(stitcher.image(), page.copy(0, 0, 200, 500));
}

void TstScrollStitcher::verticalUp()
{
    const QImage page = makePage(200, 2000);
    ScrollStitcher stitcher;
    QCOMPARE(stitcher.addFrame(verticalFrame(page, 300, 300)), ScrollAddResult::FirstFrame);
    QCOMPARE(stitcher.addFrame(verticalFrame(page, 200, 300)), ScrollAddResult::Stitched);
    QCOMPARE(stitcher.addFrame(verticalFrame(page, 100, 300)), ScrollAddResult::Stitched);
    QCOMPARE(stitcher.image(), page.copy(0, 100, 200, 500));
}

void TstScrollStitcher::duplicateFrameIsIgnored()
{
    const QImage page = makePage(200, 1000);
    const QImage frame = verticalFrame(page, 0, 300);
    ScrollStitcher stitcher;
    QCOMPARE(stitcher.addFrame(frame), ScrollAddResult::FirstFrame);
    QCOMPARE(stitcher.addFrame(frame), ScrollAddResult::Unchanged);
    QCOMPARE(stitcher.image(), frame);
}

void TstScrollStitcher::noOverlapKeepsPrevious()
{
    const QImage page = makePage(200, 2000);
    const QImage first = verticalFrame(page, 0, 300);
    ScrollStitcher stitcher;
    QCOMPARE(stitcher.addFrame(first), ScrollAddResult::FirstFrame);
    QCOMPARE(stitcher.addFrame(verticalFrame(page, 600, 300)), ScrollAddResult::NoOverlap);
    QCOMPARE(stitcher.image(), first);
}

void TstScrollStitcher::fixedHeaderAndFooter()
{
    const QImage page = makePage(200, 2000);
    const QImage header = makeBand(200, 20, qRgb(220, 30, 40));
    const QImage footer = makeBand(200, 30, qRgb(20, 200, 90));
    ScrollStitcher stitcher;
    QCOMPARE(stitcher.addFrame(verticalFrame(page, 0, 250, header, footer)), ScrollAddResult::FirstFrame);
    QCOMPARE(stitcher.addFrame(verticalFrame(page, 100, 250, header, footer)), ScrollAddResult::Stitched);
    QCOMPARE(stitcher.addFrame(verticalFrame(page, 200, 250, header, footer)), ScrollAddResult::Stitched);

    QImage expected(200, 20 + 450 + 30, QImage::Format_RGB32);
    expected.fill(Qt::black);
    QPainter painter(&expected);
    painter.drawImage(0, 0, header);
    painter.drawImage(0, 20, page.copy(0, 0, 200, 450));
    painter.drawImage(0, 20 + 450, footer);
    painter.end();
    QCOMPARE(stitcher.image(), expected);
}

void TstScrollStitcher::horizontalRight()
{
    const QImage page = makePage(2000, 200);
    ScrollStitcher stitcher;
    QCOMPARE(stitcher.addFrame(horizontalFrame(page, 0, 300)), ScrollAddResult::FirstFrame);
    QCOMPARE(stitcher.addFrame(horizontalFrame(page, 100, 300)), ScrollAddResult::Stitched);
    QCOMPARE(stitcher.addFrame(horizontalFrame(page, 200, 300)), ScrollAddResult::Stitched);
    QCOMPARE(stitcher.image(), page.copy(0, 0, 500, 200));
}

QTEST_GUILESS_MAIN(TstScrollStitcher)
#include "tst_scroll_stitcher.moc"