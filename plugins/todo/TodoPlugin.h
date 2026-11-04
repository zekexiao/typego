#pragma once
#include "plugins/BuiltinFeatureApi.h"
#include <QTranslator>

// 待办内置功能：搜索输入 todo 打开独立窗口。
class TodoPlugin final : public QObject, public TypeGo::BuiltinFeature::Plugin {
    Q_OBJECT
public:
    TypeGo::BuiltinFeature::Error initialize(TypeGo::BuiltinFeature::Host *host) override;
    TypeGo::BuiltinFeature::QueryResult query(const TypeGo::BuiltinFeature::QueryRequest &request) override;
    TypeGo::BuiltinFeature::ExecutionResult execute(const TypeGo::BuiltinFeature::Invocation &invocation) override;
    QWidget *createView(const TypeGo::BuiltinFeature::Invocation &invocation, QWidget *parent) override;
    void shutdown() override;

private:
    TypeGo::BuiltinFeature::Host *m_host = nullptr;
    QTranslator m_translator;
};
