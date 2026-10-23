#include "PluginManifest.h"
#include <QJsonArray>
#include <QRegularExpression>
#include <QSet>
#include <cmath>

namespace Api = TypeGo::PluginApi;

namespace {
bool validId(const QString &value)
{
    static const QRegularExpression pattern(QStringLiteral("^[a-z][a-z0-9]*(?:[.-][a-z0-9]+)*$"));
    return value.size() <= 128 && pattern.match(value).hasMatch();
}
bool validKeyword(const QString &value)
{
    static const QRegularExpression pattern(QStringLiteral("^[a-z][a-z0-9-]{0,63}$"));
    return pattern.match(value).hasMatch();
}
bool optionalBool(const QJsonObject &object, const QString &key)
{
    return !object.contains(key) || object.value(key).isBool();
}
}

bool validPluginParameters(const QVariantMap &parameters)
{
    if (parameters.size() > 32) return false;
    qsizetype bytes = 0;
    for (auto it = parameters.cbegin(); it != parameters.cend(); ++it) {
        if (it.key().isEmpty() || it.key().size() > 128) return false;
        bytes += it.key().size() * 2;
        switch (it.value().metaType().id()) {
        case QMetaType::QString: bytes += it.value().toString().size() * 2; break;
        case QMetaType::Bool:
        case QMetaType::Int:
        case QMetaType::UInt:
        case QMetaType::LongLong:
        case QMetaType::ULongLong: bytes += 8; break;
        case QMetaType::Double:
            if (!std::isfinite(it.value().toDouble())) return false;
            bytes += 8;
            break;
        default: return false;
        }
        if (bytes > Api::MaxParameterBytes) return false;
    }
    return true;
}

