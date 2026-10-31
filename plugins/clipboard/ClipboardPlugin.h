#pragma once

#include "plugins/BuiltinFeatureApi.h"
#include <QByteArray>
#include <QStringList>
#include <QTimer>
#include <QTranslator>
#include <memory>

#include "ClipboardStore.h"

class QClipboard;
class QMimeData;

// 剪贴板历史插件：后台监听系统剪贴板，按关键字过滤并粘贴回剪贴板。
class ClipboardPlugin final : public QObject, public TypeGo::BuiltinFeature::Plugin {
    Q_OBJECT

public:
    TypeGo::BuiltinFeature::Error initialize(TypeGo::BuiltinFeature::Host *host) override;
    TypeGo::BuiltinFeature::QueryResult query(const TypeGo::BuiltinFeature::QueryRequest &request) override;
    TypeGo::BuiltinFeature::ExecutionResult execute(const TypeGo::BuiltinFeature::Invocation &invocation) override;
    QWidget *createSettingsView(QWidget *parent) override;
    void activate(const TypeGo::BuiltinFeature::Invocation &invocation) override;
    TypeGo::BuiltinFeature::LeaveDecision prepareToLeave(TypeGo::BuiltinFeature::LeaveReason reason, quint64 token) override;
    void shutdown() override;

private:
    void startMonitoring();
    void stopMonitoring();
    void captureClipboard();
    void applyConfig();
    TypeGo::BuiltinFeature::ResultItem resultItem(const ClipboardItem &item) const;
    bool matches(const ClipboardItem &item, const QString &filter) const;
    int clearHistory();
    void saveHistory();
    // 自身写回剪贴板时系统会回送一次 dataChanged；按内容识别这次回声，
    // 免得标志残留把用户随后的真实复制误吞掉。
    bool isSelfEcho(const QMimeData *mime) const;

    // 等待系统回声的这一次写入内容。
    struct ClipboardEcho {
        bool active = false;
        ClipboardItem::Type type = ClipboardItem::Type::Text;
        QString text;
        QStringList files;
        QByteArray imageSignature;
    };

    TypeGo::BuiltinFeature::Host *m_host = nullptr;
    QClipboard *m_clipboard = nullptr;
    QTranslator m_translator;
    QTimer m_captureTimer;
    std::unique_ptr<ClipboardStore> m_store;
    ClipboardEcho m_echo;
    bool m_captureText = true;
    bool m_captureImages = true;
    bool m_captureFiles = true;
};