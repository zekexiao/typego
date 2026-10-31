#include <QtTest>
#include <QTemporaryDir>
#include "plugins/IntegratedFeatures.h"
#include "plugins/PluginManager.h"

class TstIntegratedFeatures : public QObject {
    Q_OBJECT
private slots:
    void commandsAreAvailableWithoutPluginPackages();
};

void TstIntegratedFeatures::commandsAreAvailableWithoutPluginPackages()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    PluginManager manager;
    registerIntegratedFeatures(manager, {directory.path(), directory.path(), directory.path()});
    for (const QString &command : {QStringLiteral("clip"), QStringLiteral("cb"), QStringLiteral("shot"),
             QStringLiteral("shot-full"), QStringLiteral("shot-scroll"), QStringLiteral("shot-history"),
             QStringLiteral("json"), QStringLiteral("json-viewer")}) {
        QVERIFY2(manager.hasCommand(command), qPrintable(command));
    }
    QCOMPARE(manager.pluginIdForCommand(QStringLiteral("cb")), QStringLiteral("clipboard"));
    QCOMPARE(manager.pluginIdForCommand(QStringLiteral("shot-scroll")), QStringLiteral("screenshot"));
    QCOMPARE(manager.pluginIdForCommand(QStringLiteral("json")), QStringLiteral("json-viewer"));
    QVERIFY(manager.descriptor(QStringLiteral("clipboard"))->background);
    QVERIFY(!manager.isInstantiated(QStringLiteral("clipboard")));
}

QTEST_GUILESS_MAIN(TstIntegratedFeatures)
#include "tst_integrated_features.moc"
