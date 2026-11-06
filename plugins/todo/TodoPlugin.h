#pragma once

#include "plugins/BuiltinFeatureApi.h"
#include <QTranslator>

class ThemeManager;

// 待办内置功能：搜索输入 todo 打开单层任务列表。
class TodoPlugin final : public QObject, public TypeGo::BuiltinFeature::Plugin {
    Q_OBJECT
public:
    TypeGo::BuiltinFeature::Error initialize(TypeGo::BuiltinFeature::Host *host) override;
    TypeGo::BuiltinFeature::QueryResult query(const TypeGo::BuiltinFeature::QueryRequest &request) override;
    TypeGo::BuiltinFeature::ExecutionResult execute(const TypeGo::BuiltinFeature::Invocation &invocation) override;
    QWidget *createView(const TypeGo::BuiltinFeature::Invocation &invocation, QWidget *parent) override;
    void shutdown() override;

    // 主题由注册方注入，供委托自绘取色；不注入时回退到系统调色板。
    void setThemeManager(ThemeManager *manager) { m_theme = manager; }

private:
    TypeGo::BuiltinFeature::Host *m_host = nullptr;
    ThemeManager *m_theme = nullptr;
    QTranslator m_translator;
};
