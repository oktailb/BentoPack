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
