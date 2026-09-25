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

/**
 * @file extractor.h
 * @brief Defines the Extractor base class, the pure I/O codec plugin interface for BentoPack.
 */

#ifndef EXTRACTOR_H
#define EXTRACTOR_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVersionNumber>
#include <QImage>
#include <QList>
#include "export.h"
#include "model/spritedocument.h"

/**
 * @brief Structured error reporting for extractor operations.
 */
struct SPRITESTUDIO_CORE_EXPORT ExtractorError {
    enum Code {
        NoError = 0,
        FileNotFound,
        FileNotReadable,
        FileNotWritable,
        InvalidHeader,
        CorruptedData,
        UnsupportedVersion,
        UnsupportedFormat,
        ImageLoadFailed,
        ParsingFailed,
        PackingFailed,
        WriteFailed
    };

    Code    code = NoError;
    QString message;
    QString filePath;

    bool isError() const { return code != NoError; }
    QString toString() const {
        if (code == NoError) return QStringLiteral("Success");
        return filePath.isEmpty() ? message : QStringLiteral("%1: %2").arg(filePath, message);
    }
};

/**
 * @brief Parameters for static sprite sheet edge detection and segmentation.
 */
struct SPRITESTUDIO_CORE_EXPORT SpriteSheetOptions {
    enum CropStrategy {
        MergeStrategy,
        SeparateStrategy,
        BoundaryStrategy,
        AlphaChannelStrategy
    };

    int          alphaThreshold = 20;
    int          verticalTolerance = 5;
    bool         smartCrop = true;
    double       overlapThreshold = 0.1;
    CropStrategy cropStrategy = SeparateStrategy;
};

class QWidget;

/**
 * @brief Pure I/O Codec Plugin Interface for all sprite formats.
 *
 * Each Extractor translates external file representations (PNG, GIF, JSON, Godot tres, etc.)
 * directly into or from a central SpriteDocument. It holds NO internal document state.
 */
class SPRITESTUDIO_CORE_EXPORT Extractor : public QObject
{
    Q_OBJECT

public:
    enum Capability {
        CanImport             = 0x01,
        CanExport             = 0x02,
        SupportsAnimations    = 0x04,
        SupportsAtlasMetadata = 0x08
    };
    Q_DECLARE_FLAGS(Capabilities, Capability)

    // Alias for backward compatibility
    using Box = SpriteBox;

    explicit Extractor(QObject *parent = nullptr);
    ~Extractor() override = default;

    // Metadata & Introspection
    virtual QString id() const = 0;
    virtual QString displayName() const = 0;
    virtual QString description() const = 0;
    virtual QVersionNumber version() const { return QVersionNumber(1, 0, 0); }
    virtual QStringList supportedExtensions() const = 0;
    virtual Capabilities capabilities() const { return CanImport; }

    // Format detection
    virtual bool canDecode(const QString &filePath) const;

    // Primary I/O contract
    virtual bool read(const QString &filePath, SpriteDocument &outDoc, ExtractorError *error = nullptr) = 0;
    virtual bool write(const QString &filePath, const SpriteDocument &inDoc, const ExportOptions &options, ExtractorError *error = nullptr);

    /**
     * @brief Creates an optional settings/configuration widget for this extractor.
     * Allows the plugin to expose its runtime settings dynamically in the host UI.
     * The caller takes ownership of the created widget.
     */
    virtual QWidget* createSettingsWidget(QWidget *parent = nullptr) {
        Q_UNUSED(parent);
        return nullptr;
    }

    // Compatibility wrappers
    bool extract(const QString &filePath, SpriteDocument &doc, QString *errorMsg = nullptr);
    bool exportDocument(const QString &filePath, const SpriteDocument &doc, const ExportOptions &options, QString *errorMsg = nullptr);

    // Progress & Status API
    int currentProgress() const { return m_progress; }
    QString currentStatusMessage() const { return m_statusMessage; }

protected:
    void setProgress(int percentage);
    void setStatusMessage(const QString &message);

private:
    int     m_progress = 0;
    QString m_statusMessage;

signals:
    void progress(int percentage);
    void statusMessage(const QString &message);
    void extractionFinished(int frameCount);
};

Q_DECLARE_OPERATORS_FOR_FLAGS(Extractor::Capabilities)

#define Extractor_iid "com.bentopack.Extractor/2.0"
Q_DECLARE_INTERFACE(Extractor, Extractor_iid)

#endif // EXTRACTOR_H
