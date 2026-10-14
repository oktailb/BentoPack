/**
 * Copyright (c) 2026 Vincent LECOQ
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "packer/multipagepacker.h"
#include "model/spritedocument.h"
#include <QPainter>
#include <algorithm>

QList<MultiPagePacker::FrameCluster> MultiPagePacker::buildClusters(int frameCount, const QMap<QString, SpriteAnimation> &animations)
{
    QList<FrameCluster> clusters;
    QList<bool> assigned(frameCount, false);

    // 1. First pass: group by named animations (preserve animation cohesion)
    for (auto it = animations.cbegin(); it != animations.cend(); ++it) {
        const SpriteAnimation &anim = it.value();
        FrameCluster cluster;
        cluster.name = it.key();

        for (int idx : anim.frameIndices) {
            if (idx >= 0 && idx < frameCount) {
                if (!cluster.frameIndices.contains(idx)) {
                    cluster.frameIndices.append(idx);
                }
                assigned[idx] = true;
            }
        }

        if (!cluster.frameIndices.isEmpty()) {
            clusters.append(cluster);
        }
    }

    // 2. Second pass: standalone / orphan frames not belonging to any animation
    FrameCluster orphanCluster;
    orphanCluster.name = QStringLiteral("__orphans__");
    for (int i = 0; i < frameCount; ++i) {
        if (!assigned[i]) {
            orphanCluster.frameIndices.append(i);
        }
    }
    if (!orphanCluster.frameIndices.isEmpty()) {
        clusters.append(orphanCluster);
    }

    return clusters;
}

bool MultiPagePacker::canFitInSinglePage(const QList<QImage> &frames, const AtlasPacker::PackOptions &options)
{
    if (frames.isEmpty()) return true;

    long long totalArea = 0;
    int pad = std::max(0, options.padding);
    int extrude = std::max(0, options.extrude);

    for (const QImage &img : frames) {
        if (img.isNull()) continue;
        int w = img.width() + (pad + extrude) * 2;
        int h = img.height() + (pad + extrude) * 2;
        if (w > options.maxWidth || h > options.maxHeight) {
            return false; // A single frame exceeds page limits
        }
        totalArea += static_cast<long long>(w) * h;
    }

    long long maxPageArea = static_cast<long long>(options.maxWidth) * options.maxHeight;
    // With bin packing overhead (typically 15-25% waste), if total area exceeds 85% of page area, multi-page is probable
    return totalArea <= static_cast<long long>(maxPageArea * 0.85);
}

MultiPagePackResult MultiPagePacker::packFrames(const QList<QImage> &frames,
                                                const QMap<QString, SpriteAnimation> &animations,
                                                const AtlasPacker::PackOptions &options,
                                                const QList<QPolygonF> &polygons)
{
    MultiPagePackResult result;
    const int frameCount = frames.size();
    if (frameCount == 0) {
        return result;
    }

    // Initialize mapping arrays
    result.framePageIndices.resize(frameCount);
    result.frameRects.resize(frameCount);
    result.framePageIndices.fill(-1);

    // 1. Fast Path: attempt single-page pack first
    if (canFitInSinglePage(frames, options)) {
        AtlasPackResult singleResult = AtlasPacker::pack(frames, options, polygons);
        if (singleResult.success && singleResult.dimensions.width() <= options.maxWidth &&
            singleResult.dimensions.height() <= options.maxHeight) {
            AtlasPage page;
            page.pageIndex = 0;
            page.atlas = singleResult.atlas;
            page.dimensions = singleResult.dimensions;
            page.efficiency = singleResult.efficiency;
            for (int i = 0; i < frameCount; ++i) {
                page.frameIndices.append(i);
                page.frameRects.append(singleResult.frameRects.value(i));
                result.framePageIndices[i] = 0;
                result.frameRects[i] = singleResult.frameRects.value(i);
            }
            result.pages.append(page);
            result.duplicateMapping = singleResult.duplicateMapping;
            result.overallEfficiency = singleResult.efficiency;
            result.success = true;
            return result;
        }
    }

    // 2. Multi-Page Spanning Path with Semantic Animation Cohesion
    QList<FrameCluster> clusters = buildClusters(frameCount, animations);

    struct PageCandidate {
        QList<int> frameIndices;
    };

    QList<PageCandidate> pageCandidates;
    PageCandidate currentPage;

    auto testFit = [&](const QList<int> &testIndices) -> bool {
        if (testIndices.isEmpty()) return true;
        QList<QImage> testFrames;
        testFrames.reserve(testIndices.size());
        QList<QPolygonF> testPolys;
        testPolys.reserve(testIndices.size());

        for (int idx : testIndices) {
            testFrames.append(frames[idx]);
            if (idx < polygons.size()) {
                testPolys.append(polygons[idx]);
            } else {
                testPolys.append(QPolygonF());
            }
        }

        AtlasPackResult testRes = AtlasPacker::pack(testFrames, options, testPolys);
        return testRes.success &&
               testRes.dimensions.width() <= options.maxWidth &&
               testRes.dimensions.height() <= options.maxHeight;
    };

    // Iterate over clusters
    for (const FrameCluster &cluster : clusters) {
        // Test if the entire cluster can fit into currentPage
        QList<int> combinedIndices = currentPage.frameIndices;
        combinedIndices.append(cluster.frameIndices);

        if (!currentPage.frameIndices.isEmpty() && testFit(combinedIndices)) {
            // Fits together on current page!
            currentPage.frameIndices = combinedIndices;
        } else if (currentPage.frameIndices.isEmpty()) {
            // Current page is empty, test if cluster fits alone
            if (testFit(cluster.frameIndices)) {
                currentPage.frameIndices = cluster.frameIndices;
            } else {
                // Cluster is larger than an entire page: split sequentially across pages
                for (int idx : cluster.frameIndices) {
                    QList<int> step = currentPage.frameIndices;
                    step.append(idx);
                    if (testFit(step)) {
                        currentPage.frameIndices.append(idx);
                    } else {
                        if (!currentPage.frameIndices.isEmpty()) {
                            pageCandidates.append(currentPage);
                            currentPage.frameIndices.clear();
                        }
                        currentPage.frameIndices.append(idx);
                    }
                }
            }
        } else {
            // Doesn't fit in current non-empty page: finalize current page and start a new one
            pageCandidates.append(currentPage);
            currentPage.frameIndices.clear();

            // Retest cluster on fresh new page
            if (testFit(cluster.frameIndices)) {
                currentPage.frameIndices = cluster.frameIndices;
            } else {
                // Cluster alone is too big for a single page: split sequentially
                for (int idx : cluster.frameIndices) {
                    QList<int> step = currentPage.frameIndices;
                    step.append(idx);
                    if (testFit(step)) {
                        currentPage.frameIndices.append(idx);
                    } else {
                        if (!currentPage.frameIndices.isEmpty()) {
                            pageCandidates.append(currentPage);
                            currentPage.frameIndices.clear();
                        }
                        currentPage.frameIndices.append(idx);
                    }
                }
            }
        }
    }

    if (!currentPage.frameIndices.isEmpty()) {
        pageCandidates.append(currentPage);
    }

    // 3. Finalize all pages: execute actual packing for each page candidate
    long long totalUsedPixelArea = 0;
    long long totalAllocatedArea = 0;

    for (int pIdx = 0; pIdx < pageCandidates.size(); ++pIdx) {
        const PageCandidate &cand = pageCandidates[pIdx];
        QList<QImage> pageFrames;
        pageFrames.reserve(cand.frameIndices.size());
        QList<QPolygonF> pagePolys;
        pagePolys.reserve(cand.frameIndices.size());

        for (int fIdx : cand.frameIndices) {
            pageFrames.append(frames[fIdx]);
            if (fIdx < polygons.size()) {
                pagePolys.append(polygons[fIdx]);
            } else {
                pagePolys.append(QPolygonF());
            }
        }

        AtlasPackResult packRes = AtlasPacker::pack(pageFrames, options, pagePolys);
        if (!packRes.success) {
            result.success = false;
            return result;
        }

        AtlasPage page;
        page.pageIndex = pIdx;
        page.atlas = packRes.atlas;
        page.dimensions = packRes.dimensions;
        page.efficiency = packRes.efficiency;
        page.frameIndices = cand.frameIndices;
        page.frameRects = packRes.frameRects;

        for (int localIdx = 0; localIdx < cand.frameIndices.size(); ++localIdx) {
            int globalIdx = cand.frameIndices[localIdx];
            QRect r = packRes.frameRects.value(localIdx);
            result.framePageIndices[globalIdx] = pIdx;
            result.frameRects[globalIdx] = r;
            totalUsedPixelArea += static_cast<long long>(r.width()) * r.height();
        }

        totalAllocatedArea += static_cast<long long>(page.dimensions.width()) * page.dimensions.height();
        result.pages.append(page);
    }

    result.duplicateMapping.resize(frameCount);
    for (int i = 0; i < frameCount; ++i) {
        result.duplicateMapping[i] = i;
    }

    result.overallEfficiency = (totalAllocatedArea > 0)
        ? static_cast<float>((static_cast<double>(totalUsedPixelArea) / totalAllocatedArea) * 100.0)
        : 0.0f;
    result.success = !result.pages.isEmpty();

    return result;
}

MultiPagePackResult MultiPagePacker::pack(const SpriteDocument &doc,
                                          const AtlasPacker::PackOptions &options,
                                          bool exportMaterialMaps,
                                          bool normalMapYFlip)
{
    const int frameCount = doc.frameCount();
    if (frameCount == 0) {
        return MultiPagePackResult();
    }

    QList<QPolygonF> docPolygons;
    docPolygons.reserve(frameCount);
    for (int i = 0; i < frameCount; ++i) {
        docPolygons.append(doc.box(i).polygon.size() >= 3 ? doc.box(i).polygon : QPolygonF());
    }

    MultiPagePackResult result = packFrames(doc.polygonClippedFrames(), doc.animations(), options, docPolygons);
    if (!result.success) {
        return result;
    }

    // 4. M21 Multi-Atlas Secondary Maps Synchronization per Page
    if (exportMaterialMaps && doc.hasAnyAuxiliaryMaps()) {
        for (int pIdx = 0; pIdx < result.pages.size(); ++pIdx) {
            AtlasPage &page = result.pages[pIdx];

            // Extract auxiliary layers for the frames belonging to this specific page
            QList<MaterialMapType> mapTypes = {
                MaterialMapType::Normal,
                MaterialMapType::Emissive,
                MaterialMapType::Specular
            };

            for (MaterialMapType mapType : mapTypes) {
                if (!doc.hasAuxiliaryMap(mapType)) continue;

                QList<QImage> allAuxFrames = doc.framesForMap(mapType);
                if (allAuxFrames.isEmpty()) continue;

                // Create auxiliary atlas matching page dimensions
                QImage auxAtlas(page.dimensions, QImage::Format_ARGB32_Premultiplied);
                auxAtlas.fill(MultiAtlasPacker::neutralColor(mapType));

                QPainter painter(&auxAtlas);
                painter.setCompositionMode(QPainter::CompositionMode_Source);

                for (int localIdx = 0; localIdx < page.frameIndices.size(); ++localIdx) {
                    int globalIdx = page.frameIndices[localIdx];
                    QRect r = page.frameRects.value(localIdx);
                    if (globalIdx >= 0 && globalIdx < allAuxFrames.size()) {
                        QImage auxImg = allAuxFrames[globalIdx];
                        if (!auxImg.isNull()) {
                            painter.drawImage(r, auxImg);
                            if (options.extrude > 0) {
                                AtlasPacker::applyExtrusion(auxAtlas, r, auxImg, options.extrude);
                            }
                        }
                    }
                }
                painter.end();

                if (mapType == MaterialMapType::Normal && normalMapYFlip) {
                    auxAtlas = MultiAtlasPacker::flipNormalMapY(auxAtlas);
                }

                page.auxiliaryAtlases.insert(mapType, auxAtlas);
            }
        }
    }

    return result;
}
