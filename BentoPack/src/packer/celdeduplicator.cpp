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

#include "packer/celdeduplicator.h"
#include "image/pixelhash.h"
#include <QHash>

CelDeduplicationResult CelDeduplicator::analyze(const SpriteDocument &doc)
{
    CelDeduplicationResult result;
    const int frameCount = doc.frameCount();
    if (frameCount == 0) {
        return result;
    }

    QHash<quint64, QList<int>> hashToUniqueIndex;

    for (int f = 0; f < frameCount; ++f) {
        const QList<SpriteCel> cels = doc.frameCels(f);
        for (const SpriteCel &c : cels) {
            if (c.isNull()) {
                continue;
            }

            result.totalCelsCount++;
            result.totalPixelArea += static_cast<qint64>(c.image.width()) * c.image.height();

            quint64 h = PixelHash::hashCel(c);
            int matchedUniqueIdx = -1;

            if (hashToUniqueIndex.contains(h)) {
                for (int candidateIdx : hashToUniqueIndex.value(h)) {
                    if (PixelHash::areCelsIdentical(c, result.uniqueCels[candidateIdx].cel)) {
                        matchedUniqueIdx = candidateIdx;
                        break;
                    }
                }
            }

            if (matchedUniqueIdx >= 0) {
                result.uniqueCels[matchedUniqueIdx].occurrences.append(qMakePair(f, c.layerIndex));
                result.celMapping[qMakePair(f, c.layerIndex)] = matchedUniqueIdx;
            } else {
                int newIdx = result.uniqueCels.size();
                UniqueCelInfo uinfo;
                uinfo.uniqueId = newIdx;
                uinfo.cel = c;
                uinfo.hash = h;
                uinfo.occurrences.append(qMakePair(f, c.layerIndex));

                result.uniqueCels.append(uinfo);
                result.celMapping[qMakePair(f, c.layerIndex)] = newIdx;
                hashToUniqueIndex[h].append(newIdx);

                result.uniquePixelArea += static_cast<qint64>(c.image.width()) * c.image.height();
            }
        }
    }

    result.uniqueCelsCount = result.uniqueCels.size();
    if (result.totalPixelArea > 0) {
        qint64 saved = result.totalPixelArea - result.uniquePixelArea;
        result.savingsPercentage = (static_cast<float>(saved) / static_cast<float>(result.totalPixelArea)) * 100.0f;
    }

    return result;
}

AtlasPackResult CelDeduplicator::packModularCels(const SpriteDocument &doc,
                                                const AtlasPacker::PackOptions &options,
                                                CelDeduplicationResult *outDedupResult)
{
    CelDeduplicationResult dedup = analyze(doc);
    if (outDedupResult) {
        *outDedupResult = dedup;
    }

    if (dedup.uniqueCels.isEmpty()) {
        return AtlasPackResult();
    }

    QList<QImage> uniqueImages;
    uniqueImages.reserve(dedup.uniqueCels.size());
    for (const UniqueCelInfo &u : dedup.uniqueCels) {
        uniqueImages.append(u.cel.image);
    }

    AtlasPacker::PackOptions packOpts = options;
    packOpts.deduplicate = false; // Already deduplicated at cel level

    AtlasPackResult packRes = AtlasPacker::pack(uniqueImages, packOpts);
    packRes.uniqueFramesCount = dedup.uniqueCelsCount;
    return packRes;
}
