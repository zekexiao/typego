#pragma once

#include <QPushButton>

class ThemeManager;

// 标题栏矢量图标按钮：样式表只管热区底色，图标由代码绘制，
// 避免依赖字体字形（× ‹ ↗ 在不同字号和字体下会发虚、基线也不稳）。
// 主窗口和插件设置弹窗共用同一套按钮，视觉保持一致。
class TitleBarIconButton final : public QPushButton {
public:
    enum class Glyph { Close, Back, Detach, Minimize };

    TitleBarIconButton(Glyph glyph, ThemeManager *themeManager, QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    Glyph m_glyph;
    ThemeManager *m_themeManager;
};
