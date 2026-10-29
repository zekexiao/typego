#include "ScrollStitcher.h"

#include <QPainter>

#include <cmath>

namespace {

constexpr int HashSamples = 64;
constexpr int SimilarSamples = 24;
constexpr int MinOverlap = 24;
constexpr double MinScore = 0.82;
constexpr double MaxPixelError = 14.0;

quint64 mix(quint64 hash, quint64 value)
{
    hash ^= value;
    hash *= 1099511628211ull;
    return hash;
}

} // namespace

ScrollStitcher::ScrollStitcher() = default;

int ScrollStitcher::maxPixels()
{
    // 约 24MP：1920 宽约 1.2 万像素高，避免长截图和结果预览同时占用过多内存。
    return 24 * 1000 * 1000;
}

int ScrollStitcher::axisLength(const QImage &image, ScrollAxis axis)
{
    return axis == ScrollAxis::Vertical ? image.height() : image.width();
}

int ScrollStitcher::crossLength(const QImage &image, ScrollAxis axis)
{
    return axis == ScrollAxis::Vertical ? image.width() : image.height();
}

QVector<quint64> ScrollStitcher::lineHashes(const QImage &image, ScrollAxis axis)
{
    const int lines = axisLength(image, axis);
    const int cross = crossLength(image, axis);
    const int samples = qMin(HashSamples, qMax(1, cross));
    QVector<quint64> hashes;
    hashes.reserve(lines);
    for (int line = 0; line < lines; ++line) {
        quint64 hash = 1469598103934665603ull;
        for (int i = 0; i < samples; ++i) {
            const int position = samples == 1 ? 0 : i * (cross - 1) / (samples - 1);
            const QRgb color = axis == ScrollAxis::Vertical ? image.pixel(position, line)
                                                            : image.pixel(line, position);
            hash = mix(hash, static_cast<quint64>(color) & 0xFFFFFFu);
        }
        hashes.append(hash);
    }
    return hashes;
}

bool ScrollStitcher::linesSimilar(const QImage &a, int aIndex, const QImage &b, int bIndex, ScrollAxis axis)
{
    const int cross = crossLength(a, axis);
    const int samples = qMin(SimilarSamples, qMax(1, cross));
    int error = 0;
    for (int i = 0; i < samples; ++i) {
        const int position = samples == 1 ? 0 : i * (cross - 1) / (samples - 1);
        const QRgb ca = axis == ScrollAxis::Vertical ? a.pixel(position, aIndex) : a.pixel(aIndex, position);
        const QRgb cb = axis == ScrollAxis::Vertical ? b.pixel(position, bIndex) : b.pixel(bIndex, position);
        error += qAbs(qRed(ca) - qRed(cb)) + qAbs(qGreen(ca) - qGreen(cb))
            + qAbs(qBlue(ca) - qBlue(cb));
    }
    return error <= samples * 3 * 3;
}

double ScrollStitcher::overlapError(const QImage &previous, const QImage &current, const ScrollMatch &match)
{
    const int n = axisLength(previous, match.axis);
    const int usable = n - match.leading - match.trailing;
    const int overlap = usable - qAbs(match.delta);
    if (overlap <= 0) return 255.0;
    const int lines = qMin(24, overlap);
    const int cross = crossLength(previous, match.axis);
    const int columns = qMin(32, qMax(1, cross));
    double error = 0.0;
    int count = 0;
    for (int i = 0; i < lines; ++i) {
        const int offset = lines == 1 ? 0 : i * (overlap - 1) / (lines - 1);
        int previousIndex = 0;
        int currentIndex = 0;
        if (match.delta > 0) {
            previousIndex = match.leading + match.delta + offset;
            currentIndex = match.leading + offset;
        } else {
            previousIndex = match.leading + offset;
            currentIndex = match.leading + qAbs(match.delta) + offset;
        }
        for (int j = 0; j < columns; ++j) {
            const int position = columns == 1 ? 0 : j * (cross - 1) / (columns - 1);
            const QRgb a = match.axis == ScrollAxis::Vertical
                ? previous.pixel(position, previousIndex)
                : previous.pixel(previousIndex, position);
            const QRgb b = match.axis == ScrollAxis::Vertical
                ? current.pixel(position, currentIndex)
                : current.pixel(currentIndex, position);
            error += qAbs(qRed(a) - qRed(b)) + qAbs(qGreen(a) - qGreen(b))
                + qAbs(qBlue(a) - qBlue(b));
            ++count;
        }
    }
    return count > 0 ? error / (count * 3.0) : 255.0;
}

