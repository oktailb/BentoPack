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

#ifndef RETROPALETTEFILTERDIALOG_H
#define RETROPALETTEFILTERDIALOG_H

#include "widgets/filterdialogbase.h"
#include "image/colorpalettepresets.h"
#include <QVector>
#include <QColor>

class QComboBox;
class QSlider;
class QSpinBox;
class QCheckBox;
class QPushButton;
class QLabel;
class QScrollArea;

/**
 * @brief Floating interactive tool dialog for Retro Palette Quantization & Bayer Dithering.
 */
class RetroPaletteFilterDialog : public FilterDialogBase
{
    Q_OBJECT

public:
    enum Preset {
        Standard = ColorPalettePresets::Standard,
        GameBoyDMG = ColorPalettePresets::GameBoyDMG,
        GameBoyPocket = ColorPalettePresets::GameBoyPocket,
        Pico8 = ColorPalettePresets::Pico8,
        NES = ColorPalettePresets::NES,
        SNES = ColorPalettePresets::SNES,
        Commodore64 = ColorPalettePresets::Commodore64,
        Amiga = ColorPalettePresets::Amiga,
        PCEngine = ColorPalettePresets::PCEngine,
        CGAMode1 = ColorPalettePresets::CGAMode1,
        CGAMode2 = ColorPalettePresets::CGAMode2,
        Endesga32 = ColorPalettePresets::Endesga32,
        Custom = ColorPalettePresets::Custom
    };

    enum DitherMatrix {
        DitherNone = 0,
        Bayer2x2 = 1,
        Bayer4x4 = 2,
        Bayer8x8 = 3
    };

    explicit RetroPaletteFilterDialog(SpriteDocument *doc,
                                      QUndoStack *undoStack = nullptr,
                                      QWidget *parent = nullptr);
    ~RetroPaletteFilterDialog() override = default;

    Preset activePreset() const;
    DitherMatrix ditherMatrix() const;
    int ditherStrength() const;
    bool isSelectedFramesOnly() const;
    QVector<QRgb> currentPalette() const;

    /**
     * @brief Parses palette from a file (.hex, .gpl, .pal, or .png image).
     */
    static QVector<QRgb> loadPaletteFromFile(const QString &filePath, QString *outError = nullptr);

    /**
     * @brief Gets standard colors for a built-in preset.
     */
    static QVector<QRgb> getPresetPalette(Preset preset);

    /**
     * @brief Executes palette quantization with optional Bayer dithering.
     */
    static QImage applyRetroPalette(const QImage &source,
                                    const QVector<QRgb> &palette,
                                    DitherMatrix dither,
                                    int strengthPercent,
                                    const QList<QRect> &targetAreas = QList<QRect>());

protected:
    void applyPreview() override;
    QUndoCommand* createUndoCommand() override;
    void resetDefaults() override;
    void saveSettings() override;
    bool defaultAutoDetectBoxes() const override { return false; }

private slots:
    void onParametersChanged();
    void onPresetChanged(int index);
    void importPalette();

private:
    void setupFilterUI();
    void updatePalettePreview();

    // Preview state
    QImage                          m_previewAtlas;
    QList<QImage>                  m_previewFrames;
    QList<SpriteBox>                m_previewBoxes;

    // Active palette colors
    QVector<QRgb>                   m_activePalette;
    QVector<QRgb>                   m_customPalette;

    // UI controls
    QComboBox*                      m_presetCombo = nullptr;
    QPushButton*                    m_importBtn = nullptr;
    QWidget*                        m_swatchContainer = nullptr;
    QComboBox*                      m_ditherCombo = nullptr;
    QSlider*                        m_strengthSlider = nullptr;
    QSpinBox*                       m_strengthSpin = nullptr;
    QCheckBox*                      m_selectedFramesOnlyCheck = nullptr;
    QLabel*                         m_paletteInfoLabel = nullptr;
};

#endif // RETROPALETTEFILTERDIALOG_H
