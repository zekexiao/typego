#include <QtTest>
#include <QProcess>
#include "utils/Restart.h"

class TstRestart : public QObject {
    Q_OBJECT
private slots:
    void waitsForOldProcessExit()
    {
#ifdef Q_OS_WIN
        QProcess oldProcess;
        oldProcess.start(QStringLiteral(TYPEGO_RESTART_PARENT));
        QVERIFY(oldProcess.waitForStarted());
        const qint64 pid = oldProcess.processId();
        QVERIFY(!Restart::waitForParent(pid, 0));
        QVERIFY(Restart::waitForParent(pid, 10000));
        QVERIFY(oldProcess.waitForFinished());
        QCOMPARE(oldProcess.exitCode(), 0);
#else
        QSKIP("Windows restart handoff");
#endif
    }
    void rejectsInvalidParent()
    {
        QVERIFY(!Restart::waitForParent(0, 0));
        QVERIFY(!Restart::waitForParent(-1, 0));
        QVERIFY(!Restart::waitForParent(QCoreApplication::applicationPid(), 0));
    }
};
QTEST_GUILESS_MAIN(TstRestart)
#include "tst_restart.moc"
