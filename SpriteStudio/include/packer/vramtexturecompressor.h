#ifndef VRAMTEXTURECOMPRESSOR_H
#define VRAMTEXTURECOMPRESSOR_H

#include <QImage>
#include <QByteArray>
#include <QString>
#include <QFileInfo>

enum class VramFormat {
    KTX2_UASTC,      // Universal ASTC/BC7/ETC2 4x4 block compression, highest pixel art fidelity
    KTX2_ETC1S,      // Universal global codebooks ETC1S, ultra-compact size
    Basis_UASTC,     // Native .basis file (UASTC)
    Basis_ETC1S      // Native .basis file (ETC1S)
};

struct VramCompressionOptions {
    VramFormat format = VramFormat::KTX2_UASTC;
    int qualityLevel = 2;               // UASTC: 0 (fastest) to 3 (slower, better RDO); ETC1S: 1 to 255
    bool zstdSupercompression = true;   // Supercompress KTX2 container with Zstandard
    int zstdLevel = 9;                  // Zstandard compression level (1 to 22)
    bool generateMipmaps = false;       // Typically false for 2D pixel art atlas sheets
    int threadCount = 0;                // 0 = all logical CPU cores
    bool sRGB = true;                   // Treat color channels as perceptual sRGB
};

struct VramCompressionStats {
    int originalBytes = 0;
    int compressedBytes = 0;
    double compressionRatio = 0.0;
    double vramSavingsPercent = 0.0;
    qint64 estimatedVramBytes = 0;
};

/**
 * @brief High-performance hardware texture compression engine supporting KTX2, UASTC, ETC1S, and Zstd.
 */
class VramTextureCompressor {
public:
    // Availability
    static bool isAvailable();

    // Compression routines
    static QByteArray compressToKtx2(const QImage &image,
                                     const VramCompressionOptions &options = VramCompressionOptions(),
                                     VramCompressionStats *outStats = nullptr,
                                     QString *outError = nullptr);

    static bool compressToFile(const QImage &image,
                               const QString &outputPath,
                               const VramCompressionOptions &options = VramCompressionOptions(),
                               VramCompressionStats *outStats = nullptr,
                               QString *outError = nullptr);

    // Verification & inspection
    static bool isValidKtx2(const QByteArray &data);
    static QImage transcodeToRgba(const QByteArray &ktx2Data, QString *outError = nullptr);
    static QImage loadAtlasImage(const QString &filePath, QString *outError = nullptr);

    // Utility & telemetry
    static qint64 estimateVramBytes(int width, int height, VramFormat format);
    static QString formatName(VramFormat format);
    static QString formatExtension(VramFormat format);
};

#endif // VRAMTEXTURECOMPRESSOR_H
