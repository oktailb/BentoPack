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

#ifndef SNES_DIALOG_H
#define SNES_DIALOG_H

#include <QDialog>
#include <QByteArray>
#include <QImage>
#include <QVector>
#include <QColor>
#include "extractor/retrotiledecoder.h"
#include "extractor/snessavestateloader.h"
#include "extractor/snespreset.h"

#include "extractor/snesromanalyzer.h"

class QComboBox;
class QSpinBox;
class QLineEdit;
class QCheckBox;
class QLabel;
class QScrollArea;
class QRadioButton;
class QTabWidget;
class QPushButton;
class QListWidget;
class QListWidgetItem;

class SnesDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SnesDialog(const QString &filePath, const QByteArray &fileData, QWidget *parent = nullptr);
    ~SnesDialog() override = default;

    QImage decodedAtlasImage() const { return m_decodedImage; }
    QList<QRect> detectedSpriteBoxes() const { return m_detectedBoxes; }
    bool extractAsSprites() const { return m_extractAsSprites; }

private slots:
    void onSourceChanged();
    void onAddressChanged();
    void onPresetSelected(int index);
    void onLoadPresetClicked();
    void onLoadExternalPaletteClicked();
    void onPaletteIndexChanged(int index);
    void onPaletteSourceChanged(int index);
    void onDetectedPaletteSelected(int index);
    void onHarmonicPaletteSelected(int index);
    void onCustomPaletteOffsetChanged(int value);
    void onBankSelected(QListWidgetItem *item);
    void onPatternChanged();
    void onConstraintChanged();
    void onZoomChanged();
    void onExtractSpritesClicked();
    void onImportAtlasClicked();

private:
    void setupUi();
    void updatePaletteSwatches();
    void updateDecoding();
    void autoDetectRomInfo();
    void runHeuristicAnalysis();

    QString            m_filePath;
    QByteArray         m_fileData;
    QByteArray         m_overrideData; // For decompressed stream display
    SnesSavestateData  m_savestate;
    bool               m_isSavestate = false;
    bool               m_hasCopierHeader = false;
    bool               m_isHiRom = false;
    QString            m_romTitle;

    QVector<QRgb>      m_activePalette;
    QImage             m_decodedImage;
    QList<QRect>       m_detectedBoxes;
    bool               m_extractAsSprites = true;

    // UI Controls
    QTabWidget        *m_tabWidget = nullptr;
    QLabel            *m_infoBanner = nullptr;

    // Tab 1: Savestate / VRAM Live
    QComboBox         *m_savestatePaletteCombo = nullptr;

    // Tab 2: Heuristic & Compression Bank Scanner
    QListWidget       *m_banksList = nullptr;
    QLabel            *m_banksSummaryLabel = nullptr;
    QList<SnesDetectedBank> m_detectedBanks;

    // Tab 3: ROM Cartridge Manual
    QRadioButton      *m_loRomRadio = nullptr;
    QRadioButton      *m_hiRomRadio = nullptr;
    QCheckBox         *m_copierHeaderCheck = nullptr;
    QLineEdit         *m_snesAddressEdit = nullptr;
    QLineEdit         *m_pcOffsetEdit = nullptr;
    QSpinBox          *m_dataLengthSpin = nullptr;

    // Tab 4: Presets
    QComboBox         *m_presetCombo = nullptr;
    QLabel            *m_presetDescLabel = nullptr;

    // Palette Control Group
    QComboBox         *m_paletteSourceTypeCombo = nullptr;
    QComboBox         *m_detectedPalettesCombo = nullptr;
    QComboBox         *m_harmonicPalettesCombo = nullptr;
    QSpinBox          *m_customPaletteOffsetSpin = nullptr;
    QList<SnesScoredPalette> m_detectedPalettes;

    // Shared Controls
    QComboBox         *m_bppCombo = nullptr;
    QComboBox         *m_patternCombo = nullptr;
    QSpinBox          *m_gridWidthSpin = nullptr;
    QCheckBox         *m_transpZeroCheck = nullptr;
    QCheckBox         *m_showGridCheck = nullptr;
    QCheckBox         *m_highlightSpritesCheck = nullptr;
    QComboBox         *m_zoomCombo = nullptr;

    // Sprite Detection Constraints
    QComboBox         *m_maxSizeCombo = nullptr;
    QCheckBox         *m_disallowOverlapsCheck = nullptr;
    QCheckBox         *m_alignToGridCheck = nullptr;

    // Swatches & Preview
    QWidget           *m_swatchesWidget = nullptr;
    QLabel            *m_previewLabel = nullptr;
    QScrollArea       *m_scrollArea = nullptr;
    QLabel            *m_statusLabel = nullptr;

    QList<SnesPreset>  m_presets;
};

#endif // SNES_DIALOG_H
