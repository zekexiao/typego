#include <QtTest>
#include <QAbstractItemModelTester>
#include <QTemporaryDir>
#include <QFile>
#include "JsonTreeModel.h"

class JsonTreeModelTest : public QObject {
    Q_OBJECT
private slots:
    void roundTrip() {
        JsonTreeModel model;
        model.setPrettyPrint(false);
        const QStringList values = {
            QStringLiteral("18446744073709551615"), QStringLiteral("-9223372036854775808"),
            QStringLiteral("true"), QStringLiteral("null"), QStringLiteral("1.25"),
            QStringLiteral("\"a\\u0000b\""),
            QStringLiteral("{\"a\\u0000b\":\"中\\u0000文\",\"n\":18446744073709551615}")};
        for (const auto &value : values) {
            QVERIFY(model.loadFromString(value));
            QCOMPARE(model.toJsonString(), value);
        }
        QVERIFY(!model.loadFromString(QStringLiteral("{broken")));
        QCOMPARE(model.toJsonString(), values.last());
        QVERIFY(!model.loadFromString(QString(514, '[') + "0" + QString(514, ']')));
        QCOMPARE(model.toJsonString(), values.last());
    }
    void editsAndStructure() {
        JsonTreeModel model;
        QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);
        QVERIFY(model.loadFromString(QStringLiteral("{\"a\":1,\"b\":false,\"list\":[1,2]}")));
        const auto root = model.index(0, 0);
        QVERIFY(!model.setData(model.index(0, 0, root), QStringLiteral("b")));
        QVERIFY(!model.setData(model.index(0, 1, root), QStringLiteral("oops")));
        QVERIFY(model.setData(model.index(0, 1, root), QStringLiteral("18446744073709551615")));
        QVERIFY(model.setData(model.index(1, 1, root), QStringLiteral("true")));
        QVERIFY(!model.setData(model.index(1, 1, root), QStringLiteral("anything")));
        const auto array = model.index(2, 0, root);
        QVERIFY(!(model.flags(model.index(0, 0, array)) & Qt::ItemIsEditable));
        QVERIFY(!model.insertRows(0, 0, array));
        QVERIFY(!model.removeRows(0, -1, array));
        QVERIFY(!model.insertRows(0, 1, model.index(0, 0, array)));
        QVERIFY(model.insertRows(1, 2, array));
        QVERIFY(model.removeRows(0, 3, array));
        QCOMPARE(model.data(model.index(0, 0, array), Qt::DisplayRole).toString(), QStringLiteral("[0]"));
        auto *replacement = new JsonTreeItem(QStringLiteral("list"), {}, JsonNodeType::Array);
        replacement->appendChild(new JsonTreeItem(QStringLiteral("[0]"), 3, JsonNodeType::Number));
        QVERIFY(model.replaceItem(array.siblingAtColumn(1), replacement));
        QCOMPARE(model.rowCount(array), 1);
        QVERIFY(model.removeRows(0, 1, array));
        QCOMPARE(model.rowCount(array), 0);
    }
    void filesAndMessagePack() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("data"));
        auto write = [&](const QByteArray &data) {
            QFile file(path);
            return file.open(QIODevice::WriteOnly) && file.write(data) == data.size();
        };
        JsonTreeModel model;
        model.setPrettyPrint(false);
        QVERIFY(write(QByteArray::fromHex("81a16ecfffffffffffffffff")));
        QVERIFY(model.loadJson(path));
        QCOMPARE(model.toJsonString(), QStringLiteral("{\"n\":18446744073709551615}"));
        QVERIFY(model.saveJson(path));
        QVERIFY(model.loadJson(path));
        QCOMPARE(model.toJsonString(), QStringLiteral("{\"n\":18446744073709551615}"));
        for (const auto &invalid : {"81a16e", "c40161", "d40100", "c0c0", "ddffffffff", "a1ff", "a1c3"}) {
            QVERIFY(write(QByteArray::fromHex(invalid)));
            QVERIFY(!model.loadJson(path));
            QCOMPARE(model.toJsonString(), QStringLiteral("{\"n\":18446744073709551615}"));
        }
        QVERIFY(!model.saveJson(dir.path()));
    }
};
QTEST_GUILESS_MAIN(JsonTreeModelTest)
#include "tst_json_tree_model.moc"
