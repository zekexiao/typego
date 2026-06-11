#include "ThemeManager.h"
#include <QFile>
#include <QGuiApplication>
#include <QStyleHints>

ThemeManager::ThemeManager(QObject *parent)
    : QObject(parent)
{
    updatePalette();
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
    style.replace(QStringLiteral("@ACCENT_PRESSED@"), accentPressed().name(QColor::HexRgb));
    style.replace(QStringLiteral("@PLACEHOLDER@"), placeholder().name(QColor::HexRgb));
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
    return QColor(QStringLiteral("#0a84ff"));
}

QColor ThemeManager::accentPressed() const
{
    return m_isDark ? QColor(QStringLiteral("#409cff")) : QColor(QStringLiteral("#0071e3"));
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
    return m_isDark ? QColor(QStringLiteral("#0a84ff")) : QColor(QStringLiteral("#007aff"));
}

QColor ThemeManager::placeholder() const
{
    return m_isDark ? QColor(QStringLiteral("#8e8e93")) : QColor(QStringLiteral("#8e8e93"));
}

QColor ThemeManager::error() const
{
    return m_isDark ? QColor(QStringLiteral("#f38ba8")) : QColor(QStringLiteral("#dc2626"));
}

QColor ThemeManager::closeButtonHover() const
{
    return m_isDark ? QColor(QStringLiteral("#e81123")) : QColor(QStringLiteral("#dc2626"));
}

QColor ThemeManager::iconPlaceholder() const
{
    return m_isDark ? QColor(QStringLiteral("#585b70")) : QColor(QStringLiteral("#9ca3af"));
}
