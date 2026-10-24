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

#include "iffextractor.h"
#include "image/spritedetector.h"
#include <QFile>
#include <QFileInfo>
#include <QtEndian>
#include <cstring>
#include <algorithm>

IffExtractor::IffExtractor(QObject *parent)
    : Extractor(parent)
{
}

bool IffExtractor::canDecode(const QString &filePath) const
{
    QFileInfo fi(filePath);
    if (!supportedExtensions().contains(fi.suffix().toLower())) {
        return false;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    uchar hdr[12];
    if (file.read(reinterpret_cast<char*>(hdr), 12) != 12) {
        return false;
    }

    // Must be IFF FORM container
    if (std::memcmp(hdr, "FORM", 4) != 0) {
        return false;
    }

    // Must be ILBM (InterLeaved BitMap) or PBM (Packed BitMap)
    return (std::memcmp(hdr + 8, "ILBM", 4) == 0 || std::memcmp(hdr + 8, "PBM ", 4) == 0);
}

QByteArray IffExtractor::byteRun1Compress(const uchar *src, int srcLen)
{
    QByteArray out;
    out.reserve(srcLen + srcLen / 8 + 32);

    int i = 0;
    while (i < srcLen) {
        // Detect run of identical bytes
        int runLen = 1;
        while (i + runLen < srcLen && src[i + runLen] == src[i] && runLen < 128) {
            runLen++;
        }

        if (runLen >= 3) {
            // Replicated run: count is -(runLen - 1)
            signed char n = static_cast<signed char>(-(runLen - 1));
            out.append(static_cast<char>(n));
            out.append(static_cast<char>(src[i]));
            i += runLen;
        } else {
            // Literal sequence: advance until a run of >= 3 identical bytes is encountered
            int litLen = 0;
            while (i + litLen < srcLen && litLen < 128) {
                if (i + litLen + 2 < srcLen &&
                    src[i + litLen] == src[i + litLen + 1] &&
                    src[i + litLen] == src[i + litLen + 2]) {
                    break;
                }
                litLen++;
            }
            if (litLen == 0) litLen = 1;
            signed char n = static_cast<signed char>(litLen - 1);
            out.append(static_cast<char>(n));
            out.append(reinterpret_cast<const char*>(src + i), litLen);
            i += litLen;
        }
    }
    return out;
}

bool IffExtractor::byteRun1Decompress(const uchar *src, int srcLen, uchar *dst, int dstLen)
{
    int s = 0;
    int d = 0;
    while (s < srcLen && d < dstLen) {
        signed char n = static_cast<signed char>(src[s++]);
        if (n >= 0) {
            int count = n + 1;
            count = std::min(count, std::min(srcLen - s, dstLen - d));
            std::memcpy(dst + d, src + s, count);
            s += count;
            d += count;
        } else if (n != -128) {
            int count = -n + 1;
            if (s >= srcLen) break;
            uchar val = src[s++];
            count = std::min(count, dstLen - d);
            std::memset(dst + d, val, count);
            d += count;
        }
    }
    return (d == dstLen);
}

bool IffExtractor::decodeIff(const QByteArray &data, QImage &outImage, QString *errorMessage)
{
    if (data.size() < 12) {
        if (errorMessage) *errorMessage = QStringLiteral("File too small for IFF FORM header (< 12 bytes).");
        return false;
    }

    const uchar *raw = reinterpret_cast<const uchar*>(data.constData());
    if (std::memcmp(raw, "FORM", 4) != 0) {
        if (errorMessage) *errorMessage = QStringLiteral("Invalid IFF signature: missing 'FORM'.");
        return false;
    }

    bool isPBM = (std::memcmp(raw + 8, "PBM ", 4) == 0);
    bool isILBM = (std::memcmp(raw + 8, "ILBM", 4) == 0);
    if (!isILBM && !isPBM) {
        if (errorMessage) *errorMessage = QStringLiteral("Unsupported IFF type (expected ILBM or PBM).");
        return false;
    }

    // BitMapHeader fields
    int width = 0;
    int height = 0;
    int nPlanes = 0;
    int masking = 0;
    int compression = 0;
    int transparentColor = 0;
    bool hasBMHD = false;

    // Palette & Amiga mode
    QVector<QRgb> palette;
    quint32 camgMode = 0;
    bool hasCAMG = false;

    // BODY pointer
    const uchar *bodyData = nullptr;
    int bodySize = 0;

    int offset = 12;
    int totalSize = data.size();

    while (offset + 8 <= totalSize) {
        char chunkId[5] = {0};
        std::memcpy(chunkId, raw + offset, 4);
        quint32 chunkLen = qFromBigEndian<quint32>(raw + offset + 4);
        const uchar *chunkData = raw + offset + 8;

        if (offset + 8 + static_cast<int>(chunkLen) > totalSize) {
            chunkLen = totalSize - (offset + 8);
        }

        if (std::memcmp(chunkId, "BMHD", 4) == 0 && chunkLen >= 20) {
            hasBMHD = true;
            width = qFromBigEndian<quint16>(chunkData);
            height = qFromBigEndian<quint16>(chunkData + 2);
            nPlanes = chunkData[8];
            masking = chunkData[9];
            compression = chunkData[10];
            transparentColor = qFromBigEndian<quint16>(chunkData + 12);
        } else if (std::memcmp(chunkId, "CMAP", 4) == 0) {
            int numColors = chunkLen / 3;
            palette.clear();
            palette.reserve(numColors);
            for (int i = 0; i < numColors; ++i) {
                palette.append(qRgb(chunkData[i * 3], chunkData[i * 3 + 1], chunkData[i * 3 + 2]));
            }
        } else if (std::memcmp(chunkId, "CAMG", 4) == 0 && chunkLen >= 4) {
            camgMode = qFromBigEndian<quint32>(chunkData);
            hasCAMG = true;
        } else if (std::memcmp(chunkId, "BODY", 4) == 0) {
            bodyData = chunkData;
            bodySize = chunkLen;
        }

        // Advance to next chunk (padded to even byte)
        offset += 8 + chunkLen + (chunkLen & 1);
    }

    if (!hasBMHD) {
        if (errorMessage) *errorMessage = QStringLiteral("Missing mandatory BMHD (BitMapHeader) chunk.");
        return false;
    }
    if (width <= 0 || height <= 0 || width > 16384 || height > 16384) {
        if (errorMessage) *errorMessage = QStringLiteral("Invalid image dimensions in BMHD.");
        return false;
    }
    if (!bodyData || bodySize <= 0) {
        if (errorMessage) *errorMessage = QStringLiteral("Missing or empty BODY chunk.");
        return false;
    }

    // Determine special Amiga display modes:
    // EHB (Extra Half-Brite): 6 planes, 32 colors in CMAP -> colors 32..63 are halved
    bool isEHB = false;
    bool isHAM = false;

    if (hasCAMG) {
        if (camgMode & 0x0080) isEHB = true;
        if (camgMode & 0x0800) isHAM = true;
    } else {
        if (nPlanes == 6 && palette.size() == 32) {
            isEHB = true;
        }
    }

    if (isEHB && palette.size() >= 32) {
        int baseCount = std::min(static_cast<int>(palette.size()), 32);
        for (int i = 0; i < baseCount; ++i) {
            QRgb c = palette[i];
            palette.append(qRgb(qRed(c) / 2, qGreen(c) / 2, qBlue(c) / 2));
        }
    }

    // Default grayscale palette if no CMAP found
    if (palette.isEmpty() && nPlanes <= 8) {
        int numColors = 1 << nPlanes;
        for (int i = 0; i < numColors; ++i) {
            int val = (numColors > 1) ? (i * 255 / (numColors - 1)) : 0;
            palette.append(qRgb(val, val, val));
        }
    }

    outImage = QImage(width, height, QImage::Format_ARGB32);
    outImage.fill(Qt::transparent);

    if (isPBM) {
        // PBM: Chunky 8-bit pixels, rowBytes is even
        int rowBytes = (width + 1) & ~1;
        int totalUnpacked = rowBytes * height;
        QByteArray decompressed;
        const uchar *pixelSrc = bodyData;

        if (compression == 1) {
            decompressed.resize(totalUnpacked);
            byteRun1Decompress(bodyData, bodySize, reinterpret_cast<uchar*>(decompressed.data()), totalUnpacked);
            pixelSrc = reinterpret_cast<const uchar*>(decompressed.constData());
        }

        for (int y = 0; y < height; ++y) {
            const uchar *line = pixelSrc + y * rowBytes;
            QRgb *dst = reinterpret_cast<QRgb*>(outImage.scanLine(y));
            for (int x = 0; x < width; ++x) {
                uchar idx = line[x];
                if (masking == 2 && idx == transparentColor) {
                    dst[x] = qRgba(0, 0, 0, 0);
                } else {
                    dst[x] = (idx < palette.size()) ? palette[idx] : qRgb(0, 0, 0);
                }
            }
        }
        return true;
    }

    // ILBM: Interleaved planar bitplanes
    int rowBytes = ((width + 15) / 16) * 2;
    int planesPerLine = nPlanes + (masking == 1 ? 1 : 0);
    int lineBytes = rowBytes * planesPerLine;
    int totalUnpacked = lineBytes * height;

    QByteArray decompressed;
    const uchar *unpacked = bodyData;

    if (compression == 1) {
        decompressed.resize(totalUnpacked);
        byteRun1Decompress(bodyData, bodySize, reinterpret_cast<uchar*>(decompressed.data()), totalUnpacked);
        unpacked = reinterpret_cast<const uchar*>(decompressed.constData());
    }

    for (int y = 0; y < height; ++y) {
        const uchar *lineStart = unpacked + y * lineBytes;
        QRgb *dst = reinterpret_cast<QRgb*>(outImage.scanLine(y));

        QVector<const uchar*> planePtrs(nPlanes);
        for (int p = 0; p < nPlanes; ++p) {
            planePtrs[p] = lineStart + p * rowBytes;
        }
        const uchar *maskPtr = (masking == 1) ? (lineStart + nPlanes * rowBytes) : nullptr;

        QRgb lastHamColor = palette.isEmpty() ? qRgb(0, 0, 0) : palette[0];

        for (int x = 0; x < width; ++x) {
            int byteIdx = x >> 3;
            int bitMask = 1 << (7 - (x & 7));

            if (nPlanes == 24) {
                // 24-bit TrueColor ILBM (8 planes R, 8 planes G, 8 planes B)
                quint8 r = 0, g = 0, b = 0;
                for (int p = 0; p < 8; ++p)   if (planePtrs[p][byteIdx] & bitMask)      r |= (1 << p);
                for (int p = 8; p < 16; ++p)  if (planePtrs[p][byteIdx] & bitMask)      g |= (1 << (p - 8));
                for (int p = 16; p < 24; ++p) if (planePtrs[p][byteIdx] & bitMask)      b |= (1 << (p - 16));
                dst[x] = qRgb(r, g, b);
            } else if (isHAM && (nPlanes == 6 || nPlanes == 8)) {
                // HAM6 / HAM8 decoding
                int ctrl = 0;
                int dataVal = 0;
                if (nPlanes == 6) {
                    // Planes 4 and 5 are control bits
                    if (planePtrs[4][byteIdx] & bitMask) ctrl |= 1;
                    if (planePtrs[5][byteIdx] & bitMask) ctrl |= 2;
                    for (int p = 0; p < 4; ++p) {
                        if (planePtrs[p][byteIdx] & bitMask) dataVal |= (1 << p);
                    }
                    if (ctrl == 0) {
                        lastHamColor = (dataVal < palette.size()) ? palette[dataVal] : qRgb(0, 0, 0);
                    } else if (ctrl == 1) { // Modify Blue
                        quint8 b = (dataVal << 4) | dataVal;
                        lastHamColor = qRgb(qRed(lastHamColor), qGreen(lastHamColor), b);
                    } else if (ctrl == 2) { // Modify Red
                        quint8 r = (dataVal << 4) | dataVal;
                        lastHamColor = qRgb(r, qGreen(lastHamColor), qBlue(lastHamColor));
                    } else { // Modify Green
                        quint8 g = (dataVal << 4) | dataVal;
                        lastHamColor = qRgb(qRed(lastHamColor), g, qBlue(lastHamColor));
                    }
                } else {
                    // HAM8: planes 6 and 7 are control bits
                    if (planePtrs[6][byteIdx] & bitMask) ctrl |= 1;
                    if (planePtrs[7][byteIdx] & bitMask) ctrl |= 2;
                    for (int p = 0; p < 6; ++p) {
                        if (planePtrs[p][byteIdx] & bitMask) dataVal |= (1 << p);
                    }
                    if (ctrl == 0) {
                        lastHamColor = (dataVal < palette.size()) ? palette[dataVal] : qRgb(0, 0, 0);
                    } else if (ctrl == 1) { // Modify Blue
                        quint8 b = (dataVal << 2) | (dataVal >> 4);
                        lastHamColor = qRgb(qRed(lastHamColor), qGreen(lastHamColor), b);
                    } else if (ctrl == 2) { // Modify Red
                        quint8 r = (dataVal << 2) | (dataVal >> 4);
                        lastHamColor = qRgb(r, qGreen(lastHamColor), qBlue(lastHamColor));
                    } else { // Modify Green
                        quint8 g = (dataVal << 2) | (dataVal >> 4);
                        lastHamColor = qRgb(qRed(lastHamColor), g, qBlue(lastHamColor));
                    }
                }
                dst[x] = lastHamColor;
            } else {
                // Standard planar indexed
                quint8 idx = 0;
                for (int p = 0; p < nPlanes; ++p) {
                    if (planePtrs[p][byteIdx] & bitMask) {
                        idx |= (1 << p);
                    }
                }

                if (masking == 1) {
                    bool opaque = (maskPtr[byteIdx] & bitMask) != 0;
                    dst[x] = opaque ? ((idx < palette.size()) ? palette[idx] : qRgb(0, 0, 0)) : qRgba(0, 0, 0, 0);
                } else if (masking == 2 && idx == transparentColor) {
                    dst[x] = qRgba(0, 0, 0, 0);
                } else {
                    dst[x] = (idx < palette.size()) ? palette[idx] : qRgb(0, 0, 0);
                }
            }
        }
    }

    return true;
}

bool IffExtractor::encodeIff(const QImage &image, QByteArray &outData, QString *errorMessage)
{
    if (image.isNull()) {
        if (errorMessage) *errorMessage = QStringLiteral("Cannot encode null image to IFF.");
        return false;
    }

    int width = image.width();
    int height = image.height();
    if (width <= 0 || height <= 0 || width > 16384 || height > 16384) {
        if (errorMessage) *errorMessage = QStringLiteral("Image dimensions out of bounds for IFF.");
        return false;
    }

    // Convert to indexed format
    QImage indexed = image;
    if (indexed.format() != QImage::Format_Indexed8) {
        indexed = image.convertToFormat(QImage::Format_Indexed8, Qt::AutoColor);
    }

    QList<QRgb> colorTable = indexed.colorTable();
    int maxIndex = 0;
    for (int y = 0; y < height; ++y) {
        const uchar *line = indexed.constScanLine(y);
        for (int x = 0; x < width; ++x) {
            if (line[x] > maxIndex) maxIndex = line[x];
        }
    }

    // Choose minimal bitplanes (at least 5 for Amiga standard 32 colors or 8 for 256 colors)
    int nPlanes = 5;
    if (maxIndex >= 32) nPlanes = 8;
    int numColors = 1 << nPlanes;

    while (colorTable.size() < numColors) {
        colorTable.append(qRgb(0, 0, 0));
    }

    // Check transparency
    bool hasAlpha = image.hasAlphaChannel();
    bool hasTransparentPixels = false;
    if (hasAlpha) {
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                if (qAlpha(image.pixel(x, y)) < 128) {
                    hasTransparentPixels = true;
                    break;
                }
            }
            if (hasTransparentPixels) break;
        }
    }

    quint8 masking = hasTransparentPixels ? 1 : 0; // 1 = mskHasMask
    int rowBytes = ((width + 15) / 16) * 2;
    int planesPerLine = nPlanes + (masking == 1 ? 1 : 0);
    int lineBytes = rowBytes * planesPerLine;

    // Build raw BODY data with bitplanes
    QByteArray rawBody;
    rawBody.reserve(lineBytes * height);

    QByteArray lineBuffer(lineBytes, 0);

    for (int y = 0; y < height; ++y) {
        lineBuffer.fill(0);
        const uchar *srcLine = indexed.constScanLine(y);

        for (int x = 0; x < width; ++x) {
            uchar idx = srcLine[x];
            int byteIdx = x >> 3;
            int bitMask = 1 << (7 - (x & 7));

            for (int p = 0; p < nPlanes; ++p) {
                if (idx & (1 << p)) {
                    lineBuffer.data()[p * rowBytes + byteIdx] |= bitMask;
                }
            }

            if (masking == 1) {
                bool opaque = (qAlpha(image.pixel(x, y)) >= 128);
                if (opaque) {
                    lineBuffer.data()[nPlanes * rowBytes + byteIdx] |= bitMask;
                }
            }
        }

        // Compress scanline with ByteRun1
        QByteArray compLine = byteRun1Compress(reinterpret_cast<const uchar*>(lineBuffer.constData()), lineBytes);
        rawBody.append(compLine);
    }

    // 1. BMHD chunk (20 bytes)
    QByteArray bmhd(20, 0);
    uchar *bPtr = reinterpret_cast<uchar*>(bmhd.data());
    qToBigEndian<quint16>(width, bPtr);
    qToBigEndian<quint16>(height, bPtr + 2);
    qToBigEndian<qint16>(0, bPtr + 4); // x
    qToBigEndian<qint16>(0, bPtr + 6); // y
    bPtr[8] = static_cast<uchar>(nPlanes);
    bPtr[9] = masking;
    bPtr[10] = 1; // cmpByteRun1
    bPtr[11] = 0; // pad
    qToBigEndian<quint16>(0, bPtr + 12); // transparentColor
    bPtr[14] = 10; // xAspect (PAL standard)
    bPtr[15] = 11; // yAspect
    qToBigEndian<qint16>(width, bPtr + 16); // pageWidth
    qToBigEndian<qint16>(height, bPtr + 18); // pageHeight

    // 2. CMAP chunk (numColors * 3 bytes)
    QByteArray cmap;
    cmap.reserve(numColors * 3);
    for (int i = 0; i < numColors; ++i) {
        cmap.append(static_cast<char>(qRed(colorTable[i])));
        cmap.append(static_cast<char>(qGreen(colorTable[i])));
        cmap.append(static_cast<char>(qBlue(colorTable[i])));
    }

    // 3. CAMG chunk (4 bytes)
    QByteArray camg(4, 0);
    qToBigEndian<quint32>(0, reinterpret_cast<uchar*>(camg.data()));

    // Total FORM payload size: 4 (ILBM) + (8 + 20) + (8 + cmap.size() + pad) + (8 + 4) + (8 + rawBody.size() + pad)
    int cmapPad = cmap.size() & 1;
    int bodyPad = rawBody.size() & 1;

    quint32 formLength = 4
        + 8 + bmhd.size()
        + 8 + cmap.size() + cmapPad
        + 8 + camg.size()
        + 8 + rawBody.size() + bodyPad;

    outData.clear();
    outData.reserve(formLength + 8);

    // FORM header
    outData.append("FORM", 4);
    char fLenBytes[4];
    qToBigEndian<quint32>(formLength, reinterpret_cast<uchar*>(fLenBytes));
    outData.append(fLenBytes, 4);
    outData.append("ILBM", 4);

    // BMHD chunk
    outData.append("BMHD", 4);
    char bmhdLen[4];
    qToBigEndian<quint32>(bmhd.size(), reinterpret_cast<uchar*>(bmhdLen));
    outData.append(bmhdLen, 4);
    outData.append(bmhd);

    // CMAP chunk
    outData.append("CMAP", 4);
    char cmapLen[4];
    qToBigEndian<quint32>(cmap.size(), reinterpret_cast<uchar*>(cmapLen));
    outData.append(cmapLen, 4);
    outData.append(cmap);
    if (cmapPad) outData.append('\0');

    // CAMG chunk
    outData.append("CAMG", 4);
    char camgLen[4];
    qToBigEndian<quint32>(camg.size(), reinterpret_cast<uchar*>(camgLen));
    outData.append(camgLen, 4);
    outData.append(camg);

    // BODY chunk
    outData.append("BODY", 4);
    char bodyLen[4];
    qToBigEndian<quint32>(rawBody.size(), reinterpret_cast<uchar*>(bodyLen));
    outData.append(bodyLen, 4);
    outData.append(rawBody);
    if (bodyPad) outData.append('\0');

    return true;
}