PluginManifestResult parsePluginManifest(const QJsonObject &root, const QString &expectedAbi)
{
    auto fail = [](Api::ErrorCode code, const QString &message) {
        return PluginManifestResult{{}, {code, message}};
    };
    if (root.value(QStringLiteral("IID")).toString() != QLatin1String(TYPEGO_PLUGIN_IID))
        return fail(Api::ErrorCode::Incompatible, QStringLiteral("Unsupported plugin interface"));
    const QJsonObject data = root.value(QStringLiteral("MetaData")).toObject();
    if (data.value(QStringLiteral("apiMajor")).toDouble(-1) != Api::ApiMajor
        || data.value(QStringLiteral("apiMinor")).toDouble(-1) != Api::ApiMinor
        || data.value(QStringLiteral("abi")).toString() != expectedAbi || expectedAbi == QLatin1String("unsupported"))
        return fail(Api::ErrorCode::Incompatible, QStringLiteral("Plugin API or binary compatibility does not match this host; rebuild with the matching SDK and Qt toolchain"));
    PluginManifestResult result;
    auto &manifest = result.manifest;
    manifest.abi = expectedAbi;
    manifest.metadata = data;
    auto &descriptor = manifest.descriptor;
    descriptor.id = data.value(QStringLiteral("id")).toString();
    descriptor.description = data.value(QStringLiteral("description")).toString();
    manifest.version = data.value(QStringLiteral("version")).toString();
    static const QRegularExpression versionPattern(QStringLiteral("^[0-9]+[.][0-9]+[.][0-9]+$"));
    if (!validId(descriptor.id) || descriptor.description.isEmpty() || descriptor.description.size() > 512
        || manifest.version.size() > 32 || !versionPattern.match(manifest.version).hasMatch()
        || !optionalBool(data, QStringLiteral("hasSettings")))
        return fail(Api::ErrorCode::InvalidArgument, QStringLiteral("Invalid plugin identity, version or description"));
    descriptor.hasSettings = data.value(QStringLiteral("hasSettings")).toBool();
    if (data.contains(QStringLiteral("requires")) && !data.value(QStringLiteral("requires")).isArray())
        return fail(Api::ErrorCode::InvalidArgument, QStringLiteral("requires must be an array"));
    const QSet<QString> capabilities{QStringLiteral("widgets"), QStringLiteral("async"), QStringLiteral("config")};
    for (const auto &value : data.value(QStringLiteral("requires")).toArray()) {
        if (!value.isString() || !capabilities.contains(value.toString()))
            return fail(Api::ErrorCode::Incompatible, QStringLiteral("Unsupported required capability: ") + value.toString());
    }
    const auto commands = data.value(QStringLiteral("commands")).toArray();
    if (commands.isEmpty() || commands.size() > 32)
        return fail(Api::ErrorCode::InvalidArgument, QStringLiteral("A plugin must declare 1 to 32 commands"));
    QSet<QString> ids, keywords;
    for (const auto &value : commands) {
        if (!value.isObject()) return fail(Api::ErrorCode::InvalidArgument, QStringLiteral("Invalid command"));
        const auto object = value.toObject();
        PluginCommand command;
        command.id = object.value(QStringLiteral("id")).toString();
        command.keyword = object.value(QStringLiteral("keyword")).toString();
        if (!validId(command.id) || ids.contains(command.id) || !validKeyword(command.keyword))
            return fail(Api::ErrorCode::InvalidArgument, QStringLiteral("Invalid or duplicate command identity"));
        ids.insert(command.id);
        if (object.contains(QStringLiteral("aliases")) && !object.value(QStringLiteral("aliases")).isArray())
            return fail(Api::ErrorCode::InvalidArgument, QStringLiteral("aliases must be an array"));
        const auto aliases = object.value(QStringLiteral("aliases")).toArray();
        if (aliases.size() > 16) return fail(Api::ErrorCode::InvalidArgument, QStringLiteral("Too many aliases"));
        QStringList names{command.keyword};
        for (const auto &alias : aliases) names.append(alias.toString());
        for (const QString &name : names) {
            if (!validKeyword(name) || keywords.contains(name))
                return fail(Api::ErrorCode::Conflict, QStringLiteral("Invalid or conflicting keyword: ") + name);
            keywords.insert(name);
        }
        command.aliases = names.mid(1);
        for (const QString &key : {QStringLiteral("mode"), QStringLiteral("presentation")})
            if (object.contains(key) && !object.value(key).isString())
                return fail(Api::ErrorCode::InvalidArgument, QStringLiteral("Invalid command mode or presentation type"));
        const auto mode = object.value(QStringLiteral("mode")).toString(QStringLiteral("list"));
        if (mode == QLatin1String("list")) command.launchMode = IPlugin::LaunchMode::List;
        else if (mode == QLatin1String("view")) command.launchMode = IPlugin::LaunchMode::FullView;
        else if (mode == QLatin1String("queryView")) command.launchMode = IPlugin::LaunchMode::QueryView;
        else return fail(Api::ErrorCode::InvalidArgument, QStringLiteral("Unknown launch mode"));
        const auto presentation = object.value(QStringLiteral("presentation")).toString(QStringLiteral("embedded"));
        if (presentation == QLatin1String("embedded")) command.presentation = PluginPresentation::Embedded;
        else if (presentation == QLatin1String("window")) command.presentation = PluginPresentation::Window;
        else if (presentation == QLatin1String("overlay")) command.presentation = PluginPresentation::Overlay;
        else return fail(Api::ErrorCode::InvalidArgument, QStringLiteral("Unknown presentation"));
        for (const QString &key : {QStringLiteral("requiresArguments"), QStringLiteral("autoEnter"), QStringLiteral("directActivation")})
            if (!optionalBool(object, key)) return fail(Api::ErrorCode::InvalidArgument, QStringLiteral("Invalid command flag"));
        command.requiresArguments = object.value(QStringLiteral("requiresArguments")).toBool();
        command.supportsAutoEnter = object.value(QStringLiteral("autoEnter")).toBool();
        command.directActivation = object.value(QStringLiteral("directActivation")).toBool();
        descriptor.commands.append(std::move(command));
    }
    return result;
}
