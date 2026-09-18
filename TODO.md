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
| **M3** | [Points d'Ancrage & Pivots (Origins & Offsets)](#m3--points-dancrage--pivots-origins--offsets) | **Haute (Critique)** | Faible | 🟢 Clôturé & Validé (100% CTest — Réticules interactifs atlas & aperçu, Mire déplaçable, Pan/Zoom 5000%, Fit in View, Sol, Enveloppe, Godot/JSON/SSP) |
| **M5** | [Format de Projet Natif (`.ssp` - Sprite Studio Project)](#m5--format-de-projet-natif-ssp---sprite-studio-project) | **Haute** | Faible | 🟢 Clôturé & Validé (100% CTest — Session, Lock, Crash Recovery, Atomic Save, Git Time-Travel & UI Dock) |
| **M7** | [Suppression Avancée de Fond & Système de Filtres Graphiques (Filtres GIMP, Anti-Halo, Alt-Skins)](#m7--suppression-avancée-darrière-plan--système-de-filtres-graphiques-filtres-gimp-anti-halo-alt-skins) | **Moyenne** | Moyenne | 🟢 Clôturé & Validé (100% CTest — Architecture Plugins, 7 Filtres opérationnels, Live Preview, Auto-Detect Boxes, Rollback) |
| **M6** | [Algorithme d'Empaquetage Avancé (MaxRects Bin-Packing)](#m6--algorithme-dempaquetage-avancé-maxrects-bin-packing) | **Haute** | Moyenne | 🟢 Clôturé & Validé (100% CTest — MaxRects BSSF/BAF/BLSF, POT, Extrude, Déduplication, ExportDialog) |
| **M-CLI** | [Interface Ligne de Commande & Automatisation CI/CD (`spritestudio-cli`)](#m-cli--interface-ligne-de-commande--automatisation-cicd-spritestudio-cli) | **Haute** | Moyenne | 🟢 Clôturé & Validé (100% CTest — Drop-in 100% TexturePacker, Aseprite -b, Godot 4 UID/Scene, Slice, Filter, SSP, POSIX, JSON) |
| **M4** | [Outil d'Édition de Pixels (Pixel Art Retouching)](#m4--outil-dédition-de-pixels-pixel-art-retouching) | **Moyenne** | Haute | 📝 Planifié (Périmètre Restreint / Retouche Chirurgicale) |
| **M8** | [Empaquetage Polygonal & Maillages Serrés (Polygon / Tight Mesh Packing)](#m8--empaquetage-polygonal--maillages-serrés-polygon--tight-mesh-packing) | **Moyenne** | Haute | 🟢 Clôturé & Validé (100% CTest — Marching Squares, RDP, Ear-Clipping, Wireframe HMI, Édition Sommets, Tight Packing Multithreadé & Configurable, Export Unity/Unreal/Godot, 22 tests CTest) |
| **ASSETS** | [Remplacement des Échantillons (`sample/`) par des Assets Libres de Droits](#-assets--remplacement-des-échantillons-sample-par-des-assets-originaux-libres-de-droits---terminé--validé-100) | **Haute** | Faible | 🟢 Clôturé & Validé (100% Assets originaux générés, 0 risque copyright, tests autonomes) |
| **AUDIT** | [Dette de Thread-Safety & Modèle Pur (Audit Étape 2)](#️-audit--points-de-vigilance--dette-technique-résiduelle-recommandations-damélioration) | **Haute** | Moyenne | 🟢 Clôturé & Validé (Modèle pur QImage, Cache Vignettes, 0 conversion I/O, Miniz ZIP, 116 tests CTest 100%) |

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

## M3 : Points d'Ancrage & Pivots (Origins & Offsets) — ✅ TERMINÉ & VALIDÉ (100%)

### Contexte & Objectif
Lorsqu'un personnage donne un coup d'épée ou saute, la boîte de découpe de chaque frame change souvent de taille. Si les frames sont centrées arbitrairement sans point d'ancrage commun, le personnage "saute" ou glisse visuellement dans le moteur de jeu.  
Le point d'ancrage (ou pivot) définit le point de référence (souvent au niveau des pieds ou au centre du corps) pour aligner rigoureusement les frames.

### Réalisations & Architecture Validée (100% Tests CTest Validés)
1. **Modèle de Données & Préréglages Géométriques (`SpriteBox` & `SpriteDocument`) :**
   - Ajout des champs `QPoint pivot` et `bool hasCustomPivot` dans `SpriteBox` (coordonnées locales pixel relatives au coin supérieur gauche de la tranche).
   - Méthode dynamique `effectivePivot()` : retourne le pivot personnalisé s'il a été défini ou édité, sinon calcule par défaut le bas-centre `(width / 2, height)` s'adaptant automatiquement aux redimensionnements de la boîte.
   - Énumération des 9 préréglages cardinaux `PivotPreset` (`TopLeft`, `TopCenter`, `TopRight`, `CenterLeft`, `Center`, `CenterRight`, `BottomLeft`, `BottomCenter`, `BottomRight`, `Custom`).
   - Méthodes documentaires `setBoxPivot()`, `setBoxesPivot()`, `applyPivotPreset()`, `computeAnimationEnvelope()` émettant le signal `boxPivotChanged(int index, const QPoint &pivot)`.

2. **Historique d'Annulation / Rétablissement (`ChangePivotCommand`) :**
   - Commande `QUndoCommand` unifiée gérant la modification unitaire, multi-sélection ou par lot de pivots.
   - Restauration exacte des états précédents et réactivation transparente de l'indicateur de pivot personnalisé.

3. **Édition Graphique Interactive dans l'Atlas (`AtlasBoxItem` & `AtlasViewController`) :**
   - Détection de la poignée interactive `Handle::Pivot` sur le réticule de visée.
   - Réticule de contraste élevé à double anneau (cercle blanc et mire intérieure noire) garantissant une visibilité parfaite sur tout fond clair, sombre ou transparent.
   - Déplacement fluide par glisser-déposer à la souris avec émission en temps réel du signal `boxPivotChanged`.
   - Sous-menu contextuel "Point d'ancrage (Pivot)" dans le clic-droit de la boîte atlas offrant les 9 préréglages instantanés.

4. **Stabilisation Sans Saut d'Animation (`AnimationController`) :**
   - Calcul de l'enveloppe englobante commune de l'animation (`computeAnimationEnvelope`) alignant les pivots sur une origine partagée $(originX, originY)$.
   - Élimination mathématique de tout sautillement (*jittering*) lors de la lecture d'animations aux frames de dimensions inégales.
   - Ligne de sol pointillée subtile (*dashed ground line*) et réticule de pivot centrés sur le point d'impact.
   - Bouton à bascule `[ 🎯 Mire ]` permettant d'afficher ou masquer la mire dans la scène de prévisualisation.

5. **Panneau Ergonomique IHM & Actions par Lot (`MainWindow`) :**
   - Groupe IHM `grpPivot` intégré dans le panneau d'édition avec boutons rapides `[ ⬇️ Sol ]`, `[ 🎯 Centre ]`, `[ ↖️ UI ]`.
   - Sélecteur combo des 9 préréglages et spinboxes numériques `X` et `Y`.
   - Boutons d'application en masse : "Appliquer à l'anim" et "Appliquer à tous".
   - Synchronisation bidirectionnelle instantanée lors des sélections et changements de frame.

6. **Fidélité des Codecs Moteurs & Sérialisation :**
   - **Godot 4 :** Export automatique du champ `margin = Rect2(...)` dans les sous-ressources `AtlasTexture` des fichiers `.tres`, et réimport transparent.
   - **TexturePacker / Aseprite JSON :** Export et réimport de l'objet normalisé `"pivot": { "x": ..., "y": ... }`.
   - **Projet Natif `.ssp` :** Sauvegarde et restauration complètes des coordonnées et du flag `hasCustomPivot`.

7. **Internationalisation (i18n) & Couverture de Tests :**
   - 29 nouvelles clés traduites à 100% en Français, Anglais et Japonais (519 chaînes, 0 non traduite).
   - Tests automatisés dans `tests/test_core.cpp`, `tests/test_controllers.cpp`, `tests/test_project.cpp` et `tests/test_extractors.cpp` validant la totalité des comportements.

8. **Interactivité Complète de la Vue d'Aperçu (Mire Déplaçable, Pan & Zoom, Cadrage Optimal) :**
   - Saisie et déplacement fluide de la mire à la souris directement dans la fenêtre d'aperçu de droite, avec pause automatique lors de la saisie, retour en direct sur les spinboxes X/Y et enregistrement dans la pile Undo/Redo.
   - Raccourci `Shift + Clic gauche` pour positionner instantanément le pivot sous le pointeur de la souris.
   - Cadrage optimal automatique (*Fit In View*) sur l'enveloppe de l'animation lors du chargement, bouton dédié `[ ⛶ ]` et raccourci double-clic dans la vue.
   - Zoom manuel à la molette centré sur le curseur (jusqu'à 5000% avec filtrage net sans flou pour le pixel art), translation de vue (*Pan*) par clic molette ou clic droit, et réinitialisation rapide `[ 1:1 ]`.

| Spécification M3 | Statut | Composant / Fichier | Diagnostic & Observations |
|---|:---:|---|---|
| **Structure de pivot dans SpriteBox** | ✅ **RÉSOLU & VALIDÉ** | `spritedocument.h`, `spritedocument.cpp` | `pivot`, `hasCustomPivot`, `effectivePivot()`, 9 presets cardinaux. |
| **Commande Undo/Redo ChangePivotCommand** | ✅ **RÉSOLU & VALIDÉ** | `commands.h`, `commands.cpp` | Annulation et rétablissement fiables en unitaire et par lot. |
| **Réticule interactif dans AtlasBoxItem** | ✅ **RÉSOLU & VALIDÉ** | `atlasboxitem.h`, `atlasboxitem.cpp` | Drag & drop à la souris du pivot avec poignée `Handle::Pivot` et halo de contraste. |
| **Stabilisation de lecture d'animation** | ✅ **RÉSOLU & VALIDÉ** | `animationcontroller.cpp` | Alignement sur l'enveloppe commune, élimination de tout sautillement, ligne de sol. |
| **Aperçu interactif (Mire drag, Pan/Zoom, Fit)** | ✅ **RÉSOLU & VALIDÉ** | `animationcontroller.cpp`, `mainwindow.ui` | Déplacement direct de la mire dans l'aperçu, Shift+Clic, zoom molette 5000%, pan clic droit/molette, boutons ⛶ et 1:1, double-clic fit. |
| **Contrôles IHM & Préréglages rapides** | ✅ **RÉSOLU & VALIDÉ** | `mainwindow.ui`, `mainwindow.cpp` | Boutons Sol/Centre/UI, combo 9 presets, spinboxes X/Y, batch "Appliquer à l'anim / tous". |
| **Export/Import Godot 4 (margin Rect2)** | ✅ **RÉSOLU & VALIDÉ** | `godotextractor.cpp` | Écriture et lecture du décalage de marge dans les sous-ressources `AtlasTexture`. |
| **Export/Import JSON (pivot normalisé)** | ✅ **RÉSOLU & VALIDÉ** | `jsonextractor.cpp` | Round-trip exact du pivot normalisé `{ "x", "y" }`. |
| **Format de Projet Natif .ssp** | ✅ **RÉSOLU & VALIDÉ** | `projectmanager.cpp` | Sérialisation et désérialisation JSON pérennes. |
| **Traductions FR / EN / JA** | ✅ **RÉSOLU & VALIDÉ** | `sprite_studio_*.ts` | 100% traduit (0 unfinished, 519 strings). |
| **Tests CTest automatisés** | ✅ **RÉSOLU & VALIDÉ** | `test_core`, `test_controllers`, `test_extractors`, `test_project` | 100% de succès sur la suite complète. |

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

## M6 : Algorithme d'Empaquetage Avancé (MaxRects Bin-Packing) — 🚀 PROCHAINE ÉTAPE IMMÉDIATE

### Contexte & Objectif
L'exportation actuelle vers Godot ou TexturePacker utilise un placement en grille ou un packing basique. Pour minimiser l'espace mémoire vidéo (VRAM) et optimiser la taille des atlas de sprites en production, un algorithme d'empaquetage 2D de type MaxRects est requis.

### Spécifications Fonctionnelles
1. **Algorithme MaxRects (Best Short Side Fit / Best Area Fit) :**
   - Réorganisation optimale des rectangles libres pour produire l'atlas le plus compact possible (forme carrée ou puissance de deux : $512\times 512$, $1024\times 1024$, $2048\times 2048$, $4096\times 4096$).
   - Heuristiques de placement : BSSF (*Best Short Side Fit*), BAF (*Best Area Fit*), BLSF (*Best Long Side Fit*).
2. **Options d'Empaquetage & Production de Jeux Vidéo :**
   - **Padding / Spacing :** Espacement configurable entre les frames (ex. 1px ou 2px) pour éviter le saignement de texture (*texture bleeding*).
   - **Extrude :** Répétition des pixels de bordure sur 1 pixel pour le filtrage bilinéaire dans les moteurs 3D/2D.
   - **Deduplication :** Détection des frames strictement identiques (hash 64-bit / MD5 des pixels) pour ne les stocker qu'une seule fois dans l'atlas tout en conservant les références dans les animations.
   - **Contrainte Puissance de Deux (POT) :** Ajustement automatique aux dimensions $2^n$ exigées par les GPU mobiles et consoles.
3. **Dialogue d'Exportation Dédié :**
   - Options interactives avec aperçu du taux de remplissage (*Packing Efficiency* en %) et de la taille finale de texture.

### 📊 Feuille de Route & Statut M6

| Spécification M6 | Statut | Composant / Fichier | Diagnostic & Livrables Prévus |
|---|:---:|---|---|
| **Moteur MaxRects 2D (BSSF / BAF)** | ⏳ **À implémenter** | `include/packer/maxrectspacker.h`, `src/packer/maxrectspacker.cpp` | Algorithme de gestion des rectangles libres maximaux avec découpage d'intersections. |
| **Contraintes Puissances de 2 (POT)** | ⏳ **À implémenter** | `maxrectspacker.cpp`, `atlaspacker.cpp` | Dimensionnement automatique en puissances de deux ($2^n$) minimales sans débordement. |
| **Padding anti-bleeding & Extrusion 1px** | ⏳ **À implémenter** | `atlaspacker.cpp` | Répétition de bordure et espacement configurable pour filtrage GPU net. |
| **Déduplication visuelle des frames** | ⏳ **À implémenter** | `spritedocument.cpp`, `atlaspacker.cpp` | Élimination des doublons de frames identiques avec redirection des indices d'animation. |
| **Intégration Dialogue d'Export & UI** | ⏳ **À implémenter** | `exportdialog.cpp`, `mainwindow.ui` | Choix du packer (Grid, Row, MaxRects), curseurs padding/extrude, aperçu du gain de surface. |
| **Tests CTest automatisés** | ⏳ **À implémenter** | `tests/test_core.cpp` | Validation de non-chevauchement, compacité, déduplication et stabilité multiplateforme. |

### Fichiers & Composants Cibles
- `SpriteStudio/include/packer/atlaspacker.h` / `src/packer/atlaspacker.cpp`.
- `SpriteStudio/include/packer/maxrectspacker.h` / `src/packer/maxrectspacker.cpp`.

---

## M7 : Suppression Avancée d'Arrière-Plan & Système de Filtres Graphiques (Filtres GIMP, Anti-Halo, Alt-Skins) — ✅ TERMINÉ & VALIDÉ (100%)

### 🏛️ Architecture Extensible de Plugins de Filtres Graphiques (`FilterPlugin` & `FilterRegistry`) — ✅ TERMINÉ & VALIDÉ (100%)

Pour éviter la prolifération de fenêtres ad-hoc et permettre l'ajout modulaire de nouveaux traitements sans modifier `MainWindow`, SpriteStudio s'appuie désormais sur une architecture standardisée de plugins et de registre centralisée :

- **Interface Abstraite de Plugin `FilterPlugin` (`include/filters/filterplugin.h`) :**
  - Contrat standardisé pour chaque filtre : `id()`, `name()`, `description()`, `category()`, `shortcut()`, et fabrique de dialogue `createDialog(parent, projectController)`.
  - Catégorisation hiérarchique intégrée (`Cleanup`, `Colors`, `Effects`, `Geometry`).
- **Registre Singleton `FilterRegistry` (`include/filters/filterregistry.h`, `src/filters/filterregistry.cpp`) :**
  - Enregistrement déclaratif de plugins (`registerFilter()`, `unregisterFilter()`, `filter()`, `filters()`).
  - Génération et alimentation dynamique du menu `Filtres` dans la barre de menus (`populateMenu()`), groupé par catégories logiques et séparateurs avec gestion gracieuse de l'activation selon la présence d'un document ouvert.
- **Commande Universelle `ApplyFilterCommand` (`include/commands/filtercommands.h`, `src/commands/filtercommands.cpp`) :**
  - `QUndoCommand` universelle pour les filtres graphiques préservant et restaurant l'atlas complet, les frames découpées, et les boîtes géométriques en cas d'Undo/Redo (`Ctrl+Z` / `Ctrl+Y`).
- **Composant Socle `FilterDialogBase` (`include/widgets/filterdialogbase.h`, `src/widgets/filterdialogbase.cpp`) :**
  - **Dialogue Flottant Non-Bloquant :** La fenêtre reste légère au-dessus de l'espace de travail sans masquer l'atlas ni bloquer l'interaction visuelle.
  - **Aperçu Réactif en Direct (Live Preview) :**
    - Timer anti-rebond (*debounce*) calibré à 80 ms : dès que l'utilisateur déplace un curseur, le filtre s'applique temporairement sur l'atlas et les frames affichées sur la scène principale.
    - Grâce aux optimisations de découpe $O(N)$ (`SpatialGrid2D` exécuté en 17 ms), le retour visuel est instantané et fluide, sans saccade.
    - Case à cocher permettant de désactiver l'aperçu dynamique à la demande.
  - **Gestion de l'État & Rollback Garanti (`reject()`) :**
    - À l'ouverture du filtre, un snapshot complet et non-destructif de l'état du document est conservé (`m_initialAtlas`, `m_initialFrames`, `m_initialBoxes`, `m_initialAnimations`).
    - Tout clic sur **Annuler**, appui sur la touche **Échap** ou fermeture de la fenêtre rétablit fidèlement l'état d'origine du document en 0 ms sans laisser d'effets secondaires.
  - **Validation & Annulation Complète (`QUndoStack` / `accept()`) :**
    - Tout clic sur **OK** ou appui sur **Entrée** applique l'effet et pousse un `ApplyFilterCommand` dédié sur la pile d'annulation du projet (`Ctrl+Z` / `Ctrl+Y`).
  - **Portée d'Application (Scope) :**
    - Permet d'appliquer le traitement soit à la planche entière (*Entire Atlas*), soit uniquement aux frames sélectionnées (*Selected Slices*).
  - **Bouton « Valeurs par défaut » :**
    - Réinitialise instantanément les curseurs aux valeurs recommandées ou mémorisées dans `AppConfig`.

---

### 🎨 Catalogue Détaillé des Filtres Prévus & Conçus

#### 1. Suppression d'Arrière-Plan (`BackgroundRemovalFilter` / `BackgroundRemovalDialog`) — ✅ Validé & Opérationnel
- **Rôle :** Éliminer le fond uni d'une planche importée et recalculer automatiquement les boîtes englobantes de chaque sprite.
- **Plugin :** ID `org.spritestudio.filter.background_removal`, catégorie `Cleanup`.
- **Paramètres :**
  - Échantillonnage automatique et pastille visuelle de la couleur dominante (#RRGGBB).
  - Curseur Tolérance de couleur (0 à 100).
  - Curseur Seuil Alpha (0 à 255).
  - Curseur Tolérance Verticale d'ordonnancement de lecture (0 à 50 px).
  - Options de rognage intelligent (Smart Crop) et seuil de chevauchement.
- **Retour visuel :** Badge dynamique informant du nombre exact de frames détectées en temps réel.

#### 2. Débavurage & Anti-Halo (*Despill / Edge Cleanup*) (`DespillFilter` / `DespillFilterDialog`) — ✅ Validé & Opérationnel
- **Rôle :** Éliminer le liseré verdâtre, blanc ou magenta de 1 pixel persistant sur le pourtour des sprites après détourage d'un fond JPEG ou antialiasé.
- **Plugin :** ID `org.spritestudio.filter.despill`, catégorie `Cleanup`.
- **Paramètres :**
  - Pipette / Sélecteur de couleur du liseré à neutraliser.
  - Curseur Tolérance de détection périphérique (0 à 100).
  - Mode d'action :
    - *Color Clamping / Despill doux :* Conserve l'alpha mais remplace la teinte du liseré par la couleur opaque du pixel intérieur adjacent (évite d'amputer les contours fins).
    - *Suppression stricte :* Rend transparents les pixels périphériques contaminés.
- **Portée :** Tout l'atlas ou sélection de frames actives (`isSelectedFramesOnly()`).

#### 3. Échange de Palette & Color Swap (*Alt-Skins / Recoloring*) (`ColorSwapFilter` / `ColorSwapFilterDialog`) — ✅ Validé & Opérationnel
- **Rôle :** Générer en un clic des variantes de personnages (Joueur 1 vs Joueur 2, variantes d'ennemis Gobelin vert $\rightarrow$ Gobelin de feu rouge) sans redessiner.
- **Plugin :** ID `org.spritestudio.filter.color_swap`, catégorie `Colors`.
- **Paramètres :**
  - Sélecteur de couleur source (pipette) et couleur de destination.
  - Curseur de tolérance colorimétrique (0 à 100).
  - Case à cocher *« Préserver l'ombrage (Shading HSV) »* : permute la teinte générale tout en conservant les dégradés d'ombres et reflets d'origine du pixel art.
- **Aperçu direct :** Permet d'observer la nouvelle variante s'animer immédiatement dans le lecteur de preview.

#### 4. Générateur de Contours & Silhouettes (*Outline & Stroke Generator*) (`OutlineFilter` / `OutlineFilterDialog`) — ✅ Validé & Opérationnel
- **Rôle :** Ajouter un contour marqué autour des sprites (effet de surbrillance/hover, style sticker, lisibilité sur décors sombres) ou produire des masques d'impact.
- **Plugin :** ID `org.spritestudio.filter.outline`, catégorie `Effects`.
- **Paramètres :**
  - Curseur Épaisseur (1 à 4 px).
  - Sélecteur de couleur du trait (noir `#000000`, blanc `#ffffff`, doré `#ffcc00`, ou personnalisé via dialogue de couleur).
  - Voisinage : 4-connecté (croix nette pour pixel art rétro) ou 8-connecté (diagonales lissées).
  - Option *« Silhouette pleine »* : remplit l'intérieur pour créer une ombre portée ou un flash blanc de dégât (*hit-flash*).

#### 5. Ajustements Teinte / Saturation / Valeur / Contraste (*HSV Adjust*) (`ColorAdjustFilter` / `ColorAdjustFilterDialog`) — ✅ Validé & Opérationnel
- **Rôle :** Harmoniser les couleurs d'une planche ou simuler des états d'altération (personnage empoisonné teinté de violet, personnage gelé bleuté, scène de nuit désaturée).
- **Plugin :** ID `color_adjust`, catégorie `Colors & Palettes`.
- **Paramètres :**
  - Teinte (Hue) : $-180^\circ$ à $+180^\circ$ avec sliders et spinboxes bidirectionnels.
  - Saturation : $-100\%$ (niveaux de gris complet) à $+100\%$.
  - Luminosité / Valeur : $-100\%$ (noir complet) à $+100\%$ (blanchiment/surbrillance).
  - Contraste : $-100\%$ (aplat à luminance moyenne 128) à $+100\%$ (contraste binaire marqué).
  - Ciblage sélectif : application globale ou restreinte aux frames sélectionnées.
  - Aperçu en direct réactif avec rollback non-destructif, case à cocher de relance de détection auto et intégration complète `QUndoStack`.

#### 6. Redimensionnement Pixel Art Net (*Pixel Rescale / Nearest-Neighbor & Scale2x*) (`PixelRescaleFilter` / `PixelRescaleFilterDialog`) — ✅ Validé & Opérationnel
- **Rôle :** Agrandir ou réduire un atlas sans que l'interpolation bilinéaire standard ne génère de flou destructeur sur le pixel art.
- **Plugin :** ID `pixel_rescale`, catégorie `Geometry & Transform`.
- **Paramètres :**
  - Facteur d'échelle : Entiers et fractions ($0.5\times$, $2\times$, $3\times$, $4\times$).
  - Moteur de filtrage : Nearest-Neighbor (pixels nets d'origine, respect absolu des proportions de blocs) et Scale2x / AdvMAME2x (lissage procédural sans flou des arêtes diagonales, avec Scale3x pour le facteur $3\times$).
  - Mise à l'échelle automatique des rectangles de découpe (`SpriteBox`) avec recalcul proportionnel ou relance de la détection automatique intelligente (`SpriteDetector`).

#### 7. Quantification & Palettes Rétro (*Palette Snapping & Bayer Dithering*) (`RetroPaletteFilter` / `RetroPaletteFilterDialog`) — ✅ Validé & Opérationnel
- **Rôle :** Forcer une planche à adopter une palette matérielle rétro authentique avec simulation de tramage ordonné d'époque.
- **Plugin :** ID `retro_palette`, catégorie `Colors & Palettes`.
- **Paramètres :**
  - Sélecteur de presets matériels intégrés : Game Boy DMG (4 verts authentiques), Game Boy Pocket (4 niveaux de gris), PICO-8 (16 couleurs fantasy console), NES / Famicom (54 teintes), Commodore 64 (16 couleurs VIC-II), CGA Mode 1 (cyan/magenta/blanc), CGA Mode 2 (rouge/vert/jaune), Endesga 32 (palette moderne pixel art).
  - Importation de palettes externes : prise en charge des formats `.hex` (Lospec), `.gpl` (GIMP/Aseprite), `.pal` (JASC/RGB), images `.png`/`.bmp` (extraction automatique des couleurs uniques).
  - Bandeau d'aperçu d'échantillons de couleurs dynamiques (*swatch strip*).
  - Tramage ordonné (*Ordered Bayer Dithering*) : matrices $2\times 2$, $4\times 4$ classique rétro, $8\times 8$ dégradés subtils, avec curseur d'intensité $0\%$ à $100\%$.
  - Distance colorimétrique perceptuelle pondérée pour l'œil humain ($\Delta E^2 = 2\Delta R^2 + 4\Delta G^2 + 3\Delta B^2$).

### 📊 Point d'Étape & Bilan de Clôture M7 (Statut : 🟢 100% — 7 Filtres Validés sous CTest)

| Composant M7 | Statut | Fichier(s) | Diagnostic & Réalisations |
|---|:---:|---|---|
| **Interface `FilterPlugin`** | ✅ **RÉSOLU & VALIDÉ** | `filterplugin.h` | Contrat modulaire pour plugins de filtres (id, name, category, shortcut, createDialog). |
| **Registre `FilterRegistry`** | ✅ **RÉSOLU & VALIDÉ** | `filterregistry.h`, `filterregistry.cpp` | Singleton enregistrant les filtres et générant dynamiquement le menu Filtres dans `MainWindow`. |
| **Socle `FilterDialogBase`** | ✅ **RÉSOLU & VALIDÉ** | `filterdialogbase.h`, `filterdialogbase.cpp` | Dialogue non-bloquant, Live Preview avec timer anti-rebond 80 ms, rollback fidèle sur Annuler/Échap. |
| **Case Détection Auto des Boîtes** | ✅ **RÉSOLU & VALIDÉ** | `filterdialogbase.cpp` | Checkbox intégrée dans tous les dialogues de filtres réexécutant `SpriteDetector` ou préservant les tranches. |
| **1. Suppression de Fond** | ✅ **RÉSOLU & VALIDÉ** | `backgroundremovaldialog.cpp` | Échantillonnage de dominante RGB, tolérance couleur, seuil alpha, tri vertical, live preview. |
| **2. Débavurage & Anti-Halo (Despill)** | ✅ **RÉSOLU & VALIDÉ** | `despillfilter.cpp`, `despillfilterdialog.cpp` | Suppression du liseré 1px via Color Clamping (préservation des contours fins) ou mode strict. |
| **3. Échange de Palette (Color Swap)** | ✅ **RÉSOLU & VALIDÉ** | `colorswapfilter.cpp`, `colorswapfilterdialog.cpp` | Création de variantes / alt-skins avec préservation de l'ombrage (Shading HSV). |
| **4. Générateur de Contours (Outline)** | ✅ **RÉSOLU & VALIDÉ** | `outlinefilter.cpp`, `outlinefilterdialog.cpp` | Contour 1-4px, connexités 4 et 8, presets couleurs et option silhouette pleine (hit-flash). |
| **5. Ajustements HSV (Color Adjust)** | ✅ **RÉSOLU & VALIDÉ** | `coloradjustfilter.cpp`, `coloradjustfilterdialog.cpp` | Réglages bidirectionnels Teinte (-180°/+180°), Saturation, Luminosité et Contraste. |
| **6. Redimensionnement Pixel Art** | ✅ **RÉSOLU & VALIDÉ** | `pixelrescalefilter.cpp`, `pixelrescalefilterdialog.cpp` | Facteurs 0.5x, 2x, 3x, 4x avec Nearest-Neighbor net et algorithmes Scale2x / Scale3x sans flou. |
| **7. Quantification & Palettes Rétro** | ✅ **RÉSOLU & VALIDÉ** | `retropalettefilter.cpp`, `retropalettefilterdialog.cpp` | Presets GB, PICO-8, NES, C64, CGA, Endesga32, import .hex/.gpl/.pal, Bayer Dithering 2x2/4x4/8x8. |
| **Commande `ApplyFilterCommand`** | ✅ **RÉSOLU & VALIDÉ** | `filtercommands.h`, `filtercommands.cpp` | Commande universelle Undo/Redo préservant l'atlas, les frames et les boîtes géométriques. |
| **Couverture Tests CTest (100%)** | ✅ **RÉSOLU & VALIDÉ** | `tests/test_controllers.cpp` | 8 tests unitaires dédiés validant chaque algorithme, le registre, la redétection et l'Undo/Redo. |

---

### ⚠️ Problématique Spécifique des Planches JPEG & Solutions Algorithmiques

Les planches de sprites issues du Web (rips JPEG sans couche alpha, compression DCT $8\times 8$, bruit de moustique et sous-échantillonnage 4:2:0) imposent des traitements spécialisés :
1. **Artefacts de sonnerie JPEG :** Résolu par le seuil de tolérance couplé au seuil alpha et au filtre de médiane.
2. **Sur-fusion des sprites proches :** Résolu par le partitionnement spatial `SpatialGrid2D` et la coupure de connexité par profil de projection.
3. **Cavités internes closes :** Approche hybride combinant flood-fill extérieur et ré-évaluation colorimétrique des cavités internes.

---

## M6 : Algorithme d'Empaquetage Avancé (MaxRects Bin-Packing)

### Contexte & Enjeux Métier
L'empaquetage d'atlas (*texture packing*) est au cœur des performances d'affichage des moteurs de jeu modernes (Godot, Unity, Defold, Raylib, Unreal Engine). Les algorithmes basiques en étagères (*Row/Shelf Packing*) ou en grilles uniformes gaspillent typiquement 40% à 60% de la surface de la texture dès que les sprites ont des dimensions hétérogènes.  
Le chantier **M6** dote **Sprite Studio** du standard industriel de bin-packing 2D : l'algorithme **MaxRects** (*Maximal Rectangles Algorithm* de Jukka Jylänki), couplé à la déduplication visuelle intelligente (*auto-aliasing*), au débordement de pixels anti-saignement (*extrusion*), et aux contraintes matérielles GPU (*Power of Two*).

---

### 🔬 Fonctionnalités Implémentées & Validées

#### 1. Algorithme MaxRects Complet (`MaxRectsPacker`) — ✅ Validé & Opérationnel
- **Gestion des rectangles libres maximaux :** Découpage à 4 quadrants après chaque placement (`splitFreeNode`), arithmétique en coordonnées d'intervalles demi-ouverts prévenant les erreurs de dépassement 1px de Qt, élagage strict des sous-rectangles redondants (`pruneFreeList` / `isContainedIn`).
- **5 Heuristiques de placement optimales :**
  - `BestShortSideFit` (BSSF, défaut) : Minimise le côté court résiduel de l'espace libre.
  - `BestLongSideFit` (BLSF) : Minimise le côté long résiduel.
  - `BestAreaFit` (BAF) : Minimise la surface libre restante.
  - `BottomLeft` : Favorise le regroupement vers le coin inférieur gauche.
  - `ContactPoint` : Maximise le périmètre en contact avec les bordures et les sprites déjà placés.
- **Tri préliminaire adaptatif :** Tri décroissant selon la hauteur et la surface pour maximiser le taux de remplissage.
- **Indicateur d'occupation :** Calcul en temps réel du taux d'occupation (`occupancy()`) et du rendement effectif (`efficiency`).

#### 2. Contraintes GPU & Dimensions Personnalisées (`AtlasPacker::PackOptions`) — ✅ Validé & Opérationnel
- **Recherche automatique de puissance de deux ($2^n$) :** Expansion progressive ($64, 128, 256, 512, 1024, 2048, 4096$) pour garantir la compatibilité maximale avec les GPU sans gaspillage.
- **Contrainte Carrée (*Force Square*) :** Largeur égale à la hauteur ($W = H$) sur demande.
- **Mode Compact (*Shrink to Fit*) :** Ajustement au plus près de la surface réelle occupée par les sprites pour l'exportation vers des moteurs supportant les dimensions libres (NPOT).

#### 3. Protection du Filtrage Bilinéaire & Marges (`Padding` & `Extrude`) — ✅ Validé & Opérationnel
- **Espacement interne (*Inner Padding*) :** Évite que les sprites adjacents ne se touchent.
- **Marge externe (*Border Padding*) :** Marge de sécurité sur le pourtour extérieur de la texture.
- **Extrusion de bordure (*Border Pixel Extrusion*) :** Répétition vers l'extérieur des pixels des 4 arêtes et des 4 coins (1-2 px). Éradique totalement le phénomène de saignement de texture (*texture bleeding*) causé par le filtrage bilinéaire ou le mipmapping dans les moteurs 2D/3D.

#### 4. Déduplication Visuelle Automatique (*Auto-Aliasing*) — ✅ Validé & Opérationnel
- **Comparaison bit-à-bit optimisée :** Détection des frames strictement identiques par balayage scanline et `std::memcmp`.
- **Réassignation transparente :** Les frames dupliquées partagent le même sous-rectangle dans l'atlas sans dupliquer les pixels.
- **Intégration transparente dans les animations :** `GodotExtractor` (`.tres`) et `JsonExtractor` (TexturePacker/Aseprite) conservent la timeline temporelle exacte tout en référençant la frame canonique unique.

#### 5. Boîte de Dialogue d'Export Interactive (`ExportDialog`) — ✅ Validé & Opérationnel
- **Remplacement de la boîte standard :** Raccourci `Ctrl+E` ou menu *Fichier > Exporter* ouvre désormais un dialogue complet dédié.
- **Paramétrage intuitif :** Choix du format (Godot SpriteFrames, JSON TexturePacker/Aseprite, PNG/ZIP), algorithme (Conserver l'agencement actuel WYSIWYG, MaxRects, Power of Two, Row, Grid), padding, extrusion, cases POT/Square/Deduplicate.
- **Mode WYSIWYG Direct (*Keep Current Layout*) :** Option par défaut permettant d'exporter fidèlement l'atlas et les découpes tels qu'affichés à l'écran dans l'éditeur (notamment après l'application du filtre de packing d'atlas) sans recalcul ni risque de divergence.
- **Aperçu des statistiques en temps réel :** Calcul avec anti-rebond (*debounce* 60 ms) des dimensions résultantes, du taux d'efficacité (%) et du nombre de frames dupliquées économisées.

#### 6. Empaquetage d'Atlas Interactif en Direct dans l'IHM (`AtlasPackingFilter` & `AtlasPackingDialog`) — ✅ Validé & Opérationnel
- **Prévisualisation en direct sur le canevas (*In-Editor Live Preview*) :** Déclenchée via le menu *Filtres > Géométrie & Transformations > Empaquetage d'Atlas (MaxRects)...* ou par le raccourci direct `Ctrl+Shift+P`.
- **Préservation stricte et non-destructive des animations :**
  - Sans déduplication : mise à jour des positions d'atlas et préservation absolue des indices temporels.
  - Avec déduplication (*Auto-Aliasing*) : fusion des frames graphiquement identiques et remappage automatique et transparent des index de timelines (`newIdx = duplicateMapping[oldIdx]`). La cadence FPS, la durée et les boucles d'animation sont **100% préservées**.
- **Annulation & Rétablissement universels (*Undo/Redo*) :** Intégration dans `QUndoStack` via `ApplyFilterCommand`. Un simple `Ctrl+Z` restaure l'atlas d'origine, toutes les tranches et les animations initiales.
- **Rollback instantané :** En cas d'annulation (*Annuler* ou touche `Échap`), l'état du document est restauré immédiatement sans altération.

---

### 📊 Bilan des Réalisations M6 (Statut : 🟢 100% CTest)

| Composant M6 | Statut | Fichier(s) | Diagnostic & Réalisations |
|---|:---:|---|---|
| **Moteur MaxRects** | ✅ **RÉSOLU & VALIDÉ** | `maxrectspacker.h`, `maxrectspacker.cpp` | MaxRects 2D complet, arithmétique d'intervalles demi-ouverts, 5 heuristiques, élagage. |
| **Pipeline AtlasPacker** | ✅ **RÉSOLU & VALIDÉ** | `atlaspacker.h`, `atlaspacker.cpp` | `PackOptions` enrichi, recherche POT, extrusion de bordures, déduplication scanline. |
| **Codecs Godot & JSON** | ✅ **RÉSOLU & VALIDÉ** | `godotextractor.cpp`, `jsonextractor.cpp`, `export.h` | Prise en charge des `PackOptions` et réassignation des sous-ressources dupliquées. |
| **Interface ExportDialog** | ✅ **RÉSOLU & VALIDÉ** | `exportdialog.h`, `exportdialog.cpp`, `exportdialog.ui` | Dialogue ergonomique, live stats débouncé, intégration dans `MainWindow`. |
| **Filtre Interactif IHM** | ✅ **RÉSOLU & VALIDÉ** | `atlaspackingdialog.h/cpp`, `atlaspackingfilter.h/cpp` | Live preview sur canevas, remappage animations non-destructif, Undo/Redo `Ctrl+Z`. |
| **Couverture Tests CTest (100%)** | ✅ **RÉSOLU & VALIDÉ** | `tests/test_core.cpp`, `tests/test_controllers.cpp` | 8 tests dédiés validant MaxRects, heuristiques, POT, déduplication, remappage d'animations et Undo. |
| **Internationalisation (i18n)** | ✅ **RÉSOLU & VALIDÉ** | `scripts/update_i18n.py`, `i18n/*.ts`, `i18n/*.qm` | 100% des libellés traduits (0 inachevée) en Français, Anglais et Japonais pour `ExportDialog`, `AtlasPackingDialog` et `AtlasPackingFilter`. Support dynamique `LanguageChange`. |

---

## M8 : Empaquetage Polygonal & Maillages Serrés (Polygon / Tight Mesh Packing) — ✅ TERMINÉ & VALIDÉ (100% CTest)

### Contexte & Enjeux Techniques
Dans l'empaquetage rectangulaire standard (M6), chaque frame est isolée dans un rectangle orthogonal $[x, y, w, h]$. Pour des sprites aux poses dynamiques (personnage en plein saut, bras levé, lame d'épée en diagonale, tentacules, queues, effets de foudre), ce rectangle contient souvent plus de 50% à 70% de pixels transparents inutilisés.  
L'**empaquetage polygonal (*Tight Packing / Sprite Mesh*)** substitue au rectangle une enveloppe polygonale 2D (convexe ou concave) épousant au plus près les pixels opaques de la silhouette :
- **Gain d'espace drastique (20% à 50% de surface d'atlas économisée) :** Les formes s'imbriquent comme des pièces de puzzle (la pointe d'une épée se glisse dans le creux sous l'aisselle ou entre les jambes d'une autre frame). Cette compacité permet fréquemment de faire tenir une série d'animations dans un atlas $1024\times 1024$ au lieu de devoir doubler vers un $2048\times 2048$, réduisant l'empreinte mémoire vidéo (VRAM) de **75%**.
- **Éradication de l'Overdraw GPU (Fillrate) :** Sur mobile et consoles (Switch, etc.), le processeur graphique ne gaspille plus de temps à exécuter les shaders sur des fragments transparents invisibles.

---

### 🔬 Réalisations Architecturales & Techniques Clôturées

#### 1. Pipeline Géométrique 2D Haute Performance (`SpriteStudioGeometry`) :
- **Extraction de contours étanches (`ContourTracer`) :**
  - Algorithme *Marching Squares* 2D évaluant la grille discrète du canal alpha (seuil configurable $\alpha \in [1..255]$).
  - Échantillonnage sous-pixel étanche avec grille paddée de 1 pixel garantissant la fermeture géométrique absolue sans risque de boucle infinie.
- **Simplification adaptative et anti-clipping (`PolygonSimplifier`) :**
  - Réduction de sommets par algorithme *Ramer-Douglas-Peucker* (RDP).
  - **Dilation normale sortante (*Outward Normal Padding*) :** calcul des bissectrices normales unitaires $\vec{n} = (\vec{n}_1 + \vec{n}_2) / \|\vec{n}_1 + \vec{n}_2\|$ étendant les sommets vers l'extérieur de 0.5 à 8.0 px avec bridage aux dimensions de la frame. Cette innovation élimine tout découpage accidentel des pixels d'art sur les angles vifs.
  - **Budget de sommets strict (*Vertex Budget*) :** mécanisme itératif augmentant le seuil $\varepsilon$ si le nombre de sommets dépasse le budget alloué (ex: 8, 12, 16 jusqu'à 48 sommets maximum pour l'optimisation GPU).
- **Triangulation Ear-Clipping & Métriques GPU (`Triangulator`) :**
  - Décomposition robuste des polygones non convexes en triplets d'indices $[i_0, i_1, i_2]$ compatibles avec les buffers d'index OpenGL / Vulkan / DirectX / WebGPU.
  - Calcul de l'aire par la formule de Shoelace (formule du lacet).
  - Mesure en direct du gain d'overdraw GPU :
    $$\text{Gain Overdraw (\%)} = \left(1 - \frac{\text{Aire}_{\text{Polygone}}}{\text{Aire}_{\text{BoundingBox}}}\right) \times 100\%$$
    permettant de constater des réductions typiques de **60% à 80%** du coût de fillrate transparent sur GPU mobiles et consoles.

#### 2. Modèle de Données & Persistance `.ssp` :
- **Extension de `SpriteBox` :**
  - Ajout des champs locaux : `QPolygonF polygon`, `QList<QPointF> vertices`, `QList<int> triangles`, `bool hasPolygonMesh`.
  - Méthodes `polygonArea()` et `overdrawSavings()`.
- **Sérialisation dans `ProjectManager` :**
  - Sauvegarde et restauration complètes dans le schéma JSON des projets `.ssp` avec conservation absolue de la géométrie, des sommets et de la table des triangles.

#### 3. Visualisation & Rendu HMI en Direct :
- **Superposition Wireframe dans `AtlasBoxItem` :**
  - Lignes de maillage cyan pointillés (`#00FFFF`, 50% d'opacité) reliant les triangles.
  - Contour extérieur vert néon (`#00FF88`, 1.5px cosmétique).
  - Poignées de contrôle aux sommets (cercles blancs de 3px de rayon avec liseré sombre).
- **Contrôle d'affichage dans le menu Vue :**
  - Action cochable dans le menu **Affichage** : *"Afficher les maillages polygonaux (Wireframe)"*.
  - Synchronisation instantanée avec tous les items de l'atlas via `AtlasViewController::setShowPolygonMeshes(bool)`.
- **Dialogue de réglage interactif (`PolygonMeshDialog`, raccourci `Ctrl+M`) :**
  - Zone de prévisualisation zoomée (400%) avec damier de transparence contrasté, boîte englobante d'origine en pointillés, et rendu fil de fer temps réel.
  - Curseurs pour la tolérance $\varepsilon$, le seuil Alpha, la dilatation (Padding) et le budget de sommets.
  - Tableau de bord avec nombre de sommets, de triangles, surface et pourcentage d'overdraw économisé en temps réel.
  - Boutons d'application à la sélection, à tous les sprites, ou de réinitialisation au rectangle classique.

#### 4. Édition Interactive des Sommets sur Canevas (`AtlasBoxItem`) :
- **Manipulation par point individuel :** Survol avec mire (`Qt::CrossCursor`), clic-glisser d'un sommet en direct avec re-triangulation immédiate.
- **Multi-sélection de sommets (`Shift` / `Ctrl` + Clic) :** Sélection multiple avec halo cyan `#00E5FF`, déplacement simultané du groupe de sommets sélectionnés.
- **Micro-déplacement au clavier (Touches Fléchées) :** Déplacement de 1 px (ou 5 px avec `Shift`) des sommets sélectionnés.
- **Insertion par double-clic :** Double-cliquer sur une arête insère un nouveau sommet à la projection exacte du pointeur.
- **Suppression (`Touche Suppr` / `Retour Arrière`) :** Supprime les sommets sélectionnés (tant que $\ge 3$ sommets subsistent) avec re-triangulation atomique.
- **Annulation / Rétablissement complet (`QUndoStack`) :** Toute action crée une commande `SetPolygonMeshCommand` sur la pile `QUndoStack` (`Ctrl+Z` / `Ctrl+Y`).
- **Hit-testing polygonal précis :** `AtlasBoxItem::shape()` exclut les clics dans les zones transparentes hors-polygone pour faciliter la sélection des sprites imbriqués.

#### 5. Algorithme d'Empaquetage Serré (`TightPolygonPacker`) & Accélération :
- **Nesting 2D Non-Convexe :**
  - Les boîtes englobantes ($AABB$) sont autorisées à se chevaucher sans collision entre leurs pixels opaques et leurs marges dilatées.
- **Optimisation massive des candidats (sub-20ms) :**
  - Remplacement de 26 000 points de grille redondants par ~300 points d'ancrage ciblés (coins réels de contact, bordures, et quadrillage skyline de 64 px).
  - Glissement exponentiel dichotomique dans `slidePosition()` : sauts en puissances de deux `{64, 32, 16, 8, 4, 2, 1}` (7 vérifications max au lieu de 64).
  - Calibration de l'aire initiale pour réussir dès la première passe.
- **Multithreading haute performance configurable :**
  - Parallélisation de la préparation des masques et de l'évaluation des candidats via `QThreadPool` dédié et `QtConcurrent::blockingMap`.
  - Sélecteur de threads (`QSpinBox`) dans `AtlasPackingDialog` borné de 1 au nombre maximal de cœurs logiques de la machine (`QThread::idealThreadCount()`).
  - Mémorisation de la préférence dans `QSettings` (`atlasPacking/threads`).
- **Ergonomie non-bloquante à l'ouverture :**
  - Aucun lancement automatique du packing à l'ouverture de la boîte de dialogue (ouverture instantanée).
  - Contrôles initialisés avec signaux bloqués, case `Live Preview` décochée par défaut à l'ouverture.
  - Bouton d'action proéminent « Calculer le packing » pour lancer le calcul uniquement quand l'utilisateur est prêt.

#### 6. Export Multi-Moteurs :
- **TexturePacker JSON Étendu :** Format universel enrichi avec `vertices`, `verticesUV` et `triangles`.
- **Unity 2D (`.unity.json`) :** Format compatible avec `SpriteMeshType.Tight`, avec inversion d'axe UV Y et géométrie complète.
- **Unreal Engine Paper2D (`.paper2d.json`) :** Format descriptif avec polygones de rendu et de collision.
- **Godot 4 Compagnon `_mesh.tres` :** Génération de ressources `ArrayMesh` 2D prêtes à l'emploi avec `MeshInstance2D`.
- **Dialogue d'export unifié (`ExportDialog`) :** Intégration des options de maillage polygonal dans l'interface d'export.

---

### ⚖️ Tableau Comparatif : Packing Rectangulaire vs Packing Polygonal

| Critère | M6 : MaxRects Rectangulaire | M8 : Packing Polygonal / Tight Mesh |
|---|---|---|
| **Compatibilité Moteurs** | 🟢 **100% Universelle** (Tous moteurs, tous composants 2D) | 🟢 **Large & Intégrée** (Godot 4 `ArrayMesh`, Unity `Tight`, Unreal Paper2D, TexturePacker JSON) |
| **Gain de Surface d'Atlas** | Standard (Baseline) | 🟢 **+20% à +50% de compacité** (évite de doubler la taille d'atlas) |
| **Consommation VRAM** | Moyenne | 🟢 **Minimale** (textures plus petites) |
| **Overdraw GPU (Fillrate)** | Élevé sur formes ouvertes (quads transparents) | 🟢 **Quasi-nul** (60% à 80% d'overdraw économisé) |
| **Temps CPU au Packing** | Rapide ($< 10$ ms) | 🟢 **Ultra-rapide** ($10$ à $20$ ms grâce aux ancres et au multithreading) |
| **Édition Interactive** | Découpe rectangulaire classique | 🟢 **Directe sur canevas** (sélection, glisser, insérer, supprimer des sommets) |

---

### 📁 Fichiers & Composants Réalisés
- `SpriteStudio/include/geometry/contourtracer.h` / `src/geometry/contourtracer.cpp` : Marching Squares étanche 2D.
- `SpriteStudio/include/geometry/polygonsimplifier.h` / `src/geometry/polygonsimplifier.cpp` : Simplification RDP, outward normal dilation, vertex budget.
- `SpriteStudio/include/geometry/triangulator.h` / `src/geometry/triangulator.cpp` : Ear-Clipping triangulation, formule de Shoelace, télémétrie overdraw.
- `SpriteStudio/include/packer/tightpolygonpacker.h` / `src/packer/tightpolygonpacker.cpp` : Algorithme de bin-packing polygonal avec multithreading et optimisation des ancres.
- `SpriteStudio/include/widgets/polygonmeshdialog.h` / `src/widgets/polygonmeshdialog.cpp` : Boîte de dialogue interactive de réglage de maillage.
- `SpriteStudio/include/widgets/atlaspackingdialog.h` / `src/widgets/atlaspackingdialog.cpp` : IHM d'empaquetage avec sélection de threads, calcul à la demande et contrôle de prévisualisation.
- `SpriteStudio/src/widgets/atlasboxitem.cpp` : Rendu fil de fer, manipulation de sommets, hit-testing polygonal.
- `SpriteStudio/src/extractor/unityextractor.cpp`, `unrealextractor.cpp`, `godotextractor.cpp` : Codecs d'export multi-moteurs.
- `tests/test_mesh.cpp` : Suite automatisée de 22 tests unitaires (100% de réussite sous CTest).

---

## M-CLI : Interface Ligne de Commande & Automatisation CI/CD (`spritestudio-cli`) — 🚀 Compatibilité Totale TexturePacker, Aseprite & Godot 4

### 📌 Contexte & Enjeux Industriels
Dans les studios professionnels et les productions indépendantes d'envergure, les artistes ne manipulent pas manuellement une interface graphique pour exporter 50 planches à chaque mise à jour de sprites. Des scripts de build (Makefiles, scripts Python, CMake) et des pipelines d'Intégration Continue (GitHub Actions, GitLab CI) ré-empaquettent automatiquement les atlas et régénèrent les métadonnées de moteur de jeu (`.tres`, `.json`).

**TexturePacker** doit l'essentiel de son quasi-monopole en studio à son binaire en ligne de commande scriptable. De son côté, **Aseprite** est omniprésent pour le dessin et l'export batch de frames via son interface `-b` (`aseprite -b`). Enfin, **Godot 4** est devenu le moteur 2D de référence exigeant des fichiers de ressources natifs (`SpriteFrames` `.tres`) avec sous-textures `AtlasTexture`.

**Objectifs Stratégiques Majeurs de SpriteStudio :**
1. **Drop-in Replacement 100% de TexturePacker :** Remplacer purement et simplement le binaire `TexturePacker` dans n'importe quel pipeline studio existant sans modifier un seul script de build (compatibilité syntaxique des flags et sémantique des formats d'export JSON/Godot).
2. **Compatibilité Étendue avec Aseprite CLI :** Accepter la syntaxe de compilation de feuilles de sprites d'Aseprite (`--sheet`, `--data`, `--list-tags`, `--sheet-type`, etc.).
3. **Intégration Native & Transparente Godot 4 :** Générer directement des ressources `.tres` `SpriteFrames` riches avec `margin = Rect2(...)` (pivots M3 & trim M1) et préservation des UIDs Godot 4 (`uid://...`), évitant toute importation manuelle dans l'éditeur.
4. **Moteur Headless Ultra-Rapide ($< 80$ ms) :** Binaire console autonome lié à la bibliothèque statique `SpriteStudioCore` fonctionnant hors-affichage (`QT_QPA_PLATFORM=offscreen` / `QCoreApplication`), sans dépendance GUI.

---

### 📚 Références & Liens vers les Documentations Officielles
- 📖 [TexturePacker CLI Documentation & Command-Line Arguments (CodeAndWeb)](https://www.codeandweb.com/texturepacker/documentation/command-line)
- 📖 [Aseprite Command-Line Interface Manual](https://www.aseprite.org/docs/cli/)
- 📖 [Godot 4 Command Line Tutorial & Headless Mode](https://docs.godotengine.org/en/stable/tutorials/editor/command_line_tutorial.html)
- 📖 [Godot 4 `SpriteFrames` & `AtlasTexture` Resource Specification](https://docs.godotengine.org/en/stable/classes/class_spriteframes.html)

---

### 🏛️ Architecture Multi-Saveurs du Parser CLI (`CliDispatcher`)

Pour supporter à la fois la syntaxe TexturePacker, la syntaxe Aseprite et les sous-commandes natives SpriteStudio sans collision de paramètres, le binaire repose sur une architecture à détection de saveur (*CLI Flavor Detection*) :

```
                        [Invocation CLI / argv]
                                   │
                    ┌──────────────┴──────────────┐
                    ▼                             ▼
       [argv[0] == "TexturePacker"]     [argv[0] == "aseprite"]
                    │                             │
                    ▼                             ▼
       ┌──────────────────────────┐  ┌──────────────────────────┐
       │ TexturePacker Flavor     │  │ Aseprite Flavor          │
       │ (100% Flags TexturePacker│  │ (Flags --sheet, --data,  │
       │  --sheet, --data, --opt) │  │  --list-tags, -b batch)  │
       └────────────┬─────────────┘  └────────────┬─────────────┘
                    │                             │
                    └──────────────┬──────────────┘
                                   │
        [argv[0] == "spritestudio-cli" ou --flavor=...]
                                   ▼
                    ┌──────────────────────────┐
                    │ Universal / Native Mode  │
                    │ (Subcommands pack, slice,│
                    │  filter, export, ssp)    │
                    └──────────────┬───────────┘
                                   │
                                   ▼
                      [CliOptionMapper / Core API]
                                   │
         ┌─────────────────────────┼─────────────────────────┐
         ▼                         ▼                         ▼
  [AtlasPacker]            [SpriteDetector]          [FilterRegistry]
  (MaxRects / POT)       (Auto-Slice / RemBg)      (7 Filtres Plugins)
         │                         │                         │
         └─────────────────────────┼─────────────────────────┘
                                   │
                                   ▼
                    [Sorties : Atlas PNG + JSON / TRES]
```

- **Détection Automatique par Alias (`argv[0]`) :**
  - Si le binaire est invoqué sous le nom `TexturePacker` (ou `TexturePacker.exe` via lien symbolique, wrapper shell ou copie dans le `PATH`), il active par défaut la saveur stricte TexturePacker.
  - Si invoqué sous `aseprite`, il active la saveur Aseprite.
  - Si invoqué sous `spritestudio-cli`, il accepte soit les sous-commandes natives, soit les options TexturePacker/Aseprite de façon universelle.
  - Un paramètre explicite `--flavor <texturepacker|aseprite|godot|native>` permet de forcer la saveur si nécessaire.

---

### 📦 1. Compatibilité Totale avec TexturePacker CLI (Drop-In Replacement)

TexturePacker est émulé à 100% de sa syntaxe et de ses fonctionnalités de packing d'atlas :

| Argument TexturePacker | Type / Valeurs | Rôle & Équivalence Métier SpriteStudio |
|---|---|---|
| `--sheet <file>` | Fichier (`.png`) | Fichier image de sortie de l'atlas composite (`AtlasPacker::pack()`). |
| `--data <file>` | Fichier (`.json`, `.tres`) | Fichier de métadonnées généré (JSON TexturePacker ou Godot 4 SpriteFrames). |
| `--format <format>` | `json`, `json-array`, `json-hash`, `godot`, `godot4`, `phaser`, `unity`, `libgdx` | Format des métadonnées. `json-array` et `godot4` traités nativement avec fidélité absolue. |
| `--texture-format <fmt>` | `png`, `webp`, `jpg` | Format d'encodage de la texture finale (défaut : `png` 32-bit ARGB). |
| `--algorithm <algo>` | `MaxRects`, `Basic` | Algorithme d'empaquetage 2D (MaxRects M6 par défaut, ou Grille/Ligne). |
| `--maxrects-heuristics <h>` | `BestShortSideFit`, `BestLongSideFit`, `BestAreaFit`, `BottomLeft`, `ContactPoint` | Heuristique de sélection des rectangles libres de MaxRects. |
| `--opt <pixel_format>` | `RGBA8888`, `BGRA8888`, `RGBA4444`, `RGB888`, `RGB565` | Format mémoire des pixels de l'image (mappé sur `QImage::Format`). |
| `--size-constraints <c>` | `POT`, `AnySize`, `WordAligned` | `POT` force les dimensions en puissances de deux ($2^n$, standard GPU). |
| `--max-size <w> <h>` / `--max-width <w>` / `--max-height <h>` | Entiers (ex: `2048 2048`) | Dimensions maximales autorisées pour la feuille de texture. |
| `--width <w>` / `--height <h>` | Entiers | Force une largeur/hauteur fixe de l'atlas. |
| `--scale <factor>` | Flottant (ex: `0.5`, `1.0`, `2.0`) | Facteur de redimensionnement de l'atlas ou des sprites. |
| `--scale-mode <mode>` | `Smooth`, `Fast` | `Smooth` = interpolation bilinéaire, `Fast` = Nearest-Neighbor ou Scale2x sans flou pour pixel art. |
| `--trim-mode <mode>` | `Trim`, `Crop`, `None` | `Trim` : rogne les bordures transparentes tout en conservant les dimensions d'origine dans le JSON/TRES (`computeTrimmedRect()`). `None` : conserve la boîte intégrale. |
| `--trim-threshold <0-255>` | Entier (défaut: `1`) | Seuil alpha en-deçà duquel un pixel est considéré transparent. |
| `--padding <px>` | Entier (défaut: `2`) | Espacement global entre chaque sprite pour prévenir le saignement de texture (*texture bleeding*). |
| `--shape-padding <px>` | Entier | Espacement spécifique autour de la silhouette de chaque sprite. |
| `--border-padding <px>` | Entier | Marge extérieure le long des 4 bords de l'atlas. |
| `--extrude <px>` | Entier (0, 1, 2) | Répétition des pixels de bordure sur N pixels pour le filtrage bilinéaire GPU. |
| `--enable-auto-alias` (défaut) / `--detect-identical-sprites` | Booléen | Détection et déduplication automatique des frames strictement identiques (redirection d'indices d'animation). |
| `--disable-auto-alias` | Booléen | Désactive la déduplication (conserve toutes les frames dupliquées séparément dans l'atlas). |
| `--pivot-point <x> <y>` | Flottants $[0.0, 1.0]$ ou mots-clés | Point d'ancrage global des sprites (`0.5 1.0` pour bas-centre, `0.5 0.5` pour centre, `0.0 0.0` pour haut-gauche). Mappé sur les pivots M3. |
| `--variant <scale>:<suffix>` | Chaîne (ex: `0.5:@0.5x`) | Génération multi-résolution pour écrans HD/SD. |
| `--prepend-folder-name` | Booléen | Inclut le nom des sous-dossiers dans les identifiants de frames du JSON. |
| `--quiet` / `--verbose` | Drapeaux | Contrôle de la verbosité de la sortie console. |
| `<images / dossiers...>` | Arguments positionnels | Liste des fichiers PNG/JPG sources ou dossiers récursifs à empaqueter. |

**Exemple d'Exécution en Drop-in Replacement TexturePacker :**
```bash
# Appel strict identique à TexturePacker dans un Makefile ou script de build
spritestudio-cli --sheet characters.png --data characters.json \
  --format json-array --algorithm MaxRects --maxrects-heuristics BestShortSideFit \
  --padding 2 --extrude 1 --trim-mode Trim --size-constraints POT \
  --max-size 2048 2048 assets/sprites/*.png
```

---

### 🎨 2. Compatibilité Étendue avec Aseprite CLI (`aseprite -b`)

Aseprite est le standard de création pixel art. `spritestudio-cli` supporte les commandes d'export de planches et de métadonnées d'animation d'Aseprite :

| Argument Aseprite | Type / Valeurs | Rôle & Équivalence Métier SpriteStudio |
|---|---|---|
| `-b`, `--batch` | Drapeau | Mode non-interactif / headless (implicite et natif dans `spritestudio-cli`). |
| `--sheet <file>` | Fichier image (`.png`) | Fichier de destination de la planche de sprites générée. |
| `--data <file>` | Fichier JSON (`.json`) | Fichier de données JSON exporté avec tags d'animations et frames. |
| `--format <fmt>` | `json-array`, `json-hash` | Structure du JSON Aseprite (`frames`, `meta`, `frameTags`). |
| `--sheet-type <type>` | `horizontal`, `vertical`, `matrix`, `packed` | Disposition : bande horizontale (`RowPacker`), colonne, grille (`GridPacker`), ou compactée (`MaxRects`). |
| `--sheet-width <w>` / `--sheet-height <h>` | Entiers | Dimensions cibles de la feuille. |
| `--sheet-columns <n>` / `--sheet-rows <n>` | Entiers | Nombre de colonnes ou de rangées de la grille. |
| `--list-tags` | Drapeau | Exporte le tableau `frameTags` contenant les animations nommées, leurs bornes (`from`, `to`) et leur sens (`forward`, `reverse`, `pingpong`). Directement relié à `SpriteAnimation` M2. |
| `--list-layers` | Drapeau | Exporte la liste des calques d'origine. |
| `--list-slices` | Drapeau | Exporte les découpes et boîtes (`SpriteBox`). |
| `--trim` / `--trim-sprite` | Drapeaux | Rognage automatique de la transparence périphérique. |
| `--inner-padding <px>` | Entier | Rembourrage intérieur de chaque frame. |
| `--border-padding <px>` | Entier | Marge extérieure de la feuille. |
| `--shape-padding <px>` | Entier | Espacement entre chaque frame. |
| `--extrude` | Drapeau | Extrusion des pixels de bordure anti-saignement. |
| `--ignore-empty` | Drapeau | Ignore les frames 100% transparentes sans les empaqueter. |
| `--split-tags` | Drapeau | Génère un fichier atlas distinct pour chaque animation taggée (`walk.png`, `idle.png`, `attack.png`). |
| `--save-as <file>` | Fichier de sortie | Conversion universelle en ligne de commande (ex: convertir un `.ssp` ou `.gif` en atlas PNG). |

**Exemple d'Exécution Compatible Aseprite :**
```bash
# Compilation d'atlas Aseprite avec extraction des animations
spritestudio-cli -b character.ssp --sheet character_sheet.png --data character_sheet.json \
  --format json-array --list-tags --sheet-type packed --trim
```

---

### 🎮 3. Intégration Native & Optimisations Spécifiques pour Godot 4

L'intégration avec Godot 4 va au-delà d'un simple export de texture : elle produit des ressources nativement exploitables dans l'arbre de scène de Godot :

1. **Ressource Native `.tres` (`SpriteFrames`) :**
   - Génération directe de fichiers texte `.tres` sans aucune conversion intermédiaire.
   - Sous-ressources `AtlasTexture` automatiquement imbriquées avec :
     - `atlas = ExtResource("1_atlas")` : Référence propre à la texture PNG de l'atlas.
     - `region = Rect2(x, y, w, h)` : Boîte englobante exacte de la frame dans l'atlas.
     - `margin = Rect2(offsetX, offsetY, originalWidth, originalHeight)` : Décalage de marge calculé à partir du pivot M3 et du trim M1 pour éradiquer tout sautillement (*jittering*).
     - `filter = Nearest` : Préréglage de filtrage net pour les jeux en pixel art.
2. **Gestion Intelligente des UIDs Godot 4 (`--godot-uid`) :**
   - Godot 4 associe un identifiant unique universel `uid://...` à chaque fichier de ressource.
   - Lors d'une régénération automatique en CI/CD, `spritestudio-cli` conserve l'UID préexistant dans le fichier `.tres` cible ou en calcule un déterministe pour éviter de briser les dépendances et de polluer les diffs Git.
3. **Génération Optionnelle de Scène Complète (`--godot-scene <node.tscn>`) :**
   - Option `--godot-scene player.tscn` produisant une scène 2D instantiable avec un nœud `[node name="Player" type="AnimatedSprite2D"]` pré-câblé avec toutes ses animations (`idle`, `run`, `jump`), son FPS exact et sa configuration de boucle (`loop = true/false`).
4. **Enchaînement CI Headless avec Godot :**
   - `spritestudio-cli` retourne des codes d'état POSIX stricts permettant d'enchaîner directement dans un pipeline GitHub Actions :
     ```bash
     spritestudio-cli pack --format godot4 --sheet res://assets/sprites.png --data res://assets/sprites.tres assets/raw/*.png
     godot --headless --import  # Importation automatique dans le projet Godot sans interface graphique !
     ```

---

### ⚡ 4. Super-Pouvoirs Exclusifs SpriteStudio (Au-delà de la Concurrence)

Là où TexturePacker et Aseprite exigent que les sprites soient déjà découpés en amont dans des fichiers PNG individuels, `spritestudio-cli` apporte ses algorithmes de découpage et de filtrage uniques :

1. **Découpage Automatique de Planches Brutes (`spritestudio-cli slice`) :**
   - Découpe automatique d'une planche brute ou d'un rip JPEG en sprites individuels sans fichier de métadonnées préalable.
   - Algorithme de composantes connexes accéléré par `SpatialGrid2D` ($O(N)$ en $< 20$ ms).
   - Arguments :
     - `--remove-bg [hexColor]` : Suppression automatique de la couleur de fond (ou détection de dominante).
     - `--tolerance <0-100>` : Tolérance colorimétrique sur le fond.
     - `--alpha-threshold <0-255>` : Seuil d'opacité.
     - `--smart-crop` : Recalcul serré des boîtes sur les pixels opaques.
     - `--order <row-major|column-major>` : Tri séquentiel de lecture des frames.
     - `--output-project <file.ssp>` : Sauvegarde directe sous forme de projet complet `.ssp`.

2. **Application de Filtres Graphiques Headless (`spritestudio-cli filter`) :**
   - Applique en ligne de commande les filtres M7 sur une planche ou un projet sans ouvrir l'IHM :
     - `--despill [hexColor] --despill-mode <clamp|strict>` : Suppression du liseré périphérique de 1 px.
     - `--outline <1-4> --outline-color <hexColor>` : Génération de contours vectoriels nets.
     - `--color-swap "<srcHex>:<dstHex>"` : Remplacement de couleurs avec préservation de l'ombrage HSV.
     - `--color-adjust --hue <deg> --saturation <pct> --contrast <pct>` : Harmonisation colorimétrique.
     - `--pixel-rescale <2x|3x|4x> --filter <scale2x|nearest>` : Agrandissement procédural sans flou.
     - `--retro-palette <preset|file.hex|file.gpl> --dither <bayer4x4>` : Quantification rétro.

3. **Time-Travel & Manipulation de Projets `.ssp` (`spritestudio-cli ssp`) :**
   - `--checkout-revision <hash>` : Extraction headless d'une révision Git historique embarquée dans un `.ssp`.
   - `--export <format>` : Conversion d'un `.ssp` vers Godot, JSON, ou GIF animé.

---

### 🖥️ Spécification des Codes de Sortie POSIX & Format JSON

Pour garantir une intégration sans faille dans les scripts Bash, PowerShell et les orchestrateurs CI/CD :

- **Codes de Sortie POSIX Déterministes :**
  - `0` : Succès absolu, atlas et données générés conformément.
  - `1` : Erreur de syntaxe dans les arguments de ligne de commande (option inconnue, valeur hors bornes).
  - `2` : Fichier ou répertoire source introuvable ou illisible.
  - `3` : Contrainte d'empaquetage non réalisable (ex: les sprites dépassent la taille maximale `--max-size` spécifiée).
  - `4` : Erreur d'écriture disque ou permissions insuffisantes sur la cible.

- **Option `--json` pour Intégration Pipeline :**
  En mode `--json`, les sorties d'état sont sérialisées sur `stdout` pour être analysées par d'autres scripts :
  ```json
  {
    "status": "success",
    "atlas": "characters.png",
    "data": "characters.json",
    "dimensions": { "width": 1024, "height": 1024 },
    "frames_count": 48,
    "packing_efficiency": 0.874,
    "elapsed_ms": 42
  }
  ```

---

### 📊 Feuille de Route & Statut M-CLI

| Composant M-CLI | Statut | Fichier(s) Cibles | Diagnostic & Livrables |
|---|:---:|---|---|
| **Spécification Multi-Saveur & Rétrocompatibilité** | ✅ **RÉSOLU & VALIDÉ** | `TODO.md` | Spécifications complètes 100% TexturePacker, Aseprite et Godot 4. |
| **Socle Moteur Headless (`SpriteStudioCore`)** | ✅ **RÉSOLU & VALIDÉ** | `SpriteStudioCore` | Bibliothèque découplée de l'IHM, opérant sur `QImage` pure en offscreen. |
| **Parser Universel & Dispatcher (`CliDispatcher`)** | ✅ **RÉSOLU & VALIDÉ** | `include/cli/cliparser.h`, `src/cli/cliparser.cpp` | Détection par `argv[0]`, analyse des arguments et validation POSIX. |
| **Émulateur TexturePacker (`TexturePackerAdapter`)** | ✅ **RÉSOLU & VALIDÉ** | `include/cli/tp_adapter.h`, `src/cli/tp_adapter.cpp` | Mapping complet des 22 arguments TexturePacker vers le moteur de packing M6. |
| **Émulateur Aseprite (`AsepriteAdapter`)** | ✅ **RÉSOLU & VALIDÉ** | `include/cli/aseprite_adapter.h`, `src/cli/aseprite_adapter.cpp` | Support de `-b`, `--sheet`, `--data`, `--list-tags`, `--sheet-type`, `--trim`. |
| **Pipeline Natif Godot 4 (`GodotPipeline`)** | ✅ **RÉSOLU & VALIDÉ** | `include/cli/godot_pipeline.h`, `src/cli/godot_pipeline.cpp` | Génération `.tres` SpriteFrames, UIDs stables, marges de pivots et scènes `.tscn`. |
| **Commandes Étendues (`slice`, `filter`, `ssp`)** | ✅ **RÉSOLU & VALIDÉ** | `include/cli/native_commands.h`, `src/cli/native_commands.cpp` | Automatisation headless de `SpriteDetector`, des filtres M7 et de `ProjectManager`. |
| **Cible Exécutable CMake (`spritestudio-cli`)** | ✅ **RÉSOLU & VALIDÉ** | `SpriteStudio/CMakeLists.txt` | Cible console légère liée à `SpriteStudioCore`, sans dépendance d'affichage. |
| **Suite de Tests CLI Headless** | ✅ **RÉSOLU & VALIDÉ** | `tests/test_cli.cpp` | 9 tests unitaires automatisés validant syntaxes, formats, codes POSIX et sortie JSON (100% CTest). |

### Fichiers & Composants Cibles
- `SpriteStudio/include/cli/cliparser.h` / `src/cli/cliparser.cpp` : Moteur de dispatching et parsing multi-saveur.
- `SpriteStudio/include/cli/tp_adapter.h` / `src/cli/tp_adapter.cpp` : Adaptateur de rétrocompatibilité TexturePacker 100%.
- `SpriteStudio/include/cli/aseprite_adapter.h` / `src/cli/aseprite_adapter.cpp` : Adaptateur Aseprite.
- `SpriteStudio/include/cli/godot_pipeline.h` / `src/cli/godot_pipeline.cpp` : Pipeline Godot 4 SpriteFrames & UIDs.
- `SpriteStudio/src/cli/main_cli.cpp` : Point d'entrée de l'application console.
- `SpriteStudio/CMakeLists.txt` : Déclaration de la cible console `spritestudio-cli`.

---

## 🎨 ASSETS : Remplacement des Échantillons (`sample/`) par des Assets Originaux (Libres de Droits) — ✅ TERMINÉ & VALIDÉ (100%)

### 📌 Contexte & Problématique Résolue
- Auparavant, les fichiers de test du dossier `sample/` provenaient de sprites de jeux sous copyright et devaient être exclus du dépôt Git.
- Cela rendait l'exécution de la suite de tests CTest dépendante de fichiers locaux non versionnés et bloquait l'intégration continue (CI/CD) sur GitHub Actions.

### 🎯 Réalisations & Assets Générés
1. **Générateur Procédural d'Assets (`scripts/generate_sample_assets.py`) :**
   - Script autonome utilisant la bibliothèque standard Python / Pillow pour générer un personnage original (« Hero ») en pixel art avec 4 frames distinctes.
   - Variantes produites avec fond transparent et fond coloré (#00FF00) légèrement compressé pour éprouver le détourage et les filtres graphiques M7.
2. **Formats de Test Versionnés dans le Dépôt (`sample/`) :**
   - `sample/hero.png` : Planche d'atlas 4 frames avec canal alpha transparent.
   - `sample/hero_bg.png` : Planche avec fond uni pour tester `BackgroundRemovalFilter` et `DespillFilter`.
   - `sample/hero.gif` : GIF animé 4 frames pour `GifExtractor`.
   - `sample/hero.json` : Atlas JSON normalisé pour `JsonExtractor` (TexturePacker & Aseprite).
   - `sample/hero_godot.tres` : Ressource Godot 4 SpriteFrames pour `GodotExtractor`.
3. **Pérennisation & Reproductibilité CI/CD :**
   - Tous les tests unitaires (`test_extractors.cpp`, `test_controllers.cpp`, `test_project.cpp`) s'exécutent de façon 100% autonome, déterministe et reproductible dès le clonage du dépôt.
   - Zéro risque juridique, zéro dépendance externe non versionnée, 100% de succès sous CTest.

---

## 🛠️ AUDIT : Points de Vigilance & Dette Technique Résiduelle (Recommandations d'Amélioration)

Ce volet consigne l'ensemble des axes d'amélioration, points de fragilité et dettes techniques mis en lumière lors de l'audit critique approfondi du projet (architecture logicielle, intégrité du modèle de données, build CMake, tests & DevOps, ergonomie et documentation).

### 1. Architecture & Modèle de Données (Core Model Integrity) — ✅ TERMINÉ (100%)

- **Purification de `SpriteDocument` (`QImage` vs `QPixmap`) — ✅ TERMINÉ :**
  - *Constat & Problème résolu :* `SpriteDocument::m_frames` stockait auparavant une liste de `QPixmap` (`QList<QPixmap>`), assujettie au serveur d'affichage graphique / GPU, provoquant des plantages intermittents lors des traitements asynchrones en arrière-plan (`QtConcurrent`) sous Linux (X11/Wayland) et macOS.
  - *Réalisé :* Refactorisation intégrale de `SpriteDocument` pour stocker exclusivement des `QImage` en mémoire CPU. Tous les codecs (`SpriteExtractor`, `GifExtractor`, `JsonExtractor`, `GodotExtractor`), l'empaqueteur `AtlasPacker`, le gestionnaire `ProjectManager` et les commandes `QUndoCommand` opèrent désormais sur `QImage` pure. La conversion vers `QPixmap` est strictement restreinte aux composants de rendu finaux (`AtlasViewController`, `AnimationController`, `TimelineFilmstripWidget`), garantissant une étanchéité multi-thread totale et zéro plantage en opérations de fond.

- **Virtualisation & Refonte de `ArrangementModel` (Lazy-Loading des Vignettes) — ✅ TERMINÉ :**
  - *Constat & Problème résolu :* `ArrangementModel` recopiait toutes les frames du document et exécutait le redimensionnement `scaled(64, 64)` de façon synchrone dans `MainWindow::populateFrameList()`, provoquant un gel d'interface sensible lors de l'ouverture de planches massives (200 à 500 frames).
  - *Réalisé :* Remplacement par un mécanisme de cache paresseux de vignettes `m_thumbnailCache` (`QMap<int, QPixmap>`). `MainWindow::populateFrameList()` n'effectue plus aucune opération de mise à l'échelle d'image lors du remplissage initial (chargement instantané de la liste). La génération des vignettes `QPixmap` est déclenchée uniquement à la demande lors de l'affichage dans `ArrangementModel::data(Qt::DecorationRole)` et mise en cache mémoire avec invalidation propre (`clearThumbnailCache()`, signal `frameUpdated`).

- **Réduction de la Colle Événementielle dans `MainWindow` — 🟡 En Cours (Délégation Avancée) :**
  - *Constat & Avancement :* Taille de `MainWindow` réduite de 72% grâce à l'extraction des 3 contrôleurs autonomes (`AtlasViewController`, `AnimationController`, `ProjectController`).
  - *Suivi :* Continuer le rapatriement progressif des sous-dialogues et des menus contextuels au sein de composants autonomes.

- **Élimination de la Dépendance Privée Qt (`Qt6::CorePrivate`) — ✅ TERMINÉ :**
  - *Constat :* La compression et décompression des archives `.ssp` s'appuyait sur `<QtCore/private/qzipreader_p.h>` et `qzipwriter_p.h`.
  - *Problème & Risque :* Les distributions Linux (Ubuntu/Debian) n'exposent pas `Qt6CorePrivateConfig.cmake` pour des raisons de stabilité d'ABI, provoquant l'échec de `find_package(Qt6CorePrivate)` et imposant des contournements complexes et fragiles dans le CI (`install-qt-action` + Python).
  - *Réalisé :* Intégration de la bibliothèque ZIP autonome et éprouvée `miniz` (v3.1.2, domaine public / MIT) dans `SpriteStudio/include/zip/miniz.h` et `SpriteStudio/src/zip/miniz.c`. Migration complète de `sessionmanager.cpp` sur l'API `miniz`, suppression totale de `CorePrivate` dans CMake, et gestion propre de la libération des descripteurs de fichiers évitant les verrous de renommage sous Windows. Résultat : 18/18 sous-tests `.ssp` validés sous CTest sans dépendance privée.

---

### 2. Performance & Optimisations Algorithmiques — ✅ TERMINÉ

- **Désengorgement de la Vérification d'Inclusion dans `SpriteDetector` ($O(N^2) \rightarrow O(N)$) — ✅ TERMINÉ & VALIDÉ (100%) :**
  - *Constat :* Dans `SpriteDetector::detectToImages()`, le filtrage des boîtes englobantes entièrement incluses dans d'autres utilisait une double boucle imbriquée $N \times N$ naïve (`componentRects[j].contains(componentRects[i])`).
  - *Problème résolu :* Sur une planche bruitée générant 2000 à 5000 composantes parasites, ce test effectuait entre 4 et 25 millions de comparaisons géométriques, provoquant un gel CPU notable.
  - *Réalisé :*
    - Implémentation d'une structure de partitionnement spatial 2D contiguë `SpatialGrid2D` dans `SpriteStudio/src/image/spritedetector.cpp`.
    - Dimensionnement adaptatif du maillage (grille de $16\times 16$ à $64\times 64$ cellules indexées en $O(1)$) selon les dimensions de l'atlas et la densité de composantes.
    - Propriété géométrique exploitée : tout rectangle englobant $R_j$ contenant $R_i$ couvre impérativement son coin supérieur gauche $(R_i.\text{left}(), R_i.\text{top}())$ et réside obligatoirement dans la cellule correspondante.
    - Élimination immédiate des faux candidats par pré-filtrage de dimensions ($W_j \ge W_i$ et $H_j \ge H_i$) et gestion déterministe des égalités strictes (évitant la suppression mutuelle de boîtes identiques).
    - **Résultat de benchmark :** Détection et filtrage de 640 composantes (dont 512 îlots imbriqués) sur un atlas $1024\times 1024$ exécutés en **17 ms** seulement !
    - **Validation par tests unitaires (`tests/test_extractors.cpp`) :** Deux nouveaux tests automatisés (`testSpriteDetectorNestedContainment`, `testSpriteDetectorLargeScaleInclusionPerformance`) validant la fidélité géométrique et la rapidité d'exécution. 100% de succès sous CTest (4/4 suites passées en 1.04s).

- **Dialogue Flottant & Aperçu Temps Réel de Suppression d'Arrière-Plan (Style Filtres GIMP) — ✅ TERMINÉ & VALIDÉ (100%) :**
  - *Constat :* Les contrôles de suppression d'arrière-plan et d'ordonnancement des sprites occupaient une barre fixe encombrante (`AtlasButtons`) sous la vue graphique de l'atlas dans `mainwindow.ui`, gaspillant l'espace vertical sans retour visuel dynamique.
  - *Réalisé :*
    - **Éradication de `AtlasButtons` :** Suppression de la barre fixe sous la vue atlas, restituant 100% de la hauteur disponible à la zone d'édition graphique.
    - **Création du composant flottant `BackgroundRemovalDialog` (`include/widgets/backgroundremovaldialog.h`, `src/widgets/backgroundremovaldialog.cpp`) :**
      - Fenêtre modale non-bloquante flottante inspirée des dialogues de filtres GIMP / Photoshop.
      - Échantillonnage automatique et pastille visuelle de la couleur d'arrière-plan dominante détectée via `ProjectController::detectDominantBackgroundColor`.
      - Curseurs synchronisés (Sliders + SpinBoxes) pour la tolérance de couleur (0-100), le seuil alpha (0-255) et la tolérance verticale d'ordonnancement (0-50 px).
      - Options de découpe intelligente (Smart Crop) et seuil de chevauchement.
      - **Aperçu interactif en direct (Live Preview) :** Mise à jour en continu de l'atlas et des rectangles de sprites détectés sur la scène principale dès la manipulation des sliders (timer anti-rebond / debounce à 80 ms, tirant parti des 17 ms du `SpriteDetector` optimisé).
      - Badge dynamique informant du nombre exact de frames détectées en temps réel.
      - **Annulation intégrale (Cancel / Échap / Croix) :** Restauration immédiate et fidèle de l'atlas d'origine, des frames et des animations.
      - **Validation Undoable (OK / Entrée) :** Création et empilement d'une commande `RemoveBackgroundCommand` dans `QUndoStack` (permettant un `Ctrl+Z` / `Ctrl+Y` instantané) et mémorisation des préférences dans `AppConfig`.
    - **Validation par tests automatisés (`tests/test_controllers.cpp`) :** Test `testProjectControllerDominantBackgroundColorAndUndo` validant la détection de dominante RGB et la réversibilité stricte `undo()` / `redo()` du `RemoveBackgroundCommand`. 100% des tests CTest validés.

- **Système Extensible de Plugins de Filtres Graphiques & Filtres à Valeur Ajoutée (M7) — ✅ TERMINÉ & VALIDÉ (100%) :**
  - *Constat :* Chaque traitement d'image (détourage, correction de liseré, création de variantes) risquait d'être codé en dur dans `MainWindow`, provoquant couplage fort et dette technique.
  - *Réalisé :*
    - **Architecture de Plugins (`FilterPlugin` / `FilterRegistry`) :**
      - Création de l'interface abstraite `FilterPlugin` (`include/filters/filterplugin.h`) avec métadonnées (`id`, `name`, `description`, `category`, `shortcut`) et fabrique virtuelle `createDialog()`.
      - Création du registre singleton `FilterRegistry` (`include/filters/filterregistry.h`, `src/filters/filterregistry.cpp`) orchestrant l'enregistrement des filtres et la génération dynamique du menu `Filtres` dans `MainWindow`.
      - Commande Undo/Redo universelle `ApplyFilterCommand` (`include/commands/filtercommands.h`, `src/commands/filtercommands.cpp`) assurant la réversibilité stricte (atlas complet, frames découpées, boîtes englobantes).
    - **Migration du Filtre de Fond en Plugin (`BackgroundRemovalFilter`) :**
      - Intégration transparente de `BackgroundRemovalDialog` dans le registre sous la catégorie `Cleanup`.
    - **Filtre Débavurage & Anti-Halo / Despill (`DespillFilter` / `DespillFilterDialog`) :**
      - Neutralisation du liseré de 1 px avec deux modes : *Color Clamping* (substitution de teinte par le pixel intérieur opaque sans amputer les contours fins) et *Suppression stricte* (alpha à 0).
      - Sélecteur de couleur cible, curseur de tolérance et support du ciblage sélectif des frames actives (`isSelectedFramesOnly()`).
    - **Filtre Générateur de Contours & Silhouettes (`OutlineFilter` / `OutlineFilterDialog`) :**
      - Épaisseur paramétrable (1 à 4 px), connexité 4 (pixel art net) ou 8 (diagonales lissées), palette de couleurs (presets noir, blanc, or, personnalisé).
      - Option *Silhouette pleine* pour générer des masques de flash de dégât (*hit-flash*) ou des ombres portées.
    - **Filtre Échange de Palette & Variantes Alt-Skins (`ColorSwapFilter` / `ColorSwapFilterDialog`) :**
      - Remplacement colorimétrique ciblé avec tolérance ajustable et préservation subtile du dégradé d'ombrage (espace colorimétrique HSV Shading Preservation).
    - **Généralisation de la Redétection Automatique des Boîtes (`FilterDialogBase`) — ✅ TERMINÉ :**
      - Intégration dans la barre de contrôle commune d'une case à cocher *Détection auto des boîtes* (`Auto-detect Sprite Boxes`), au même niveau que *Live Preview*.
      - Méthode factorisée `updatePreviewFramesAndBoxes()` ré-exécutant `SpriteDetector::detectToImages()` sur l'atlas filtré lorsque la case est cochée (ajustant instantanément les boîtes aux contours élargis de l'Outline ou aux découpes du Despill), ou préservant fidèlement le partitionnement initial si décochée.
      - Activée par défaut pour la *Suppression de fond* et pour le *Générateur de contours* (où l'élargissement de 1 à 4 px impose un agrandissement des boîtes), débrayable en un clic.
    - **Validation par tests automatisés (`tests/test_controllers.cpp`) :**
      - 6 tests unitaires complets (`testFilterRegistry`, `testDespillFilterAlgorithm`, `testOutlineFilterAlgorithm`, `testColorSwapFilterAlgorithm`, `testApplyFilterCommandUndoRedo`, `testFilterAutoDetectBoxes`). 100% de succès sous CTest (49/49 tests passés).

- **Internationalisation Complète (i18n) & Localisation (FR, EN, JA) — ✅ TERMINÉ :**
  - *Contexte & Problème résolu :* Élimination des clés internes brutes (`KEY_MENU_FILTERS`, `KEY_DIALOG_REMOVE_BG_TITLE`) affichées dans l'IHM et des textes français codés en dur dans les boîtes de dialogue de filtres lorsque l'interface était basculée en anglais ou japonais.
  - *Standardisation des sources :* Normalisation de l'ensemble des chaînes sources C++ en anglais standard (`tr("Live Preview")`, `tr("Reset Defaults")`, `tr("Background Removal")`, `tr("Despill & Edge Cleanup")`, `tr("Outline & Silhouette Generator")`, `tr("Color Swap & Alt-Skins")`, tooltips, badges et labels).
  - *Couverture intégrale des catalogues linguistiques :*
    - `sprite_studio_fr_FR.ts` / `.qm` : 419 chaînes traduites (0 inachevée).
    - `sprite_studio_en_US.ts` / `.qm` : 419 chaînes traduites (0 inachevée).
    - `sprite_studio_ja_JA.ts` / `.qm` : 419 chaînes traduites (0 inachevée).
  - *Mise à jour dynamique de l'IHM :* Prise en charge du rechargement à la volée du menu Filtres et de ses catégories lors d'un événement `QEvent::LanguageChange`.
  - *Validation par tests automatisés :* Extension de `testLanguageCatalogLoad()` dans `tests/test_controllers.cpp` validant la traduction de `KEY_MENU_FILTERS`, `BackgroundRemovalDialog`, `FilterDialogBase` et du contrôleur de projet pour les 3 langues.

---

### 3. DevOps, Build & Automatisation (Tests & CI/CD)

- **Factorisation CMake (Bibliothèque Commune `SpriteStudioCore`) — ✅ TERMINÉ :**
  - *Réalisé :* Bibliothèque statique `SpriteStudioCore` créée dans `SpriteStudio/CMakeLists.txt` liant l'ensemble du moteur, UI, traductions et ressources. `tests/CMakeLists.txt` allégé de 264 à 48 lignes avec liaison directe à `SpriteStudioCore`. Temps de compilation des tests divisé par 3.

- **Pipeline d'Intégration Continue Robuste (GitHub Actions CI/CD) — ✅ TERMINÉ :**
  - *Réalisé :* Fichier `.github/workflows/ci.yml` configuré et stabilisé sans artifices fragiles :
    - **Linux (Ubuntu GCC / Ninja) :** Installation directe via les paquets officiels APT (`qt6-base-dev qt6-tools-dev qt6-tools-dev-tools qt6-l10n-tools libgit2-dev`). Suppression intégrale de Python et d'`aqtinstall`. Temps d'exécution réduit à quelques secondes avec une fiabilité totale.
    - **Windows (MinGW 64-bit / Ninja) :** Déploiement propre via `msys2/setup-msys2` avec la chaîne MinGW64 officielle (`gcc`, `ninja`, `qt6-base`, `qt6-tools`, `libgit2`).
    - **macOS (Apple Silicon Clang / Ninja) :** Déploiement propre avec Homebrew (`qt@6`, `libgit2`, `ninja`).
    - **Haiku OS (x86_64 QEMU VM via `vmactions/haiku-vm`) :**
      - Éradication des blocages VM (remplacement du montage `sshfs` sujet aux deadlocks FUSE par `sync: rsync`, allocation `mem: 4096` et `cpu: 2`, limitation de la concurrence Ninja à `-j 2` pour prévenir l'épuisement mémoire, `timeout-minutes: 35`).
      - Automatisation non interactive de `pkgman` avec injection de confirmation (`echo 1 | pkgman install ...`).
      - Rétrocompatibilité universelle `libgit2` : prise en charge des versions historiques (`libgit2 0.25` fourni par HaikuPorts) en fournissant un alias automatique `git_buf_dispose` vers `git_buf_free` pour les versions `< 0.28` dans `appconfig.cpp`.
      - Découplage du packaging HPKG : suppression du hook intrusif `POST_BUILD` sur l'exécutable `SpriteStudio`, remplacement par une cible dédiée `haiku_package` et installation conforme des licences dans `data/licenses` (`Apache License Version 2.0` / `Apache v2`) pour valider les règles de conformité Haiku.
    - Exécution automatisée de `ctest --output-on-failure --verbose` sur l'ensemble des cibles avec 100% de succès.

- **Automatisation Intégrale des Releases GitHub (Multiplateforme) — ✅ TERMINÉ :**
  - *Réalisé :* Workflow `.github/workflows/release.yml` étendu et fiabilisé :
    - Déclenchement universel sur les tags standards SemVer sans forcer de préfixe `v` (motifs `'v*'` et `'[0-9]+.*'`), ainsi que via `workflow_dispatch` manuel avec paramètre optionnel `tag_name`.
    - Résolution dynamique du tag et publication automatisée des assets via `softprops/action-gh-release@v2` (fichiers `.deb`, `.rpm`, `.AppImage`, `.zip` portable, `Setup.exe` NSIS, `.dmg`, `.hpkg`).
    - Élimination des restrictions de condition `startsWith(..., 'refs/tags/v')` qui sautaient la publication lors de déclenchements manuels ou de tags sans `v`.

- **Portabilité Multiplateforme & Rétrocompatibilité Versions Qt (Qt 6.4 à 6.10+) — ✅ TERMINÉ :**
  - *Réalisé :*
    - Résolution de l'omission de l'en-tête `<QGuiApplication>` dans `atlasviewcontroller.h` requis pour `QGuiApplication::keyboardModifiers()`.
    - Remplacement de l'énumération `QIcon::ThemeIcon` (introduite uniquement en Qt 6.7) par les noms de thèmes freedesktop standards sous forme de chaînes (`"document-new"`, `"document-open"`, `"document-save"`, `"zoom-in"`, `"zoom-out"`), garantissant une compatibilité native avec Qt 6.2/6.4/6.6 (dépôts Ubuntu LTS) comme avec Qt 6.10+.
    - Élimination des chemins Windows absolus en dur (`C:\...`) dans les tests unitaires et normalisation automatique des séparateurs de chemins (`\` unifiés en `/` universel Qt) dans `SpriteDocument::setFilePath` et `SpriteDocument::projectName()`.

- **Pérennisation des Fixtures de Tests (Suite à la Purge Copyright) — ✅ TERMINÉ :**
  - *Réalisé :* Script de génération d'assets originaux `scripts/generate_sample_assets.py` (Pillow). Fixtures 100% libres de droits produites dans `sample/` (`hero.png`, `hero_bg.png`, `hero.gif`, `hero.json`, `hero_godot.tres`). Tests de codecs `test_extractors.cpp` et `test_controllers.cpp` réarmés avec 100% de réussite et 0 test skippé.

---

### 4. Documentation & Visibilité Externe

- **Refonte Majeure du `README.md` — ✅ TERMINÉ :**
  - *Réalisé :* `README.md` entièrement réécrit avec badges CI/Licence/Qt6/C++17/CMake, description complète des atouts récents (.ssp, Time Travel Git, Filmstrip, Godot 4, détection intelligente), prérequis exacts (CMake 3.20+, Qt 6.5+) et instructions de build Linux / Windows détaillées.

---

## 📅 Ordre de Déploiement Recommandé & Phasing Stratégique

L'ordonnancement des chantiers est articulé en 3 phases progressives pour maximiser la valeur métier à chaque jalon :

### 🚀 Phase A — Utilité Métier Immédiate & Robustesse (Court Terme) — 🟢 100% CLÔTURÉE
1. **Étape 4 — Points d'Ancrage / Pivots (M3) — ✅ TERMINÉ & VALIDÉ (100% CTest) :**
   - *Objectif :* Éradiquer le sautillement ("jittering") des animations en jeu vidéo lors de l'export vers Godot ou JSON.
   - *Réalisé :* `QPoint pivot`, `hasCustomPivot` et `effectivePivot()` dans `SpriteBox`, 9 presets cardinaux, réticules interactifs haute visibilité dans l'atlas et sur la vue d'aperçu d'animation, mire déplaçable avec synchronisation spinbox live, zoom molette 5000% et pan manuel, cadrage optimal automatique (*Fit in View*), ligne de sol et enveloppe commune, exports Godot 4 `margin = Rect2(...)`, TexturePacker JSON `pivot` et projet natif `.ssp`. 100% CTest validé.
2. **Étape 5 — Assainissement Architectural & Thread-Safety (AUDIT-Phase 2) — ✅ TERMINÉ & VALIDÉ (100% CTest) :**
   - *Objectif :* Sécuriser l'étanchéité multi-thread pour les traitements asynchrones (`QtConcurrent`) et l'export batch.
   - *Réalisé :* Migration complète de `SpriteDocument::m_frames`, `AtlasPacker`, codecs (`SpriteExtractor`, `GifExtractor`, `JsonExtractor`, `GodotExtractor`), `ProjectManager` et `QUndoCommand` vers `QImage` pure (zéro conversion display-server en mémoire). Déport de la conversion `QPixmap` uniquement dans les composants graphiques finaux (`AtlasViewController`, `AnimationController`, `TimelineFilmstripWidget`). Virtualisation et cache paresseux des vignettes dans `ArrangementModel` (affichage instantané même avec plusieurs centaines de frames).
3. **Étape 6 — Suppression de Fond, Anti-Halo, Filtres Plugins & Retouche (M7) — ✅ TERMINÉ & VALIDÉ (100% CTest) :**
   - *Objectif :* Offrir une architecture de filtres modulaire, un détourage parfait sans liseré de 1 px, des variantes de couleurs, des contours, des ajustements HSV, du redimensionnement pixel art net et des palettes rétro.
   - *Réalisé :* Socle `FilterPlugin` + `FilterRegistry`, commande d'annulation universelle `ApplyFilterCommand`, dialogue socle non-bloquant `FilterDialogBase` avec Live Preview (debounce 80 ms), redétection automatique optionnelle des boîtes (*Auto-detect Sprite Boxes*), et 7 filtres complets validés sous CTest (`BackgroundRemovalFilter`, `DespillFilter`, `ColorSwapFilter`, `OutlineFilter`, `ColorAdjustFilter`, `PixelRescaleFilter`, `RetroPaletteFilter`).

---

### 📦 Phase B — Compacité d'Atlas & Automatisation Industrielle (Moyen Terme) — 🟢 100% CLÔTURÉE
4. **Étape 7 — Empaquetage Avancé MaxRects (M6) — ✅ TERMINÉ & VALIDÉ (100% CTest) :**
   - *Objectif :* Atteindre une densité d'atlas comparable à TexturePacker pour minimiser la VRAM en production.
   - *Livrables :* Algorithmes *Best Short Side Fit* (BSSF) et *Best Area Fit* (BAF), padding anti-saignement, extrusion de bordure (1 px), déduplication des frames identiques et dialogue interactif dans l'IHM.
5. **Étape 8 — Outil en Ligne de Commande Headless (M-CLI / `spritestudio-cli`) — ✅ TERMINÉ & VALIDÉ (100% CTest) :**
   - *Objectif :* Intégrer SpriteStudio dans les chaînes de compilation automatisées (CI/CD) des studios pros en fournissant un drop-in replacement 100% compatible avec TexturePacker, une compatibilité avec la CLI Aseprite (`aseprite -b`) et un pipeline natif Godot 4.
   - *Livrables :* Binaire autonome `spritestudio-cli` sans serveur d'affichage (`QT_QPA_PLATFORM=offscreen`), dispatcher multi-saveurs, support complet des arguments TexturePacker (`--sheet`, `--data`, `--format`, `--opt`, `--trim-mode`, `--extrude`, etc.), options Aseprite (`-b`, `--list-tags`, `--sheet-type`), génération directe de ressources Godot 4 `.tres` (avec préservation des UIDs et marges de pivots), sous-commandes natives (`pack`, `slice`, `filter`, `ssp`), codes de sortie POSIX et sortie JSON structurée (`--json`).

---

### 🎯 Phase C — Spécialisation, Retouche & Haute Performance GPU (Long Terme)
6. **Étape 9 — Outil de Retouche Pixel Chirurgicale (M4 allégé) :**
   - *Objectif :* Corriger rapidement un pixel oublié ou un artefact sans devoir rouvrir un éditeur externe.
   - *Cadrage strict :* Outils limités (crayon 1px, gomme, pipette, seau de remplissage) pour éviter le risque de dispersion (*feature creep*).
7. **Étape 10 — Empaquetage Polygonal & Maillages Serrés (M8 - Tight Mesh) — ✅ TERMINÉ & VALIDÉ (100% CTest) :**
   - *Objectif :* Éradiquer l'overdraw GPU (60% à 80% de fillrate économisé) et maximiser la compacité (+20% à +50%) pour mobile et Nintendo Switch.
   - *Livrables :* Contouring Marching Squares étanche, simplification RDP avec dilatation normale et budget de sommets (3-48), triangulation Ear-Clipping, édition interactive directe des sommets sur canevas (sélection, déplacement souris/clavier, insertion par double-clic, suppression `Suppr`), algorithme `TightPolygonPacker` haute densité avec multithreading configurable (1 à $N$ cœurs logiques `QThread::idealThreadCount()`), optimisation des ancres de placement (< 20 ms), IHM non-bloquante avec calcul à la demande et mémorisation des préférences, et exports multi-moteurs (Godot 4 `_mesh.tres`, Unity `.unity.json`, Unreal Paper2D `.paper2d.json`, TexturePacker JSON). 22 tests unitaires sous CTest validés à 100%.

---

## ⚠️ Matrice des Risques & Stratégies d'Atténuation

| Risque Identifié | Gravité | Probabilité | Impact Métier & Technique | Stratégie d'Atténuation Adoptée |
|---|:---:|:---:|---|---|
| **1. Absence de Pivots (M3)** | **Critique** | **Nulle (Résolu)** | Risque de sautillement d'animation et décalages moteurs de jeu. | **✅ Résolu & Validé :** Système de pivots M3 complet, enveloppe d'animation sans jittering, réticules interactifs atlas et aperçu, exports Godot/JSON/SSP, 100% CTest. |
| **2. Absence d'Interface CLI** | **Élevée** | **Haute** | SpriteStudio reste exclu des pipelines de production automatisés (CI/CD) des studios professionnels de jeux vidéo. | Création de la cible légère `spritestudio-cli` liée à `SpriteStudioCore` sans dépendance GUI. |
| **3. Thread-Safety du Modèle (`QPixmap`)** | **Moyenne** | **Nulle (Résolu)** | Instanciation de `QPixmap` hors-thread provoquant des plantages intermittents sous Linux (X11/Wayland) et macOS. | **✅ Résolu & Validé :** Modèle, codecs et commandes 100% migrés sur `QImage` pure en mémoire CPU. |
| **4. Dispersion Fonctionnelle (*Feature Creep*)** | **Élevée** | **Moyenne** | Vouloir réinventer Aseprite (dessin pixel) et Photoshop épuise les ressources et dégrade la clarté du produit. | Définir SpriteStudio comme le **couteau suisse du conditionnement et de la préparation**, pas un outil d'illustration. Cadrer M4 sur la retouche chirurgicale. |
| **5. Consommation RAM sur Grands Atlas** | **Moyenne** | **Faible** | Clonage d'images volumineuses dans la pile `QUndoStack` (atlas 4K avec 50 étapes d'annulation). | Exploiter le Copy-On-Write (COW) implicite de `QImage` et stocker uniquement des rectangles de diffs pour les filtres locaux. |