ScrollMatch ScrollStitcher::findBestOnAxis(const QImage &previous, const QImage &current, ScrollAxis axis)
{
    ScrollMatch bestPositive;
    ScrollMatch bestNegative;
    ScrollMatch best;
    if (previous.size() != current.size()) return best;
    const int n = axisLength(previous, axis);
    if (n < 64) return best;

    const QVector<quint64> previousHashes = lineHashes(previous, axis);
    const QVector<quint64> currentHashes = lineHashes(current, axis);

    // 固定标题栏/页脚在每一帧都完全相同。只在中间滚动区域里找重叠。
    const int maxSticky = n / 5;
    int leading = 0;
    while (leading < maxSticky && linesSimilar(previous, leading, current, leading, axis)) ++leading;
    int trailing = 0;
    while (trailing < maxSticky
           && linesSimilar(previous, n - 1 - trailing, current, n - 1 - trailing, axis))
        ++trailing;
    if (leading + trailing > n / 2) trailing = qMax(0, n / 2 - leading);

    const int usable = n - leading - trailing;
    const int minimumOverlap = qMax(MinOverlap, usable / 4);
    if (usable <= minimumOverlap) return best;

    const int maxDelta = usable - minimumOverlap;
    for (int delta = 1; delta <= maxDelta; ++delta) {
        const int overlap = usable - delta;
        int equal = 0;
        for (int i = 0; i < overlap; ++i) {
            if (previousHashes.at(leading + delta + i) == currentHashes.at(leading + i)) ++equal;
        }
        const double ratio = static_cast<double>(equal) / overlap;
        if (ratio < MinScore || ratio <= bestPositive.score) continue;
        ScrollMatch candidate;
        candidate.valid = true;
        candidate.axis = axis;
        candidate.delta = delta;
        candidate.leading = leading;
        candidate.trailing = trailing;
        candidate.score = ratio;
        // 先保留最佳候选，最后再做像素级校验，避免哈希碰撞。
        bestPositive = candidate;
    }

    // 反向（向上/向左）滚动。
    for (int delta = 1; delta <= maxDelta; ++delta) {
        const int overlap = usable - delta;
        int equal = 0;
        for (int i = 0; i < overlap; ++i) {
            if (previousHashes.at(leading + i) == currentHashes.at(leading + delta + i)) ++equal;
        }
        const double ratio = static_cast<double>(equal) / overlap;
        if (ratio < MinScore || ratio <= bestNegative.score) continue;
        ScrollMatch candidate;
        candidate.valid = true;
        candidate.axis = axis;
        candidate.delta = -delta;
        candidate.leading = leading;
        candidate.trailing = trailing;
        candidate.score = ratio;
        bestNegative = candidate;
    }

    // 相同匹配度时优先较小的位移，避免把重复图案误判为大幅滚动。
    constexpr double ScoreEpsilon = 1e-6;
    if (bestNegative.valid
        && (!bestPositive.valid || bestNegative.score > bestPositive.score + ScoreEpsilon
            || (std::abs(bestNegative.score - bestPositive.score) <= ScoreEpsilon
                && std::abs(bestNegative.delta) < std::abs(bestPositive.delta))))
        best = bestNegative;
    else
        best = bestPositive;

    if (!best.valid || overlapError(previous, current, best) > MaxPixelError) {
        ScrollMatch empty;
        return empty;
    }
    return best;
}

ScrollMatch ScrollStitcher::findMatch(const QImage &previous, const QImage &current)
{
    const ScrollMatch vertical = findBestOnAxis(previous, current, ScrollAxis::Vertical);
    const ScrollMatch horizontal = findBestOnAxis(previous, current, ScrollAxis::Horizontal);
    if (vertical.valid && horizontal.valid) {
        // 垂直滚动更常见；只有横向明显更匹配时才选择横向。
        return horizontal.score > vertical.score + 0.02 ? horizontal : vertical;
    }
    return vertical.valid ? vertical : horizontal;
}

ScrollMatch ScrollStitcher::findMatchOnAxis(const QImage &previous, const QImage &current, ScrollAxis axis)
{
    return findBestOnAxis(previous, current, axis);
}

QImage ScrollStitcher::slice(const QImage &image, ScrollAxis axis, int start, int length)
{
    if (length <= 0) return {};
    if (axis == ScrollAxis::Vertical) {
        start = qBound(0, start, image.height());
        length = qMin(length, image.height() - start);
        return image.copy(0, start, image.width(), length);
    }
    start = qBound(0, start, image.width());
    length = qMin(length, image.width() - start);
    return image.copy(start, 0, length, image.height());
}

