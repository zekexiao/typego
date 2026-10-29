#include "ThemeManager.h"
#include <QFile>
#include <QGuiApplication>
#include <QStyleHints>
#include <QApplication>
#include <QEvent>
#include <QStyle>
#include <QWidget>

#ifdef Q_OS_WIN
#include <windows.h>
#include <dwmapi.h>
#endif

namespace {
QString translucentColor(const QColor &color, int alpha)
{
    return QStringLiteral("rgba(%1, %2, %3, %4)")
        .arg(color.red()).arg(color.green()).arg(color.blue()).arg(alpha);
}
}

ThemeManager::ThemeManager(QObject *parent)
    : QObject(parent)
{
    updatePalette();
    qApp->installEventFilter(this);
    connect(this, &ThemeManager::paletteChanged, this, [this]() {
        for (QWidget *window : QApplication::topLevelWidgets()) {
            if (window->isVisible()) applyWindowMaterial(window);
        }
    });
}

bool ThemeManager::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::Polish) {
        if (auto *window = qobject_cast<QWidget *>(watched);
            window && window->isWindow()
            && !window->property("skipWindowMaterial").toBool()
            && !window->property("useWindowMaterial").toBool()) {
            // 在首次绘制前确定底色，避免弹出过程中重新 polish 改变布局与背景。
            window->setProperty("frostedWindow", false);
        }
    }
    if (event->type() == QEvent::Show) {
        if (auto *window = qobject_cast<QWidget *>(watched); window && window->isWindow()) {
            applyWindowMaterial(window);
        }
    }
    return QObject::eventFilter(watched, event);
}

void ThemeManager::applyWindowMaterial(QWidget *window)
{
    // 悬浮图标使用逐像素透明，不能扩展为矩形系统材质。
    if (window->property("skipWindowMaterial").toBool()) return;

    const bool frosted = m_frostedGlass && window->property("useWindowMaterial").toBool();
    const auto cached = m_windowMaterials.constFind(window);
    // 隐藏后再次唤起不重复提交 DWM 材质；句柄重建或外观变化时才更新。
    if (cached != m_windowMaterials.cend()
        && cached->nativeId == quintptr(window->internalWinId())
        && cached->dark == m_isDark && cached->frosted == frosted) return;
    if (cached == m_windowMaterials.cend()) {
        connect(window, &QObject::destroyed, this, [this, window]() {
            m_windowMaterials.remove(window);
        });
    }

    bool enabled = false;
#ifdef Q_OS_WIN
    const HWND hwnd = reinterpret_cast<HWND>(window->winId());
    // 系统负责顶层圆角裁剪，弹出窗口仍保持实色，避免透明角落和边框残影。
    const DWM_WINDOW_CORNER_PREFERENCE corners = DWMWCP_ROUND;
    DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &corners, sizeof(corners));
    const BOOL dark = m_isDark;
    DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));
    // 仅显式接入的自定义主窗口扩展玻璃，不能改动 Qt 弹出容器和原生对话框的边框。
    if (window->property("useWindowMaterial").toBool()) {
        const DWM_SYSTEMBACKDROP_TYPE backdrop = frosted ? DWMSBT_TRANSIENTWINDOW : DWMSBT_NONE;
        if (SUCCEEDED(DwmSetWindowAttribute(hwnd, DWMWA_SYSTEMBACKDROP_TYPE,
                                           &backdrop, sizeof(backdrop)))) {
            // 与 QWindowKit 保持一致，只从左侧扩展，避免系统强调色重新露出原生标题栏。
            const MARGINS margins = {frosted ? 65536 : 0, 0, 0, 0};
            enabled = SUCCEEDED(DwmExtendFrameIntoClientArea(hwnd, &margins)) && frosted;
        }
    }
