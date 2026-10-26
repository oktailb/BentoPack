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

#ifndef RETROTILEEXPLORERDIALOG_H
#define RETROTILEEXPLORERDIALOG_H

#include <QDialog>
#include <QString>
#include <QByteArray>
#include <QImage>
#include <QList>
#include "bentopackwidgets_export.h"
#include "extractor/retrotiledecoder.h"
#include "extractor/romprobe.h"
#include "model/spritedocument.h"

class QComboBox;
class QSpinBox;
class QSlider;
class QLineEdit;
class QCheckBox;
class QLabel;
class QPushButton;
class QScrollArea;
class ProjectController;

class BENTOPACK_WIDGETS_EXPORT RetroTileExplorerDialog : public QDialog
{
    Q_OBJECT

public:
    explicit RetroTileExplorerDialog(const QString &filePath,
                                     QWidget *parent = nullptr,
                                     ProjectController *projectController = nullptr);
    ~RetroTileExplorerDialog() override = default;

    QImage renderedSheet() const { return m_currentSheet; }
    QList<QImage> extractedFrames() const { return m_extractedFrames; }
    QList<SpriteBox> extractedBoxes() const { return m_extractedBoxes; }

private slots:
    void onBankSelected(int index);
    void onOffsetSliderChanged(int value);
    void onOffsetSpinChanged(int value);
    void onOffsetHexEdited(const QString &text);
    void onFormatChanged(int index);
    void onPatternChanged(int index);
    void onPaletteChanged(int index);
    void onDetectedPaletteSelected(int index);
    void onPaletteOffsetChanged(int value);
    void onPaletteHexEdited(const QString &text);
    void onStepPaletteOffset(int deltaBytes);
    void onTilesPerRowChanged(int value);
    void onMaxTilesChanged(int value);
    void onOptionsToggled();
    void onStepOffset(int deltaBytes);
    void onZoomChanged(int value);
    void onExtractSpritesClicked();
    void onImportSheetClicked();
    void updateView();

private:
    void setupUi();
    void loadFileData();
    void populateBanks();
    void populateDetectedPalettes();
    void updatePaletteList();
    void updatePaletteStrip();

    QString            m_filePath;
    ProjectController *m_projectController = nullptr;
    QByteArray         m_romData;
    RomProbeResult     m_probeResult;

    // UI Controls
    QLabel      *m_lblConsole = nullptr;
    QLabel      *m_lblFileInfo = nullptr;
    QComboBox   *m_comboBanks = nullptr;
    QSpinBox    *m_spinOffset = nullptr;
    QLineEdit   *m_editOffsetHex = nullptr;
    QSlider     *m_sliderOffset = nullptr;
    QComboBox   *m_comboFormat = nullptr;
    QComboBox   *m_comboPattern = nullptr;
    QComboBox   *m_comboPalette = nullptr;
    QComboBox   *m_comboDetectedPalettes = nullptr;
    QLineEdit   *m_editPaletteHex = nullptr;
    QSpinBox    *m_spinPaletteOffset = nullptr;
    QWidget     *m_paletteStrip = nullptr;
    QSpinBox    *m_spinTilesPerRow = nullptr;
    QSpinBox    *m_spinMaxTiles = nullptr;
    QSlider     *m_sliderZoom = nullptr;
    QLabel      *m_lblZoomVal = nullptr;
    QCheckBox   *m_chkColor0Transp = nullptr;
    QCheckBox   *m_chkShowGrid = nullptr;
    QCheckBox   *m_chkDetectSprites = nullptr;
    QLabel      *m_lblPreview = nullptr;
    QScrollArea *m_scrollArea = nullptr;
    QLabel      *m_lblStats = nullptr;

    // Rendered state
    QImage           m_currentSheet;
    QList<QImage>    m_extractedFrames;
    QList<SpriteBox> m_extractedBoxes;
    TilePattern      m_currentPattern = TilePattern::Linear;
    QVector<QRgb>    m_activeCustomPalette;
    bool             m_useRomPalette = false;
    double           m_zoomFactor = 2.0;
};

#endif // RETROTILEEXPLORERDIALOG_H
