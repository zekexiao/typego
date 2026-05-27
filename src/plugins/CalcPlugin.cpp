#include "CalcPlugin.h"
#include <QVariantMap>
#include <QDebug>
#include <QGuiApplication>
#include <QClipboard>

CalcPlugin::CalcPlugin(QObject *parent)
    : IPlugin(parent)
{
}

QVariantList CalcPlugin::query(const QString &args)
{
    QVariantList results;
    if (args.trimmed().isEmpty()) {
        // 无参数时显示提示
        QVariantMap item;
        item["title"] = QString::fromUtf8(u8"输入表达式进行计算");
        item["subtitle"] = QString::fromUtf8(u8"例如: 1+2, sqrt(16), 2^10");
        item["icon"] = "";
        QVariantMap action;
        action["type"] = QString::fromUtf8(u8"none");
        action["plugin"] = command();
        item["action"] = action;
        results.append(item);
        return results;
    }

    // 用 QJSEngine 计算表达式
    QJSValue value = m_engine.evaluate(args);
    if (value.isError()) {
        QVariantMap item;
        item["title"] = QString::fromUtf8(u8"表达式错误");
        item["subtitle"] = value.toString();
        item["icon"] = "";
        QVariantMap action;
        action["type"] = QString::fromUtf8(u8"none");
        action["plugin"] = command();
        item["action"] = action;
        results.append(item);
        return results;
    }

    QString resultStr = value.toString();
    QVariantMap item;
    item["title"] = resultStr;
    item["subtitle"] = args + QString::fromUtf8(u8" = ") + resultStr;
    item["icon"] = "";
    QVariantMap action;
    action["type"] = QString::fromUtf8(u8"copy");
    action["data"] = resultStr;
    action["plugin"] = command();
    item["action"] = action;
    results.append(item);
    return results;
}

bool CalcPlugin::execute(const QVariantMap &action)
{
    QString type = action.value("type").toString();
    if (type == "copy") {
        QString text = action.value("data").toString();
        if (!text.isEmpty()) {
            QGuiApplication::clipboard()->setText(text);
            return true;
        }
    }
    return false;
}
