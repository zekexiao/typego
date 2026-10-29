#pragma once

#include <QImage>
#include <QVector>

// 滚动方向。正向表示内容向下/向右移动（用户向下/向右滚动）。
enum class ScrollAxis { Vertical, Horizontal };

struct ScrollMatch {
    bool valid = false;
    ScrollAxis axis = ScrollAxis::Vertical;
    int delta = 0;
    int leading = 0;  // 固定头部/左栏
    int trailing = 0; // 固定底部/右栏
    double score = 0.0;
};

enum class ScrollAddResult {
    FirstFrame,
    Unchanged,
    Stitched,
    NoOverlap,
    LimitReached,
    Invalid,
};

// 把连续滚动截取的多帧拼成一张长图。
// 只保留首帧、固定头和尾以及新增的条带，结束时一次性合成，避免逐帧复制整张图。
class ScrollStitcher {
public:
    ScrollStitcher();

    void reset();
    ScrollAddResult addFrame(const QImage &frame, ScrollMatch *match = nullptr);
    QImage image() const;
    int stitchedLength() const { return m_totalLength; }
    bool limitReached() const { return m_limitReached; }

    static int maxPixels();
    static ScrollMatch findMatch(const QImage &previous, const QImage &current);
    static ScrollMatch findMatchOnAxis(const QImage &previous, const QImage &current, ScrollAxis axis);

private:
    static int axisLength(const QImage &image, ScrollAxis axis);
    static int crossLength(const QImage &image, ScrollAxis axis);
    static QVector<quint64> lineHashes(const QImage &image, ScrollAxis axis);
    static bool linesSimilar(const QImage &a, int aIndex, const QImage &b, int bIndex, ScrollAxis axis);
    static ScrollMatch findBestOnAxis(const QImage &previous, const QImage &current, ScrollAxis axis);
    static double overlapError(const QImage &previous, const QImage &current, const ScrollMatch &match);
    static QImage slice(const QImage &image, ScrollAxis axis, int start, int length);

    QImage m_firstFrame;
    QImage m_header;
    QImage m_body;
    QImage m_footer;
    QVector<QImage> m_above; // 向上/向左滚动新增
    QVector<QImage> m_below; // 向下/向右滚动新增
    QImage m_lastFrame;
    ScrollAxis m_axis = ScrollAxis::Vertical;
    bool m_initialized = false;
    bool m_limitReached = false;
    int m_fixedLeading = 0;
    int m_fixedTrailing = 0;
    int m_totalLength = 0;
    qint64 m_totalPixels = 0;
};