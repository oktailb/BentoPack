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

#include "gifextractor.h"
#include "packer/atlaspacker.h"
#include <QImageReader>
#include <QFileInfo>
#include <QDir>
#include <QFile>
#include <QPainter>
#include <QDebug>
#include <cmath>

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#endif

#define MSF_GIF_IMPL
#include "msf_gif.h"

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

GifExtractor::GifExtractor(QObject *parent)
    : Extractor(parent)
{
}

bool GifExtractor::canDecode(const QString &filePath) const
{
    QFileInfo fi(filePath);
    return fi.suffix().toLower() == QStringLiteral("gif");
}

bool GifExtractor::read(const QString &filePath, SpriteDocument &outDoc, ExtractorError *error)
{
    setStatusMessage(tr("Reading GIF frames from %1...").arg(QFileInfo(filePath).fileName()));
    setProgress(5);

    QFileInfo fi(filePath);
    if (!fi.exists()) {
        if (error) {
            error->code = ExtractorError::FileNotFound;
            error->message = tr("File not found: %1").arg(filePath);
            error->filePath = filePath;
        }
        return false;
    }

    QImageReader reader(filePath);
    if (!reader.canRead()) {
        if (error) {
            error->code = ExtractorError::CorruptedData;
            error->message = tr("Unable to read GIF format: %1").arg(reader.errorString());
            error->filePath = filePath;
        }
        return false;
    }

    int expectedCount = reader.imageCount();
    QList<QImage> frameImages;
    int totalDelayMs = 0;
    int frameIndex = 0;

    while (reader.canRead()) {
        int delay = reader.nextImageDelay();
        if (delay <= 0) delay = 100; // default 10 fps
        totalDelayMs += delay;

        QImage frameImg = reader.read();
        if (frameImg.isNull()) break;

        frameImages.append(frameImg);
        frameIndex++;

        if (expectedCount > 0) {
            setProgress(qMin(70, 5 + (65 * frameIndex / expectedCount)));
        }
    }

    if (frameImages.isEmpty()) {
        if (error) {
            error->code = ExtractorError::CorruptedData;
            error->message = tr("No valid frames could be decoded from GIF: %1").arg(filePath);
            error->filePath = filePath;
        }
        return false;
    }

    setStatusMessage(tr("Assembling GIF atlas..."));
    setProgress(75);

    // Pack frames into an atlas
    AtlasPackResult packResult = AtlasPacker::pack(frameImages, 2);
    if (!packResult.success) {
        if (error) {
            error->code = ExtractorError::PackingFailed;
            error->message = tr("Failed to pack GIF frames into texture atlas.");
            error->filePath = filePath;
        }
        return false;
    }

    QList<SpriteBox> boxes;
    boxes.reserve(packResult.frameRects.size());
    for (int i = 0; i < packResult.frameRects.size(); ++i) {
        SpriteBox sb;
        sb.rect = packResult.frameRects[i];
        sb.index = i;
        sb.selected = false;
        boxes.append(sb);
    }

    // Determine FPS
    int avgDelay = totalDelayMs / qMax(1, frameImages.size());
    int fps = (avgDelay > 0) ? qRound(1000.0 / avgDelay) : 12;
    if (fps <= 0) fps = 12;

    outDoc.setFilePath(filePath);
    outDoc.setAtlas(packResult.atlas);
    outDoc.setFrames(frameImages, boxes);

    QList<int> allIndices;
    allIndices.reserve(frameImages.size());
    for (int i = 0; i < frameImages.size(); ++i) {
        allIndices.append(i);
    }
    outDoc.setAnimation(QStringLiteral("default"), allIndices, fps, true);

    setProgress(100);
    setStatusMessage(tr("Extracted %1 GIF frames").arg(frameImages.size()));
    emit extractionFinished(frameImages.size());
    return true;
}

bool GifExtractor::write(const QString &filePath, const SpriteDocument &inDoc, const ExportOptions &options, ExtractorError *error)
{
    Q_UNUSED(options);

    if (inDoc.frameCount() == 0) {
        if (error) {
            error->code = ExtractorError::WriteFailed;
            error->message = tr("No frames in document to export.");
            error->filePath = filePath;
        }
        return false;
    }

    QFileInfo fi(filePath);
    QDir dir = fi.dir();
    if (!dir.exists()) {
        dir.mkpath(QStringLiteral("."));
    }

    QString projectName = inDoc.projectName();
    if (projectName.isEmpty() || projectName == QStringLiteral("untitled")) {
        projectName = fi.completeBaseName();
    }
    if (projectName.isEmpty()) {
        projectName = QStringLiteral("project");
    }

    // Récupérer la liste des animations à exporter
    QList<SpriteAnimation> animationsToExport;
    const auto &docAnims = inDoc.animations();
    if (!docAnims.isEmpty()) {
        animationsToExport = docAnims.values();
    } else {
        SpriteAnimation defAnim;
        defAnim.name = QStringLiteral("default");
        for (int i = 0; i < inDoc.frameCount(); ++i) {
            defAnim.frameIndices.append(i);
        }
        defAnim.fps = 12;
        defAnim.loop = true;
        defAnim.loopMode = SpriteAnimation::Loop;
        animationsToExport.append(defAnim);
    }

    int total = animationsToExport.size();
    for (int i = 0; i < total; ++i) {
        const SpriteAnimation &anim = animationsToExport.at(i);
        setStatusMessage(tr("Exporting GIF animation %1 (%2/%3)...").arg(anim.name).arg(i + 1).arg(total));
        setProgress(qRound(100.0 * i / total));

        // Format requis : projectname_animationname.gif
        QString outFileName = QStringLiteral("%1_%2.gif").arg(projectName, anim.name);
        QString targetPath = dir.filePath(outFileName);

        if (!writeSingleAnimation(targetPath, inDoc, anim, error)) {
            return false;
        }
    }

    setProgress(100);
    setStatusMessage(tr("Exported %1 GIF animation(s) successfully.").arg(total));
    return true;
}

