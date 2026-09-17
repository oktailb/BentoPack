#include "packer/maxrectspacker.h"
#include <limits>
#include <algorithm>

MaxRectsPacker::MaxRectsPacker()
    : m_binWidth(0)
    , m_binHeight(0)
{
}

MaxRectsPacker::MaxRectsPacker(int width, int height)
{
    init(width, height);
}

void MaxRectsPacker::init(int width, int height)
{
    m_binWidth = width;
    m_binHeight = height;

    m_usedRectangles.clear();
    m_freeRectangles.clear();

    if (width > 0 && height > 0) {
        m_freeRectangles.append(QRect(0, 0, width, height));
    }
}

QRect MaxRectsPacker::insert(int width, int height, MaxRectsHeuristic heuristic)
{
    QRect newNode;
    int score1 = std::numeric_limits<int>::max();
    int score2 = std::numeric_limits<int>::max();

    switch (heuristic) {
    case MaxRectsHeuristic::BestShortSideFit:
        newNode = findPositionForNewNodeBestShortSideFit(width, height, score1, score2);
        break;
    case MaxRectsHeuristic::BestAreaFit:
        newNode = findPositionForNewNodeBestAreaFit(width, height, score1, score2);
        break;
    case MaxRectsHeuristic::BestLongSideFit:
        newNode = findPositionForNewNodeBestLongSideFit(width, height, score1, score2);
        break;
    case MaxRectsHeuristic::BottomLeft:
        newNode = findPositionForNewNodeBottomLeft(width, height, score1, score2);
        break;
    case MaxRectsHeuristic::ContactPoint:
        score1 = -1;
        newNode = findPositionForNewNodeContactPoint(width, height, score1);
        break;
    }

    if (newNode.width() == 0 || newNode.height() == 0) {
        return QRect();
    }

    int numRectanglesToProcess = m_freeRectangles.size();
    for (int i = 0; i < numRectanglesToProcess; ++i) {
        if (splitFreeNode(m_freeRectangles.at(i), newNode)) {
            m_freeRectangles.removeAt(i);
            --i;
            --numRectanglesToProcess;
        }
    }

    pruneFreeList();

    m_usedRectangles.append(newNode);
    return newNode;
}

QList<QRect> MaxRectsPacker::insert(const QList<QSize> &rectSizes, MaxRectsHeuristic heuristic)
{
    QList<QRect> result;
    result.resize(rectSizes.size());

    if (rectSizes.isEmpty()) {
        return result;
    }

    // Sort rectangles by descending height, then descending width for optimal packing
    QList<int> sortedIndices;
    sortedIndices.reserve(rectSizes.size());
    for (int i = 0; i < rectSizes.size(); ++i) {
        sortedIndices.append(i);
    }

    std::sort(sortedIndices.begin(), sortedIndices.end(), [&rectSizes](int a, int b) {
        const QSize &sa = rectSizes.at(a);
        const QSize &sb = rectSizes.at(b);
        if (sa.height() != sb.height()) {
            return sa.height() > sb.height();
        }
        if (sa.width() != sb.width()) {
            return sa.width() > sb.width();
        }
        return (sa.width() * sa.height()) > (sb.width() * sb.height());
    });

    for (int idx : sortedIndices) {
        const QSize &sz = rectSizes.at(idx);
        QRect placed = insert(sz.width(), sz.height(), heuristic);
        result[idx] = placed;
    }

    return result;
}

float MaxRectsPacker::occupancy() const
{
    if (m_binWidth <= 0 || m_binHeight <= 0) {
        return 0.0f;
    }

    long long usedArea = 0;
    for (const QRect &r : m_usedRectangles) {
        usedArea += static_cast<long long>(r.width()) * r.height();
    }

    long long totalArea = static_cast<long long>(m_binWidth) * m_binHeight;
    return static_cast<float>(usedArea) / static_cast<float>(totalArea);
}

QRect MaxRectsPacker::findPositionForNewNodeBestShortSideFit(int width, int height,
                                                             int &bestShortSideFit, int &bestLongSideFit) const
{
    QRect bestNode;
    bestShortSideFit = std::numeric_limits<int>::max();
    bestLongSideFit = std::numeric_limits<int>::max();

    for (const QRect &free : m_freeRectangles) {
        if (free.width() >= width && free.height() >= height) {
            int leftoverHoriz = free.width() - width;
            int leftoverVert = free.height() - height;
            int shortSide = std::min(leftoverHoriz, leftoverVert);
            int longSide = std::max(leftoverHoriz, leftoverVert);

            if (shortSide < bestShortSideFit || (shortSide == bestShortSideFit && longSide < bestLongSideFit)) {
                bestNode = QRect(free.x(), free.y(), width, height);
                bestShortSideFit = shortSide;
                bestLongSideFit = longSide;
            }
        }
    }

    return bestNode;
}

QRect MaxRectsPacker::findPositionForNewNodeBestAreaFit(int width, int height,
                                                        int &bestAreaFit, int &bestShortSideFit) const
{
    QRect bestNode;
    bestAreaFit = std::numeric_limits<int>::max();
    bestShortSideFit = std::numeric_limits<int>::max();

    for (const QRect &free : m_freeRectangles) {
        if (free.width() >= width && free.height() >= height) {
            int areaFit = free.width() * free.height() - width * height;
            int leftoverHoriz = free.width() - width;
            int leftoverVert = free.height() - height;
            int shortSide = std::min(leftoverHoriz, leftoverVert);

            if (areaFit < bestAreaFit || (areaFit == bestAreaFit && shortSide < bestShortSideFit)) {
                bestNode = QRect(free.x(), free.y(), width, height);
                bestAreaFit = areaFit;
                bestShortSideFit = shortSide;
            }
        }
    }

    return bestNode;
}

