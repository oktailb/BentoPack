/**
 Licensed to the Apache Software Foundation (ASF) under one
 or more contributor license agreements.  See the NOTICE file
 distributed with this work for additional information
 regarding copyright ownership.  The ASF licenses this file
 to you under the Apache License, Version 2.0 (the
 "License"); you may not use this file except in compliance
 with the License.  You may obtain a copy of the License at

 http://www.apache.org/licenses/LICENSE-2.0

 Unless required by applicable law or agreed to in writing,
 software distributed under the License is distributed on an
 "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 KIND, either express or implied.  See the License for the
 specific language governing permissions and limitations
 under the License.
*/

#ifndef MAXRECTSPACKER_H
#define MAXRECTSPACKER_H

#include <QList>
#include <QRect>
#include <QSize>

/**
 * @brief Heuristic rules for choosing the best candidate free rectangle in MaxRects.
 */
enum class MaxRectsHeuristic {
    BestShortSideFit, ///< Minimizes the smaller leftover dimension (BSSF, recommended default)
    BestAreaFit,      ///< Minimizes the unused area in the free rectangle (BAF)
    BestLongSideFit,  ///< Minimizes the larger leftover dimension (BLSF)
    BottomLeft,       ///< Places the rectangle as low and as far left as possible (BL)
    ContactPoint      ///< Maximizes the perimeter contact with placed rectangles and boundaries (CP)
};

/**
 * @brief The MaxRectsPacker class implements the 2D MaxRects bin-packing algorithm.
 *
 * It maintains a list of maximal free rectangles, splits intersecting free spaces
 * upon insertion, and prunes contained rectangles to achieve optimal packing density.
 */
class MaxRectsPacker
{
public:
    MaxRectsPacker();
    MaxRectsPacker(int width, int height);

    /**
     * @brief Initializes or resets the bin with given dimensions.
     */
    void init(int width, int height);

    int binWidth() const { return m_binWidth; }
    int binHeight() const { return m_binHeight; }

    /**
     * @brief Inserts a single rectangle of size (width, height) using the given heuristic.
     * @return The positioned QRect in the bin, or an invalid QRect if it could not fit.
     */
    QRect insert(int width, int height, MaxRectsHeuristic heuristic = MaxRectsHeuristic::BestShortSideFit);

    /**
     * @brief Inserts a batch of rectangles, sorting them first for maximum packing efficiency.
     * @param rectSizes Sizes of rectangles to pack.
     * @param heuristic The placement heuristic.
     * @return List of placed rectangles corresponding 1:1 by index to rectSizes.
     */
    QList<QRect> insert(const QList<QSize> &rectSizes, MaxRectsHeuristic heuristic = MaxRectsHeuristic::BestShortSideFit);

    /**
     * @brief Computes the occupancy ratio (used area / total bin area) in range [0.0, 1.0].
     */
    float occupancy() const;

    const QList<QRect>& usedRectangles() const { return m_usedRectangles; }
    const QList<QRect>& freeRectangles() const { return m_freeRectangles; }

private:
    QRect findPositionForNewNodeBestShortSideFit(int width, int height, int &bestShortSideFit, int &bestLongSideFit) const;
    QRect findPositionForNewNodeBestAreaFit(int width, int height, int &bestAreaFit, int &bestShortSideFit) const;
    QRect findPositionForNewNodeBestLongSideFit(int width, int height, int &bestLongSideFit, int &bestShortSideFit) const;
    QRect findPositionForNewNodeBottomLeft(int width, int height, int &bestY, int &bestX) const;
    QRect findPositionForNewNodeContactPoint(int width, int height, int &bestContactScore) const;

    int contactPointScoreNode(int x, int y, int width, int height) const;

    bool splitFreeNode(const QRect &freeNode, const QRect &usedNode);
    void pruneFreeList();

    static bool isContainedIn(const QRect &a, const QRect &b);

    int m_binWidth = 0;
    int m_binHeight = 0;

    QList<QRect> m_usedRectangles;
    QList<QRect> m_freeRectangles;
};

#endif // MAXRECTSPACKER_H
