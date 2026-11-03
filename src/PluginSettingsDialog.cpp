#include "PluginSettingsDialog.h"
#include "TitleBarButton.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWKWidgets/widgetwindowagent.h>

PluginSettingsDialog::PluginSettingsDialog(const QString &title, QWidget *content,
                                           ThemeManager *themeManager, QWidget *parent)
    : QDialog(parent)
{
    setObjectName(QStringLiteral("pluginSettingsDialog"));
    setWindowTitle(title);
    setModal(true);
    // 接入 DWM 材质：磨砂由系统在窗口背后合成，窗口自身只画一层淡色。
    // ThemeManager 看到 useWindowMaterial 才会给窗口挂上材质并打开 frostedWindow。
    setProperty("useWindowMaterial", true);
    setAttribute(Qt::WA_DontCreateNativeAncestors);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // 无边框窗口交给 QWindowKit 接管：标题栏负责拖动，关闭键按系统按钮处理。
    auto *windowAgent = new QWK::WidgetWindowAgent(this);
    windowAgent->setup(this);

    auto *titleBar = new QWidget(this);
    titleBar->setObjectName(QStringLiteral("windowTitleBar"));
    titleBar->setFixedHeight(36);
    auto *titleLayout = new QHBoxLayout(titleBar);
    titleLayout->setContentsMargins(12, 0, 12, 0);
    auto *titleLabel = new QLabel(title, titleBar);
    titleLabel->setObjectName(QStringLiteral("windowTitle"));
    titleLayout->addWidget(titleLabel);
    titleLayout->addStretch();
    auto *closeIconButton = new TitleBarIconButton(TitleBarIconButton::Glyph::Close, themeManager, titleBar);
    closeIconButton->setObjectName(QStringLiteral("windowCloseButton"));
    closeIconButton->setToolTip(tr("Close"));
    closeIconButton->setAccessibleName(tr("Close"));
    titleLayout->addWidget(closeIconButton);
    windowAgent->setTitleBar(titleBar);
    windowAgent->setSystemButton(QWK::WindowAgentBase::Close, closeIconButton);
    windowAgent->setHitTestVisible(closeIconButton, true);
    connect(closeIconButton, &QPushButton::clicked, this, &QDialog::accept);
    layout->addWidget(titleBar);

    auto *body = new QWidget(this);
    auto *bodyLayout = new QVBoxLayout(body);
    bodyLayout->setContentsMargins(16, 12, 16, 14);
    bodyLayout->setSpacing(8);
    if (content) bodyLayout->addWidget(content);
    // 内容顶到上面，多余空间留在下方，底部按钮不会和表单挤在一起。
    bodyLayout->addStretch();
    layout->addWidget(body, 1);

    // 底部只放关闭按钮。设置项即时生效，这里没有「保存」动作，
    // 按钮保持中性样式，不抢主操作的视觉重量。
    // 用普通按钮而不是 QDialogButtonBox：
    // 标准按钮组会在 LanguageChange 时把文案重置成 Qt 自带英文。
    auto *footer = new QWidget(this);
    auto *footerLayout = new QHBoxLayout(footer);
    footerLayout->setContentsMargins(16, 0, 16, 14);
    auto *closeButton = new QPushButton(tr("Close"), footer);
    closeButton->setMinimumWidth(84);
    footerLayout->addStretch();
    footerLayout->addWidget(closeButton);
    connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);
    layout->addWidget(footer);

    // 宽度固定下限，高度跟着内容走。
    setMinimumWidth(460);
}
