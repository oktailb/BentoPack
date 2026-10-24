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

#include "pcxextractor.h"
#include "image/spritedetector.h"
#include <QFile>
#include <QFileInfo>
#include <QtEndian>
#include <cstring>

PcxExtractor::PcxExtractor(QObject *parent)
    : Extractor(parent)
{
}

bool PcxExtractor::canDecode(const QString &filePath) const
{
    QFileInfo fi(filePath);
    if (!supportedExtensions().contains(fi.suffix().toLower())) {
        return false;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    uchar hdr[4];
    if (file.read(reinterpret_cast<char*>(hdr), 4) != 4) {
        return false;
    }

    // PCX check: manufacturer == 0x0A, encoding == 1
    return (hdr[0] == 0x0A && hdr[2] == 0x01);
}

bool PcxExtractor::decodePcx(const QByteArray &data, QImage &outImage, QString *errorMessage)
{
    if (data.size() < 128) {
        if (errorMessage) *errorMessage = QStringLiteral("File too small for PCX header (< 128 bytes).");
        return false;
    }

    const uchar *raw = reinterpret_cast<const uchar*>(data.constData());
    quint8 manufacturer = raw[0];
    quint8 version = raw[1];
    quint8 encoding = raw[2];
    quint8 bitsPerPixel = raw[3];
    quint16 xMin = qFromLittleEndian<quint16>(raw + 4);
    quint16 yMin = qFromLittleEndian<quint16>(raw + 6);
    quint16 xMax = qFromLittleEndian<quint16>(raw + 8);
    quint16 yMax = qFromLittleEndian<quint16>(raw + 10);
    quint8 numPlanes = raw[65];
    quint16 bytesPerLine = qFromLittleEndian<quint16>(raw + 66);

    Q_UNUSED(version);

    if (manufacturer != 0x0A) {
        if (errorMessage) *errorMessage = QStringLiteral("Invalid PCX manufacturer signature (expected 0x0A).");
        return false;
    }
    if (encoding != 1) {
        if (errorMessage) *errorMessage = QStringLiteral("Unsupported PCX encoding (expected RLE 1).");
        return false;
    }
    if (xMax < xMin || yMax < yMin) {
        if (errorMessage) *errorMessage = QStringLiteral("Invalid PCX dimensions.");
        return false;
    }

    int width = xMax - xMin + 1;
    int height = yMax - yMin + 1;

    if (width <= 0 || height <= 0 || width > 65535 || height > 65535) {
        if (errorMessage) *errorMessage = QStringLiteral("PCX image dimensions out of valid bounds.");
        return false;
    }

    // Determine palette
    QList<QRgb> palette(256, qRgb(0, 0, 0));
    bool has256Palette = false;

    if (numPlanes == 1 && bitsPerPixel == 8 && data.size() >= 128 + 769) {
        int palMarkerPos = data.size() - 769;
        if (raw[palMarkerPos] == 0x0C) {
            has256Palette = true;
            const uchar *palRaw = raw + (data.size() - 768);
            for (int i = 0; i < 256; ++i) {
                palette[i] = qRgb(palRaw[i * 3], palRaw[i * 3 + 1], palRaw[i * 3 + 2]);
            }
        }
    }

    if (!has256Palette) {
        // Fall back to 16-color EGA palette embedded in the header
        for (int i = 0; i < 16; ++i) {
            palette[i] = qRgb(raw[16 + i * 3], raw[16 + i * 3 + 1], raw[16 + i * 3 + 2]);
        }
    }

    int dataOffset = 128;
    int dataEnd = has256Palette ? (data.size() - 769) : data.size();

    outImage = QImage(width, height, QImage::Format_ARGB32);
    outImage.fill(Qt::transparent);

    int scanlineBytes = bytesPerLine * numPlanes;
    QByteArray lineBuf(scanlineBytes, 0);

    for (int y = 0; y < height; ++y) {
        int linePos = 0;
        while (linePos < scanlineBytes && dataOffset < dataEnd) {
            quint8 b = raw[dataOffset++];
            int count = 1;
            quint8 val = b;
            if ((b & 0xC0) == 0xC0) {
                count = b & 0x3F;
                if (dataOffset < dataEnd) {
                    val = raw[dataOffset++];
                }
            }
            int toFill = qMin(count, scanlineBytes - linePos);
            std::memset(lineBuf.data() + linePos, val, toFill);
            linePos += toFill;
        }

        const uchar *srcBuf = reinterpret_cast<const uchar*>(lineBuf.constData());
        QRgb *dstScanLine = reinterpret_cast<QRgb*>(outImage.scanLine(y));

        if (numPlanes == 1 && bitsPerPixel == 8) {
            for (int x = 0; x < width; ++x) {
                dstScanLine[x] = palette[srcBuf[x]];
            }
        } else if (numPlanes == 3 && bitsPerPixel == 8) {
            const uchar *rPlane = srcBuf;
            const uchar *gPlane = rPlane + bytesPerLine;
            const uchar *bPlane = gPlane + bytesPerLine;
            for (int x = 0; x < width; ++x) {
                dstScanLine[x] = qRgb(rPlane[x], gPlane[x], bPlane[x]);
            }
        } else if (numPlanes == 1 && bitsPerPixel == 4) {
            for (int x = 0; x < width; ++x) {
                quint8 byteVal = srcBuf[x / 2];
                quint8 idx = (x % 2 == 0) ? ((byteVal >> 4) & 0x0F) : (byteVal & 0x0F);
                dstScanLine[x] = palette[idx % palette.size()];
            }
        } else {
            // General 8-bit index mapping fallback
            for (int x = 0; x < width; ++x) {
                dstScanLine[x] = palette[srcBuf[x] % palette.size()];
            }
        }
    }

    return true;
}

bool PcxExtractor::encodePcx(const QImage &image, QByteArray &outData, QString *errorMessage)
{
    if (image.isNull()) {
        if (errorMessage) *errorMessage = QStringLiteral("Cannot encode null image to PCX.");
        return false;
    }

    int width = image.width();
    int height = image.height();
    if (width <= 0 || height <= 0 || width > 65535 || height > 65535) {
        if (errorMessage) *errorMessage = QStringLiteral("Invalid image dimensions for PCX.");
        return false;
    }

    QImage indexed = image;
    if (indexed.format() != QImage::Format_Indexed8) {
        indexed = image.convertToFormat(QImage::Format_Indexed8, Qt::AutoColor);
    }

    QList<QRgb> colorTable = indexed.colorTable();
    while (colorTable.size() < 256) {
        colorTable.append(qRgb(0, 0, 0));
    }

    quint16 bytesPerLine = static_cast<quint16>((width + 1) & ~1); // Must be an even number

    outData.clear();
    outData.reserve(128 + height * bytesPerLine + 769);

    // 128-byte header
    QByteArray header(128, 0);
    uchar *hdr = reinterpret_cast<uchar*>(header.data());
    hdr[0] = 0x0A; // Manufacturer (ZSoft)
    hdr[1] = 5;    // Version 5 (PC Paintbrush 3.0+)
    hdr[2] = 1;    // Encoding 1 (RLE)
    hdr[3] = 8;    // 8 bits per pixel
    qToLittleEndian<quint16>(0, hdr + 4);              // xMin
    qToLittleEndian<quint16>(0, hdr + 6);              // yMin
    qToLittleEndian<quint16>(width - 1, hdr + 8);      // xMax
    qToLittleEndian<quint16>(height - 1, hdr + 10);    // yMax
    qToLittleEndian<quint16>(72, hdr + 12);            // hDpi
    qToLittleEndian<quint16>(72, hdr + 14);            // vDpi
    // First 16 colors in header palette
    for (int i = 0; i < 16; ++i) {
        hdr[16 + i * 3]     = static_cast<uchar>(qRed(colorTable[i]));
        hdr[16 + i * 3 + 1] = static_cast<uchar>(qGreen(colorTable[i]));
        hdr[16 + i * 3 + 2] = static_cast<uchar>(qBlue(colorTable[i]));
    }
    hdr[64] = 0; // Reserved
    hdr[65] = 1; // numPlanes = 1
    qToLittleEndian<quint16>(bytesPerLine, hdr + 66);  // bytesPerLine
    qToLittleEndian<quint16>(1, hdr + 68);             // paletteInfo = 1 (Color)

    outData.append(header);

    // RLE scanlines
    QByteArray lineBuf(bytesPerLine, 0);
    for (int y = 0; y < height; ++y) {
        const uchar *srcLine = indexed.constScanLine(y);
        std::memcpy(lineBuf.data(), srcLine, width);
        if (bytesPerLine > width) {
            std::memset(lineBuf.data() + width, 0, bytesPerLine - width);
        }

        const uchar *p = reinterpret_cast<const uchar*>(lineBuf.constData());
        int x = 0;
        while (x < bytesPerLine) {
            uchar val = p[x];
            int run = 1;
            while (x + run < bytesPerLine && p[x + run] == val && run < 63) {
                run++;
            }
            if (run > 1 || (val & 0xC0) == 0xC0) {
                outData.append(static_cast<char>(0xC0 | run));
                outData.append(static_cast<char>(val));
            } else {
                outData.append(static_cast<char>(val));
            }
            x += run;
        }
    }

    // 256-color palette marker 0x0C
    outData.append(static_cast<char>(0x0C));

    // 256 RGB triplets
    for (int i = 0; i < 256; ++i) {
        outData.append(static_cast<char>(qRed(colorTable[i])));
        outData.append(static_cast<char>(qGreen(colorTable[i])));
        outData.append(static_cast<char>(qBlue(colorTable[i])));
    }

    return true;
}

bool PcxExtractor::read(const QString &filePath, SpriteDocument &outDoc, ExtractorError *error)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error) {
            error->code = ExtractorError::FileNotFound;
            error->message = tr("Cannot open PCX file: %1").arg(file.errorString());
            error->filePath = filePath;
        }
        return false;
    }

    QByteArray data = file.readAll();
    file.close();

    QString errStr;
    QImage decodedImage;
    if (!decodePcx(data, decodedImage, &errStr)) {
        if (error) {
            error->code = ExtractorError::CorruptedData;
            error->message = tr("Failed to decode PCX image: %1").arg(errStr);
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

bool PcxExtractor::write(const QString &filePath, const SpriteDocument &inDoc, const ExportOptions &options, ExtractorError *error)
{
    Q_UNUSED(options);

    if (inDoc.atlas().isNull()) {
        if (error) {
            error->code = ExtractorError::WriteFailed;
            error->message = tr("Cannot export PCX: Document atlas image is null.");
            error->filePath = filePath;
        }
        return false;
    }

    QByteArray pcxData;
    QString errStr;
    if (!encodePcx(inDoc.atlas(), pcxData, &errStr)) {
        if (error) {
            error->code = ExtractorError::WriteFailed;
            error->message = tr("Failed to encode PCX: %1").arg(errStr);
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

    if (file.write(pcxData) != pcxData.size()) {
        if (error) {
            error->code = ExtractorError::WriteFailed;
            error->message = tr("Failed to write all PCX bytes to disk.");
            error->filePath = filePath;
        }
        return false;
    }

    return true;
}
