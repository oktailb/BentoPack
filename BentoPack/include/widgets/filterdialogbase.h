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

#ifndef FILTERDIALOGBASE_H
#define FILTERDIALOGBASE_H

#include <QDialog>
#include <QImage>
#include <QList>
#include <QMap>
#include <QTimer>
#include "model/spritedocument.h"

class QVBoxLayout;
class QHBoxLayout;
class QCheckBox;
class QLabel;
class QPushButton;
class QDialogButtonBox;
class QUndoStack;
class QUndoCommand;

/**
 * @brief Abstract base class for floating image and sprite processing filters (GIMP/Photoshop style).
 *
 * Provides:
 * - Automatic document state snapshots (atlas, frames, boxes, animations) upon dialog opening.
 * - Guaranteed non-destructive rollback on Cancel/Escape/close (reject()).
 * - Real-time live preview with single-shot debounce timer (default 80 ms).
 * - Standardized control bar: "Live preview" checkbox, dynamic status badge, "Reset defaults" button, and OK/Cancel button box.
 * - Seamless Undo/Redo integration: pushes created QUndoCommand onto QUndoStack on OK (accept()).
 */
#include "bentopackwidgets_export.h"

struct SpriteDetectionOptions;

class BENTOPACK_WIDGETS_EXPORT FilterDialogBase : public QDialog
{
    Q_OBJECT

public:
    explicit FilterDialogBase(SpriteDocument *doc,
                              QUndoStack *undoStack = nullptr,
                              QWidget *parent = nullptr);
    ~FilterDialogBase() override = default;

    SpriteDocument* document() const { return m_document; }
    QUndoStack* undoStack() const { return m_undoStack; }

    bool isLivePreviewEnabled() const;
    bool isAutoDetectBoxesEnabled() const;
    void setAutoDetectBoxesEnabled(bool enabled);
    void setStatusText(const QString &text);

public slots:
    void schedulePreview();
    void resetToDefaults();
    void reject() override;
    void accept() override;

protected:
    void changeEvent(QEvent *event) override;
    virtual void retranslateBaseUi();

    // Pure virtual lifecycle hooks to be implemented by derived filters
    virtual void applyPreview() = 0;
    virtual QUndoCommand* createUndoCommand() = 0;
    virtual void resetDefaults() = 0;
    virtual void saveSettings() {}

    // Default configuration for auto-detection checkbox
    virtual bool defaultAutoDetectBoxes() const { return false; }

    // Factorized bounding box and frame slicing helper for all derived filters
    void updatePreviewFramesAndBoxes(const QImage &previewAtlas,
                                    QList<QImage> &outFrames,
                                    QList<SpriteBox> &outBoxes,
                                    const SpriteDetectionOptions *customOpts = nullptr);

    // Layout extension point: derived dialogs add custom controls to this layout
    QVBoxLayout* contentLayout() const { return m_contentLayout; }

    // State restoration
    void restoreInitialState();

    // Document and undo stack pointers
    SpriteDocument*                 m_document;
    QUndoStack*                     m_undoStack;

    // Snapshot of document state when the dialog opened
    QImage                          m_initialAtlas;
    QList<QImage>                  m_initialFrames;
    QList<SpriteBox>                m_initialBoxes;
    QMap<QString, SpriteAnimation>  m_initialAnimations;

    // Preview state flag
    bool                            m_previewApplied = false;

    // Common UI controls
    QVBoxLayout*                    m_mainLayout = nullptr;
    QVBoxLayout*                    m_contentLayout = nullptr;
    QCheckBox*                      m_livePreviewCheck = nullptr;
    QCheckBox*                      m_autoDetectBoxesCheck = nullptr;
    QLabel*                         m_statusBadge = nullptr;
    QPushButton*                    m_resetDefaultsBtn = nullptr;
    QDialogButtonBox*               m_buttonBox = nullptr;

private slots:
    void onPreviewTimeout();

private:
    void setupBaseUI();

    QTimer                          m_debounceTimer;
};

#endif // FILTERDIALOGBASE_H
