#pragma once

#include "plugins/BuiltinFeatureApi.h"
#include <QTranslator>
#include <QPointer>
class JsonViewerWindow;

class NeoJsonViewerPlugin final : public QObject, public TypeGo::BuiltinFeature::Plugin {
    Q_OBJECT

public:
    TypeGo::BuiltinFeature::Error initialize(TypeGo::BuiltinFeature::Host *host) override;
    TypeGo::BuiltinFeature::QueryResult query(const TypeGo::BuiltinFeature::QueryRequest &request) override;
    TypeGo::BuiltinFeature::ExecutionResult execute(const TypeGo::BuiltinFeature::Invocation &invocation) override;
    QWidget *createView(const TypeGo::BuiltinFeature::Invocation &invocation, QWidget *parent) override;
    TypeGo::BuiltinFeature::LeaveDecision prepareToLeave(TypeGo::BuiltinFeature::LeaveReason reason, quint64 token) override;
    void shutdown() override;
private:
    TypeGo::BuiltinFeature::Host *m_host = nullptr;
    QTranslator m_translator;
    QPointer<JsonViewerWindow> m_view;
};
