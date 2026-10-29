#pragma once

#include <QColor>
#include <QIcon>

#include "Annotation.h"

// 工具栏图标全部用 QPainter 矢量绘制，避免依赖字体字形。
QIcon shotToolIcon(ShotTool tool);
QIcon shotColorIcon(const QColor &color);
QIcon shotWidthIcon(int width);
QIcon shotActionIcon(const QString &name);