#endif
    m_windowMaterials.insert(window, {quintptr(window->internalWinId()), m_isDark, frosted});
    const QVariant previous = window->property("frostedWindow");
    if (!previous.isValid() || previous.toBool() != enabled) {
        window->setProperty("frostedWindow", enabled);
        window->style()->unpolish(window);
        window->style()->polish(window);
        window->update();
    }
}

QString ThemeManager::theme() const
{
    return m_theme;
}

QString ThemeManager::styleSheet() const
{
    QFile file(QStringLiteral(":/assets/style.qss"));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QString();
    }

    QString style = QString::fromUtf8(file.readAll());
    style.replace(QStringLiteral("@GLASS_TINT@"), translucentColor(bg(), m_frostedGlass ? (m_isDark ? 72 : 100) : 255));
    style.replace(QStringLiteral("@GLASS_SURFACE@"), translucentColor(surface(), m_frostedGlass ? (m_isDark ? 100 : 125) : 255));
    style.replace(QStringLiteral("@GLASS_INSET@"), translucentColor(bg(), m_frostedGlass ? 65 : 255));
    style.replace(QStringLiteral("@BG@"), bg().name(QColor::HexRgb));
    style.replace(QStringLiteral("@SURFACE@"), surface().name(QColor::HexRgb));
    style.replace(QStringLiteral("@SURFACE_PRESSED@"), surfacePressed().name(QColor::HexRgb));
    style.replace(QStringLiteral("@TEXT@"), text().name(QColor::HexRgb));
    style.replace(QStringLiteral("@TEXT_SECONDARY@"), textSecondary().name(QColor::HexRgb));
    style.replace(QStringLiteral("@BORDER@"), border().name(QColor::HexRgb));
    style.replace(QStringLiteral("@BORDER_FOCUS@"), borderFocus().name(QColor::HexRgb));
    style.replace(QStringLiteral("@SELECTION@"), selection().name(QColor::HexRgb));
    style.replace(QStringLiteral("@HOVER@"), hover().name(QColor::HexRgb));
    style.replace(QStringLiteral("@ACCENT@"), accent().name(QColor::HexRgb));
    style.replace(QStringLiteral("@ACCENT_FILL@"), accentFill().name(QColor::HexRgb));
    style.replace(QStringLiteral("@ACCENT_TEXT@"), accentText().name(QColor::HexRgb));
    style.replace(QStringLiteral("@ACCENT_PRESSED@"), accentFillPressed().name(QColor::HexRgb));
    style.replace(QStringLiteral("@PLACEHOLDER@"), placeholder().name(QColor::HexRgb));
    style.replace(QStringLiteral("@ERROR@"), error().name(QColor::HexRgb));
    style.replace(QStringLiteral("@ERROR_FILL@"), errorFill().name(QColor::HexRgb));
    // 关闭按钮只给热区加一层红色染底，图标颜色在 TitleBarIconButton 里取 closeButtonHover()。
    const QColor closeHover = closeButtonHover();
    style.replace(QStringLiteral("@CLOSE_TINT@"), translucentColor(closeHover, m_isDark ? 64 : 26));
    style.replace(QStringLiteral("@CLOSE_TINT_PRESSED@"), translucentColor(closeHover, m_isDark ? 104 : 48));
    return style;
}

void ThemeManager::setTheme(const QString &theme)
{
    if (m_theme == theme)
        return;
    m_theme = theme;
    emit themeChanged();
    updatePalette();
}

void ThemeManager::refresh()
{
    updatePalette();
}

void ThemeManager::setFrostedGlass(bool enabled)
{
    if (m_frostedGlass == enabled) return;
    m_frostedGlass = enabled;
    for (QWidget *window : QApplication::topLevelWidgets()) {
        if (window->isVisible()) applyWindowMaterial(window);
    }
    emit frostedGlassChanged();
}

bool ThemeManager::isDark() const
{
    return m_isDark;
}

