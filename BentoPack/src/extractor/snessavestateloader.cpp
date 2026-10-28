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

#include "extractor/snessavestateloader.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QtEndian>

QRgb SnesSavestateData::bgr555ToRgb(quint16 word, int alpha)
{
    // SNES 15-bit BGR555: 0bbbbbgggggrrrrr
    quint8 r5 = static_cast<quint8>(word & 0x1F);
    quint8 g5 = static_cast<quint8>((word >> 5) & 0x1F);
    quint8 b5 = static_cast<quint8>((word >> 10) & 0x1F);

    // Expand 5-bit to 8-bit using (val * 255 / 31) equivalent bit replication: (val << 3) | (val >> 2)
    quint8 r8 = static_cast<quint8>((r5 << 3) | (r5 >> 2));
    quint8 g8 = static_cast<quint8>((g5 << 3) | (g5 >> 2));
    quint8 b8 = static_cast<quint8>((b5 << 3) | (b5 >> 2));

    return qRgba(r8, g8, b8, alpha);
}

QVector<QRgb> SnesSavestateData::getPalette(int paletteIndex, bool transparentColorZero) const
{
    QVector<QRgb> colors(16);
    if (paletteIndex < 0 || paletteIndex > 15 || cgram.size() < (paletteIndex + 1) * 32) {
        // Fallback default grayscale ramp if CGRAM is missing or invalid
        for (int i = 0; i < 16; ++i) {
            int v = (i * 255) / 15;
            int a = (transparentColorZero && i == 0) ? 0 : 255;
            colors[i] = qRgba(v, v, v, a);
        }
        return colors;
    }

    const uchar *raw = reinterpret_cast<const uchar*>(cgram.constData()) + (paletteIndex * 32);
    for (int i = 0; i < 16; ++i) {
        quint16 word = qFromLittleEndian<quint16>(raw + (i * 2));
        int alpha = (transparentColorZero && i == 0) ? 0 : 255;
        colors[i] = bgr555ToRgb(word, alpha);
    }
    return colors;
}

QVector<QVector<QRgb>> SnesSavestateData::getAllPalettes(bool transparentColorZero) const
{
    QVector<QVector<QRgb>> all(16);
    for (int p = 0; p < 16; ++p) {
        all[p] = getPalette(p, transparentColorZero);
    }
    return all;
}

bool SnesAddress::detectHiRom(const QByteArray &data, bool hasHeader)
{
    int headerOffset = hasHeader ? 512 : 0;
    int loOffset = headerOffset + 0x007FDC;
    int hiOffset = headerOffset + 0x00FFDC;

    // SNES internal header checksum and complement: Checksum + Complement == 0xFFFF
    auto checkSumValid = [&](int offset) -> bool {
        if (data.size() < offset + 4) return false;
        const uchar *p = reinterpret_cast<const uchar*>(data.constData()) + offset;
        quint16 complement = qFromLittleEndian<quint16>(p);
        quint16 checksum = qFromLittleEndian<quint16>(p + 2);
        return ((complement ^ checksum) == 0xFFFF);
    };

    bool hiValid = checkSumValid(hiOffset);
    bool loValid = checkSumValid(loOffset);

    if (hiValid && !loValid) return true;
    if (loValid && !hiValid) return false;

    // Secondary heuristic: check ROM map mode byte at 0x7FD5 vs 0xFFD5
    if (data.size() >= hiOffset + 0x20) {
        uchar hiMode = static_cast<uchar>(data.at(headerOffset + 0x00FFD5));
        if (hiMode == 0x21 || hiMode == 0x25 || hiMode == 0x31 || hiMode == 0x35) {
            return true;
        }
    }
    return false;
}

QString SnesAddress::readInternalRomTitle(const QByteArray &data, bool hasHeader, bool isHiRom)
{
    int offset = (hasHeader ? 512 : 0) + (isHiRom ? 0x00FFC0 : 0x007FC0);
    if (data.size() < offset + 21) {
        return QString();
    }

    QByteArray rawTitle = data.mid(offset, 21);
    // Replace unprintable characters with space
    for (int i = 0; i < rawTitle.size(); ++i) {
        uchar c = static_cast<uchar>(rawTitle[i]);
        if (c < 32 || c > 126) {
            rawTitle[i] = ' ';
        }
    }
    return QString::fromLatin1(rawTitle).trimmed();
}

SnesSavestateData SnesSavestateLoader::load(const QString &filePath)
{
    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly)) {
        return SnesSavestateData();
    }
    QByteArray data = f.readAll();
    return loadFromData(data, QFileInfo(filePath).fileName());
}

bool SnesSavestateLoader::isSavestate(const QByteArray &data, const QString &fileNameHint)
{
    if (data.size() < 512) return false;

    QString ext = QFileInfo(fileNameHint).suffix().toLower();
    if (ext == QStringLiteral("vram") || ext == QStringLiteral("cgram")) {
        return true;
    }
    if (ext.startsWith(QLatin1String("s9")) || ext.startsWith(QLatin1String("zs")) ||
        ext == QStringLiteral("000") || ext == QStringLiteral("zst") || ext == QStringLiteral("mss")) {
        return true;
    }

    // Snes9x magic check "#!s9x"
    if (data.size() >= 5 && data.startsWith("#!s9x")) {
        return true;
    }

    // ZSNES signature check
    if (data.size() >= 16 && data.contains("ZSNES Save State")) {
        return true;
    }

    // Raw VRAM dump exact sizes
    if (data.size() == 65536 || data.size() == 66048) {
        return true;
    }

    return false;
}

