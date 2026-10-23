#include "CalcPlugin.h"

#include "ExpressionParser.h"

#include <QClipboard>
#include <QGuiApplication>
#include <QVariantMap>

CalcPlugin::CalcPlugin(QObject *parent)
    : IPlugin(parent)
{
}

QVariantList CalcPlugin::query(const QString &args)
{
    QVariantMap item;
    QVariantMap action;
    action["plugin"] = command();

    if (args.trimmed().isEmpty()) {
        item["title"] = tr("Enter an expression to calculate");
        item["subtitle"] = tr("e.g. 1+2, sqrt(16), 2^10");
        item["icon"] = "";
        action["type"] = QStringLiteral("none");
        item["action"] = action;
        return QVariantList() << item;
    }

    double value = 0.0;
    QString error;
    ExpressionParser parser(args);
    if (!parser.parse(&value, &error)) {
        item["title"] = tr("Expression Error");
        item["subtitle"] = error;
        item["icon"] = "";
        action["type"] = QStringLiteral("none");
        item["action"] = action;
        return QVariantList() << item;
    }

    const QString result = QString::number(value, 'g', 15);
    item["title"] = result;
    item["subtitle"] = args + QStringLiteral(" = ") + result;
    item["icon"] = "";
    action["type"] = QStringLiteral("copy");
    action["data"] = result;
    item["action"] = action;
    return QVariantList() << item;
}

PluginExecutionResult CalcPlugin::execute(const PluginInvocation &invocation)
{
    const QVariantMap action = invocation.action;
    if (action.value("type").toString() != QStringLiteral("copy"))
        return {PluginExecutionResult::Status::Failed, PluginExecutionResult::HostAction::Keep};
    const QString text = action.value("data").toString();
    if (text.isEmpty())
        return {PluginExecutionResult::Status::Failed, PluginExecutionResult::HostAction::Keep};
    QGuiApplication::clipboard()->setText(text);
    return {PluginExecutionResult::Status::Succeeded, PluginExecutionResult::HostAction::Hide};
}
