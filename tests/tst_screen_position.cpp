#include <QtTest>
#include <QTemporaryDir>
#include "utils/ScreenPosition.h"
#include "Settings.h"

class TstScreenPosition : public QObject {
    Q_OBJECT
private slots:
    void preservesNegativeCoordinates()
    {
        const QList<QRect> screens{{0, 0, 1920, 1040}, {-1920, 0, 1920, 1080}, {0, -1080, 1920, 1080}};
        QCOMPARE(ScreenPosition::restore({-1000, 100}, true, {52, 52}, screens), QPoint(-1000, 100));
        QCOMPARE(ScreenPosition::restore({100, -500}, true, {52, 52}, screens), QPoint(100, -500));
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        Settings settings(dir.filePath("settings.ini"));
        QVERIFY(!settings.hasFloatIconPos());
        settings.setFloatIconPos({-1, -1});
        QVERIFY(settings.hasFloatIconPos());
        QCOMPARE(settings.floatIconPos(), QPoint(-1, -1));
    }
    void disconnectedScreenAndPartialVisibility()
    {
        const QRect primary(0, 0, 1280, 720);
        const QSize size(52, 52);
        for (const QPoint saved : {QPoint(2500, 100), QPoint(1270, 700), QPoint(-20, 100)}) {
            const QPoint restored = ScreenPosition::restore(saved, true, size, {primary});
            QVERIFY(primary.contains(QRect(restored, size)));
        }
        QVERIFY(primary.contains(QRect(ScreenPosition::restore({}, false, size, {primary}), size)));
        QCOMPARE(ScreenPosition::restore({10, 20}, true, size, {}), QPoint(10, 20));
    }
};
QTEST_GUILESS_MAIN(TstScreenPosition)
#include "tst_screen_position.moc"
