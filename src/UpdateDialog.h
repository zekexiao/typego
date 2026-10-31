#pragma once

#include <QDialog>
#include <QList>
#include <QString>

#include "utils/ReleaseParser.h"

class QLabel;
class QTabWidget;
class QTextBrowser;

// 新版本提示弹窗。
// 中英文分页展示从当前版本到最新版本的全部更新内容；正文放在可滚动区域里，
// 内容再长弹窗高度也保持上限，不会把整个窗口撑开。
class UpdateDialog : public QDialog
{
    Q_OBJECT
public:
    explicit UpdateDialog(const ReleaseParser::Result &result, QWidget *parent = nullptr);

    // 用新的检查结果刷新弹窗，用于同一次会话里重复检查时只保留一个窗口。
    void applyResult(const ReleaseParser::Result &result);
    QString installerUrl() const { return m_installerUrl; }

signals:
    void updateRequested(const QString &url);

private:
    QLabel *m_titleLabel = nullptr;
    QLabel *m_subtitleLabel = nullptr;
    QTabWidget *m_tabs = nullptr;
    QTextBrowser *m_chineseBrowser = nullptr;
    QTextBrowser *m_englishBrowser = nullptr;
    QString m_installerUrl;
};
