#include <QtTest>

#include "utils/UpdateCheckGate.h"

// 自动检查与手动检查撞车时的去重策略：只允许一个在途请求，
// 期间的手动请求并入该请求并把口径升级为手动，避免用户看到两个更新弹窗。
class TstUpdateCheckGate : public QObject
{
    Q_OBJECT

private slots:
    void firstRequestStartsCheck();
    void secondRequestJoinsPendingCheck();
    void manualRequestUpgradesPendingMode();
    void automaticRequestKeepsManualMode();
    void finishClearsBusyState();
    void repeatedManualRequestsStillReportManual();
};

void TstUpdateCheckGate::firstRequestStartsCheck()
{
    UpdateCheckGate::Gate gate;
    QVERIFY(!gate.isBusy());
    QVERIFY(gate.request(UpdateCheckGate::Mode::Automatic));
    QVERIFY(gate.isBusy());
}

void TstUpdateCheckGate::secondRequestJoinsPendingCheck()
{
    UpdateCheckGate::Gate gate;
    QVERIFY(gate.request(UpdateCheckGate::Mode::Automatic));
    // 第二次请求不再发起网络请求，避免自动与手动各弹一次提示。
    QVERIFY(!gate.request(UpdateCheckGate::Mode::Automatic));
    QCOMPARE(int(gate.finish()), int(UpdateCheckGate::Mode::Automatic));
}

void TstUpdateCheckGate::manualRequestUpgradesPendingMode()
{
    UpdateCheckGate::Gate gate;
    QVERIFY(gate.request(UpdateCheckGate::Mode::Automatic));
    QVERIFY(!gate.request(UpdateCheckGate::Mode::Manual));
    // 手动请求并入后，结果按手动口径汇报，用户一定能看到反馈。
    QCOMPARE(int(gate.finish()), int(UpdateCheckGate::Mode::Manual));
}

void TstUpdateCheckGate::automaticRequestKeepsManualMode()
{
    UpdateCheckGate::Gate gate;
    QVERIFY(gate.request(UpdateCheckGate::Mode::Manual));
    QVERIFY(!gate.request(UpdateCheckGate::Mode::Automatic));
    QCOMPARE(int(gate.finish()), int(UpdateCheckGate::Mode::Manual));
}

void TstUpdateCheckGate::finishClearsBusyState()
{
    UpdateCheckGate::Gate gate;
    gate.request(UpdateCheckGate::Mode::Automatic);
    gate.finish();
    QVERIFY(!gate.isBusy());
    // 结束后可以重新发起一次检查。
    QVERIFY(gate.request(UpdateCheckGate::Mode::Automatic));
    QCOMPARE(int(gate.finish()), int(UpdateCheckGate::Mode::Automatic));
}

void TstUpdateCheckGate::repeatedManualRequestsStillReportManual()
{
    UpdateCheckGate::Gate gate;
    QVERIFY(gate.request(UpdateCheckGate::Mode::Manual));
    QVERIFY(!gate.request(UpdateCheckGate::Mode::Manual));
    QVERIFY(!gate.request(UpdateCheckGate::Mode::Manual));
    QCOMPARE(int(gate.finish()), int(UpdateCheckGate::Mode::Manual));
}

QTEST_GUILESS_MAIN(TstUpdateCheckGate)

#include "tst_update_check_gate.moc"
