#include "ThemeManager.h"
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
    return m_isDark ? QColor(QStringLiteral("#1e1e2e")) : QColor(QStringLiteral("#ffffff"));
}

QColor ThemeManager::surface() const
{
    return m_isDark ? QColor(QStringLiteral("#313244")) : QColor(QStringLiteral("#f0f0f5"));
}

QColor ThemeManager::surfacePressed() const
{
    return m_isDark ? QColor(QStringLiteral("#45475a")) : QColor(QStringLiteral("#d8d8e5"));
}

QColor ThemeManager::text() const
{
    return m_isDark ? QColor(QStringLiteral("#cdd6f4")) : QColor(QStringLiteral("#1e1e2e"));
}

QColor ThemeManager::textSecondary() const
{
    return m_isDark ? QColor(QStringLiteral("#6c7086")) : QColor(QStringLiteral("#6c6c7a"));
}

QColor ThemeManager::accent() const
{
    return m_isDark ? QColor(QStringLiteral("#89b4fa")) : QColor(QStringLiteral("#2563eb"));
}

QColor ThemeManager::accentPressed() const
{
    return m_isDark ? QColor(QStringLiteral("#74c7ec")) : QColor(QStringLiteral("#1d4ed8"));
}

QColor ThemeManager::border() const
{
    return m_isDark ? QColor(QStringLiteral("#45475a")) : QColor(QStringLiteral("#c8c8d5"));
}

QColor ThemeManager::borderFocus() const
{
    return m_isDark ? QColor(QStringLiteral("#89b4fa")) : QColor(QStringLiteral("#2563eb"));
}

QColor ThemeManager::hover() const
{
    return m_isDark ? QColor(QStringLiteral("#252538")) : QColor(QStringLiteral("#f0f0f5"));
}

QColor ThemeManager::selection() const
{
    return m_isDark ? QColor(QStringLiteral("#45475a")) : QColor(QStringLiteral("#d8d8e5"));
}

QColor ThemeManager::placeholder() const
{
    return m_isDark ? QColor(QStringLiteral("#6c7086")) : QColor(QStringLiteral("#9ca3af"));
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