bool GifExtractor::writeSingleAnimation(const QString &targetFilePath,
                                        const SpriteDocument &doc,
                                        const SpriteAnimation &anim,
                                        ExtractorError *error)
{
    QList<int> frameSeq;
    if (anim.frameIndices.isEmpty()) {
        for (int i = 0; i < doc.frameCount(); ++i) {
            frameSeq.append(i);
        }
    } else {
        frameSeq = anim.frameIndices;
    }

    if (frameSeq.isEmpty()) {
        if (error) {
            error->code = ExtractorError::WriteFailed;
            error->message = tr("Animation %1 has no frames.").arg(anim.name);
            error->filePath = targetFilePath;
        }
        return false;
    }

    // Gestion du mode PingPong : 0, 1, 2, ..., n-1, n-2, ..., 1
    if (anim.loopMode == SpriteAnimation::PingPong && frameSeq.size() > 2) {
        int originalCount = frameSeq.size();
        for (int k = originalCount - 2; k >= 1; --k) {
            frameSeq.append(frameSeq.at(k));
        }
    }

    // Calcul de l'enveloppe de l'animation pour un alignement anti-jittering par pivot
    QRect envelope = doc.computeAnimationEnvelope(anim.name);
    int canvasW = envelope.width();
    int canvasH = envelope.height();

    // Fallback si l'enveloppe n'est pas calculable
    if (canvasW <= 0 || canvasH <= 0) {
        for (int idx : frameSeq) {
            QImage f = doc.frame(idx);
            if (!f.isNull()) {
                if (f.width() > canvasW) canvasW = f.width();
                if (f.height() > canvasH) canvasH = f.height();
            }
        }
    }
    if (canvasW <= 0) canvasW = 32;
    if (canvasH <= 0) canvasH = 32;

    // Calcul du timing en centièmes de seconde (1/100 s)
    int fps = anim.fps > 0 ? anim.fps : 12;
    int centiSeconds = qRound(100.0 / fps);
    if (centiSeconds < 2) centiSeconds = 2; // Limite standard des visualiseurs GIF

    // Configuration de msf_gif
    // Loop mode : 0 = boucle infinie (Loop ou PingPong), 1 = jouer une fois (Once)
    if (anim.loopMode == SpriteAnimation::Once) {
        msf_gif_loop_count = 1;
    } else {
        msf_gif_loop_count = 0; // infini
    }

    // Activer la transparence GIF (alpha < 128 = transparent)
    msf_gif_alpha_threshold = 128;
    msf_gif_bgra_flag = 0;

    MsfGifState state = {};
    if (!msf_gif_begin(&state, canvasW, canvasH)) {
        if (error) {
            error->code = ExtractorError::WriteFailed;
            error->message = tr("Failed to initialize GIF encoder for %1.").arg(targetFilePath);
            error->filePath = targetFilePath;
        }
        return false;
    }

    for (int idx : frameSeq) {
        QImage canvas(canvasW, canvasH, QImage::Format_RGBA8888);
        canvas.fill(Qt::transparent);

        QImage frameImg = doc.polygonClippedFrame(idx);
        if (frameImg.isNull()) {
            frameImg = doc.frame(idx);
        }
        if (!frameImg.isNull()) {
            QPainter painter(&canvas);
            int drawX = 0;
            int drawY = 0;
            if (idx >= 0 && idx < doc.boxes().size()) {
                const SpriteBox &b = doc.box(idx);
                QPoint pivot = b.effectivePivot();
                drawX = envelope.x() - pivot.x();
                drawY = envelope.y() - pivot.y();
            }
            painter.drawImage(drawX, drawY, frameImg);
            painter.end();
        }

        if (!msf_gif_frame(&state, const_cast<uint8_t*>(canvas.constBits()), centiSeconds, 16, canvas.bytesPerLine())) {
            msf_gif_end(&state);
            if (error) {
                error->code = ExtractorError::WriteFailed;
                error->message = tr("Failed to encode GIF frame in %1.").arg(targetFilePath);
                error->filePath = targetFilePath;
            }
            return false;
        }
    }

    MsfGifResult result = msf_gif_end(&state);
    if (!result.data || result.dataSize == 0) {
        msf_gif_free(result);
        if (error) {
            error->code = ExtractorError::WriteFailed;
            error->message = tr("GIF encoding generated no data for %1.").arg(targetFilePath);
            error->filePath = targetFilePath;
        }
        return false;
    }

    QFile file(targetFilePath);
    if (!file.open(QIODevice::WriteOnly)) {
        msf_gif_free(result);
        if (error) {
            error->code = ExtractorError::FileNotWritable;
            error->message = tr("Cannot open destination file %1: %2").arg(targetFilePath, file.errorString());
            error->filePath = targetFilePath;
        }
        return false;
    }

    qint64 written = file.write(reinterpret_cast<const char*>(result.data), static_cast<qint64>(result.dataSize));
    file.close();
    msf_gif_free(result);

    if (written != static_cast<qint64>(result.dataSize)) {
        if (error) {
            error->code = ExtractorError::WriteFailed;
            error->message = tr("Incomplete file write to %1.").arg(targetFilePath);
            error->filePath = targetFilePath;
        }
        return false;
    }

    return true;
}