QRect MaxRectsPacker::findPositionForNewNodeBestLongSideFit(int width, int height,
                                                           int &bestLongSideFit, int &bestShortSideFit) const
{
    QRect bestNode;
    bestLongSideFit = std::numeric_limits<int>::max();
    bestShortSideFit = std::numeric_limits<int>::max();

    for (const QRect &free : m_freeRectangles) {
        if (free.width() >= width && free.height() >= height) {
            int leftoverHoriz = free.width() - width;
            int leftoverVert = free.height() - height;
            int shortSide = std::min(leftoverHoriz, leftoverVert);
            int longSide = std::max(leftoverHoriz, leftoverVert);

            if (longSide < bestLongSideFit || (longSide == bestLongSideFit && shortSide < bestShortSideFit)) {
                bestNode = QRect(free.x(), free.y(), width, height);
                bestLongSideFit = longSide;
                bestShortSideFit = shortSide;
            }
        }
    }

    return bestNode;
}

QRect MaxRectsPacker::findPositionForNewNodeBottomLeft(int width, int height, int &bestY, int &bestX) const
{
    QRect bestNode;
    bestY = std::numeric_limits<int>::max();
    bestX = std::numeric_limits<int>::max();

    for (const QRect &free : m_freeRectangles) {
        if (free.width() >= width && free.height() >= height) {
            int topSide = free.y();
            int leftSide = free.x();

            if (topSide < bestY || (topSide == bestY && leftSide < bestX)) {
                bestNode = QRect(free.x(), free.y(), width, height);
                bestY = topSide;
                bestX = leftSide;
            }
        }
    }

    return bestNode;
}

QRect MaxRectsPacker::findPositionForNewNodeContactPoint(int width, int height, int &bestContactScore) const
{
    QRect bestNode;
    bestContactScore = -1;

    for (const QRect &free : m_freeRectangles) {
        if (free.width() >= width && free.height() >= height) {
            int score = contactPointScoreNode(free.x(), free.y(), width, height);
            if (score > bestContactScore) {
                bestNode = QRect(free.x(), free.y(), width, height);
                bestContactScore = score;
            }
        }
    }

    return bestNode;
}

int MaxRectsPacker::contactPointScoreNode(int x, int y, int width, int height) const
{
    int score = 0;

    if (x == 0 || x + width == m_binWidth) {
        score += height;
    }
    if (y == 0 || y + height == m_binHeight) {
        score += width;
    }

    for (const QRect &r : m_usedRectangles) {
        if (r.x() == x + width || r.x() + r.width() == x) {
            int commonStart = std::max(r.y(), y);
            int commonEnd = std::min(r.y() + r.height(), y + height);
            if (commonEnd > commonStart) {
                score += (commonEnd - commonStart);
            }
        }
        if (r.y() == y + height || r.y() + r.height() == y) {
            int commonStart = std::max(r.x(), x);
            int commonEnd = std::min(r.x() + r.width(), x + width);
            if (commonEnd > commonStart) {
                score += (commonEnd - commonStart);
            }
        }
    }

    return score;
}

bool MaxRectsPacker::splitFreeNode(const QRect &freeNode, const QRect &usedNode)
{
    int usedLeft = usedNode.x();
    int usedRight = usedNode.x() + usedNode.width();
    int usedTop = usedNode.y();
    int usedBottom = usedNode.y() + usedNode.height();

    int freeLeft = freeNode.x();
    int freeRight = freeNode.x() + freeNode.width();
    int freeTop = freeNode.y();
    int freeBottom = freeNode.y() + freeNode.height();

    // Test if nodes do not intersect at all
    if (usedLeft >= freeRight || usedRight <= freeLeft ||
        usedTop >= freeBottom || usedBottom <= freeTop) {
        return false;
    }

    // New node at the top of used node
    if (usedTop > freeTop && usedTop < freeBottom) {
        m_freeRectangles.append(QRect(freeLeft, freeTop, freeNode.width(), usedTop - freeTop));
    }

    // New node at the bottom of used node
    if (usedBottom < freeBottom && usedBottom > freeTop) {
        m_freeRectangles.append(QRect(freeLeft, usedBottom, freeNode.width(), freeBottom - usedBottom));
    }

    // New node at the left of used node
    if (usedLeft > freeLeft && usedLeft < freeRight) {
        m_freeRectangles.append(QRect(freeLeft, freeTop, usedLeft - freeLeft, freeNode.height()));
    }

    // New node at the right of used node
    if (usedRight < freeRight && usedRight > freeLeft) {
        m_freeRectangles.append(QRect(usedRight, freeTop, freeRight - usedRight, freeNode.height()));
    }

    return true;
}

void MaxRectsPacker::pruneFreeList()
{
    for (int i = 0; i < m_freeRectangles.size(); ++i) {
        for (int j = i + 1; j < m_freeRectangles.size(); ++j) {
            if (isContainedIn(m_freeRectangles.at(i), m_freeRectangles.at(j))) {
                m_freeRectangles.removeAt(i);
                --i;
                break;
            }
            if (isContainedIn(m_freeRectangles.at(j), m_freeRectangles.at(i))) {
                m_freeRectangles.removeAt(j);
                --j;
            }
        }
    }
}

bool MaxRectsPacker::isContainedIn(const QRect &a, const QRect &b)
{
    return a.x() >= b.x() && a.y() >= b.y() &&
           (a.x() + a.width()) <= (b.x() + b.width()) &&
           (a.y() + a.height()) <= (b.y() + b.height());
}
