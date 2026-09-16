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
| **M3** | [Points d'Ancrage & Pivots (Origins & Offsets)](#m3--points-dancrage--pivots-origins--offsets) | **Haute (Critique)** | Faible | 📝 Planifié (Prochaine Étape Immédiate) |
| **M5** | [Format de Projet Natif (`.ssp` - Sprite Studio Project)](#m5--format-de-projet-natif-ssp---sprite-studio-project) | **Haute** | Faible | 🟢 Clôturé & Validé (85 tests CTest 100% — Session, Lock, Crash Recovery, Atomic Save, LibGit2 Find) |
| **M7** | [Suppression Avancée de Fond & Système de Filtres Graphiques (Filtres GIMP, Anti-Halo, Alt-Skins)](#m7--suppression-avancée-darrière-plan--système-de-filtres-graphiques-filtres-gimp-anti-halo-alt-skins) | **Moyenne** | Moyenne | 🟢 Clôturé & Validé (Socle FilterRegistry, Despill, Outline, ColorSwap 100% CTest) |
| **M6** | [Algorithme d'Empaquetage Avancé (MaxRects Bin-Packing)](#m6--algorithme-dempaquetage-avancé-maxrects-bin-packing) | **Haute** | Moyenne | 📝 Planifié (Compacité de Production) |
| **M-CLI** | [Interface Ligne de Commande & Automatisation CI/CD (`spritestudio-cli`)](#m-cli--interface-ligne-de-commande--automatisation-cicd-spritestudio-cli) | **Haute** | Faible | 📝 Spécifié (Intégration Pipelines Studios) |
| **M4** | [Outil d'Édition de Pixels (Pixel Art Retouching)](#m4--outil-dédition-de-pixels-pixel-art-retouching) | **Moyenne** | Haute | 📝 Planifié (Périmètre Restreint / Retouche Chirurgicale) |
| **M8** | [Empaquetage Polygonal & Maillages Serrés (Polygon / Tight Mesh Packing)](#m8--empaquetage-polygonal--maillages-serrés-polygon--tight-mesh-packing) | **Basse** | Haute | 📝 Spécifications Détaillées (Optimisation Mobile & Switch) |
| **AUDIT** | [Dette de Thread-Safety & Modèle Pur (Audit Étape 2)](#️-audit--points-de-vigilance--dette-technique-résiduelle-recommandations-damélioration) | **Haute** | Moyenne | 🟢 Clôturé & Validé (Modèle pur QImage, Cache Vignettes, 0 conversion I/O, 100% CTest) |

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

## M7 : Suppression Avancée d'Arrière-Plan & Système de Filtres Graphiques (Filtres GIMP, Anti-Halo, Alt-Skins)

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

---

### ⚠️ Problématique Spécifique des Planches JPEG & Solutions Algorithmiques

Les planches de sprites issues du Web (rips JPEG sans couche alpha, compression DCT $8\times 8$, bruit de moustique et sous-échantillonnage 4:2:0) imposent des traitements spécialisés :
1. **Artefacts de sonnerie JPEG :** Résolu par le seuil de tolérance couplé au seuil alpha et au filtre de médiane.
2. **Sur-fusion des sprites proches :** Résolu par le partitionnement spatial `SpatialGrid2D` et la coupure de connexité par profil de projection.
3. **Cavités internes closes :** Approche hybride combinant flood-fill extérieur et ré-évaluation colorimétrique des cavités internes.

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

## M-CLI : Interface Ligne de Commande & Automatisation CI/CD (`spritestudio-cli`)

### Contexte & Enjeux Industriels
Dans les studios professionnels et les productions indépendantes d'envergure, les artistes poussent leurs fichiers sources (Aseprite, Photoshop, PNG) sur le dépôt Git. Des scripts de build et des pipelines d'Intégration Continue (GitHub Actions, GitLab CI) ré-empaquettent automatiquement les atlas de sprites et régénèrent les métadonnées de moteur de jeu (`.tres`, `.json`) sans nécessiter d'intervention humaine dans une interface graphique.
TexturePacker doit l'essentiel de son monopole en studio à son binaire en ligne de commande scriptable.  
Grâce à la factorisation de la bibliothèque statique `SpriteStudioCore`, SpriteStudio peut fournir une cible autonome légère `spritestudio-cli` sans serveur d'affichage (`QT_QPA_PLATFORM=offscreen` / `QCoreApplication`).

### Spécifications Fonctionnelles
1. **Commandes & Actions Principales :**
   - `spritestudio-cli pack <options>` : Empaquette un ensemble d'images ou découpe une planche selon les paramètres spécifiés.
   - `spritestudio-cli slice <image> <options>` : Découpe automatique d'une planche avec seuil alpha et tolérance de fond.
   - `spritestudio-cli export <projet.ssp> <options>` : Convertit un projet `.ssp` existant vers un format cible (Godot 4 `.tres`, JSON TexturePacker) de manière headless.
2. **Options & Arguments Standardisés :**
   - `--input <path>` / `-i <path>` : Fichier source ou dossier d'images à traiter.
   - `--output <path>` / `-o <path>` : Fichier image atlas généré (`.png`).
   - `--format <godot4|json|aseprite>` / `-f` : Format d'export des métadonnées d'animation et de texture.
   - `--padding <px>` (défaut: 2) : Espacement anti-saignement (*bleeding*) entre les sprites.
   - `--algorithm <maxrects|row|grid>` (défaut: maxrects) : Algorithme d'empaquetage 2D.
   - `--pot` : Force des dimensions d'atlas en puissances de deux ($2^n$, standard GPU).
   - `--trim` / `--no-trim` : Rognage automatique des bordures transparentes (Alpha Trim).
   - `--pivot <preset>` : Positionnement des points d'ancrage (`bottom-center`, `center`, `top-left`).
   - `--remove-bg [hexColor]` : Détection et suppression automatique de la couleur de fond spécifiée (ex. `#00FF00`).
   - `--tolerance <0-100>` : Tolérance colorimétrique pour la suppression de fond.
3. **Comportement en Sortie & Intégration CI :**
   - Sortie console claire et concise, avec option `--json` pour exploitation directe par d'autres outils de pipeline.
   - Codes de retour POSIX déterministes (0 en succès, 1 sur argument invalide, 2 sur fichier introuvable, 3 sur échec d'export).

### Fichiers & Composants Cibles
- `SpriteStudio/src/cli/main_cli.cpp` : Point d'entrée de la commande console utilisant `QCommandLineParser`.
- `SpriteStudio/CMakeLists.txt` : Déclaration de la cible exécutable `spritestudio-cli` liée directement à `SpriteStudioCore`.

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

### 🚀 Phase A — Utilité Métier Immédiate & Robustesse (Court Terme)
1. **Étape 4 — Points d'Ancrage / Pivots (M3) — 🔥 PRIORITÉ ABSOLUE & CRITIQUE :**
   - *Objectif :* Éradiquer le sautillement ("jittering") des animations en jeu vidéo lors de l'export vers Godot ou JSON.
   - *Livrables :* `QPoint origin` dans `SpriteBox`, réticule interactif sur l'atlas et l'animation preview, presets (`Bottom-Center`, `Center`, `Top-Left`), injection dans les ressources Godot 4 (`AtlasTexture` offsets) et le JSON TexturePacker.
2. **Étape 5 — Assainissement Architectural & Thread-Safety (AUDIT-Phase 2) — ✅ TERMINÉ & VALIDÉ (100% CTest) :**
   - *Objectif :* Sécuriser l'étanchéité multi-thread pour les traitements asynchrones (`QtConcurrent`) et l'export batch.
   - *Livrables :* Migration complète de `SpriteDocument::m_frames`, `AtlasPacker`, codecs (`SpriteExtractor`, `GifExtractor`, `JsonExtractor`, `GodotExtractor`), `ProjectManager` et `QUndoCommand` vers `QImage` pure (zéro conversion display-server en mémoire). Déport de la conversion `QPixmap` uniquement dans les composants graphiques finaux (`AtlasViewController`, `AnimationController`, `TimelineFilmstripWidget`). Virtualisation et cache paresseux des vignettes dans `ArrangementModel` (affichage instantané même avec plusieurs centaines de frames).
3. **Étape 6 — Nettoyage Périphérique Anti-Halo, Filtres Plugins & Retouche (M7) — ✅ TERMINÉ & VALIDÉ (100%) :**
   - *Objectif :* Offrir une architecture de filtres modulaire, un détourage parfait sans liseré de 1 px, des variantes de couleurs et des contours.
   - *Livrables :* Socle `FilterPlugin` + `FilterRegistry`, commande d'annulation universelle `ApplyFilterCommand`, filtres `DespillFilter` (Color Clamping / strict), `OutlineFilter` (1-4px, connexité 4/8, silhouette pleine), et `ColorSwapFilter` (Shading HSV préservé). 5 tests automatisés dédiés validés sous CTest.

---

### 📦 Phase B — Compacité d'Atlas & Automatisation Industrielle (Moyen Terme)
4. **Étape 7 — Empaquetage Avancé MaxRects (M6) :**
   - *Objectif :* Atteindre une densité d'atlas comparable à TexturePacker pour minimiser la VRAM en production.
   - *Livrables :* Algorithmes *Best Short Side Fit* (BSSF) et *Best Area Fit* (BAF), padding anti-saignement, extrusion de bordure (1 px) et déduplication des frames identiques.
5. **Étape 8 — Outil en Ligne de Commande Headless (M-CLI / `spritestudio-cli`) :**
   - *Objectif :* Intégrer SpriteStudio dans les chaînes de compilation automatisées (CI/CD) des studios pros.
   - *Livrables :* Binaire autonome `spritestudio-cli` sans serveur d'affichage (`QT_QPA_PLATFORM=offscreen`) supportant `pack`, `slice` et `export`.

---

### 🎯 Phase C — Spécialisation, Retouche & Haute Performance GPU (Long Terme)
6. **Étape 9 — Outil de Retouche Pixel Chirurgicale (M4 allégé) :**
   - *Objectif :* Corriger rapidement un pixel oublié ou un artefact sans devoir rouvrir un éditeur externe.
   - *Cadrage strict :* Outils limités (crayon 1px, gomme, pipette, seau de remplissage) pour éviter le risque de dispersion (*feature creep*).
7. **Étape 10 — Empaquetage Polygonal & Maillages Serrés (M8 - Tight Mesh) :**
   - *Objectif :* Éradiquer l'overdraw GPU et maximiser la compacité (+20% à +50%) pour mobile et Nintendo Switch.
   - *Livrables :* Contouring Marching Squares, simplification Ramer-Douglas-Peucker (6-12 sommets), triangulation Ear-Clipping et export Godot `Polygon2D`.

---

## ⚠️ Matrice des Risques & Stratégies d'Atténuation

| Risque Identifié | Gravité | Probabilité | Impact Métier & Technique | Stratégie d'Atténuation Adoptée |
|---|:---:|:---:|---|---|
| **1. Absence de Pivots (M3)** | **Critique** | **Haute** | Les animations exportées dans les moteurs de jeux subissent des décalages visuels si les boîtes ont des tailles hétérogènes. | **Priorisation immédiate de M3** avant tout autre nouveau filtre ou fonctionnalité graphique. |
| **2. Absence d'Interface CLI** | **Élevée** | **Haute** | SpriteStudio reste exclu des pipelines de production automatisés (CI/CD) des studios professionnels de jeux vidéo. | Création de la cible légère `spritestudio-cli` liée à `SpriteStudioCore` sans dépendance GUI. |
| **3. Thread-Safety du Modèle (`QPixmap`)** | **Moyenne** | **Nulle (Résolu)** | Instanciation de `QPixmap` hors-thread provoquant des plantages intermittents sous Linux (X11/Wayland) et macOS. | **✅ Résolu & Validé :** Modèle, codecs et commandes 100% migrés sur `QImage` pure en mémoire CPU. |
| **4. Dispersion Fonctionnelle (*Feature Creep*)** | **Élevée** | **Moyenne** | Vouloir réinventer Aseprite (dessin pixel) et Photoshop épuise les ressources et dégrade la clarté du produit. | Définir SpriteStudio comme le **couteau suisse du conditionnement et de la préparation**, pas un outil d'illustration. Cadrer M4 sur la retouche chirurgicale. |
| **5. Consommation RAM sur Grands Atlas** | **Moyenne** | **Faible** | Clonage d'images volumineuses dans la pile `QUndoStack` (atlas 4K avec 50 étapes d'annulation). | Exploiter le Copy-On-Write (COW) implicite de `QImage` et stocker uniquement des rectangles de diffs pour les filtres locaux. |
