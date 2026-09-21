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

#include "packer/vramtexturecompressor.h"
#include <QFile>
#include <QDir>
#include <QThread>
#include <algorithm>
#include <mutex>

#if defined(HAVE_BASIS_UNIVERSAL)
#ifdef emit
#pragma push_macro("emit")
#undef emit
#define RESTORE_EMIT
#endif

#include "encoder/basisu_comp.h"
#include "transcoder/basisu_transcoder.h"

#ifdef RESTORE_EMIT
#pragma pop_macro("emit")
#undef RESTORE_EMIT
#endif
#endif

namespace {
#if defined(HAVE_BASIS_UNIVERSAL)
static void ensureBasisuInitialized()
{
    static std::once_flag initFlag;
    std::call_once(initFlag, []() {
        basisu::basisu_encoder_init();
        basist::basisu_transcoder_init();
    });
}
#endif
} // namespace

bool VramTextureCompressor::isAvailable()
{
#if defined(HAVE_BASIS_UNIVERSAL)
    return true;
#else
    return false;
#endif
}

QByteArray VramTextureCompressor::compressToKtx2(const QImage &image,
                                                const VramCompressionOptions &options,
                                                VramCompressionStats *outStats,
                                                QString *outError)
{
    if (image.isNull()) {
        if (outError) *outError = QStringLiteral("Source image is null or empty.");
        return QByteArray();
    }

#if !defined(HAVE_BASIS_UNIVERSAL)
    if (outError) *outError = QStringLiteral("Basis Universal VRAM compression library is not available in this build.");
    return QByteArray();
#else
    ensureBasisuInitialized();

    const int width = image.width();
    const int height = image.height();

    if (width <= 0 || height <= 0) {
        if (outError) *outError = QStringLiteral("Invalid image dimensions (%1x%2).").arg(width).arg(height);
        return QByteArray();
    }

    // Convert QImage to continuous 32-bit RGBA
    QImage rgba = (image.format() == QImage::Format_RGBA8888)
                      ? image
                      : image.convertToFormat(QImage::Format_RGBA8888);

    basist::basis_tex_format mode = basist::basis_tex_format::cUASTC_LDR_4x4;
    bool isKtx2 = true;
    switch (options.format) {
    case VramFormat::KTX2_UASTC:
        mode = basist::basis_tex_format::cUASTC_LDR_4x4;
        isKtx2 = true;
        break;
    case VramFormat::KTX2_ETC1S:
        mode = basist::basis_tex_format::cETC1S;
        isKtx2 = true;
        break;
    case VramFormat::Basis_UASTC:
        mode = basist::basis_tex_format::cUASTC_LDR_4x4;
        isKtx2 = false;
        break;
    case VramFormat::Basis_ETC1S:
        mode = basist::basis_tex_format::cETC1S;
        isKtx2 = false;
        break;
    }

    uint32_t flags = 0;
    if (isKtx2) {
        flags |= basisu::cFlagKTX2;
    }
    if (options.zstdSupercompression && isKtx2) {
        flags |= basisu::cFlagKTX2UASTCSuperCompression;
    }
    if (options.sRGB) {
        flags |= basisu::cFlagSRGB;
    }
    if (options.generateMipmaps) {
        flags |= basisu::cFlagGenMipsClamp;
    }

    int threads = options.threadCount > 0 ? options.threadCount : QThread::idealThreadCount();
    if (threads > 1) {
        flags |= basisu::cFlagThreaded;
    }

    // Configure quality / effort flags
    if (mode == basist::basis_tex_format::cUASTC_LDR_4x4) {
        int uLevel = std::clamp(options.qualityLevel, 0, 3);
        flags |= static_cast<uint32_t>(uLevel);
    } else {
        uint32_t quality = static_cast<uint32_t>(std::clamp(options.qualityLevel, 1, 255));
        flags |= (quality & 0xFF);
    }

    size_t outSize = 0;
    void *pData = basisu::basis_compress(
        mode,
        rgba.constBits(),
        static_cast<uint32_t>(width),
        static_cast<uint32_t>(height),
        static_cast<uint32_t>(width),
        flags,
        0.0f,
        &outSize,
        nullptr
    );

    if (!pData || outSize == 0) {
        if (outError) *outError = QStringLiteral("Basis Universal compression failed.");
        return QByteArray();
    }

    QByteArray result(reinterpret_cast<const char *>(pData), static_cast<int>(outSize));
    basisu::basis_free_data(pData);

    if (outStats) {
        outStats->originalBytes = width * height * 4;
        outStats->compressedBytes = result.size();
        outStats->compressionRatio = (outStats->originalBytes > 0 && outStats->compressedBytes > 0)
                                         ? (static_cast<double>(outStats->originalBytes) / static_cast<double>(outStats->compressedBytes))
                                         : 1.0;
        outStats->estimatedVramBytes = estimateVramBytes(width, height, options.format);
        outStats->vramSavingsPercent = (outStats->originalBytes > 0)
                                           ? (100.0 * (1.0 - static_cast<double>(outStats->estimatedVramBytes) / static_cast<double>(outStats->originalBytes)))
                                           : 0.0;
    }

    return result;
#endif
}

bool VramTextureCompressor::compressToFile(const QImage &image,
                                          const QString &outputPath,
                                          const VramCompressionOptions &options,
                                          VramCompressionStats *outStats,
                                          QString *outError)
{
    QByteArray bytes = compressToKtx2(image, options, outStats, outError);
    if (bytes.isEmpty()) {
        return false;
    }

    QFileInfo fi(outputPath);
    QDir().mkpath(fi.dir().absolutePath());

    QFile file(outputPath);
    if (!file.open(QIODevice::WriteOnly)) {
        if (outError) *outError = QStringLiteral("Failed to open file for writing: ") + outputPath;
        return false;
    }

    if (file.write(bytes) != bytes.size()) {
        if (outError) *outError = QStringLiteral("Failed to write complete data to: ") + outputPath;
        return false;
    }

    return true;
}

