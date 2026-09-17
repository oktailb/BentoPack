#ifndef ATLASPACKER_H
#define ATLASPACKER_H

#include <QImage>
#include <QList>
#include <QRect>
#include <QSize>
#include "packer/maxrectspacker.h"

/**
 * @brief Represents the result of an atlas packing operation.
 */
struct AtlasPackResult {
    QImage          atlas;
    QList<QRect>    frameRects;         ///< 1:1 mapping corresponding to input frames
    QList<int>      duplicateMapping;   ///< Canonical index for each frame (for deduplication)
    QSize           dimensions;
    float           efficiency = 0.0f;  ///< Occupancy / compaction percentage (0.0% to 100.0%)
    int             uniqueFramesCount = 0;
    bool            success = false;
};

/**
 * @brief The AtlasPacker class provides advanced 2D atlas packing algorithms.
 *
 * Supports MaxRects (BSSF, BAF, BLSF, BottomLeft, ContactPoint), uniform grids,
 * row/shelf packing, power-of-two constraints, border extrusion, and visual deduplication.
 */
class AtlasPacker
{
public:
    enum Algorithm {
        RowPacker,        // Fast row-by-row shelf packer
        GridPacker,       // Uniform grid packer
        PowerOfTwoPacker, // Row packing rounded up to next power-of-two texture dimensions
        MaxRects          // Advanced 2D bin-packing with minimal wasted space
    };

    /**
     * @brief Detailed configuration options for packing.
     */
    struct PackOptions {
        Algorithm           algorithm = MaxRects;
        MaxRectsHeuristic   heuristic = MaxRectsHeuristic::BestShortSideFit;
        int                 padding = 2;            ///< Inner padding between sprites (px)
        int                 borderPadding = 0;      ///< Outer margin around atlas edges (px)
        int                 extrude = 0;            ///< Border pixel extrusion outward (px, 0-2)
        bool                powerOfTwo = false;     ///< Force dimensions to powers of two (2^n)
        bool                forceSquare = false;    ///< Force square dimensions (width == height)
        bool                deduplicate = false;    ///< Detect and share identical frames
        int                 maxWidth = 4096;
        int                 maxHeight = 4096;
    };

    /**
     * @brief Packs a list of images using comprehensive PackOptions.
     */
    static AtlasPackResult pack(const QList<QImage> &frames, const PackOptions &options);

    /**
     * @brief Backward-compatible pack method.
     */
    static AtlasPackResult pack(const QList<QImage> &frames, int padding = 2, Algorithm algo = RowPacker);

    /**
     * @brief Packs only the frames referenced by specific animation frame indices.
     */
    static AtlasPackResult packIndices(const QList<QImage> &allFrames, const QList<int> &frameIndices, int padding = 2);
    static AtlasPackResult packIndices(const QList<QImage> &allFrames, const QList<int> &frameIndices, const PackOptions &options);

    /**
     * @brief Determines whether two images have visually identical pixel contents.
     */
    static bool areImagesIdentical(const QImage &a, const QImage &b);

    /**
     * @brief Extrudes outermost pixel rows and columns outward by extrudeAmount pixels.
     */
    static void applyExtrusion(QImage &atlas, const QRect &targetRect, const QImage &sprite, int extrudeAmount);

    /**
     * @brief Computes the next power of two greater than or equal to n.
     */
    static int nextPowerOfTwo(int n);

private:
    static AtlasPackResult packRow(const QList<QImage> &uniqueFrames, const QList<int> &mapping, const PackOptions &options);
    static AtlasPackResult packGrid(const QList<QImage> &uniqueFrames, const QList<int> &mapping, const PackOptions &options);
    static AtlasPackResult packMaxRects(const QList<QImage> &uniqueFrames, const QList<int> &mapping, const PackOptions &options);
};

#endif // ATLASPACKER_H