SnesSavestateData SnesSavestateLoader::loadFromData(const QByteArray &data, const QString &fileNameHint)
{
    SnesSavestateData result;
    if (data.isEmpty()) return result;

    QString ext = QFileInfo(fileNameHint).suffix().toLower();

    // 1. Raw CGRAM Dump (512 bytes)
    if (data.size() == 512 || ext == QStringLiteral("cgram")) {
        result.isValid = true;
        result.emulatorName = QStringLiteral("Raw CGRAM Palette Dump");
        result.cgram = data.left(512);
        return result;
    }

    // 2. Raw VRAM Dump (65536 bytes) or VRAM+CGRAM (66048 bytes)
    if (data.size() == 65536 || data.size() == 66048 || ext == QStringLiteral("vram")) {
        result.isValid = true;
        result.emulatorName = QStringLiteral("Raw VRAM Dump");
        result.vram = data.left(65536);
        if (data.size() >= 66048) {
            result.cgram = data.mid(65536, 512);
        }
        return result;
    }

    // 3. Snes9x Snapshot Format ("#!s9x")
    if (data.size() >= 5 && data.startsWith("#!s9x")) {
        result.emulatorName = QStringLiteral("Snes9x Savestate");
        
        // Scan for tagged chunks: 4-byte ASCII tag followed by 4-byte length or direct data
        // Search for "VRAM" tag (65536 bytes block)
        int vramIdx = data.indexOf("VRAM");
        if (vramIdx != -1 && vramIdx + 8 + 65536 <= data.size()) {
            quint32 lenBE = qFromBigEndian<quint32>(reinterpret_cast<const uchar*>(data.constData()) + vramIdx + 4);
            quint32 lenLE = qFromLittleEndian<quint32>(reinterpret_cast<const uchar*>(data.constData()) + vramIdx + 4);
            if (lenBE == 65536 || lenLE == 65536) {
                result.vram = data.mid(vramIdx + 8, 65536);
            } else {
                result.vram = data.mid(vramIdx + 4, 65536);
            }
        } else if (vramIdx != -1 && vramIdx + 4 + 65536 <= data.size()) {
            result.vram = data.mid(vramIdx + 4, 65536);
        }

        // Search for "CRA " tag (CGRAM, 512 bytes block)
        int craIdx = data.indexOf("CRA ");
        if (craIdx != -1 && craIdx + 8 + 512 <= data.size()) {
            quint32 lenBE = qFromBigEndian<quint32>(reinterpret_cast<const uchar*>(data.constData()) + craIdx + 4);
            quint32 lenLE = qFromLittleEndian<quint32>(reinterpret_cast<const uchar*>(data.constData()) + craIdx + 4);
            if (lenBE == 512 || lenLE == 512) {
                result.cgram = data.mid(craIdx + 8, 512);
            } else {
                result.cgram = data.mid(craIdx + 4, 512);
            }
        } else if (craIdx != -1 && craIdx + 4 + 512 <= data.size()) {
            result.cgram = data.mid(craIdx + 4, 512);
        }

        // If PPU block is present, extract CGRAM if not already found
        if (result.cgram.isEmpty()) {
            int ppuIdx = data.indexOf("PPU ");
            if (ppuIdx != -1 && ppuIdx + 8 + 512 <= data.size()) {
                // Usually CGRAM starts inside PPU block at offset +8 or +16
                result.cgram = data.mid(ppuIdx + 8, 512);
            }
        }

        if (!result.vram.isEmpty()) {
            result.isValid = true;
            return result;
        }
    }

    // 4. ZSNES Snapshot Format (.zst, .zs1..9, or "ZSNES Save State" magic)
    if (ext.startsWith(QLatin1String("zs")) || ext == QStringLiteral("zst") || data.contains("ZSNES Save State")) {
        result.emulatorName = QStringLiteral("ZSNES Savestate");
        
        // In ZSNES savestates, VRAM is standard 64KB uncompressed.
        // It is located at offset 0x301 or 0x317 in classic formats.
        int candidates[] = { 0x0317, 0x0301, 0x0024, 0x0200 };
        for (int cOff : candidates) {
            if (cOff + 65536 <= data.size()) {
                result.vram = data.mid(cOff, 65536);
                result.isValid = true;
                break;
            }
        }

        // CGRAM in ZSNES is often 512 bytes directly preceding or following VRAM or at offset 0x10317
        int cgramCandidates[] = { 0x10317, 0x10301, 0x0117 };
        for (int cgOff : cgramCandidates) {
            if (cgOff + 512 <= data.size()) {
                result.cgram = data.mid(cgOff, 512);
                break;
            }
        }

        if (result.isValid) {
            return result;
        }
    }

    // 5. Fallback signature scan: Locate any 64KB contiguous VRAM block inside arbitrary savestate
    int vramScan = data.indexOf("VRAM");
    if (vramScan != -1 && vramScan + 4 + 65536 <= data.size()) {
        result.vram = data.mid(vramScan + 4, 65536);
        result.isValid = true;
        result.emulatorName = QStringLiteral("Generic SNES Savestate");

        int cgramScan = data.indexOf("CGRAM");
        if (cgramScan != -1 && cgramScan + 5 + 512 <= data.size()) {
            result.cgram = data.mid(cgramScan + 5, 512);
        }
        return result;
    }

    return result;
}