bool VramTextureCompressor::isValidKtx2(const QByteArray &data)
{
    // KTX2 Identifier: 0xAB, 0x4B, 0x54, 0x58, 0x20, 0x32, 0x30, 0xBB, 0x0D, 0x0A, 0x1A, 0x0A
    static const uint8_t ktx2Magic[12] = {
        0xAB, 0x4B, 0x54, 0x58, 0x20, 0x32, 0x30, 0xBB, 0x0D, 0x0A, 0x1A, 0x0A
    };
    if (data.size() < 12) return false;
    return memcmp(data.constData(), ktx2Magic, 12) == 0;
}

QImage VramTextureCompressor::transcodeToRgba(const QByteArray &ktx2Data, QString *outError)
{
    if (ktx2Data.isEmpty()) {
        if (outError) *outError = QStringLiteral("KTX2 data buffer is empty.");
        return QImage();
    }

#if !defined(HAVE_BASIS_UNIVERSAL)
    if (outError) *outError = QStringLiteral("Basis Universal transcoder not available.");
    return QImage();
#else
    ensureBasisuInitialized();

    basist::ktx2_transcoder transcoder;
    if (!transcoder.init(ktx2Data.constData(), static_cast<uint32_t>(ktx2Data.size()))) {
        if (outError) *outError = QStringLiteral("Failed to initialize KTX2 transcoder.");
        return QImage();
    }

    uint32_t w = transcoder.get_width();
    uint32_t h = transcoder.get_height();
    if (w == 0 || h == 0) {
        if (outError) *outError = QStringLiteral("Invalid KTX2 image dimensions: %1x%2").arg(w).arg(h);
        return QImage();
    }

    if (!transcoder.start_transcoding()) {
        if (outError) *outError = QStringLiteral("Failed to start KTX2 transcoding.");
        return QImage();
    }

    QImage outImage(static_cast<int>(w), static_cast<int>(h), QImage::Format_RGBA8888);
    outImage.fill(Qt::transparent);

    bool ok = transcoder.transcode_image_level(
        0, 0, 0,
        outImage.bits(),
        w * h,
        basist::transcoder_texture_format::cTFRGBA32
    );

    if (!ok) {
        if (outError) *outError = QStringLiteral("Failed to transcode KTX2 image level to RGBA32.");
        return QImage();
    }

    return outImage.convertToFormat(QImage::Format_ARGB32);
#endif
}

QImage VramTextureCompressor::loadAtlasImage(const QString &filePath, QString *outError)
{
    if (filePath.isEmpty() || !QFile::exists(filePath)) {
        if (outError) *outError = QObject::tr("Image file does not exist: %1").arg(filePath);
        return QImage();
    }

    QFileInfo fi(filePath);
    QString ext = fi.suffix().toLower();

    if (ext == QStringLiteral("ktx2") || ext == QStringLiteral("basis")) {
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly)) {
            if (outError) *outError = QObject::tr("Failed to open file for reading: %1").arg(filePath);
            return QImage();
        }
        QByteArray data = file.readAll();
        return transcodeToRgba(data, outError);
    }

    // Try standard Qt image decoders (PNG, JPG, BMP, etc.)
    QImage img(filePath);
    if (!img.isNull()) {
        return (img.format() == QImage::Format_ARGB32) ? img : img.convertToFormat(QImage::Format_ARGB32);
    }

    // Fallback: check if the file is actually a KTX2 container despite unusual extension
    QFile file(filePath);
    if (file.open(QIODevice::ReadOnly)) {
        QByteArray data = file.readAll();
        if (isValidKtx2(data)) {
            return transcodeToRgba(data, outError);
        }
    }

    if (outError) *outError = QObject::tr("Failed to decode image from: %1").arg(filePath);
    return QImage();
}

qint64 VramTextureCompressor::estimateVramBytes(int width, int height, VramFormat format)
{
    if (width <= 0 || height <= 0) return 0;
    int blocksX = (width + 3) / 4;
    int blocksY = (height + 3) / 4;
    switch (format) {
    case VramFormat::KTX2_UASTC:
    case VramFormat::Basis_UASTC:
        // 16 bytes per 4x4 block (8 bits per pixel)
        return static_cast<qint64>(blocksX) * blocksY * 16;
    case VramFormat::KTX2_ETC1S:
    case VramFormat::Basis_ETC1S:
        // 8 bytes per 4x4 block (4 bits per pixel GPU footprint)
        return static_cast<qint64>(blocksX) * blocksY * 8;
    }
    return static_cast<qint64>(width) * height * 4;
}

QString VramTextureCompressor::formatName(VramFormat format)
{
    switch (format) {
    case VramFormat::KTX2_UASTC:
        return QStringLiteral("KTX2 (UASTC Universal 4x4)");
    case VramFormat::KTX2_ETC1S:
        return QStringLiteral("KTX2 (ETC1S Ultra-Compact)");
    case VramFormat::Basis_UASTC:
        return QStringLiteral("Basis (UASTC)");
    case VramFormat::Basis_ETC1S:
        return QStringLiteral("Basis (ETC1S)");
    }
    return QStringLiteral("Unknown");
}

QString VramTextureCompressor::formatExtension(VramFormat format)
{
    switch (format) {
    case VramFormat::KTX2_UASTC:
    case VramFormat::KTX2_ETC1S:
        return QStringLiteral(".ktx2");
    case VramFormat::Basis_UASTC:
    case VramFormat::Basis_ETC1S:
        return QStringLiteral(".basis");
    }
    return QStringLiteral(".ktx2");
}
