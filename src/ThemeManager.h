#pragma once

#include <QObject>
#include <QColor>
#include <QHash>

class QWidget;

// 管理当前主题及界面调色板。
class ThemeManager : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString theme READ theme WRITE setTheme NOTIFY themeChanged)
    Q_PROPERTY(bool isDark READ isDark NOTIFY paletteChanged)
    Q_PROPERTY(QColor bg READ bg NOTIFY paletteChanged)
    Q_PROPERTY(QColor surface READ surface NOTIFY paletteChanged)
    Q_PROPERTY(QColor surfacePressed READ surfacePressed NOTIFY paletteChanged)
    Q_PROPERTY(QColor text READ text NOTIFY paletteChanged)
    Q_PROPERTY(QColor textSecondary READ textSecondary NOTIFY paletteChanged)
    Q_PROPERTY(QColor accent READ accent NOTIFY paletteChanged)
    Q_PROPERTY(QColor accentPressed READ accentPressed NOTIFY paletteChanged)
    Q_PROPERTY(QColor border READ border NOTIFY paletteChanged)
    Q_PROPERTY(QColor borderFocus READ borderFocus NOTIFY paletteChanged)
    Q_PROPERTY(QColor hover READ hover NOTIFY paletteChanged)
    Q_PROPERTY(QColor selection READ selection NOTIFY paletteChanged)
    Q_PROPERTY(QColor placeholder READ placeholder NOTIFY paletteChanged)
    Q_PROPERTY(QColor error READ error NOTIFY paletteChanged)
    Q_PROPERTY(QColor closeButtonHover READ closeButtonHover NOTIFY paletteChanged)
    Q_PROPERTY(QColor iconPlaceholder READ iconPlaceholder NOTIFY paletteChanged)

public:
    explicit ThemeManager(QObject *parent = nullptr);

    QString theme() const;
    QString styleSheet() const;
    void setTheme(const QString &theme);
    void refresh();
    void setFrostedGlass(bool enabled);

    bool isDark() const;

    QColor bg() const;
    QColor surface() const;
    QColor surfacePressed() const;
    QColor text() const;
    QColor textSecondary() const;
    QColor accent() const;
    QColor accentPressed() const;
    QColor border() const;
    QColor borderFocus() const;
    QColor hover() const;
    QColor selection() const;
    QColor placeholder() const;
    QColor error() const;
    QColor closeButtonHover() const;
    QColor iconPlaceholder() const;

signals:
    void themeChanged();
    void paletteChanged();
    void frostedGlassChanged();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void applyWindowMaterial(QWidget *window);
    void updatePalette();

    QString m_theme = QStringLiteral("system");
    bool m_isDark = true;
    bool m_frostedGlass = true;
    struct WindowMaterialState {
        quintptr nativeId;
        bool dark;
        bool frosted;
    };
    QHash<QWidget *, WindowMaterialState> m_windowMaterials;
};
