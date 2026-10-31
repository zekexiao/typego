#include <QtTest>
#include <QImage>

#include "Annotation.h"

// 标注合成只依赖 Qt Gui：验证矢量标注在 HiDPI 下的落点与坐标平移。
class TstScreenshotAnnotation : public QObject {
    Q_OBJECT
private slots:
    void rectStaysAtLogicalPositionOnHiDpi();
    void annotationNearEdgeSurvivesHiDpi();
    void translateMovesAllPoints();
    void globalAnnotationsMapIntoCrop();
};

namespace {

ShotAnnotation rectAnnotation(const QPointF &topLeft, const QPointF &bottomRight)
{
    ShotAnnotation annotation;
    annotation.tool = ShotTool::Rect;
    annotation.color = QColor(255, 0, 0);
    annotation.width = 1;
    annotation.start = topLeft;
    annotation.end = bottomRight;
    return annotation;
}

// 空白底图；dpr 是物理像素与逻辑像素的比例。
QImage whiteBase(int deviceSize, qreal dpr)
{
    QImage image(deviceSize, deviceSize, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::white);
    image.setDevicePixelRatio(dpr);
    return image;
}

// 判断物理像素上是否落着红色笔迹。
bool isRed(const QImage &image, int x, int y)
{
    if (x < 0 || y < 0 || x >= image.width() || y >= image.height()) return false;
    const QRgb pixel = image.pixel(x, y);
    return qRed(pixel) > 200 && qGreen(pixel) < 200 && qBlue(pixel) < 200;
}

} // namespace

void TstScreenshotAnnotation::rectStaysAtLogicalPositionOnHiDpi()
{
    // 逻辑 100x100、物理 200x200 的底图。
    QImage base = whiteBase(200, 2.0);
    const QImage composed = composeShotImage(base, {rectAnnotation(QPointF(10, 10), QPointF(30, 30))});
    QCOMPARE(composed.devicePixelRatio(), 2.0);
    // 逻辑 (10,10) 对应物理 (20,20)，矩形上边中点应落在这里。
    QVERIFY(isRed(composed, 40, 20));
    // 不能按 DPR 平方放大：那样上边会跑到物理 y=40。
    QVERIFY(!isRed(composed, 80, 40));
}

void TstScreenshotAnnotation::annotationNearEdgeSurvivesHiDpi()
{
    QImage base = whiteBase(200, 2.0);
    const QImage composed = composeShotImage(base, {rectAnnotation(QPointF(80, 80), QPointF(100, 100))});
    // 逻辑 (80,80) 对应物理 (160,160)，贴着画布边角的标注也不能被整块裁掉。
    QVERIFY(isRed(composed, 180, 160));
}

void TstScreenshotAnnotation::translateMovesAllPoints()
{
    ShotAnnotation annotation = rectAnnotation(QPointF(10, 10), QPointF(30, 30));
    annotation.tool = ShotTool::Pen;
    annotation.points = {QPointF(1, 2), QPointF(3, 4)};
    const QVector<ShotAnnotation> moved =
        shotAnnotationsTranslate({annotation}, QPointF(-5, -7));
    QCOMPARE(moved.size(), 1);
    QCOMPARE(moved.first().start, QPointF(5, 3));
    QCOMPARE(moved.first().end, QPointF(25, 23));
    QCOMPARE(moved.first().points.size(), 2);
    QCOMPARE(moved.first().points.at(0), QPointF(-4, -5));
    QCOMPARE(moved.first().points.at(1), QPointF(-2, -3));
}

void TstScreenshotAnnotation::globalAnnotationsMapIntoCrop()
{
    // 选区左上角在全局 (100,200)，标注同样用全局坐标。
    const QPointF cropOrigin(100, 200);
    QImage base = whiteBase(200, 2.0);
    const ShotAnnotation annotation = rectAnnotation(QPointF(110, 210), QPointF(130, 230));
    const QVector<ShotAnnotation> relative = shotAnnotationsTranslate(
        {annotation}, QPointF(-cropOrigin.x(), -cropOrigin.y()));
    const QImage composed = composeShotImage(base, relative);
    // 平移后是逻辑 (10,10)-(30,30)，物理上边在 y=20。
    QVERIFY(isRed(composed, 40, 20));
    // 矩形内部不填充，不能出现笔迹。
    QVERIFY(!isRed(composed, 40, 40));
}

QTEST_GUILESS_MAIN(TstScreenshotAnnotation)
#include "tst_screenshot_annotation.moc"
