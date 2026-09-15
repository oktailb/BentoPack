# 📋 Feuille de Route & Spécifications Métier — Sprite Studio

Ce document détaille la planification des fonctionnalités métier de **Sprite Studio**.  
L'objectif est d'élever l'application d'un simple outil de découpe technique au rang d'**atelier complet de préparation, retouche et séquençage de sprites 2D pour le jeu vidéo et le pixel art**.

---

## 🗺️ Vue d'Ensemble des Chantiers Métier

| ID | Chantier | Priorité | Complexité | Statut |
|---|---|---|---|---|
| **M0** | [Assainissement Architectural & Dette Technique (Audit Critique)](#m0--assainissement-architectural--dette-technique-audit-critique) | **Haute** | Haute | 🟢 Clôturé & Validé (73 tests CTest 100% — Multiplateforme) |
| **M1** | [Édition Interactive des Bounding Boxes (Atlas Slicing)](#m1--édition-interactive-des-bounding-boxes-atlas-slicing) | **Haute** | Moyenne | 🟢 Clôturé & Validé (100% — Poignées, Group Drag, Shift Slice) |
| **M2** | [Gestionnaire Complet d'Animations & Timeline](#m2--gestionnaire-complet-danimations--timeline) | **Haute** | Moyenne | 🟢 Clôturé & Validé (100% CTest — Ergonomie Splitters, Filmstrip Drag&Drop, LoopModes, Undo/Redo) |
| **M3** | [Points d'Ancrage & Pivots (Origins & Offsets)](#m3--points-dancrage--pivots-origins--offsets) | **Moyenne** | Faible | 📝 Planifié |
| **M4** | [Outil d'Édition de Pixels (Pixel Art Retouching)](#m4--outil-dédition-de-pixels-pixel-art-retouching) | **Moyenne** | Haute | 📝 Planifié |
| **M5** | [Format de Projet Natif (`.ssp` - Sprite Studio Project)](#m5--format-de-projet-natif-ssp---sprite-studio-project) | **Haute** | Faible | 🟢 Clôturé & Validé (85 tests CTest 100% — Session, Lock, Crash Recovery, Atomic Save, LibGit2 Find) |
| **M6** | [Algorithme d'Empaquetage Avancé (MaxRects Bin-Packing)](#m6--algorithme-dempaquetage-avancé-maxrects-bin-packing) | **Basse** | Moyenne | 📝 Planifié |
| **M7** | [Suppression Avancée de Fond & Segmentation Robuste (JPEG Bruités, Anti-Halo)](#m7--suppression-avancée-darrière-plan--segmentation-robuste-planches-jpeg-bruit-anti-halo) | **Moyenne** | Moyenne | 📝 Notes & Pistes Techniques |
| **M8** | [Empaquetage Polygonal & Maillages Serrés (Polygon / Tight Mesh Packing)](#m8--empaquetage-polygonal--maillages-serrés-polygon--tight-mesh-packing) | **Basse** | Haute | 📝 Spécifications Détaillées |
| **ASSETS** | [Remplacement des Échantillons (`sample/`) par des Assets Originaux (Libres de Droits)](#assets--remplacement-des-échantillons-sample-par-des-assets-originaux-libres-de-droits) | **Moyenne** | Faible | 📝 Planifié (Création de sprites originaux & pérennisation des tests) |
| **AUDIT** | [Points à Revoir & Dette Technique Résiduelle (Recommandations d'Amélioration)](#️-audit--points-de-vigilance--dette-technique-résiduelle-recommandations-damélioration) | **Haute** | Moyenne | 📝 À Traiter (Architecture, QImage, CMake, CI/CD, Optimisation O(N²)) |

---

## M0 : Assainissement Architectural & Dette Technique (Audit Critique)

### Contexte & Constat d'Audit
L'application souffre d'une transition inachevée entre un code impératif legacy et une architecture orientée document. Pour garantir la maintenabilité des futurs modules (timeline M2, éditeur pixel M4), les faiblesses structurelles suivantes doivent être traitées :

### 1. Dualité des Modèles de Données (`SpriteDocument` vs `Extractor`) — ✅ TERMINÉ
- **État :** ✅ **Réfracté & Validé par tests unitaires**
- **Réalisations :**
  - Élimination complète de l'état interne dans `Extractor` (`m_frames`, `m_atlas`, `m_atlas_index`, `m_animationsData` supprimés).
  - Suppression intégrale de la synchronisation bidirectionnelle fragile (`syncToDocument()` / `syncFromDocument()` obsolète).
  - `SpriteDocument` est désormais l'**unique source de vérité** pour toute l'application.
  - Standardisation du contrat de codec d'I/O pur :
    - `read(filePath, outDoc, &error)`
    - `write(filePath, inDoc, options, &error)`
    - `canDecode(filePath)`
    - Structure typée d'erreur `ExtractorError` (codes `FileNotFound`, `CorruptedData`, `ParsingFailed`, etc.).
  - Tous les 4 codecs refactorisés et testables en mode sans interface graphique (headless) :
    - `SpriteExtractor` (PNG, JPG, BMP)
    - `GifExtractor` (GIF animé via `QImageReader` + `AtlasPacker`)
    - `JsonExtractor` (TexturePacker & Aseprite JSON import/export)
    - `GodotExtractor` (Godot 4 `.tres` SpriteFrames import/export)
  - Suite de tests unitaires automatisée `tests/test_extractors.cpp` intégrée à CMake/CTest (10 tests, 100% succès).

### 2. Monolithe "God Object" `MainWindow` — ✅ TERMINÉ
- **État :** ✅ **Réfracté & Validé par tests unitaires (18 tests, 100% succès)**
- **Réalisations :**
  - Démantèlement complet du "God Object" `MainWindow` en 3 contrôleurs autonomes :
    - `AtlasViewController` (`include/controller/atlasviewcontroller.h` / `src/controller/atlasviewcontroller.cpp`) : gestion exclusive de `QGraphicsView`, zoom/pan, dessin interactif de découpe (`ToolAddSlice`), synchronisation des `AtlasBoxItem`, sélection par boîte et marquee selection, commandes de découpe (`Trim`, `Merge`, `Delete`, `Nudge`).
    - `AnimationController` (`include/controller/animationcontroller.h` / `src/controller/animationcontroller.cpp`) : intégration du lecteur `AnimationPlayer`, arborescence `QTreeWidget`, rendu de frame sur scène d'aperçu, cadences FPS, synchronisation de l'animation active et commandes d'animation (`CreateAnimationCommand`, `ReverseAnimationCommand`, `DeleteAnimationCommand`).
    - `ProjectController` (`include/controller/projectcontroller.h` / `src/controller/projectcontroller.cpp`) : chargement/sauvegarde de fichiers via les codecs `ExtractorRegistry`, gestion persistante de l'historique des fichiers récents (`QSettings`), suppression automatique d'arrière-plan de l'atlas.
  - Transformation de `MainWindow` en orchestrateur léger reliant les signaux/slots des contrôleurs et déléguant l'ensemble de la logique métier (taille réduite de 72%, suppression des membres monolithiques).
  - Résolution du segfault lors de la sélection au lasso / marquee :
    - Détection et coupure de la récursion infinie de signaux entre `AtlasViewController::selectionChanged` et `AnimationController::updateCurrentAnimation` / `selectAnimation("current")`.
    - Optimisation de la sélection par glissement : mise à jour visuelle légère des contours en cours de drag, application effective du document au relâchement de la souris (`endMarqueeSelection`).
    - Suppression du rafraîchissement destructif de colonnes (`m_treeWidget->clear()` et `resizeColumnToContents` en boucle qui surchargeaient le heap via `QTextEngine::itemize` / `RtlAllocateHeap`).
  - Suite de tests unitaires dédiée `tests/test_controllers.cpp` (18 tests couvrant les 3 contrôleurs en mode headless/offscreen, la sélection rectangulaire avec modificateurs Ctrl/Shift et la non-récursion des signaux croisés, 100% succès sous CTest).

### 3. Gestion Mémoire & Pratiques Modernes C++17 — ✅ TERMINÉ
- **État :** ✅ **Réfracté & Validé par tests unitaires**
- **Réalisations :**
  - **Adoption de `std::unique_ptr` et élimination des `delete` manuels :**
    - `ExtractorRegistry` gère désormais ses instances d'extracteurs possédés via `std::vector<std::unique_ptr<Extractor>>`. Destructeur automatique et RAII garanti.
    - `MainWindow::ui` et `jsonExtractorDialog::ui` migrés vers `std::unique_ptr`, suppression intégrale des `delete ui;` manuels.
    - Hiérarchie d'ownership QObject clarifiée pour l'ensemble des sous-modèles (`ArrangementModel`, `FrameDelegate`, `AnimationPlayer`).
  - **Sécurisation du cycle de vie des objets `QGraphicsScene` :**
    - Unification du nettoyage dans `AtlasViewController::clearAtlas()` (détachement et libération explicite des items de preview `m_newSlicePreviewItem` et `m_selectionRectItem` avant l'appel à `m_scene->clear()`).
    - Destructeur `~AtlasViewController()` simplifié et sécurisé contre les doubles libérations et pointeurs pendants.
  - **Gestionnaire central de configuration maintenable (`AppConfig`) :**
    - Création de `include/config/appconfig.h` et `src/config/appconfig.cpp` produisant et chargeant un fichier JSON propre et indenté (`spritestudio_config.json`).
    - Localisation hybride : répertoire local/portable en priorité, puis chemin système standard `QStandardPaths::AppConfigLocation`.
    - Tolérance totale aux pannes (*fail-safe*) : en cas de syntaxe JSON corrompue ou de champs manquants suite à une édition manuelle par l'utilisateur, l'application ne crashe jamais et bascule automatiquement sur les valeurs par défaut saines en consignant un avertissement.
  - **Élimination complète des constantes magiques disséminées :**
    - `AtlasBoxItem` : dimensions des poignées (`handleSize`), marges de survol (`handleMargin`), taille minimale (`minSliceSize`) et palette de couleurs complète (boîtes sélectionnées, non sélectionnées, survol, badges) branchées sur `AppConfig`.
    - `AtlasViewController` : pas de zoom (`zoomStep`), bornes min/max (`zoomMin`, `zoomMax`), seuil alpha par défaut (`defaultAlphaThreshold`), pas de déplacement clavier (`nudgeStepSmall`, `nudgeStepLarge`), padding de cadrage (`fitViewPadding`) et couleurs d'aperçu lues depuis `AppConfig`.
    - `AnimationController` : cadence FPS par défaut (`defaultFps`), bornes min/max (`minFps`, `maxFps`) pilotées par `AppConfig`.
    - `ProjectController` : nombre maximum de fichiers récents (`maxRecentFiles`), tolérance de suppression d'arrière-plan (`backgroundRemovalTolerance`) et seuil alpha (`backgroundMinAlpha`) issus de `AppConfig`.
  - **Suite de tests automatisée étendue :**
    - 3 nouveaux tests unitaires dans `tests/test_controllers.cpp` (`testAppConfigDefaults`, `testAppConfigSaveAndLoad`, `testAppConfigCorruptJsonFallback`) portant la suite à 21 tests (100% de succès sous CTest).

### 4. Performance & Traitement d'Images sur le Thread Principal — ✅ TERMINÉ
- **État :** ✅ **Réfracté, Accéléré & Validé par tests unitaires**
- **Réalisations :**
  - **Accès direct en mémoire contiguë (`scanLine()` / `constScanLine()`) :**
    - `SpriteExtractor` : réécriture intégrale du flood-fill et du calcul de composantes connexes. Élimination des appels `QImage::pixel(x, y)` et de `QStack<QPoint>` au profit d'un pointeur par ligne `constScanLine(y)`, indexation 1D contiguë `y * w + x` et pile plate `std::vector<Point2D>`.
    - Découpage et extraction des frames (`outFrames`) par écriture directe en mémoire de scanline (`frame.scanLine(ly)`), supprimant les boucles imbriquées lentes.
    - `ProjectController::removeBackgroundFromImage` : passage à `constScanLine(y)`, échantillonnage par pas de 2 avec table de hachage $O(1)$ `QHash<QRgb, int>` et suppression directe en mémoire ligne par ligne (`scanLine(y)`).
    - **Résultat de benchmark :** Découpage de 64 composantes sur une image 512x512 exécuté en **7 ms** seulement !
  - **Déportation des traitements lourds en asynchrone (`QtConcurrent` / `QFutureWatcher`) :**
    - Ajout de `openFileAsync(filePath)` et `removeAtlasBackgroundAndRefreshAsync(...)` dans `ProjectController`.
    - Exécution du décodage d'image, du flood-fill, de la segmentation et de la suppression de fond sur un thread de travail en arrière-plan sans bloquer l'interface utilisateur.
    - Câblage non bloquant dans `MainWindow` : mise en place de curseurs d'attente dynamiques (`Qt::WaitCursor`), barre de progression réactive, notifications d'état et reconversion sécurisée des frames `QImage` en textures `QPixmap` uniquement sur le thread GUI principal lors de `onAsyncJobFinished()`.
  - **Optimisation mémoire de la pile Undo/Redo (`QUndoStack`) :**
    - Ajout du paramètre configurable `undoLimit` (50 par défaut) dans `AppConfig` (`projectConfig`).
    - Migration des structures de sauvegarde de commandes (`DeleteFramesCommand`, `MergeFramesCommand`) : remplacement des `QPixmap` par des `QImage` brutes, éliminant les fuites de descripteurs GDI/GPU en RAM lors des opérations d'annulation/rétablissement répétées.
  - **Découplage architectural de la vision par ordinateur (`SpriteDetector`) :**
    - Extraction intégrale de la détection de silhouettes et de composantes connexes hors de `SpriteExtractor` vers un moteur autonome `SpriteDetector` (`include/image/spritedetector.h` / `src/image/spritedetector.cpp`).
    - Respect strict du principe de responsabilité unique (SRP) : les `Extractor` redeviennent des codecs de formats de fichiers purs. `SpriteDetector` est directement utilisable par `ProjectController`, le futur éditeur de pixels ou tout codec sans couplage artificiel.
  - **Nettoyage d'atlas : Effacement destructif de pixels (`Shift + Suppr`) :**
    - Création de `EraseAtlasPixelsCommand` (`include/commands/commands.h`) : permet d'effacer les pixels de l'atlas sous les rectangles sélectionnés (remplissage à `alpha = 0`) tout en supprimant les tranches associées, avec support complet de l'annulation (`Ctrl+Z`) et du rétablissement (`Ctrl+Y`).
    - Intégration du raccourci clavier `Shift + Delete` et d'une action dédiée dans le menu contextuel clic-droit de l'atlas (*Erase Pixels from Atlas*).
  - **Fluidification du lecteur d'animation & Auto-play :**
    - **Maintien de la lecture active** : la modification de la sélection de frames n'interrompt plus brutalement la lecture en cours si le player tournait déjà.
    - **Auto-play configurable** : ajout de `autoPlayOnSelection` dans `AppConfig` (`AnimationConfig`). Dès qu'au moins 2 frames sont sélectionnées, le player démarre automatiquement la boucle. Sélectionner 1 seule frame affiche cette frame en pause.
    - **Raccourci universel `Espace`** : la barre d'espace bascule `Play / Pause` depuis n'importe où dans la fenêtre principale sans conflit avec les champs textuels.
  - **Suite de tests automatisée étendue :**
    - Nouveaux tests dans `tests/test_extractors.cpp` (`testExtractToImagesEquivalence`, `testExtractPerformance`, `testSpriteDetectorBasics`).
    - Nouveaux tests dans `tests/test_controllers.cpp` (`testProjectControllerOpenAsync`, `testProjectControllerRemoveBgAsync`, `testUndoStackLimitAndImageStorage`, `testAnimationControllerAutoPlay`, `testAtlasViewControllerErasePixels`, `testAtlasViewControllerMultiSelectAndDelete`).
    - Total de **43 tests unitaires individuels** sous CTest avec **100% de réussite**.

### 5. Standardisation de l'Ergonomie & Internationalisation (i18n) — ✅ TERMINÉ
- **État :** ✅ **Réfracté, Standardisé & Validé par tests unitaires**
- **Réalisations :**
  - **Zoom interactif centré sur la souris (`zoomAt`) :**
    - Suppression intégrale de `resetTransform()` qui réinitialisait la position de la vue au centre à chaque coup de molette.
    - Implémentation de `AtlasViewController::zoomAt(viewportPos, step)` : calcul précis du point de scène sous le pointeur (`mapToScene`), mise à l'échelle continue et compensation immédiate des barres de défilement (`horizontalScrollBar`, `verticalScrollBar`).
    - Déplacement et zoom parfaitement fluides, stabilité au pixel près validée par test unitaire automatisé (`diff = QPointF(0, 0)`).
  - **Standardisation stricte de l'internationalisation sous le format `KEY_...` :**
    - Remplacement de tous les libellés codés en dur, des chaînes bilingues (`Trim to Pixels / Ajuster aux pixels`) et des anciennes clés à underscores (`_file_error`) par une nomenclature claire en majuscules :
      - Menus & Actions : `KEY_MENU_FILE`, `KEY_ACTION_OPEN`, `KEY_ACTION_SAVE`, `KEY_ACTION_EXPORT`, `KEY_ACTION_EXIT`, `KEY_ACTION_UNDO`, `KEY_ACTION_REDO`, `KEY_ACTION_REMOVE_BG`...
      - Outils de slicing : `KEY_TOOL_SELECT`, `KEY_TOOL_ADD_SLICE`, `KEY_TOOL_TRIM`, `KEY_TOOL_REMOVE_BG` et leurs infobulles `KEY_TOOLTIP_...`.
      - Menus contextuels atlas & animation : `KEY_CTX_CREATE_ANIM`, `KEY_CTX_REVERSE_ANIM`, `KEY_CTX_DELETE_ANIM`, `KEY_CTX_TRIM_SLICE`, `KEY_CTX_MERGE_SLICES`, `KEY_CTX_DELETE_FRAMES`, `KEY_CTX_ERASE_PIXELS`, `KEY_CTX_REMOVE_BG`, `KEY_CTX_INVERT_SEL`.
      - Messages & dialogues : `KEY_DIALOG_OPEN_TITLE`, `KEY_DIALOG_ABOUT_TITLE`, `KEY_MSG_LOAD_ERROR`, `KEY_MSG_SAVE_ERROR`, `KEY_STATUS_READY`, `KEY_LABEL_TIMING`...
    - **Visibilité immédiate des manques :** Si une traduction est omise dans les fichiers `.ts`/`.qm`, la clé brute `KEY_...` s'affiche directement dans l'interface graphique, rendant toute régression ou oubli immédiatement détectable visuellement.
    - Synchronisation et traduction intégrale (100%) des catalogues linguistiques `sprite_studio_fr_FR.ts`, `sprite_studio_en_US.ts` et `sprite_studio_ja_JA.ts`.
    - Fallback automatique dans `main.cpp` vers la langue anglaise `sprite_studio_en_US` si la locale système de l'utilisateur n'est pas prise en charge.
  - **Intégration d'icônes modernes sur la barre d'outils de découpe :**
    - Ajout de 4 icônes PNG nettes 24x24 (`icons/tool_select.png`, `icons/tool_slice.png`, `icons/tool_trim.png`, `icons/tool_remove_bg.png`) compilées dans le fichier de ressource `images` (`:/drawer/...`).
    - Présentation visuelle soignée avec icônes aux côtés du texte (`Qt::ToolButtonTextBesideIcon`).
  - **Perspectives ergonomiques (Listes des Sprites et Animations) :**
    - Les chantiers d'ergonomie avancée sur les listes (redimensionnement dynamique des vignettes par curseur/Ctrl+Molette, badges animés, timeline filmstrip et drag-and-drop fluide) sont consignés pour le chantier d'enrichissement de l'animation **M2**.
  - **Suite de tests automatisée étendue :**
    - Deux nouveaux tests unitaires dans `tests/test_controllers.cpp` : `testAtlasViewControllerMouseCenteredZoom` (vérification de la stabilité de la position sous le curseur) et `testI18nKeyTranslations` (vérification du chargement et du comportement des dictionnaires FR, EN et du fallback sur les clés non traduites).
    - Suite de tests globale portée à **45 tests unitaires individuels** sous CTest avec **100% de succès**.

### 6. DevOps, Tests Automatisés & Qualité Multiplateforme — ✅ TERMINÉ
- **État :** ✅ **Validé & Testé (3 suites de tests CTest, 73 tests unitaires, 100% succès)**
- **Réalisations :**
  - **Infrastructure de tests complète sous `QTest` / `CTest` :**
    - Architecture modulaire articulée en 3 suites de tests spécialisées exécutables sans interface graphique (`QT_QPA_PLATFORM=offscreen`) :
      - `test_extractors` (13 tests) : décodage et encodage des codecs (`SpriteExtractor`, `GifExtractor`, `JsonExtractor`, `GodotExtractor`), détection de contours et segmentation par vision par ordinateur (`SpriteDetector`).
      - `test_controllers` (32 tests) : orchestration haut-niveau (`ProjectController`, `AnimationController`, `AtlasViewController`), zoom interactif centré au pixel près (`zoomAt`), gestion de configuration tolérante aux pannes (`AppConfig`), pile d'annulation et catalogues i18n trilingues (FR, EN, JA).
      - `test_core` (28 tests) : empaquetage d'atlas (`AtlasPacker`), modèle de données documentaire (`SpriteDocument`), commandes réversibles (`QUndoCommand`) et robustesse multiplateforme.
  - **Couverture exhaustive de l'empaqueteur d'atlas (`AtlasPacker`) :**
    - Algorithmes d'étagère (`RowPacker`) et de grille (`GridPacker`) testés sur des jeux de sprites hétérogènes.
    - Vérification stricte du non-chevauchement des boîtes calculées (`!rectA.intersects(rectB)`), du respect du rembourrage (*padding*) anti-saignement (*texture bleeding*) et de la fidélité des pixels copiés.
    - Algorithme en puissances de deux (`PowerOfTwoPacker`) : garantie que les dimensions de l'atlas résultant sont rigoureusement des puissances de 2 ($2^n$, standard OpenGL, Vulkan, DirectX et Metal).
    - Empaquetage ciblé par sous-ensemble d'indices de frames (`packIndices`).
  - **Couverture rigoureuse du modèle central (`SpriteDocument`) :**
    - Insertion (`insertFrame`), remplacement (`replaceFrame`), suppressions multiples ordonnées (`removeFrames`), permutation d'ordre (`reorderFrames`) avec propagation automatique aux indices d'animation.
    - Fusion géométrique de frames (`mergeFrames`) avec calcul d'union de rectangles.
    - Calcul fin de rognage alpha (`computeTrimmedRect`) selon différents seuils de transparence (0, 1, 128, 255) et gestion des frames vides.
  - **Couverture isolée de l'ensemble des commandes `QUndoCommand` :**
    - `AddSliceCommand`, `ChangeBoxRectCommand`, `CreateAnimationCommand`, `ReverseAnimationCommand`, `DeleteAnimationCommand`, `MergeFramesCommand`, `DeleteFramesCommand`, `EraseAtlasPixelsCommand`.
    - Vérification systématique de la réversibilité stricte : application (`redo`), retour à l'état initial (`undo`) et réapplication conforme.
  - **Priorité Multiplateforme (Windows, Linux, Apple macOS, Haiku OS) :**
    - **Résolution des chemins :** Prise en charge universelle des séparateurs de dossiers (`\` sous Windows, `/` sous POSIX/Linux, macOS et Haiku) et insensibilité à la casse des extensions dans `projectName()`.
    - **Alignement mémoire & endianness :** Validation de l'intégrité des pixels 32-bit ARGB et préservation des canaux alpha sans artefact sur architectures standard.
    - **Configuration système portable :** Résolution adaptative via `QStandardPaths::AppConfigLocation` (`%APPDATA%` sous Windows, `~/.config` sous Linux, `Library/Application Support` sous macOS, `~/config/settings` sous Haiku).
    - **Déploiement multiplateforme (CPack) :** Configurations prêtes pour Windows (`NSIS;ZIP`), Linux (`TGZ;RPM;DEB`), macOS (`PACKAGEMAKER;DRAGANDROP;BUNDLE`) et Haiku (`haiku.PackageInfo`).
  - *Note :* L'automatisation GitHub Actions est conservée pour une étape ultérieure conformément à la demande de l'utilisateur.

---

## M1 : Édition Interactive des Bounding Boxes (Atlas Slicing)

### Contexte & Objectif
La détection automatique par seuil alpha ou tolérance de couleur est efficace pour des planches simples, mais montre ses limites sur des sprites découpés en plusieurs morceaux disjoints (ex. un projectile séparé du personnage, des effets de particules, des membres détachés).  
L'utilisateur doit pouvoir ajuster visuellement et manuellement les boîtes de découpe directement sur la vue de l'atlas.

### Spécifications Fonctionnelles Initiales
1. **Interaction Directe sur la Vue Atlas (`QGraphicsScene`) :**
   - **Poignées de redimensionnement (Handles) :** Chaque boîte sélectionnée affiche 8 poignées (coins et milieux des côtés) avec changement dynamique du curseur (`Qt::SizeHorCursor`, `Qt::SizeVerCursor`, `Qt::SizeFDiagCursor`, `Qt::SizeBDiagCursor`).
   - **Déplacement à la souris :** Glisser-déposer une boîte pour corriger son positionnement.
   - **Déplacement au clavier :** Déplacement fin au pixel près via les flèches directionnelles (`Shift + Flèches` pour un pas de 10 px).
2. **Création Manuelle de Boîte :**
   - Outil *Rectangle / New Slice* dans la barre d'outils : cliquer-glisser pour tracer une nouvelle boîte de découpe sur l'atlas.
   - Génération instantanée de la frame correspondante dans la liste des frames.
3. **Opérations Contextuelles (Menu Clic Droit & Raccourcis) :**
   - **Ajuster aux pixels (*Trim / Shrink to Alpha*) :** Réduire automatiquement le rectangle sélectionné au bounding box exact des pixels non transparents contenus à l'intérieur.
   - **Fusionner les boîtes sélectionnées (*Merge Slices*) :** Englober plusieurs rectangles en une seule boîte englobante commune.
   - **Supprimer (*Delete*) :** Supprimer la boîte et la frame associée (touche `Suppr`).
4. **Intégration Undo / Redo :**
   - Chaque déplacement, redimensionnement ou création doit passer par `QUndoCommand` pour permettre l'annulation (`Ctrl+Z`).

### 📊 Point d'Étape & Bilan de Clôture (Statut : 🟢 100% — Validé sous CTest)

| Spécification M1 | Statut | Composant / Fichier | Diagnostic & Observations |
|---|:---:|---|---|
| **Poignées de redimensionnement (8 Handles)** | ✅ **RÉSOLU & VALIDÉ** | `atlasboxitem.h` / `.cpp` | 8 poignées cosmétiques à dimension stable à l'écran, sans chevauchement ni explosion visuelle à fort zoom (x4, x8, x16) pour le pixel art. |
| **Déplacement souris (Drag & Drop)** | ✅ **RÉSOLU & VALIDÉ** | `atlasboxitem.cpp`, `atlasviewcontroller.cpp` | Déplacement fluide mono et multi-sélection (group drag) synchronisé en temps réel avec contrainte aux bornes de l'atlas et commande/macro Undo unique. |
| **Déplacement clavier (Flèches, Shift)** | ✅ **RÉSOLU & VALIDÉ** | `mainwindow.cpp` / `mainwindow_events.cpp` | Priorisation établie : le stepping de l'animation cède le pas dès qu'une boîte est sélectionnée pour permettre le déplacement fin au pixel. |
| **Création manuelle (Outil Add Slice)** | ✅ **RÉSOLU & VALIDÉ** | `atlasviewcontroller.cpp` | Rebasculement automatique sur `ToolSelect`, ou maintien de l'outil pour découpes rapides enchaînées si la touche `Shift` est maintenue. |
| **Génération instantanée de la frame** | ✅ **RÉSOLU & VALIDÉ** | `spritedocument.cpp`, `mainwindow_atlas.cpp` | La frame est créée directement dans `SpriteDocument` avec notification par signaux. |
| **Trim to Pixels (Shrink to Alpha)** | ✅ **RÉSOLU & VALIDÉ** | `spritedocument.cpp`, `mainwindow_atlas.cpp` | Calcul de boîte englobante opaque opérationnel (mono et multi-sélection). |
| **Merge Slices (Fusion)** | ✅ **RÉSOLU & VALIDÉ** | `mainwindow_atlas.cpp`, `commands.cpp` | Opérationnel via clic droit (si ≥ 2 boîtes). Utilise `MergeFramesCommand`. |
| **Suppression (Touche Suppr / Shift+Suppr)** | ✅ **RÉSOLU & VALIDÉ** | `mainwindow_events.cpp`, `commands.cpp` | `Suppr` pour suppression de la boîte/frame, `Shift+Suppr` pour effacement destructif de pixels sur l'atlas avec Undo. |
| **Badge d'index de frame** | ✅ **RÉSOLU & VALIDÉ** | `atlasboxitem.cpp` | Tracé cosmétique local isolé dans le repère de la vue, éliminant tout débordement de police textuelle à fort zoom. |
| **Intégration Undo / Redo** | ✅ **RÉSOLU & VALIDÉ** | `commands.h` / `commands.cpp` | Toutes les modifications géométriques (mono et groupe), créations, fusions et suppressions passent par `QUndoStack`. |

---

## M2 : Gestionnaire Complet d'Animations & Rôle de `animationList`

### Contexte & Rôle Actuel de `animationList` dans `animationArea`
Dans l'interface actuelle, le bloc de droite `animationArea` combine :
1. La vue de prévisualisation (`graphicsViewResult`) avec les boutons de lecture (`Play`, `Pause`), le champ `FPS` et un indicateur de timing.
2. Des contrôles temporels (`sliderFrom`, `timeFrom`, `timeTo`) initialement pensés pour le scrubbing temporel.
3. Le widget `animationList` (`QTreeWidget`), situé en bas de `animationArea`, avec 3 colonnes : `Name`, `FPS`, `Frames`.

**Rôle actuel de `animationList` :**
- Sert de sélecteur d'animation active pour le lecteur : cliquer sur un item déclenche immédiatement la lecture de l'animation correspondante.
- Héberge l'animation spéciale pseudo-dynamique `"current"` (qui reflète à la volée la sélection active dans la liste centrale des frames `frameList`).
- Dispose d'un menu contextuel (clic droit) permettant de supprimer, renommer ou inverser l'ordre des frames d'une animation.

### Limites Identifiées & Axes d'Évolution de `animationList`
1. **Affichage textuel brut des frames :**  
   La colonne `Frames` affiche une chaîne de texte séparée par des virgules (`1, 2, 3, 4, 5...`), ce qui devient illisible dès qu'une animation dépasse une dizaine de frames et n'offre aucune interaction visuelle.
   La posibilité de re-ordoner/sequencer les frames manque.
2. **Ambiguïté entre sélection temporaire et animation sauvegardée :**  
   L'item `"current"` cohabite avec les animations réelles du projet (`idle`, `walk`), ce qui peut prêter à confusion. Il faut rendre cette distinction évidente (ex. statut visuel distinct, icône dédiée, ou bouton explicite "Créer une animation depuis la sélection").
3. **Contrôles d'actions manquants :**  
   L'ajout d'une animation dépend actuellement d'une sélection puis d'une commande indirecte (menu contextuel) ; il manque une barre d'outils compacte au-dessus ou en pied de `animationList` avec boutons d'action visibles : `+ Nouveau`, `- Supprimer`, `Dupliquer`, `Renommer`.
4. **Clarification de la zone `DataTiming` (`sliderFrom` / `timeFrom`) :**  
   Remplacer les champs `QTimeEdit` (peu adaptés aux frames de jeux vidéo) par un véritable curseur de scrubbing image par image (`Frame X / Total`, curseur de tête de lecture) connecté au player.
5. **Évolution du player :** 
   - Ajouter un slider pour la durée de lecture de l'animation qui recalcule le FPS en fonction du nombre de frames.
   - Clarifier le bouton existant pour inverser le sens de lecture.
   - Zoomer correctement l'animation en cours
   - Permettre d'editer facilement la position de chaque frame (offset/point fixe) -> cf M3

### Spécifications Fonctionnelles Cibles
1. **Évolution du widget `animationList` :**
   - **Barre d'actions intégrée :** Boutons iconiques `+` (nouvelle animation vide ou depuis sélection), `-` (supprimer), `Dupliquer`, `Inverser`.
   - **Édition directe (Inline) :** Double-clic sur le nom pour renommer, double-clic sur le FPS pour le modifier directement dans le tableau.
   - **Propriétés étendues par animation :**
     - Colonne Mode de boucle : `Loop` (boucle standard), `Once` (lecture unique avec arrêt sur la dernière frame), `Ping-Pong` (aller-retour).
     - Nombre total de frames et durée en millisecondes.
2. **Visualisation / Timeline de l'Animation Active :**
   - Plutôt que d'écrire les numéros en texte brut dans la colonne, le clic sur une animation affiche sous le lecteur (ou dans un volet rétractable) un **ruban de vignettes (Filmstrip)** montrant les sprites ordonnés de la séquence.
   - Glisser-déposer sur ce ruban pour réordonner, insérer ou supprimer des frames de l'animation sélectionnée.
3. **Contrôles du Lecteur :**
   - Barre de progression pas-à-pas synchrone avec la frame en cours de lecture.
   - Raccourcis clavier : `Espace` (Play/Pause), `Flèche Gauche / Droite` (Frame step).

### Fichiers & Composants Réalisés
- `SpriteStudio/src/mainwindow.ui` : Réorganisation ergonomique par `QSplitter` horizontal et vertical avec `QTabWidget` inférieur (`[🎞️ Timeline]`, `[🗃️ Atlas Frames]`), barre de transport moderne et barre d'outils d'animations.
- `SpriteStudio/include/widgets/timelinefilmstripwidget.h` & `src/widgets/timelinefilmstripwidget.cpp` : Ruban de vignettes ordonnées, surbrillance temps réel, réordonnancement par glisser-déposer, duplication/suppression/sélection.
- `SpriteStudio/include/model/spritedocument.h` & `src/model/spritedocument.cpp` : `SpriteAnimation::LoopMode` (`Loop`, `Once`, `PingPong`), `durationMs()`, et méthodes documentaires de séquence.
- `SpriteStudio/include/animation/animationplayer.h` & `src/animation/animationplayer.cpp` : Moteur de lecture prenant en charge `PingPong`, `Once`, `advanceFrame()`, `firstFrame()`, `lastFrame()`, `seek()`.
- `SpriteStudio/include/commands/commands.h` & `src/commands/commands.cpp` : Commandes Undo/Redo (`RenameAnimationCommand`, `DuplicateAnimationCommand`, `ReorderAnimationFramesCommand`, `ChangeAnimationPropertiesCommand`).
- `SpriteStudio/src/controller/animationcontroller.cpp` : Synchronisation bidirectionnelle scrubber/timeline/list/preview, édition inline (double-clic nom et FPS).
- `SpriteStudio/src/project/projectmanager.cpp` : Sérialisation et désérialisation de `loop_mode` dans `.ssp`.

| Spécification M2 | Statut | Composant / Fichier | Diagnostic & Observations |
|---|:---:|---|---|
| **Disposition IHM ergonomique (Splitters)** | ✅ **RÉSOLU & VALIDÉ** | `mainwindow.ui`, `mainwindow.cpp` | `mainSplitter` horizontal et `leftSplitter` vertical fluides. Les panneaux s'adaptent dynamiquement. |
| **Onglets inférieurs (Timeline & Atlas)** | ✅ **RÉSOLU & VALIDÉ** | `mainwindow.ui`, `mainwindow.cpp` | `QTabWidget` documentaire fluide permettant de basculer instantanément entre la Timeline active et les Frames découpées de l'atlas. |
| **Timeline Filmstrip interactive** | ✅ **RÉSOLU & VALIDÉ** | `timelinefilmstripwidget.cpp` | Ruban horizontal de vignettes carrées numérotées, surbrillance dynamique de la tête de lecture, drag & drop réversible pour réordonner les étapes. |
| **Modes de boucle (Loop, Once, Ping-Pong)** | ✅ **RÉSOLU & VALIDÉ** | `animationplayer.cpp`, `spritedocument.h` | 3 modes de lecture supportés dans le moteur et sérialisés de manière rétrocompatible dans `.ssp`. |
| **Barre de Transport moderne (Scrubber)** | ✅ **RÉSOLU & VALIDÉ** | `mainwindow.ui`, `animationcontroller.cpp` | Curseur pas-à-pas avec compteur dynamique `Frame X / Total (ms)`, boutons `First`, `Prev`, `Play/Pause`, `Next`, `Last`. |
| **Barre d'outils d'animations** | ✅ **RÉSOLU & VALIDÉ** | `mainwindow_animation.cpp` | Boutons `+ Nouveau`, `+ Depuis Sélection`, `📋 Dupliquer`, `⇄ Inverser`, `🗑 Supprimer`. |
| **Édition inline (Nom & FPS)** | ✅ **RÉSOLU & VALIDÉ** | `animationcontroller.cpp` | Double-clic sur le nom ou le FPS dans `animationList` avec validation et annulation Undo/Redo. |
| **Raccourcis clavier transport** | ✅ **RÉSOLU & VALIDÉ** | `mainwindow_events.cpp` | `Espace` (Play/Pause), `Flèches Gauche/Droite` (Step frame si aucune boîte atlas sélectionnée), `Home` / `End` (Première / Dernière frame). |
| **Mémorisation des panneaux (Docks)** | ✅ **RÉSOLU & VALIDÉ** | `mainwindow.cpp`, `mainwindow_events.cpp` | Sauvegarde/restauration pérenne sous `QSettings` (`saveState()`, `restoreState()`), auto-sauvegarde immédiate sur déplacement, fermeture, flottement et redimensionnement. |
| **Sélecteur de langue dans les Réglages** | ✅ **RÉSOLU & VALIDÉ** | `settingsdialog.cpp`, `appconfig.cpp`, `main.cpp` | Choix de langue (Système, FR, EN, JA) dans la page Générale des Réglages, persistance JSON et chargement au démarrage. |
| **Tests unitaires automatisés (100% CTest)** | ✅ **RÉSOLU & VALIDÉ** | `test_controllers.cpp`, `test_project.cpp` | Validation de Ping-Pong, Once, duplication, renommage, réordonnancement, persistance des docks et des réglages de langue. |

---

## M3 : Points d'Ancrage & Pivots (Origins & Offsets)

### Contexte & Objectif
Lorsqu'un personnage donne un coup d'épée ou saute, la boîte de découpe de chaque frame change souvent de taille. Si les frames sont centrées arbitrairement sans point d'ancrage commun, le personnage "saute" ou glisse visuellement dans le moteur de jeu.  
Le point d'ancrage (ou pivot) définit le point de référence (souvent au niveau des pieds ou au centre du corps) pour aligner rigoureusement les frames.

### Spécifications Fonctionnelles
1. **Édition Visuelle du Pivot :**
   - Affichage d'un réticule / mire (croix colorée semi-transparente) sur la vue de la frame ou dans le lecteur d'animation.
   - Déplacement interactif à la souris du point de pivot.
   - Préréglages rapides en un clic par frame:
     - `Bottom-Center` (standard pour personnages au sol).
     - `Center` (standard pour projectiles, vaisseaux, effets visuels).
     - `Top-Left` (standard pour éléments d'interface).
     - `Custom (X, Y)` avec champs numériques spinbox.
2. **Portée d'Application :**
   - Bouton "Appliquer à toute l'animation" ou "Appliquer à tous les sprites".
3. **Export dans les Moteurs de Jeux :**
   - **Godot 4 :** Enregistrement dans le champ d'offset ou la sous-ressource de l'AtlasTexture.
   - **JSON (TexturePacker / Aseprite) :** Calcul des champs `spriteSourceSize`, `sourceSize` et offset correspondant.

### Fichiers & Composants Cibles
- `SpriteStudio/include/model/spritedocument.h` : Ajout de `QPoint origin` dans `SpriteBox`.
- `SpriteStudio/src/extractor/godotextractor.cpp` et `jsonextractor.cpp` : Prise en compte dans la sérialisation.

---

## M4 : Outil d'Édition de Pixels (Pixel Art Retouching)

### Contexte & Objectif
Les utilisateurs perdent un temps précieux s'ils doivent rouvrir Aseprite ou Photoshop pour corriger un unique pixel oublié, enlever un artefact de compression, ou boucher un trou transparent.  
Sprite Studio doit intégrer un mini-éditeur de pixels intégré dédié à la retouche rapide de frames.

### Spécifications Fonctionnelles
1. **Espace de Travail Pixel Art :**
   - Dialogue ou dock dédié s'ouvrant sur la frame active (double-clic sur une frame).
   - Niveau de zoom élevé (de 100% à 3200%) avec affichage optionnel de la **grille de pixels (*Pixel Grid*)**.
   - Fond en damier pour visualiser la transparence.
2. **Boîte à Outils Fondamentale :**
   - **Crayon (*Pencil*) :** Dessin au pixel (taille 1px ou brosses carrées 2px, 3px).
   - **Gomme (*Eraser*) :** Efface en restaurant l'alpha à 0.
   - **Pipette (*Eyedropper*) :** Prélèvement de couleur sur la frame courante ou sur la palette.
   - **Remplissage (*Paint Bucket*) :** Remplissage par flot (Flood Fill) des pixels contigus de même couleur/alpha.
3. **Gestion de la Palette & Couleurs :**
   - Sélecteur de couleur avec canaux RGBA et code Hexadécimal.
   - Bandeau d'historique des couleurs récemment utilisées.
   - Palette auto-extraite des couleurs uniques présentes dans le sprite en cours d'édition.
4. **Synchronisation & Undo/Redo :**
   - Historique Undo/Redo dédié à l'éditeur de pixels.
   - Dès validation ou en temps réel : mise à jour de la frame dans la liste, dans l'atlas composite et dans le lecteur d'animation.

### Fichiers & Composants Cibles
- `SpriteStudio/include/pixeleditor/pixeleditordialog.h` (ou `pixeleditorwidget.h`).
- `SpriteStudio/src/pixeleditor/pixeleditorcanvas.cpp` : Canvas dérivé de `QGraphicsView` ou `QWidget` gérant le tracé pixelisé sans lissage (filtrage `Qt::FastTransformation`).

---

## M5 : Format de Projet Natif (`.ssp` - Sprite Studio Project) — ✅ TERMINÉ & VALIDÉ (100%)

### Contexte & Objectif
Actuellement, si un utilisateur découpe 50 frames, crée 4 animations, règle des FPS et retire le fond, toutes ces métadonnées de montage sont perdues à la fermeture de l'application s'il n'a pas exporté dans un format compatible. De plus, les formats d'export finaux (comme Godot) ne conservent pas forcément toute la disposition d'origine.  
Un format de sauvegarde de session de travail (`.ssp`) est indispensable.

### Réalisations & Architecture Validée (85 tests CTest 100% Succès)
1. **Archive Conteneur ZIP (`.ssp`) & Gestionnaire de Session (`SessionManager`) :**
   - Implémentation de `SessionManager` (`include/project/sessionmanager.h` / `src/project/sessionmanager.cpp`).
   - Espace de travail temporaire automatique sur disque : `%TEMP%/SpriteStudio/sessions/<session_uuid>/` (garantissant 0 saturation RAM sur les gros atlas).
   - Prise en charge native de la compression/décompression ZIP cross-platform via `Qt6::CorePrivate` (`QZipReader` et `QZipWriter`), éliminant toute dépendance tierce (comme zlib externe ou libzip).
   - **Sauvegarde atomique sécurisée :** Écriture vers `.ssp.tmp` puis renommage atomique vers `.ssp` avec remplacement propre, évitant toute corruption en cas d'interruption système.

2. **Sérialisation Complète du Document (`ProjectManager`) :**
   - Implémentation de `ProjectManager` (`include/project/projectmanager.h` / `src/project/projectmanager.cpp`).
   - Fichier manifeste `project.json` (signature `"SpriteStudioProject"`, version `"1.0"`, métadonnées, dates, nom du projet).
   - Sauvegarde de l'atlas embarqué sous `assets/atlas.png` pour une portabilité totale du projet.
   - Sérialisation exhaustive de toutes les boîtes (`rect`, `index`, `selected`, `groupId`, `overlapping`, et `pivot` préparé pour M3), ainsi que de l'ensemble des animations (`name`, `fps`, `loop`, `frames`).
   - Mémorisation de l'état de vue (`zoomFactor`, `panX`, `panY`).

3. **Verrou de Session & Détection de Crash au Démarrage :**
   - Fichier de verrou `.session_lock` maintenu dans chaque session avec `pid`, `sessionUuid`, `originalFilePath`, `status` (`"active"` ou `"clean_closed"`), horodatages de création et modification.
   - Détection multiplateforme de la vivacité des processus (`OpenProcess` sous Windows, `kill(pid, 0)` sous POSIX/Linux/macOS/Haiku).
   - Inspection automatique au lancement de l'application : détection des sessions interrompues/orphelines et invite utilisateur permettant de restaurer immédiatement la session de travail ou de purger les résidus disque.

4. **Moteur Git Invisible Embarqué (M5-Git-Core) — ✅ TERMINÉ & VALIDÉ (100%) :**
   - **Découverte CMake & Runtime Windows :** Politiques `CMP0074` et `CMP0144`, module `cmake/FindLibGit2.cmake` avec détection automatique des builds MinGW/MSVC (`Desktop_Qt_6_10_2_MinGW_64_bit-Debug`), et commande `POST_BUILD` copiant `libgit2.dll` vers les répertoires d'exécution (résolution de `STATUS_DLL_NOT_FOUND` / `0xc0000135`).
   - **Snapshots continus calqués sur l'UndoStack :** Chaque commande (`QUndoStack::indexChanged`) écrit l'état sérialisé du projet et crée un commit Git instantané avec le libellé de l'action (`"Add Slice"`, `"Resize/Move Slice"`, `"Merge Frames"`, etc.).
   - **Exclusion stricte du verrou :** Génération automatique de `.gitignore` et suppression explicite de `.session_lock` de l'index Git (`git_index_remove_bypath`).
   - **Sauvegarde et persistance dans le `.ssp` :** Résolution du bug de normalisation de chemin dans `QZipReader` (préfixage interne `ssp/` empêchant l'écrasement des points de tête sur `.git` et `.gitignore`). Tout fichier `.ssp` conserve l'intégralité de son historique Git.
   - **Moteur de Time Travel (`checkoutRevision`) :** Restauration fidèle de tout commit historique en mode HEAD détaché (`git_checkout_tree(GIT_CHECKOUT_FORCE)`), réinitialisation propre du document et des contrôleurs sans fuite de mémoire.
   - **Suite de tests automatisée (`tests/test_project.cpp`) :** 14 tests unitaires (dont `testGitIntegration`, `testGitContinuousSnapshots`, `testGitSavedInSsp`, `testGitTimeTravelCheckout`), 100% de réussite sur les 4 suites CTest (89 tests au total).

5. **Visualiseur d'Historique Git & Time Travel Graphique (M5-Git-UI) — ✅ TERMINÉ & VALIDÉ (100%) :**
   - Inspiré du projet de référence `geometryEditor` (`geometryeditormainwindow_history.cpp` / `geometryeditormainwindow_history_log.cpp`).
   - **Composant Dock Widget (`GitHistoryDock` / `GitHistoryGraphicsView`) :**
     - Vue graphique (`QGraphicsView` / `QGraphicsScene`) interactive intégrée dans un dock amovible (`QDockWidget`).
     - Représentation nodale des commits (disques/LEDs colorés : vert émeraude pour HEAD, bleu ardoise pour les commits normaux, anneau d'ambre doré pour la sélection).
     - Liaisons vectorielles nettes entre les commits et leurs parents (`parentHashes`).
     - Étiquettes d'informations : badge de hash court en police monospace, horodatage formaté, libellé d'action exact, badge `[HEAD]`.
     - Volet d'inspection inférieur détaillé (hash complet sélectionnable, horodatage local, auteur et email, message intégral).
   - **Interactivité & Time Travel :**
     - Simple clic : sélection du nœud et mise à jour instantanée du volet d'inspection.
     - Double-clic : checkout immédiat du commit sélectionné via `ProjectController::checkoutRevision(hash)`, rechargement en direct de l'atlas, des découpes et des animations sur la scène principale.
     - Bouton toolbar *"Revenir au présent"* (HEAD) : retour en un clic sur la dernière révision active.
     - Menu `Affichage -> Historique Git` (`Ctrl+H`) pour masquer/afficher le dock.
     - Prise en charge du mode dégradé gracieux : bannière informative propre si `libgit2` n'est pas compilé.
   - **Validation par tests unitaires (`tests/test_project.cpp`) :** Test `testGitHistoryDockUI` validant l'instanciation, la synchronisation du log et le time-travel bidirectionnel (passé $\leftrightarrow$ présent). 100% de réussite sur l'ensemble des 4 suites CTest (90 tests au total).

6. **Intégration Interface Utilisateur (`MainWindow` & `ProjectController`) :**
   - Menus Fichier dédiés :
     - `Nouveau Projet` (`Ctrl+N`).
     - `Ouvrir Projet...` (`Ctrl+O`) et ouverture directe par drag & drop des `.ssp`.
     - `Enregistrer Projet` (`Ctrl+S`) et `Enregistrer Projet Sous...` (`Ctrl+Shift+S`).
     - `Exporter...` (`Ctrl+E`) et `Exporter Sous...` (`Ctrl+Shift+E`).
     - Sous-menu `Projets Récents` (historique séparé des fichiers récents importés).
   - Indicateur de modification non enregistrée `*` dans la barre de titre (`SpriteStudio - MonProjet.ssp *`).
   - Dialogue de confirmation à la fermeture de l'application et à la création/ouverture de projet (`maybeSave`) protégeant les données non enregistrées.
   - Prise en charge des traductions complètes en français et anglais.

---

## M6 : Algorithme d'Empaquetage Avancé (MaxRects Bin-Packing)

### Contexte & Objectif
L'exportation actuelle vers Godot ou TexturePacker utilise un placement en grille ou un packing basique. Pour minimiser l'espace mémoire vidéo (VRAM) et optimiser la taille des atlas de sprites en production, un algorithme d'empaquetage 2D de type MaxRects est requis.

### Spécifications Fonctionnelles
1. **Algorithme MaxRects (Best Short Side Fit / Best Area Fit) :**
   - Réorganisation optimale des rectangles pour produire l'atlas le plus compact possible (forme carrée ou puissance de deux : $512\times 512$, $1024\times 1024$, $2048\times 2048$).
2. **Options d'Empaquetage :**
   - **Padding / Spacing :** Espacement configurable entre les frames (ex. 1px ou 2px) pour éviter le saignement de texture (*texture bleeding*).
   - **Extrude :** Répétition des pixels de bordure sur 1 pixel pour le filtrage bilinéaire dans les moteurs 3D/2D.
   - **Deduplication :** Détection des frames strictement identiques pour ne les stocker qu'une seule fois dans l'atlas tout en conservant les références dans les animations.

### Fichiers & Composants Cibles
- `SpriteStudio/include/packer/atlaspacker.h` / `src/packer/atlaspacker.cpp`.

---

## M7 : Suppression Avancée d'Arrière-Plan & Segmentation Robuste (Planches JPEG, Bruit, Anti-Halo)

### Contexte & Problématique Observée (Exemple : Planche Street Fighter / Ryu)
Les planches de sprites récupérées sur le Web (rips d'émulateurs, archives) sont très fréquemment stockées au format **JPEG** :
- Absence totale de couche alpha native ($\alpha = 255$ partout).
- Compression à perte (DCT $8\times 8$ et sous-échantillonnage chromatique YUV 4:2:0).
- Fond aplat uniforme en théorie (souvent vert `#3b7b0a`, cyan ou magenta), mais fortement altéré et bruité en pratique autour des personnages.
- Sprites agencés de façon extrêmement compacte (espacement de 1 à 2 pixels seulement entre deux frames consécutives).
- Présence d'éléments parasites non graphiques : annotations textuelles de rippers ("Ryu ripped by..."), flèches explicatives, encadrés de texte, notes d'animation.
- Poses variées avec cavités corporelles complexes (jambes écartées lors des sauts, bras repliés) et effets spéciaux d'énergie (Hadouken, auras) dont les dégradés semi-transparents ont été fusionnés avec la couleur du fond.

---

### ⚠️ Inventaire des Problèmes et Risques d'Échec

1. **Artefacts de Compression JPEG & Bruit de Contour (Ringing / Mosquito Noise) :**
   - Aux abords des silhouettes à fort contraste (kimono blanc, cheveux noirs, bandeau rouge sur fond vert), la transformée en cosinus discrète (DCT) produit des ondulations de teinte. Le vert de fond fluctue localement de $\pm 15$ à $\pm 30$ en valeurs RVB.
   - **Conséquence :** Un seuil de tolérance trop bas laisse un "nuage de moustiques" de pixels verts flottant autour des sprites. Un seuil trop élevé commence à grignoter les pixels clairs ou colorés du personnage.

2. **Halo Résiduel & Frange de Transition (Color Spill / Green Fringe) :**
   - En raison de l'interpolation bilinéaire et du sous-échantillonnage chroma (4:2:0), les pixels à la frontière exacte du sprite sont un mélange optique de la couleur du trait du sprite et de la couleur du fond vert.
   - **Conséquence :** Une fois le fond supprimé au seuil strict, chaque sprite conserve un liseré verdâtre disgracieux (*green halo*) qui gâche le rendu dès qu'on place le sprite sur un fond sombre ou dans un moteur de jeu.

3. **Sur-fusion des Sprites Resserrés (Over-merging) :**
   - Entre deux frames d'animation très proches (ex: un coup de pied qui frôle la pose suivante à 1 pixel d'écart), le moindre pixel de bruit résiduel non éliminé sert de "pont" conducteur pour le flood-fill.
   - **Conséquence :** Deux ou trois frames distinctes se retrouvent agglutinées en une seule boîte englobante géante.

4. **Le Dilemme des Cavités Internes Closes (Holes & Enclosed Background Islands) :**
   - Les trous d'arrière-plan situés à l'intérieur du corps (triangle entre les jambes écartées lors d'un saut, espace sous l'aisselle, boucle d'un bras replié) posent un dilemme algorithmique :
     - *Inondation depuis l'extérieur (Flood-fill pur) :* Elle isole parfaitement la silhouette externe sans toucher au sprite, mais laisse tous les trous intérieurs remplis de vert opaque.
     - *Substitution globale de couleur (Color Replacement global) :* Elle vide correctement les trous intérieurs, mais risque de percer des trous dans le sprite si le personnage porte un vêtement ou un accessoire de teinte voisine du fond (ex: Blanka, gants, liserés).

5. **Pollution par les Micro-Composantes Textuelles (Stray Text & Credits) :**
   - Les crédits de ripping et flèches disséminés entre les rangées de sprites sont découpés en dizaines de micro-boîtes parasites ($2\times 3$ px, $5\times 5$ px), polluant la liste des frames et faussant les calculs de cadence ou d'alignement.

6. **Dégradation des Effets Semi-Transparents (Hadouken, Projectiles, Auras) :**
   - Les flammes et boules d'énergie bleues avec transparence d'origine ont été aplaties sur le vert lors de l'enregistrement JPEG, créant des pixels cyan/verts hybrides impossibles à isoler par un seuil binaire.

---

### 💡 Pistes Techniques & Solutions Envisagées

1. **Détection Colorimétrique Évoluée (Espace Perceptuel CIELAB / $\Delta E$) :**
   - Abandonner la simple distance Manhattan RVB ($|R_1-R_2| + |G_1-G_2| + |B_1-B_2|$) au profit de la distance euclidienne $\Delta E$ dans l'espace **CIELAB** ou en décomposition **YCbCr**.
   - En séparant la luminance ($Y/L$) de la chrominance ($Cb, Cr / a, b$), on peut appliquer une tolérance étroite sur la teinte du fond tout en autorisant les variations de luminosité induites par les blocs JPEG.
   - **Échantillonnage statistique :** Échantillonner les 4 coins et le périmètre extérieur pour calculer la médiane de la couleur de fond ainsi que son écart-type ($\sigma$), permettant de définir un seuil adaptatif automatique.

2. **Algorithme Hybride en 2 Passes (Silhouette Externe + Cavités Validées) :**
   - **Passe 1 (Masquage Extérieur) :** Flood-fill depuis les bords de l'image pour marquer tout l'arrière-plan externe continu sans jamais pénétrer dans le sprite.
   - **Passe 2 (Cavités Internes) :** Pour les îlots internes non connectés à l'extérieur :
     - Calculer la compacité et la proximité colorimétrique avec le fond extérieur.
     - Remplacer par la transparence uniquement si la couleur moyenne de l'îlot concorde avec le fond à $\Delta E < \text{seuil}$, ou proposer un mode interactif "clic pour déboucher la cavité".

3. **Traitement Anti-Halo / Dé-frangeage (Color Despill & Alpha Matte) :**
   - **Algorithme de Green Despill :** Sur les pixels de contour (bordure de transition de 1 pixel), calculer la proportion de vert parasite et la soustraire en ajustant la composante alpha (technique similaire au chromakey vidéo professionnel).
   - **Érosion morphologique optionnelle :** Permettre un rognage d'un demi-pixel ou 1 pixel sur le masque alpha pour éradiquer les franges bruitées tenaces.

4. **Filtrage Intelligent des Parasites & Débruitage Géométrique (Pruning) :**
   - **Seuils dimensionnels minimaux :** Ignorer automatiquement lors de la segmentation toutes les composantes connexes dont $\text{largeur} < \text{seuilMin}$ OU $\text{hauteur} < \text{seuilMin}$ (ex. $< 8$ px) ou surface $< 32\text{ px}^2$.
   - **Outil "Zone d'Exclusion / Masque Rectangulaire" :** Permettre à l'utilisateur de tracer un ou plusieurs rectangles rouges "Ignorer cette zone" sur l'atlas (ex. par-dessus le bloc de texte de crédits) avant de lancer la détection automatique.

5. **Désagglomération par Profils de Projection (Histogram Slicing / Watershed) :**
   - Calculer les histogrammes de projection de densité de pixels opaques selon les axes horizontaux (lignes) et verticaux (colonnes).
   - Détecter les "cols" et vallées étroites où deux sprites ne se touchent que par 1 ou 2 pixels aberrants pour couper automatiquement le lien et séparer les boîtes englobantes.

6. **Pipette Manuelle & Prévisualisation en Direct (Live Overlay) :**
   - Ajouter un outil pipette dans la barre d'outils pour sélectionner manuellement la couleur de fond sur l'atlas en cas de couleur non majoritaire.
   - Prévisualisation instantanée par damier de transparence ou masque binaire dynamique avec curseur de tolérance en direct avant d'appliquer définitivement la transformation sur le document.

---

## M8 : Empaquetage Polygonal & Maillages Serrés (Polygon / Tight Mesh Packing)

### Contexte & Enjeux Techniques
Dans l'empaquetage rectangulaire standard (M6), chaque frame est isolée dans un rectangle orthogonal $[x, y, w, h]$. Pour des sprites aux poses dynamiques (personnage en plein saut, bras levé, lame d'épée en diagonale, tentacules, queues, effets de foudre), ce rectangle contient souvent plus de 50% à 70% de pixels transparents inutilisés.  
L'**empaquetage polygonal (*Tight Packing / Sprite Mesh*)** substitue au rectangle une enveloppe polygonale 2D (convexe ou concave) épousant au plus près les pixels opaques de la silhouette :
- **Gain d'espace drastique (20% à 50% de surface d'atlas économisée) :** Les formes s'imbriquent comme des pièces de puzzle (la pointe d'une épée se glisse dans le creux sous l'aisselle ou entre les jambes d'une autre frame). Cette compacité permet fréquemment de faire tenir une série d'animations dans un atlas $1024\times 1024$ au lieu de devoir doubler vers un $2048\times 2048$, réduisant l'empreinte mémoire vidéo (VRAM) de **75%**.
- **Éradication de l'Overdraw GPU (Fillrate) :** Sur mobile et consoles (Switch, etc.), le processeur graphique ne gaspille plus de temps à exécuter les shaders sur des fragments transparents invisibles.

---

### 🔬 Pipeline Algorithmique Complet en 5 Étapes

```
[Canal Alpha] 
     │
     ▼ 1. Détection de Contour
[Contour Pixel (Marching Squares / Moore-Neighbor)]
     │
     ▼ 2. Simplification Géométrique
[Polygone Simplifié 8-12 sommets (Ramer-Douglas-Peucker)]
     │
     ▼ 3. Triangulation 2D
[Maillage Triangulé GPU (Ear Clipping / Constrained Delaunay)]
     │
     ▼ 4. Bin-Packing Non-Convexe
[Imbrication Puzzle Optimisée (No-Fit Polygon / Raster Dilation)]
     │
     ▼ 5. Export Multi-Moteurs
[Atlas PNG Compact + JSON Maillage (Vertices, UVs, Triangles)]
```

#### 1. Détection de Contour Silhouette (*Contour Tracing*) :
- Analyse du canal alpha de chaque frame selon un seuil d'opacité configurable ($\alpha > \text{alphaThreshold}$, ex. $\alpha \ge 1$).
- Algorithme de contouring 2D (**Marching Squares** ou traçage de frontières de Moore-Neighbor) pour extraire la chaîne fermée ordonnée des pixels de bordure.
- Prise en charge des silhouettes à composantes multiples ou îles disjointes (ex. projectile séparé du corps).

#### 2. Simplification Géométrique Adaptative (*Ramer-Douglas-Peucker*) :
- Les contours bruts contiennent souvent 100 à 400 sommets par sprite, ce qui saturerait inutilement le GPU au stade vertex.
- Application de l'algorithme de **Ramer-Douglas-Peucker (RDP)** avec paramètre d'écart $\epsilon$ (en pixels, ex: $\epsilon = 1.0$ à $2.5$ px) :
  - Réduction de l'enveloppe à un polygone épuré de **6 à 12 sommets** seulement.
  - **Plafond strict de sommets ($N \le 16$) :** Garantie d'un coût de transformation de sommets négligeable pour le moteur de jeu.
  - **Garantie d'inclusion externe :** Dilatation légère (expansion d'un demi-pixel) pour garantir qu'aucun pixel opaque ne soit accidentellement tronqué par une arête simplifiée.

#### 3. Triangulation 2D du Maillage (*Mesh Triangulation*) :
- Transformation du polygone 2D simple (pouvant être concave) en un ensemble de triangles prêt pour le pipeline graphique GPU.
- Algorithme d'**Ear-Clipping** (découpage d'oreilles) ou **Constrained Delaunay Triangulation (CDT)**.
- Génération des triplets d'indices de faces (`triangles: [0, 1, 2, 0, 2, 3...]`).

#### 4. Algorithme d'Empaquetage 2D Non-Convexe (*Polygon Bin-Packing*) :
- Contrairement aux rectangles qui ne se superposent que sur leurs projections orthogonales, les polygones peuvent s'interpénétrer dans leurs zones concaves :
  - **Méthode NFP (No-Fit Polygon) :** Calcul de la zone interdite entre deux polygones pour trouver la trajectoire de contact la plus étroite sans collision.
  - **Approche Raster-Assisted (Hybride Rapide) :** Dilatation du masque de silhouette par la valeur de rembourrage (*padding*), et balayage par transformée de distance ou bounding boxes orientées (OBB - *Oriented Bounding Box*) à pas angulaire ($0^\circ, 90^\circ, 180^\circ, 270^\circ$).
- Respect rigoureux d'un espacement de sécurité polygonal (*Polygon Padding*, 2 px par défaut) pour prévenir tout artefact de saignement de texture (*texture bleeding*) lors du filtrage bilinéaire.

#### 5. Données d'Exportation & Formats Cibles :
- **Format JSON Étendu (Structure Universelle) :**
  Pour chaque frame de l'atlas :
  - `vertices` : tableau des coordonnées 2D des sommets relatifs au point d'ancrage/pivot ($[x_0, y_0, x_1, y_1 \dots]$).
  - `uvs` : tableau des coordonnées de texture normalisées $[0.0, 1.0]$ sur l'atlas final.
  - `triangles` : liste d'indices reliant les sommets par triplets.
  - `bounds` : bounding box rectangulaire de fallback pour compatibilité.
- **Export Dédié Godot 4 :**
  - Génération de fichiers de ressources maillage avec nœuds `Polygon2D` ou `ArrayMesh` 2D, utilisables directement dans les scènes sans aucune ligne de code supplémentaire.
- **Export Dédié Unity :**
  - Fichier de métadonnées `.meta` / JSON compatible avec le mode `SpriteMeshType.Tight` d'Unity.

---

### 🎛️ Paramètres & Options dans l'Interface Utilisateur (UI)

1. **Sélecteur de Mode d'Empaquetage :**
   - `Rectangulaire (MaxRects — Standard & Universel)`
   - `Polygonal (Tight Mesh — Optimisation VRAM & Overdraw)`
2. **Curseur de Complexité du Maillage (Vertex Budget) :**
   - *Ultra-Léger (6 à 8 sommets)* : Idéal pour jeux mobiles massifs et scènes à très grand nombre d'entités (bullet hell, foules).
   - *Équilibré (8 à 12 sommets — recommandé)* : Compromis parfait entre gain d'atlas et charge géométrique.
   - *Précis (12 à 16 sommets)* : Épouse au plus près les armes et détails fins.
3. **Prévisualisation Interactive du Maillage :**
   - Case à cocher *Afficher le maillage polygonal (Wireframe)* sur la vue de l'atlas pour inspecter visuellement les arêtes et les triangles générés.
   - Statistiques en direct : comparaison du taux de remplissage (*Packing Efficiency : 64% en Rectangulaire $\rightarrow$ 89% en Polygonal*).

---

### ⚖️ Tableau Comparatif : Packing Rectangulaire vs Packing Polygonal

| Critère | M6 : MaxRects Rectangulaire | M8 : Packing Polygonal / Tight Mesh |
|---|---|---|
| **Compatibilité Moteurs** | 🟢 **100% Universelle** (Tous moteurs, tous composants 2D) | 🟡 **Spécialisée** (Nécessite support `Polygon2D`, `MeshInstance` ou custom) |
| **Gain de Surface d'Atlas** | Standard (Baseline) | 🟢 **+20% à +50% de compacité** (évite de doubler la taille d'atlas) |
| **Consommation VRAM** | Moyenne | 🟢 **Minimale** (textures plus petites) |
| **Overdraw GPU (Fillrate)** | Élevé sur formes ouvertes (quads transparents) | 🟢 **Quasi-nul** (les pixels transparents ne sont pas dessinés) |
| **Coût CPU au Packing** | Rapide ($< 50$ ms) | Modéré (100 ms à 1-2 s selon le nombre de frames) |
| **Complexité d'Intégration** | Faible | Haute (Tracé de contour + Simplification + Triangulation + NFP) |

---

### Fichiers & Composants Cibles
- `SpriteStudio/include/geometry/contourtracer.h` / `src/geometry/contourtracer.cpp` : Extraction de contours alpha par Marching Squares / Moore-Neighbor.
- `SpriteStudio/include/geometry/triangulator.h` / `src/geometry/triangulator.cpp` : Simplification Ramer-Douglas-Peucker et triangulation Ear-Clipping.
- `SpriteStudio/include/packer/polygonpacker.h` / `src/packer/polygonpacker.cpp` : Algorithme de bin-packing 2D non-convexe.
- `SpriteStudio/include/extractor/godotextractor.h` : Extension d'export Godot vers nœuds `Polygon2D`.

---

## 🎨 ASSETS : Remplacement des Échantillons (`sample/`) par des Assets Originaux (Libres de Droits)

### 📌 Contexte & Problématique
- Actuellement, les fichiers du dossier `sample/` (ex. sprites et planches de Ryu, Chun-Li, etc.) sont issus d'œuvres existantes sous droits d'auteur (copyright).
- Pour cette raison, ces fichiers de test sont **volontairement exclus des commits Git** (non suivis / untracked).
- **Conséquences & Limites actuelles :**
  - Risque juridique et éthique si ces contenus tiers venaient à être diffusés ou intégrés publiquement dans le dépôt.
  - Fragilité des tests unitaires automatisés (`test_extractors`, `test_controllers`, `test_project`) qui dépendent de la présence de ces fichiers locaux non versionnés.
  - Impossibilité pour un tiers ou un serveur d'intégration continue (CI) de cloner le dépôt et d'exécuter la suite CTest sans devoir récupérer manuellement ces échantillons protégés.
  - Absence d'illustrations légitimes pour la documentation, le README et la mise en valeur du logiciel.

### 🎯 Objectifs & Plan d'Action
1. **Création Graphique Originale ("Maison") :**
   - Dessiner soi-même quelques assets originaux en pixel art (ex. un personnage avec 2 ou 3 cycles d'animation : *idle*, *walk*, *action/attack*, ainsi qu'un item/effet).
   - Concevoir une planche de test avec arrière-plan uni et une variante légèrement compressée/bruitée pour éprouver la détection automatique de fond et le détourage (M7).
2. **Standardisation & Pérennisation des Formats de Test :**
   - Générer à partir de ces créations originales les jeux de tests complets :
     - PNG / BMP (planches brutes).
     - GIF animé.
     - TexturePacker / Aseprite JSON (`.json` + `.png`).
     - Godot 4 SpriteFrames (`.tres` + `.png`).
     - Projet natif SpriteStudio (`.ssp`).
3. **Intégration Propre dans le Dépôt & Automatisation CTest :**
   - Versionner officiellement ces nouveaux assets originaux dans le dépôt Git (ex. sous `sample/` ou `tests/data/`).
   - Mettre à jour les suites de tests unitaires pour qu'elles s'exécutent de façon 100% autonome et reproductible dès le clonage du projet.

---

## 🛠️ AUDIT : Points de Vigilance & Dette Technique Résiduelle (Recommandations d'Amélioration)

Ce volet consigne l'ensemble des axes d'amélioration, points de fragilité et dettes techniques mis en lumière lors de l'audit critique approfondi du projet (architecture logicielle, intégrité du modèle de données, build CMake, tests & DevOps, ergonomie et documentation).

### 1. Architecture & Modèle de Données (Core Model Integrity)

- **Purification de `SpriteDocument` (`QImage` vs `QPixmap`) :**
  - *Constat :* `SpriteDocument::m_frames` stocke une liste de `QPixmap` (`QList<QPixmap>`), alors que l'atlas d'origine est conservé sous forme de `QImage`.
  - *Problème & Risque :* En Qt, un `QPixmap` est directement assujetti au serveur d'affichage graphique / GPU. Manipuler ou instancier des `QPixmap` en dehors du thread GUI principal provoque des assertions, des fuites de ressources ou des comportements indéfinis sous Linux (X11/Wayland) et macOS lors des opérations asynchrones (`QtConcurrent`).
  - *Action requise :* Refactoriser `SpriteDocument` pour stocker exclusivement des `QImage`. La conversion vers `QPixmap` doit être repoussée à la couche de vue et de rendu (`AtlasViewController`, `TimelineFilmstripWidget`, délégués d'affichage).

- **Virtualisation & Refonte de `ArrangementModel` (Lazy-Loading des Vignettes) :**
  - *Constat :* `ArrangementModel` hérite de `QStandardItemModel` et recopie toutes les frames du document sous forme de `QStandardItem`. Dans `MainWindow::populateFrameList()`, le redimensionnement `scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation)` est exécuté de façon synchrone pour chaque frame sur le thread principal.
  - *Problème & Risque :* Goulot d'étranglement perceptible lors du chargement de planches massives (200 à 500 frames), figeant temporairement l'interface.
  - *Action requise :* Remplacer `ArrangementModel` par un `QAbstractListModel` personnalisé indexant directement `SpriteDocument` sans recopie, avec génération asynchrone des vignettes ou mise en cache LRU à la demande (`data(Qt::DecorationRole)`).

- **Réduction de la Colle Événementielle dans `MainWindow` :**
  - *Constat :* Bien que délestée de ses responsabilités monolithiques, la classe `MainWindow` reste dispersée sur 6 fichiers source (`mainwindow.cpp`, `mainwindow_animation.cpp`, `mainwindow_atlas.cpp`, `mainwindow_callbacks.cpp`, `mainwindow_events.cpp`, `mainwindow_frames.cpp`).
  - *Action requise :* Rapatrier la glue d'événements et de menus directement dans les contrôleurs respectifs (`AtlasViewController`, `AnimationController`, `ProjectController`) ou au sein de sous-composants/docks autonomes pour alléger l'orchestrateur.

- **Isolation de la Dépendance Privée Qt (`Qt6::CorePrivate`) :**
  - *Constat :* La compression et décompression des archives `.ssp` s'appuie sur `<private/qzipreader_p.h>` et `qzipwriter_p.h`.
  - *Problème & Risque :* Les en-têtes privés de Qt ne bénéficient d'aucune garantie de stabilité d'API/ABI entre versions mineures de Qt, et certaines distributions Linux n'installent pas par défaut les paquets de développement privés.
  - *Action requise :* Encapsuler l'accès ZIP derrière une interface d'abstraction pour permettre, si nécessaire, un basculement aisé vers une bibliothèque tierce standardisée (ex: `minizip-ng` ou `libzip`).

---

### 2. Performance & Optimisations Algorithmiques

- **Désengorgement de la Vérification d'Inclusion dans `SpriteDetector` ($O(N^2)$) :**
  - *Constat :* Dans `SpriteDetector::detectToImages()`, le filtrage des boîtes englobantes entièrement incluses dans d'autres utilise une double boucle imbriquée $N \times N$ (`componentRects[j].contains(componentRects[i])`).
  - *Problème & Risque :* Sur une planche fortement bruitée (JPEG issu du web) générant 2000 à 5000 composantes parasites, ce test effectue entre 4 et 25 millions de comparaisons géométriques sur le CPU.
  - *Action requise :* Remplacer la boucle naïve par un partitionnement spatial (grille spatiale uniforme ou QuadTree) pour ramener la complexité à $O(N \log N)$.

---

### 3. DevOps, Build & Automatisation (Tests & CI/CD)

- **Factorisation CMake (Bibliothèque Commune `SpriteStudioCore`) :**
  - *Constat :* Le répertoire `lib/` est vide. L'application principale et les 4 exécutables de test (`test_extractors`, `test_controllers`, `test_project`, `test_core`) recompilent chacun l'intégralité des fichiers sources `.cpp` (les mêmes fichiers sont compilés jusqu'à 5 fois).
  - *Action requise :* Définir une bibliothèque statique `SpriteStudioCore` dans CMake et la lier aux cibles de tests et à l'exécutable principal. Temps de compilation divisé par 3 et maintenance centralisée.

- **Pipeline d'Intégration Continue (GitHub Actions CI/CD) :**
  - *Constat :* Le dossier `.github/` ne contient aucun workflow (`.github/workflows/ci.yml`). La validation des 93 tests CTest dépend exclusivement des exécutions manuelles en local.
  - *Action requise :* Créer un workflow GitHub Actions automatisant la compilation et l'exécution de `ctest --output-on-failure` (en mode `QT_QPA_PLATFORM=offscreen`) sur les 3 environnements cibles : Ubuntu (GCC), Windows (MinGW/MSVC) et macOS (Clang).

- **Pérennisation des Fixtures de Tests (Suite à la Purge Copyright) :**
  - *Constat :* Suite au commit `bad56df` supprimant les planches de test sous droits d'auteur (`sample/ryu.png`, etc.), plusieurs tests de codecs recourent à `QSKIP` et ne s'exécutent plus réellement.
  - *Action requise :* Créer et versionner dans `tests/data/` un jeu minimal d'assets originaux libres de droits (ou générés par code via `QImage`) afin de garantir une exécution 100% autonome et effective des tests en environnement vierge (CI).

---

### 4. Documentation & Visibilité Externe

- **Refonte Majeure du `README.md` :**
  - *Constat :* Le `README.md` actuel est lourdement désynchronisé des avancées du logiciel. Il ignore le format natif `.ssp`, l'historique Git et le Time-Travel interactif, la timeline filmstrip, le support de Godot 4, et annonce des prérequis obsolètes (CMake 3.10 au lieu de 3.20+ et Qt 6).
  - *Action requise :* Réécrire le README avec présentation moderne, actualisation des fonctionnalités réelles, prérequis exacts, et nouvelles captures d'écran / GIFs animés représentatifs de l'interface actuelle.

---

## 📅 Ordre de Déploiement Recommandé

1. **Étape 0 — Stabilisation & Clôture de M1 (M1-Fix) — ✅ TERMINÉ & VALIDÉ (100%)** :
   Poignées cosmétiques anti-chevauchement à fort zoom pixel art, déplacement synchronisé de multi-sélection (group drag), badges d'index sans débordement et découpe continue avec Shift validés par tests unitaires automatisés.
2. **Étape 1 — Sauvegarde & Projet Natif (M5) — ✅ TERMINÉ & VALIDÉ (100%)** :
   Sécuriser le travail de l'utilisateur avec format `.ssp` ZIP atomique, détection de crash, snapshots Git continus calqués sur l'UndoStack et Time Travel graphique via le dock d'historique.
3. **Étape 2 — Séquençage & Multi-Animations (M2) — ✅ TERMINÉ & VALIDÉ (100%)** :
   Donner toute la dimension "studio d'animation" avec la création d'animations multiples, le réglage de cadence, les boucles (Loop, Once, Ping-Pong) via une timeline ergonomique par splitters et ruban filmstrip.
4. **Étape 3 — Quick Wins & Consolidation Technique (AUDIT-Phase 1)** :
   - Factorisation de la cible `SpriteStudioCore` dans CMake (accélération x3 des compilations de tests).
   - Génération/intégration des fixtures d'assets originaux libres de droits (`ASSETS` / `tests/data/`) pour réarmer les tests skippés.
   - Mise en place du workflow GitHub Actions CI/CD multiplateforme (`.github/workflows/ci.yml`).
   - Actualisation du `README.md` (mise en valeur des atouts M2/M5/Godot).
5. **Étape 4 — Points d'Ancrage / Pivots (M3)** :
   Assurer la cohérence physique des animations avant l'export dans les moteurs de jeux (réticule interactif, presets, offsets Godot/JSON).
6. **Étape 5 — Assainissement Architectural & Performance (AUDIT-Phase 2)** :
   - Migration de `SpriteDocument::m_frames` vers `QImage` pour purifier le modèle de données et garantir l'étanchéité hors-thread.
   - Virtualisation de `ArrangementModel` via `QAbstractListModel` avec lazy-loading des vignettes.
   - Optimisation de la détection de boîtes imbriquées dans `SpriteDetector` via partitionnement spatial ($O(N \log N)$).
7. **Étape 6 — Outil d'Édition de Pixels (M4)** :
   Offrir l'atelier de retouche pixel art autonome directement au cœur du workflow.
8. **Étape 7 — Optimisation du Packing (M6)** :
   Perfectionner le rendement de l'atlas PNG final pour la production avec MaxRects (Best Short Side Fit / Best Area Fit).
9. **Étape 8 — Suppression Avancée de Fond & Débruitage Robuste (M7)** :
   Doter SpriteStudio d'un moteur de segmentation tolérant au bruit JPEG, anti-halo (*despill*), filtrage de textes parasites et désagglomération pour les planches de sprites complexes.
10. **Étape 9 — Empaquetage Polygonal & Maillages Serrés (M8)** :
    Extension haute performance pour moteurs 2D modernes (Godot Polygon2D, Unity Tight) : tracé de contours alpha, simplification Douglas-Peucker, triangulation et imbrication type puzzle pour maximiser la densité d'atlas et éradiquer l'overdraw GPU.