ScrollAddResult ScrollStitcher::addFrame(const QImage &frameArgument, ScrollMatch *matchOut)
{
    if (matchOut) *matchOut = {};
    if (frameArgument.isNull()) return ScrollAddResult::Invalid;
    QImage frame = frameArgument.format() == QImage::Format_RGB32
        ? frameArgument
        : frameArgument.convertToFormat(QImage::Format_RGB32);

    if (m_firstFrame.isNull()) {
        m_firstFrame = frame;
        m_lastFrame = frame;
        m_totalPixels = static_cast<qint64>(frame.width()) * frame.height();
        return ScrollAddResult::FirstFrame;
    }
    if (frame.size() != m_lastFrame.size()) return ScrollAddResult::Invalid;
    if (frame == m_lastFrame) return ScrollAddResult::Unchanged;

    const ScrollMatch match = m_initialized
        ? findBestOnAxis(m_lastFrame, frame, m_axis)
        : findMatch(m_lastFrame, frame);
    if (!match.valid) return ScrollAddResult::NoOverlap;

    if (!m_initialized) {
        m_axis = match.axis;
        m_fixedLeading = match.leading;
        m_fixedTrailing = match.trailing;
        const int n = axisLength(m_firstFrame, m_axis);
        m_header = slice(m_firstFrame, m_axis, 0, m_fixedLeading);
        m_body = slice(m_firstFrame, m_axis, m_fixedLeading,
                       qMax(0, n - m_fixedLeading - m_fixedTrailing));
        m_footer = slice(m_firstFrame, m_axis, n - m_fixedTrailing, m_fixedTrailing);
        m_totalLength = n;
        m_initialized = true;
    }

    const int n = axisLength(frame, m_axis);
    const int contentStart = m_fixedLeading;
    const int contentEnd = n - m_fixedTrailing;
    int stripStart = 0;
    int stripLength = 0;
    if (match.delta > 0) {
        stripStart = contentEnd - match.delta;
        stripLength = match.delta;
    } else {
        stripStart = contentStart;
        stripLength = -match.delta;
    }
    stripStart = qMax(stripStart, contentStart);
    stripLength = qMin(stripLength, contentEnd - stripStart);
    if (stripLength <= 0) {
        m_lastFrame = frame;
        return ScrollAddResult::Unchanged;
    }

    const int cross = crossLength(frame, m_axis);
    const qint64 addedPixels = static_cast<qint64>(cross) * stripLength;
    if (m_totalPixels + addedPixels > maxPixels()) {
        m_limitReached = true;
        return ScrollAddResult::LimitReached;
    }

    const QImage strip = slice(frame, m_axis, stripStart, stripLength);
    if (strip.isNull()) return ScrollAddResult::Invalid;
    if (match.delta > 0) m_below.append(strip);
    else m_above.prepend(strip);

    m_totalLength += stripLength;
    m_totalPixels += addedPixels;
    m_lastFrame = frame;
    if (matchOut) *matchOut = match;
    return ScrollAddResult::Stitched;
}

QImage ScrollStitcher::image() const
{
    if (m_firstFrame.isNull()) return {};
    if (!m_initialized) return m_firstFrame.copy();

    const int cross = crossLength(m_firstFrame, m_axis);
    int total = 0;
    const auto lengthOf = [this](const QImage &image) {
        return image.isNull() ? 0 : axisLength(image, m_axis);
    };
    total += lengthOf(m_header) + lengthOf(m_body) + lengthOf(m_footer);
    for (const QImage &strip : m_above) total += lengthOf(strip);
    for (const QImage &strip : m_below) total += lengthOf(strip);

    QImage result;
    QPainter painter;
    if (m_axis == ScrollAxis::Vertical) {
        result = QImage(cross, total, QImage::Format_RGB32);
        result.fill(Qt::black);
        painter.begin(&result);
        int y = 0;
        const auto draw = [&painter, &y](const QImage &image) {
            if (image.isNull()) return;
            painter.drawImage(0, y, image);
            y += image.height();
        };
        draw(m_header);
        for (const QImage &strip : m_above) draw(strip);
        draw(m_body);
        for (const QImage &strip : m_below) draw(strip);
        draw(m_footer);
    } else {
        result = QImage(total, cross, QImage::Format_RGB32);
        result.fill(Qt::black);
        painter.begin(&result);
        int x = 0;
        const auto draw = [&painter, &x](const QImage &image) {
            if (image.isNull()) return;
            painter.drawImage(x, 0, image);
            x += image.width();
        };
        draw(m_header);
        for (const QImage &strip : m_above) draw(strip);
        draw(m_body);
        for (const QImage &strip : m_below) draw(strip);
        draw(m_footer);
    }
    painter.end();
    return result;
}

void ScrollStitcher::reset()
{
    m_firstFrame = {};
    m_header = {};
    m_body = {};
    m_footer = {};
    m_above.clear();
    m_below.clear();
    m_lastFrame = {};
    m_axis = ScrollAxis::Vertical;
    m_initialized = false;
    m_limitReached = false;
    m_fixedLeading = 0;
    m_fixedTrailing = 0;
    m_totalLength = 0;
    m_totalPixels = 0;
}