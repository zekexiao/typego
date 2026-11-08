#pragma once

#include <QColor>
#include <QRect>
#include <QString>

class QIcon;
class QPainter;
class ThemeManager;

// 优先级的四档定义与共用画法：行内旗标、弹窗菜单、提示文案都从这里取。
// 档位阶梯集中在本文件，新增档位时不必再去各处改 if 判断。
namespace TodoPriority {
constexpr int Low = 0;
constexpr int Medium = 1;
constexpr int High = 2;
constexpr int Urgent = 3;

// 档位名，已按界面语言翻译。
QString label(int level);
// 行尾元信息里的档位文案：中档是默认值，不占位置，返回空串。
QString metaLabel(int level);
// 档位色：紧急用错误色、高用强调色、中次级灰、低更淡。
QColor color(ThemeManager *theme, int level);
// 16px 旗标图标，供菜单项使用；行内旗标直接调用 drawFlag。
QIcon icon(ThemeManager *theme, int level);
// 在 hotspot 中心画一面旗，线宽与标题栏按钮一致。
void drawFlag(QPainter *painter, const QRect &hotspot, const QColor &ink);
} // namespace TodoPriority
