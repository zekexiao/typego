#pragma once

#include "ClipboardStore.h"
#include <QString>

// 没有缩略图的条目用文字图标（T / F / I）占位，绘制一次后复用缓存文件。
// 图标不依赖字体与主题，直接用几何形状绘制，浅色和深色背景都清晰。
QString clipboardTypeIcon(ClipboardItem::Type type, const QString &cacheDir);