void ThemeManager::updatePalette()
{
    bool wasDark = m_isDark;

    if (m_theme == QStringLiteral("dark")) {
        m_isDark = true;
    } else if (m_theme == QStringLiteral("light")) {
        m_isDark = false;
    } else {
        // system
        Qt::ColorScheme scheme = QGuiApplication::styleHints()->colorScheme();
        if (scheme == Qt::ColorScheme::Light) {
            m_isDark = false;
        } else {
            // Unknown or Dark default to dark
            m_isDark = true;
        }
    }

    if (wasDark != m_isDark) {
        emit paletteChanged();
    }
}

QColor ThemeManager::bg() const
{
    return m_isDark ? QColor(QStringLiteral("#1e1e1e")) : QColor(QStringLiteral("#f5f5f7"));
}

QColor ThemeManager::surface() const
{
    return m_isDark ? QColor(QStringLiteral("#2c2c2e")) : QColor(QStringLiteral("#ffffff"));
}

QColor ThemeManager::surfacePressed() const
{
    return m_isDark ? QColor(QStringLiteral("#48484a")) : QColor(QStringLiteral("#e5e5ea"));
}

QColor ThemeManager::text() const
{
    return m_isDark ? QColor(QStringLiteral("#f5f5f7")) : QColor(QStringLiteral("#1d1d1f"));
}

QColor ThemeManager::textSecondary() const
{
    return m_isDark ? QColor(QStringLiteral("#98989d")) : QColor(QStringLiteral("#6e6e73"));
}

QColor ThemeManager::accent() const
{
    // 描边与指示色在深浅两套主题下都可辨识；填充和文字用下面两个专用取值。
    return QColor(QStringLiteral("#0a84ff"));
}

QColor ThemeManager::accentFill() const
{
    // 填充面上放白字：深色主题需要压暗到 #0a6fd8 才达到 4.9:1 对比度。
    return m_isDark ? QColor(QStringLiteral("#0a6fd8")) : QColor(QStringLiteral("#0071e3"));
}

QColor ThemeManager::accentFillPressed() const
{
    return m_isDark ? QColor(QStringLiteral("#0b5cb0")) : QColor(QStringLiteral("#0062c4"));
}

QColor ThemeManager::accentText() const
{
    // 作为文字直接落在背景或卡片上，需要比描边更亮的蓝。
    return m_isDark ? QColor(QStringLiteral("#4da3ff")) : QColor(QStringLiteral("#0071e3"));
}

QColor ThemeManager::border() const
{
    return m_isDark ? QColor(QStringLiteral("#48484a")) : QColor(QStringLiteral("#d2d2d7"));
}

QColor ThemeManager::borderFocus() const
{
    return QColor(QStringLiteral("#0a84ff"));
}

QColor ThemeManager::hover() const
{
    return m_isDark ? QColor(QStringLiteral("#3a3a3c")) : QColor(QStringLiteral("#f0f0f2"));
}

QColor ThemeManager::selection() const
{
    // 选中行是实心填充 + 白字，必须用填充色而不是描边色。
    return accentFill();
}

QColor ThemeManager::placeholder() const
{
    // 占位与禁用文字同样属于正文尺寸下的文字，深色主题维持 4.9:1，浅色主题 4.5:1。
    return m_isDark ? QColor(QStringLiteral("#9a9aa0")) : QColor(QStringLiteral("#76767b"));
}

QColor ThemeManager::error() const
{
    return m_isDark ? QColor(QStringLiteral("#f38ba8")) : QColor(QStringLiteral("#dc2626"));
}

QColor ThemeManager::errorFill() const
{
    // 危险操作的按下填充：两套主题都用深红，白字对比度 6.5:1。
    return QColor(QStringLiteral("#b91c1c"));
}

QColor ThemeManager::closeButtonHover() const
{
    return m_isDark ? QColor(QStringLiteral("#e81123")) : QColor(QStringLiteral("#dc2626"));
}

QColor ThemeManager::iconPlaceholder() const
{
    return m_isDark ? QColor(QStringLiteral("#585b70")) : QColor(QStringLiteral("#9ca3af"));
}
