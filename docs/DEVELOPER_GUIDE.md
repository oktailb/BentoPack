# 🛠️ Guide de Développement & d'Extension — BentoPack

Bienvenue dans le guide d'extension de **BentoPack**. Ce document s'adresse aux développeurs et contributeurs souhaitant enrichir BentoPack en concevant :
1. **De nouveaux Codecs d'I/O (`Extractor`)** : pour importer et exporter des formats d'atlas 2D, des séquences d'animation ou des structures de métadonnées spécifiques à des moteurs de jeu.
2. **De nouveaux Plugins de Filtres Graphiques (`FilterPlugin`)** : pour intégrer des algorithmes de retouche, de nettoyage, de transformation géométrique ou d'effets visuels avec prévisualisation en direct et annulation non-destructive.
3. **Des modules de traitement haute performance** : respectant la thread-safety, l'accès direct en mémoire scanline et l'internationalisation.

---

## 📑 Sommaire
- [1. Vue d'Ensemble de l'Architecture](#1-vue-densemble-de-larchitecture)
- [2. Développer un Nouveau Codec (`Extractor`)](#2-développer-un-nouveau-codec-extractor)
  - [2.1. Le contrat de l'interface `Extractor`](#21-le-contrat-de-linterface-extractor)
  - [2.2. Gestion structurée des erreurs (`ExtractorError`)](#22-gestion-structurée-des-erreurs-extractorerror)
  - [2.3. Enregistrement dans `ExtractorRegistry`](#23-enregistrement-dans-extractorregistry)
  - [2.4. Exemple complet : Codec d'export pour un moteur custom (`CustomEngineExtractor`)](#24-exemple-complet--codec-dexport-pour-un-moteur-custom-customengineextractor)
  - [2.5. Écrire un test unitaire automatisé headless](#25-écrire-un-test-unitaire-automatisé-headless)
- [3. Développer un Nouveau Plugin de Filtre (`FilterPlugin`)](#3-développer-un-nouveau-plugin-de-filtre-filterplugin)
  - [3.1. Le contrat d'interface `FilterPlugin`](#31-le-contrat-dinterface-filterplugin)
  - [3.2. Le socle interactif `FilterDialogBase`](#32-le-socle-interactif-filterdialogbase)
  - [3.3. Commandes d'annulation transactionnelles (`ApplyFilterCommand`)](#33-commandes-dannulation-transactionnelles-applyfiltercommand)
  - [3.4. Exemple complet : Filtre d'inversion colorimétrique (`InvertFilter`)](#34-exemple-complet--filtre-dinversion-colorimétrique-invertfilter)
  - [3.5. Enregistrement dans `FilterRegistry`](#35-enregistrement-dans-filterregistry)
- [4. Règles d'Or de Performance & Thread-Safety](#4-règles-dor-de-performance--thread-safety)
  - [4.1. Modèle 100% `QImage` en mémoire CPU contiguë](#41-modèle-100-qimage-en-mémoire-cpu-contiguë)
  - [4.2. Accès direct `scanLine()` vs `pixel()`](#42-accès-direct-scanline-vs-pixel)
  - [4.3. Déportation asynchrone non-bloquante (`QtConcurrent`)](#43-déportation-asynchrone-non-bloquante-qtconcurrent)
- [5. Internationalisation (i18n) & Bonnes Pratiques](#5-internationalisation-i18n--bonnes-pratiques)
- [6. Moteur Géométrique : Triangulation CDT & Maillages Anti-Overdraw](#6-moteur-géométrique--triangulation-cdt--maillages-anti-overdraw)
  - [6.1. Signatures et Contrats d'Invariance (`Triangulator`)](#61-signatures-et-contrats-dinvariance-triangulator)
  - [6.2. Format de Stockage dans `SpriteBox` & Métadonnées Projet](#62-format-de-stockage-dans-spritebox--métadonnées-projet)
  - [6.3. Injection dans les Extracteurs Moteurs (Godot 4, Unity, Unreal Paper2D)](#63-injection-dans-les-extracteurs-moteurs-godot-4-unity-unreal-paper2d)

---

## 1. Vue d'Ensemble de l'Architecture

BentoPack repose sur un patron architectural strict séparant le modèle de données, les codecs d'E/S, les commandes d'annulation et l'interface utilisateur :

```
                     ┌────────────────────────┐
                     │ External Files & Disks │
                     │ (.png, .json, .tres)   │
                     └───────────┬────────────┘
                                 │
                     ┌───────────┴────────────┐
                     │   ExtractorRegistry    │
                     │  (Codec Auto-detect)   │
                     └───────────┬────────────┘
                                 │ read() / write()
                                 ▼
                     ┌────────────────────────┐
                     │     SpriteDocument     │ ◄── Unique Source de Vérité
                     │  - QImage m_atlas      │     (Modèle 100% QImage CPU)
                     │  - QList<QImage> frames│
                     │  - QList<SpriteBox>    │
                     │  - QList<Animation>    │
                     └───────────┬────────────┘
                                 │
            ┌────────────────────┼────────────────────┐
            ▼                    ▼                    ▼
 ┌─────────────────────┐ ┌───────────────┐ ┌────────────────────┐
 │     QUndoStack      │ │ FilterPlugins │ │    View & GUI      │
 │ (ApplyFilterCommand,│ │ (Despill,     │ │ (AtlasView,        │
 │  EditPixelsCommand, │ │  Outline,     │ │  Filmstrip,        │
 │  AddSliceCommand)   │ │  ColorSwap)   │ │  PixelCanvas)      │
 └─────────────────────┘ └───────────────┘ └────────────────────┘
```

### Principes Fondamentaux :
1. **`SpriteDocument` est l'unique source de vérité** : Aucun codec ni widget ne stocke d'état de document concurrent.
2. **Découplage Headless Total** : `BentoPackCore` compile et s'exécute sans serveur d'affichage (`QT_QPA_PLATFORM=offscreen`).
3. **Réversibilité Absolue (Undo/Redo)** : Toute modification de géométrie, d'animation ou de pixels passe par `QUndoStack`.

---

## 2. Développer un Nouveau Codec (`Extractor`)

### 2.1. Le contrat de l'interface `Extractor`

Tous les codecs dérivent de la classe de base abstraite `Extractor` (`BentoPack/include/extractor/extractor.h`).

```cpp
class Extractor : public QObject
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

    explicit Extractor(QObject *parent = nullptr);
    ~Extractor() override = default;

    virtual QString name() const = 0;
    virtual QString description() const = 0;
    virtual QStringList supportedExtensions() const = 0;
    virtual Capabilities capabilities() const = 0;

    virtual bool canDecode(const QString &filePath) const = 0;

    virtual bool read(const QString &filePath,
                      SpriteDocument &outDoc,
                      ExtractorError *error = nullptr) = 0;

    virtual bool write(const QString &filePath,
                       const SpriteDocument &inDoc,
                       const ExportOptions &options = ExportOptions(),
                       ExtractorError *error = nullptr) = 0;
};
```

---

### 2.2. Gestion structurée des erreurs (`ExtractorError`)

Ne renvoyez jamais un simple `false` silencieux. Renseignez la structure typée `ExtractorError` :

```cpp
if (!file.open(QIODevice::ReadOnly)) {
    if (error) {
        error->code = ExtractorError::FileNotReadable;
        error->filePath = filePath;
        error->message = QObject::tr("Impossible d'ouvrir le fichier en lecture : %1").arg(file.errorString());
    }
    return false;
}
```

Codes d'erreur standards disponibles : `FileNotFound`, `FileNotReadable`, `FileNotWritable`, `InvalidHeader`, `CorruptedData`, `UnsupportedVersion`, `ParsingFailed`, `WriteFailed`.

---

### 2.3. Enregistrement dans `ExtractorRegistry`

Les codecs sont enregistrés auprès du singleton `ExtractorRegistry` (`BentoPack/include/extractor/extractorregistry.h`).

Dans `BentoPack/src/extractor/extractorregistry.cpp` :
```cpp
#include "extractor/monmoteurextractor.h"

void ExtractorRegistry::registerDefaultExtractors()
{
    // ...
    registerExtractor(std::make_unique<MonMoteurExtractor>());
}
```

Le registre sélectionne automatiquement le codec approprié selon la méthode `canDecode(filePath)` ou l'extension du fichier.

---

### 2.4. Exemple complet : Codec d'export pour un moteur custom (`CustomEngineExtractor`)

Voici un exemple complet d'un codec d'export exportant un atlas PNG et un descripteur d'animations JSON compact pour un moteur de jeu 2D.

#### En-tête : `customengineextractor.h`
```cpp
#ifndef CUSTOMENGINEEXTRACTOR_H
#define CUSTOMENGINEEXTRACTOR_H

#include "extractor/extractor.h"

class CustomEngineExtractor : public Extractor
{
    Q_OBJECT
public:
    explicit CustomEngineExtractor(QObject *parent = nullptr);
    ~CustomEngineExtractor() override = default;

    QString name() const override { return QStringLiteral("Mon Moteur 2D"); }
    QString description() const override { return QStringLiteral("Export d'atlas et animations pour MonMoteur"); }
    QStringList supportedExtensions() const override { return { QStringLiteral("mm2d"), QStringLiteral("json") }; }
    Capabilities capabilities() const override { return CanExport | SupportsAnimations | SupportsAtlasMetadata; }

    bool canDecode(const QString &filePath) const override;

    bool read(const QString &filePath,
              SpriteDocument &outDoc,
              ExtractorError *error = nullptr) override;

    bool write(const QString &filePath,
               const SpriteDocument &inDoc,
               const ExportOptions &options = ExportOptions(),
               ExtractorError *error = nullptr) override;
};

#endif // CUSTOMENGINEEXTRACTOR_H
```

#### Implémentation : `customengineextractor.cpp`
```cpp
#include "customengineextractor.h"
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

CustomEngineExtractor::CustomEngineExtractor(QObject *parent)
    : Extractor(parent)
{
}

bool CustomEngineExtractor::canDecode(const QString &filePath) const
{
    return filePath.endsWith(QStringLiteral(".mm2d"), Qt::CaseInsensitive);
}

bool CustomEngineExtractor::read(const QString &filePath, SpriteDocument &outDoc, ExtractorError *error)
{
    Q_UNUSED(outDoc);
    if (error) {
        error->code = ExtractorError::UnsupportedFormat;
        error->filePath = filePath;
        error->message = tr("L'importation de ce format n'est pas supportée.");
    }
    return false;
}

bool CustomEngineExtractor::write(const QString &filePath,
                                  const SpriteDocument &inDoc,
                                  const ExportOptions &options,
                                  ExtractorError *error)
{
    Q_UNUSED(options);
    QFileInfo fi(filePath);
    QString imagePath = fi.dir().filePath(fi.baseName() + QStringLiteral(".png"));

    // 1. Sauvegarde de la texture atlas
    if (!inDoc.atlas().save(imagePath, "PNG")) {
        if (error) {
            error->code = ExtractorError::WriteFailed;
            error->filePath = imagePath;
            error->message = tr("Échec de l'écriture du fichier d'atlas PNG.");
        }
        return false;
    }

    // 2. Génération des métadonnées JSON
    QJsonObject rootObj;
    rootObj[QStringLiteral("texture")] = fi.baseName() + QStringLiteral(".png");
    rootObj[QStringLiteral("width")] = inDoc.atlas().width();
    rootObj[QStringLiteral("height")] = inDoc.atlas().height();

    // Export des frames & boîtes
    QJsonArray framesArray;
    const auto &boxes = inDoc.boxes();
    for (int i = 0; i < boxes.size(); ++i) {
        const auto &box = boxes[i];
        QJsonObject frameObj;
        frameObj[QStringLiteral("index")] = i;
        frameObj[QStringLiteral("x")] = box.rect.x();
        frameObj[QStringLiteral("y")] = box.rect.y();
        frameObj[QStringLiteral("w")] = box.rect.width();
        frameObj[QStringLiteral("h")] = box.rect.height();

        // Support du pivot précis (M3)
        QPoint p = box.effectivePivot();
        frameObj[QStringLiteral("pivot_x")] = p.x();
        frameObj[QStringLiteral("pivot_y")] = p.y();

        framesArray.append(frameObj);
    }
    rootObj[QStringLiteral("frames")] = framesArray;

    // Export des animations (M2)
    QJsonArray animsArray;
    for (const auto &anim : inDoc.animations()) {
        QJsonObject animObj;
        animObj[QStringLiteral("name")] = anim.name;
        animObj[QStringLiteral("fps")] = anim.fps;
        animObj[QStringLiteral("loop_mode")] = static_cast<int>(anim.loopMode);

        QJsonArray seqArray;
        for (int frameIdx : anim.frames) {
            seqArray.append(frameIdx);
        }
        animObj[QStringLiteral("sequence")] = seqArray;
        animsArray.append(animObj);
    }
    rootObj[QStringLiteral("animations")] = animsArray;

    // Écriture sur disque
    QFile jsonFile(filePath);
    if (!jsonFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (error) {
            error->code = ExtractorError::FileNotWritable;
            error->filePath = filePath;
            error->message = tr("Impossible d'écrire le fichier descripteur : %1").arg(jsonFile.errorString());
        }
        return false;
    }

    jsonFile.write(QJsonDocument(rootObj).toJson(QJsonDocument::Indented));
    return true;
}
```

---

### 2.5. Écrire un test unitaire automatisé headless

Tous les codecs doivent être validés par des tests sans serveur graphique (`QtTest`) dans `tests/test_extractors.cpp` :

```cpp
void TestExtractors::testCustomEngineExport()
{
    SpriteDocument doc;
    // Préparation d'une planche de test 64x64
    QImage testImg(64, 64, QImage::Format_ARGB32);
    testImg.fill(Qt::blue);
    doc.setAtlas(testImg);

    SpriteBox box;
    box.rect = QRect(0, 0, 32, 32);
    box.pivot = QPoint(16, 32);
    box.hasCustomPivot = true;
    doc.setBoxes({ box });

    CustomEngineExtractor extractor;
    QString outPath = QDir::temp().filePath("test_export.mm2d");
    ExtractorError err;
    bool ok = extractor.write(outPath, doc, ExportOptions(), &err);

    QVERIFY2(ok, qPrintable(err.toString()));
    QVERIFY(QFile::exists(outPath));
    QVERIFY(QFile::exists(QDir::temp().filePath("test_export.png")));

    // Nettoyage
    QFile::remove(outPath);
    QFile::remove(QDir::temp().filePath("test_export.png"));
}
```

---

## 3. Développer un Nouveau Plugin de Filtre (`FilterPlugin`)

### 3.1. Le contrat d'interface `FilterPlugin`

L'interface `FilterPlugin` (`BentoPack/include/filters/filterplugin.h`) encapsule les métadonnées d'un filtre et sa fabrique de boîte de dialogue :

```cpp
class FilterPlugin
{
public:
    virtual ~FilterPlugin() = default;

    virtual QString id() const = 0;              // Identifiant unique (ex: "color_invert")
    virtual QString name() const = 0;            // Nom affiché (ex: tr("Inverser les Couleurs"))
    virtual QString description() const = 0;     // Infobulle détaillée
    virtual QString category() const = 0;        // Catégorie de regroupement dans le menu
    virtual QKeySequence shortcut() const;       // Raccourci clavier par défaut (optionnel)
    virtual QIcon icon() const;                  // Icône d'action (optionnel)

    virtual FilterDialogBase* createDialog(SpriteDocument *doc,
                                           QUndoStack *undoStack = nullptr,
                                           QWidget *parent = nullptr) = 0;
};
```

---

### 3.2. Le socle interactif `FilterDialogBase`

Toute boîte de dialogue de filtre dérive de `FilterDialogBase` (`BentoPack/include/widgets/filterdialogbase.h`).

Ce socle prend automatiquement en charge :
- **L'instantané d'état initial** : sauvegarde transparente de l'atlas, des frames, des boîtes et des animations à l'ouverture.
- **La prévisualisation en direct (`applyPreview()`)** : temporisée par un timer anti-rebond (debounce à 80 ms) pour garantir la réactivité sans saturer le processeur.
- **La barre d'action standardisée** : Case *Aperçu direct*, case *Détection auto des boîtes*, badge d'état dynamique, bouton *Réinitialiser* et boutons *Valider/Annuler*.
- **L'annulation garantie non-destructive (`reject()`)** : En cas de fermeture, touche `Échap` ou clic sur *Annuler*, l'état initial complet est restauré à l'identique.
- **L'enregistrement dans l'historique d'annulation (`accept()`)** : À la validation, la commande `createUndoCommand()` est automatiquement poussée sur la pile `QUndoStack`.

#### Méthodes virtuelles pures à implémenter :
```cpp
virtual void applyPreview() = 0;
virtual QUndoCommand* createUndoCommand() = 0;
virtual void resetDefaults() = 0;
```

---

### 3.3. Commandes d'annulation transactionnelles (`ApplyFilterCommand`)

Pour assurer une annulation atomique (`Ctrl+Z`), utilisez `ApplyFilterCommand` (`BentoPack/include/commands/filtercommands.h`) :

```cpp
QUndoCommand* MonFilterDialog::createUndoCommand()
{
    return new ApplyFilterCommand(
        m_document,
        m_previewAtlas,    // Atlas après filtrage
        m_previewFrames,   // Frames découpées
        m_previewBoxes,    // Boîtes de délimitation
        tr("Appliquer Mon Filtre")
    );
}
```

---

### 3.4. Exemple complet : Filtre d'inversion colorimétrique (`InvertFilter`)

Voici l'implémentation complète d'un filtre inversant les couleurs RGB tout en préservant le canal alpha et les bordures.

#### 1. Dialogue : `invertfilterdialog.h`
```cpp
#ifndef INVERTFILTERDIALOG_H
#define INVERTFILTERDIALOG_H

#include "widgets/filterdialogbase.h"

class QCheckBox;

class InvertFilterDialog : public FilterDialogBase
{
    Q_OBJECT
public:
    explicit InvertFilterDialog(SpriteDocument *doc,
                                QUndoStack *undoStack = nullptr,
                                QWidget *parent = nullptr);
    ~InvertFilterDialog() override = default;

protected:
    void applyPreview() override;
    QUndoCommand* createUndoCommand() override;
    void resetDefaults() override;

private:
    QCheckBox *m_chkInvertAlpha = nullptr;
    QImage     m_filteredAtlas;
    QList<QImage> m_filteredFrames;
    QList<SpriteBox> m_filteredBoxes;
};

#endif // INVERTFILTERDIALOG_H
```

#### 2. Dialogue : `invertfilterdialog.cpp`
```cpp
#include "invertfilterdialog.h"
#include "commands/filtercommands.h"
#include <QCheckBox>
#include <QVBoxLayout>

InvertFilterDialog::InvertFilterDialog(SpriteDocument *doc, QUndoStack *undoStack, QWidget *parent)
    : FilterDialogBase(doc, undoStack, parent)
{
    setWindowTitle(tr("Inverser les Couleurs"));

    m_chkInvertAlpha = new QCheckBox(tr("Inverser également le canal Alpha"), this);
    contentLayout()->addWidget(m_chkInvertAlpha);

    connect(m_chkInvertAlpha, &QCheckBox::toggled, this, &FilterDialogBase::schedulePreview);

    // Première prévisualisation
    schedulePreview();
}

void InvertFilterDialog::resetDefaults()
{
    m_chkInvertAlpha->setChecked(false);
}

void InvertFilterDialog::applyPreview()
{
    if (!m_document) return;

    QImage img = m_initialAtlas.copy();
    bool invertAlpha = m_chkInvertAlpha->isChecked();

    const int height = img.height();
    const int width = img.width();

    // Accès contigu haute performance par ligne
    for (int y = 0; y < height; ++y) {
        QRgb *scanline = reinterpret_cast<QRgb*>(img.scanLine(y));
        for (int x = 0; x < width; ++x) {
            QRgb pixel = scanline[x];
            int r = 255 - qRed(pixel);
            int g = 255 - qGreen(pixel);
            int b = 255 - qBlue(pixel);
            int a = invertAlpha ? (255 - qAlpha(pixel)) : qAlpha(pixel);
            scanline[x] = qRgba(r, g, b, a);
        }
    }

    m_filteredAtlas = img;
    updatePreviewFramesAndBoxes(m_filteredAtlas, m_filteredFrames, m_filteredBoxes);

    // Application en temps réel sur le document
    m_document->setAtlas(m_filteredAtlas);
    m_document->setFrames(m_filteredFrames);
    m_document->setBoxes(m_filteredBoxes);

    setStatusText(tr("Inversion appliquée (%1 frames)").arg(m_filteredFrames.size()));
}

QUndoCommand* InvertFilterDialog::createUndoCommand()
{
    return new ApplyFilterCommand(
        m_document,
        m_filteredAtlas,
        m_filteredFrames,
        m_filteredBoxes,
        tr("Inverser les Couleurs")
    );
}
```

#### 3. Déclaration du Plugin : `invertfilter.h`
```cpp
#ifndef INVERTFILTER_H
#define INVERTFILTER_H

#include "filters/filterplugin.h"
#include "invertfilterdialog.h"

class InvertFilter : public FilterPlugin
{
public:
    QString id() const override { return QStringLiteral("invert_color"); }
    QString name() const override { return QObject::tr("Inversion de Couleurs..."); }
    QString description() const override { return QObject::tr("Inverse les canaux RGB du sprite."); }
    QString category() const override { return QObject::tr("Effets"); }
    QKeySequence shortcut() const override { return QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_I); }

    FilterDialogBase* createDialog(SpriteDocument *doc,
                                   QUndoStack *undoStack = nullptr,
                                   QWidget *parent = nullptr) override
    {
        return new InvertFilterDialog(doc, undoStack, parent);
    }
};

#endif // INVERTFILTER_H
```

---

### 3.5. Enregistrement dans `FilterRegistry`

Dans `BentoPack/src/filters/filterregistry.cpp` :
```cpp
#include "filters/invertfilter.h"

void FilterRegistry::registerDefaultFilters()
{
    // ...
    registerPlugin(std::make_unique<InvertFilter>());
}
```

Le filtre apparaîtra automatiquement dans la barre de menus sous le menu **Filtres > Effets > Inversion de Couleurs...** avec son raccourci `Ctrl+Shift+I` !

---

## 4. Règles d'Or de Performance & Thread-Safety

### 4.1. Modèle 100% `QImage` en mémoire CPU contiguë
Ne stockez **JAMAIS** de `QPixmap` dans `SpriteDocument` ni dans les algorithmes de traitement.
- `QPixmap` est une ressource dépendante du serveur d'affichage graphique (X11/Wayland/GDI/Metal). Son instanciation en arrière-plan (`QThread` ou `QtConcurrent`) provoque des crashs fatals intermittents.
- Le modèle de BentoPack est **strictement composé de `QImage`**. La conversion vers `QPixmap` (`QPixmap::fromImage()`) est réservée aux composants finaux d'affichage UI (`AtlasViewController`, `TimelineFilmstripWidget`).

---

### 4.2. Accès direct `scanLine()` vs `pixel()`
N'utilisez jamais `image.pixel(x, y)` ou `image.setPixelColor(x, y)` dans des boucles de traitement d'atlas. Ces méthodes effectuent des vérifications de bornes superflues et un déréférencement à chaque appel.

```cpp
// ❌ TRÈS LENT (120 ms sur un atlas 2048x2048) :
for (int y = 0; y < img.height(); ++y) {
    for (int x = 0; x < img.width(); ++x) {
        QColor c = img.pixelColor(x, y);
        // ...
        img.setPixelColor(x, y, newColor);
    }
}

// ✅ ULTRA-RAPIDE (4 ms sur un atlas 2048x2048) :
const int h = img.height();
const int w = img.width();
for (int y = 0; y < h; ++y) {
    QRgb *scanline = reinterpret_cast<QRgb*>(img.scanLine(y));
    for (int x = 0; x < w; ++x) {
        QRgb p = scanline[x];
        // Calcul contigu en mémoire directe
        scanline[x] = transformPixel(p);
    }
}
```

---

### 4.3. Déportation asynchrone non-bloquante (`QtConcurrent`)
Pour les calculs lourds (ex: empaquetage polygonal M8, segmentation automatique, compression VRAM), utilisez `QtConcurrent::run` couplé à `QFutureWatcher` :

```cpp
QFutureWatcher<PackResult> *watcher = new QFutureWatcher<PackResult>(this);
connect(watcher, &QFutureWatcher<PackResult>::finished, this, [this, watcher]() {
    applyResult(watcher->result());
    watcher->deleteLater();
});

// Exécution sur le pool de threads de travail
watcher->setFuture(QtConcurrent::run([opts, frames]() {
    return HeavyPacker::pack(opts, frames);
}));
```

---

## 5. Internationalisation (i18n) & Bonnes Pratiques

BentoPack supporte intégralement le Français (`fr_FR`), l'Anglais (`en_US`) et le Japonais (`ja_JA`) avec basculement dynamique à chaud (`QEvent::LanguageChange`).

### Règle d'or sur les `Q_OBJECT` et contextes de traduction :
Si votre dialogue ou widget hérite de `Q_OBJECT`, le contexte de traduction utilisé par `tr()` est exactement le nom de la classe.  
> **Attention aux Namespaces C++ :**  
> Une classe déclarée dans `namespace Foo { class MyDialog : public QDialog { Q_OBJECT ... }; }` utilisera `"Foo::MyDialog"` comme contexte.  
> Pour maintenir l'harmonie avec les catalogues `.ts`, définissez les dialogues dans le namespace global ou declarez un alias.

### Mettre à jour les catalogues :
Après avoir ajouté de nouvelles chaînes `tr("Nouvelle Fonctionnalité")`, exécutez le script d'extraction :
```bash
python scripts/update_i18n.py
```
Puis recompilez les catalogues avec `lrelease` ou via le build CMake (`cmake --build build`).

---

## 6. Moteur Géométrique : Triangulation CDT & Maillages Anti-Overdraw

BentoPack intègre dans son namespace `BentoPackGeometry` une suite complète de triangulation 2D (Ear-Clipping et Constrained Delaunay Triangulation — CDT) conçue pour éradiquer l'overdraw GPU tout en fournissant des structures de maillage optimisées pour le rigging et l'animation squelettique 2D.

### 6.1. Signatures et Contrats d'Invariance (`Triangulator`)

Les méthodes clés sont exposées par la classe statique `Triangulator` (`BentoPack/include/geometry/triangulator.h`) :

```cpp
namespace BentoPackGeometry {

class BENTOPACK_CORE_EXPORT Triangulator
{
public:
    static QList<int> triangulate(const QPolygonF &polygon);

    static QList<int> triangulateCDT(const QPolygonF &outerPolygon, 
                                     const QList<QPointF> &allVertices);

    static SmartMeshResult generateSmartMesh(const QPolygonF &outerPolygon,
                                             const QImage &image,
                                             const SmartMeshParams &params = SmartMeshParams());

    static double calculateArea(const QPolygonF &polygon);
    static double calculateOverdrawSavings(const QPolygonF &polygon, const QSize &boxSize);
};

} // namespace BentoPackGeometry
```

#### Contrats d'Invariance et Garanties Algorithmiques :
1. **Préservation Stricte des Contraintes Frontières (*Boundary Constraint Preservation*) :**
   - L'enveloppe rigide `outerPolygon` définit la frontière infranchissable du sprite.
   - Lors de la triangulation CDT (`triangulateCDT`), les arêtes formant `outerPolygon` sont obligatoirement présentes comme segments dans le maillage triangulé final.
2. **Élagage Hors-Frontière de Bowyer-Watson (*Boundary Clipping & Hollow Gap Pruning*) :**
   - L'algorithme incrémental de Bowyer-Watson opère initialement sur un super-triangle englobant.
   - Tous les triangles générés dont le centroïde se situe hors du polygone frontière (`poly.containsPoint(centroid, Qt::OddEvenFill) == false`) sont strictement éliminés.
   - De même, pour les formes concaves (ex: formes en U, bras écartés) ou comportant des cavités transparentes, toute arête de triangle coupant le contour extérieur sans être une arête légitime est rejetée.
3. **Enroulement Counter-Clockwise (CCW) Garanti :**
   - Chaque triplet d'indices `[i0, i1, i2]` renvoyé dans `triangles` respecte un enroulement direct (anti-horaire) :
     $$\det \begin{pmatrix} x_1 - x_0 & x_2 - x_0 \\ y_1 - y_0 & y_2 - y_0 \end{pmatrix} > 0$$
   - Cela assure une compatibilité native et immédiate avec le backface-culling des moteurs graphiques modernes (OpenGL, Vulkan, Metal, Direct3D).
4. **Résilience aux Cas Dégénérés (*Corner Cases*) :**
   - *Sprite 1x1 ou monochrome vide :* Si le contour détecté a moins de 3 sommets ou une aire nulle, `generateSmartMesh` renvoie un quad minimal sans crash ni boucle infinie.
   - *Garde-fou d'intégrité polygonale :* La suppression de sommets (au couteau ou via la touche Suppr) impose un seuil minimal absolu de $N \ge 3$ sommets.

---

### 6.2. Format de Stockage dans `SpriteBox` & Métadonnées Projet

L'état géométrique de chaque frame est encapsulé dans `SpriteBox` (`BentoPack/include/model/spritedocument.h`) :

```cpp
struct SpriteBox {
    QRect           rect;            // Rectangle englobant dans l'atlas (coordonnées globales atlas)
    QPoint          pivot;           // Point d'ancrage relatif au coin supérieur gauche de rect
    bool            hasCustomPivot;

    // Données de maillage polygonal 2D
    bool            hasPolygonMesh;  // True si un maillage polygonal est actif
    QPolygonF       polygon;         // Enveloppe extérieure rigide (repère local [0, W] x [0, H])
    QList<QPointF>  vertices;        // Ensemble des sommets (contour + points Steiner intérieurs)
    QList<int>      triangles;       // Triplets d'indices [i0, i1, i2, ...] indexant vertices
    
    double polygonArea() const;
    double overdrawSavings() const;
};
```

#### Sérialisation dans le fichier `.bento` (`project.json`) :
```json
{
  "rect": [128, 64, 48, 64],
  "pivot": [24, 60],
  "hasPolygonMesh": true,
  "polygon": [0.0, 12.0, 14.0, 0.0, 48.0, 20.0, 36.0, 64.0, 8.0, 64.0],
  "vertices": [0.0, 12.0, 14.0, 0.0, 48.0, 20.0, 36.0, 64.0, 8.0, 64.0, 24.0, 32.0],
  "triangles": [0, 1, 5, 1, 2, 5, 2, 3, 5, 3, 4, 5, 4, 0, 5]
}
```

---

### 6.3. Injection dans les Extracteurs Moteurs (Godot 4, Unity, Unreal Paper2D)

Lors de l'implémentation d'un nouvel `Extractor` ou de l'export vers les moteurs de jeu, voici la convention de conversion recommandée :

#### 1. Godot 4 (`ArrayMesh` / `Polygon2D`) :
```gdscript
var arr_mesh = ArrayMesh.new()
var arrays = []
arrays.resize(Mesh.ARRAY_MAX)

# Sommets 2D convertis en Vector2 ou Vector3
var godot_vertices = PackedVector2Array()
var godot_uvs = PackedVector2Array()
for pt in box.vertices:
    godot_vertices.append(Vector2(pt.x - box.pivot.x, pt.y - box.pivot.y))
    godot_uvs.append(Vector2((box.rect.x + pt.x) / atlas_width, (box.rect.y + pt.y) / atlas_height))

arrays[Mesh.ARRAY_VERTEX] = godot_vertices
arrays[Mesh.ARRAY_TEX_UV] = godot_uvs
arrays[Mesh.ARRAY_INDEX] = PackedInt32Array(box.triangles)

arr_mesh.add_surface_from_arrays(Mesh.PRIMITIVE_TRIANGLES, arrays)
```

#### 2. Unity (`Sprite.OverrideGeometry`) :
```csharp
// Pour un Sprite Unity configuré en SpriteMeshType.Tight :
Vector2[] unityVertices = new Vector2[box.vertices.Count];
Vector2[] uvs = new Vector2[box.vertices.Count];

for (int i = 0; i < box.vertices.Count; i++) {
    var pt = box.vertices[i];
    // Conversion repère Unity (Y inversé)
    unityVertices[i] = new Vector2(pt.x - box.pivot.x, (box.rect.height - pt.y) - (box.rect.height - box.pivot.y)) / pixelsPerUnit;
    uvs[i] = new Vector2((box.rect.x + pt.x) / atlasWidth, 1.0f - (box.rect.y + pt.y) / atlasHeight);
}

ushort[] unityTriangles = box.triangles.Select(idx => (ushort)idx).ToArray();
sprite.OverrideGeometry(unityVertices, unityTriangles);
```

#### 3. Unreal Engine (`UPaperSprite` / Render Geometry) :
Dans Unreal Paper2D, la géométrie de rendu personnalisée est définie via `FSpriteGeometryCollection`. Les triplets d'indices de `box.triangles` définissent les triangles des polygones de rendu de la structure `FPaperSpriteSocket` et du mesh tight render, réduisant drastiquement l'instruction count des pixel shaders 2D.
