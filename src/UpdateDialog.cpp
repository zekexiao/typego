#include "UpdateDialog.h"

#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QFrame>
#include <QGuiApplication>
#include <QLabel>
#include <QLocale>
#include <QPushButton>
#include <QScreen>
#include <QScrollBar>
#include <QTabBar>
#include <QTabWidget>
#include <QTextBrowser>
#include <QTranslator>
#include <QVBoxLayout>

namespace {

// 界面当前是否是中文：按 main.cpp 载入翻译器的同一套判断，
// 系统语言能装上中文翻译时默认展示中文页签。
bool chineseUiActive()
{
    const QStringList languages = QLocale::system().uiLanguages();
    for (const QString &language : languages) {
        QTranslator translator;
        if (translator.load(QStringLiteral(":/i18n/TypeGo_") + QLocale(language).name())) return true;
    }
    return false;
}

// 把版本列表渲染成 HTML：每个版本一个标题，下面跟条目。
QString notesHtml(const QList<ReleaseParser::ReleaseNote> &notes, bool chinese)
{
    QString html;
    for (const ReleaseParser::ReleaseNote &note : notes) {
        QString heading = note.version;
        if (!note.date.isEmpty()) {
            heading += QStringLiteral(" · ");  // 版本号与日期之间的分隔点
            heading += note.date;
        }
        html += QStringLiteral("<h3>%1</h3><ul>").arg(heading.toHtmlEscaped());
        const QList<ReleaseParser::NoteItem> &items = chinese ? note.chinese : note.english;
        for (const ReleaseParser::NoteItem &item : items) {
            // 条目沿用发布页里的行内格式（如 <code>），正文本身来自官方发布说明。
            html += QStringLiteral("<li>%1</li>").arg(item.html);
        }
        html += QStringLiteral("</ul>");
    }
    return html;
}

bool hasItems(const QList<ReleaseParser::ReleaseNote> &notes, bool chinese)
{
    for (const ReleaseParser::ReleaseNote &note : notes) {
        if (!(chinese ? note.chinese : note.english).isEmpty()) return true;
    }
    return false;
}

QTextBrowser *createBrowser()
{
    auto *browser = new QTextBrowser;
    browser->setOpenExternalLinks(true);
    browser->setFrameShape(QFrame::NoFrame);
    return browser;
}

} // namespace

UpdateDialog::UpdateDialog(const ReleaseParser::Result &result, QWidget *parent)
    : QDialog(parent)
{
    setObjectName(QStringLiteral("updateDialog"));
    setWindowTitle(tr("Update available"));
    setModal(true);
    // 更新条目普遍较长，给一个稳定的宽度，避免每行都被折断。
    setMinimumWidth(560);
    resize(560, 480);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 20, 20, 16);
    layout->setSpacing(12);

    m_titleLabel = new QLabel(tr("Update available"));
    m_titleLabel->setObjectName(QStringLiteral("pageTitle"));
    layout->addWidget(m_titleLabel);

    m_subtitleLabel = new QLabel;
    m_subtitleLabel->setObjectName(QStringLiteral("muted"));
    m_subtitleLabel->setWordWrap(true);
    layout->addWidget(m_subtitleLabel);

    m_tabs = new QTabWidget;
    m_chineseBrowser = createBrowser();
    m_englishBrowser = createBrowser();
    m_tabs->addTab(m_chineseBrowser, QStringLiteral("中文"));
    m_tabs->addTab(m_englishBrowser, QStringLiteral("English"));
    m_tabs->setCurrentIndex(chineseUiActive() ? 0 : 1);
    layout->addWidget(m_tabs, 1);

    // 正文再长也只占固定高度，超出部分在页面内滚动。
    int screenHeight = 900;
    if (QScreen *screen = QGuiApplication::primaryScreen()) {
        screenHeight = screen->availableGeometry().height();
    }
    const int maxBodyHeight = qBound(160, screenHeight / 2, 520);
    const int minBodyHeight = qMin(200, maxBodyHeight);
    m_tabs->setMinimumHeight(minBodyHeight + m_tabs->tabBar()->sizeHint().height());
    m_tabs->setMaximumHeight(maxBodyHeight + m_tabs->tabBar()->sizeHint().height());
    for (QTextBrowser *browser : {m_chineseBrowser, m_englishBrowser}) {
        browser->setMinimumHeight(minBodyHeight);
        browser->setMaximumHeight(maxBodyHeight);
    }

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    buttons->button(QDialogButtonBox::Ok)->setText(tr("Update now"));
    buttons->button(QDialogButtonBox::Ok)->setObjectName(QStringLiteral("primary"));
    buttons->button(QDialogButtonBox::Cancel)->setText(tr("Later"));
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, this, [this]() {
        emit updateRequested(m_installerUrl);
        accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    applyResult(result);
}

void UpdateDialog::applyResult(const ReleaseParser::Result &result)
{
    m_installerUrl = result.installerUrl;
    m_titleLabel->setText(tr("Update available"));
    m_subtitleLabel->setText(tr("Update from TypeGo %1 to %2")
                                 .arg(result.currentVersion, result.latestVersion));

    const bool chinese = hasItems(result.notes, true);
    const bool english = hasItems(result.notes, false);
    m_tabs->setTabVisible(0, chinese);
    m_tabs->setTabVisible(1, english);

    const QString empty = tr("No release notes are available for this update.");
    m_chineseBrowser->setHtml(chinese ? notesHtml(result.notes, true) : QStringLiteral("<p>%1</p>").arg(empty.toHtmlEscaped()));
    m_englishBrowser->setHtml(english ? notesHtml(result.notes, false) : QStringLiteral("<p>%1</p>").arg(empty.toHtmlEscaped()));
    m_chineseBrowser->verticalScrollBar()->setValue(0);
    m_englishBrowser->verticalScrollBar()->setValue(0);
}