bool IffExtractor::read(const QString &filePath, SpriteDocument &outDoc, ExtractorError *error)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error) {
            error->code = ExtractorError::FileNotFound;
            error->message = tr("Cannot open IFF file: %1").arg(file.errorString());
            error->filePath = filePath;
        }
        return false;
    }

    QByteArray data = file.readAll();
    file.close();

    QString errStr;
    QImage decodedImage;
    if (!decodeIff(data, decodedImage, &errStr)) {
        if (error) {
            error->code = ExtractorError::CorruptedData;
            error->message = tr("Failed to decode IFF image: %1").arg(errStr);
            error->filePath = filePath;
        }
        return false;
    }

    QList<QImage> frameImages;
    QList<SpriteBox> boxes;

    SpriteDetectionOptions detOpts;
    detOpts.alphaThreshold = 1;
    detOpts.minSliceSize = 3;

    if (!SpriteDetector::detectToImages(decodedImage, frameImages, boxes, detOpts)) {
        SpriteBox singleBox(QRect(0, 0, decodedImage.width(), decodedImage.height()));
        boxes.append(singleBox);
        frameImages.append(decodedImage);
    }

    outDoc.setAtlas(decodedImage);
    outDoc.setFrames(frameImages, boxes);
    outDoc.setFilePath(filePath);
    return true;
}

bool IffExtractor::write(const QString &filePath, const SpriteDocument &inDoc, const ExportOptions &options, ExtractorError *error)
{
    Q_UNUSED(options);

    if (inDoc.atlas().isNull()) {
        if (error) {
            error->code = ExtractorError::WriteFailed;
            error->message = tr("Cannot export IFF: Document atlas image is null.");
            error->filePath = filePath;
        }
        return false;
    }

    QByteArray iffData;
    QString errStr;
    if (!encodeIff(inDoc.atlas(), iffData, &errStr)) {
        if (error) {
            error->code = ExtractorError::WriteFailed;
            error->message = tr("Failed to encode IFF: %1").arg(errStr);
            error->filePath = filePath;
        }
        return false;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error) {
            error->code = ExtractorError::FileNotWritable;
            error->message = tr("Cannot open file for writing: %1").arg(file.errorString());
            error->filePath = filePath;
        }
        return false;
    }

    if (file.write(iffData) != iffData.size()) {
        if (error) {
            error->code = ExtractorError::WriteFailed;
            error->message = tr("Failed to write all IFF bytes to disk.");
            error->filePath = filePath;
        }
        return false;
    }

    return true;
}
