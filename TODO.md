# 📋 Feuille de Route & Spécifications Métier — BentoPack

> **Dernière mise à jour :** 2026-09-25  
> **Statut global :** Socle fonctionnel stabilisé (201 tests CTest validés). Focus sur la refonte architecturale, la suppression des reliques de renommage, et le déploiement des intégrations moteurs (M10).

---

## 🗺️ Tableau de Bord des Chantiers

### 🟢 Jalons Clôturés & Validés (Historique Synthétique)

| ID | Chantier | Complexité | Résumé des Livrables & Validation |
|---|---|:---:|---|
| **M0** | **Assainissement Architectural** | Haute | Modèle unique `SpriteDocument`, démantèlement du God-Object `MainWindow` en 3 contrôleurs, RAII, scanlines contiguës, i18n trilingue (FR/EN/JA). |
| **M1** | **Édition Interactive des Bounding Boxes** | Moyenne | 8 poignées interactives, déplacement groupé, outil découpe rapide (`Shift`), trim alpha automatique, effacement destructif (`Shift+Suppr`). |
| **M2** | **Timeline & Gestionnaire d'Animations** | Moyenne | Splitters fluides, ruban filmstrip, modes Loop/Once/Ping-Pong, transport moderne (scrubber), persistance QSettings des docks. |
| **M3** | **Points d'Ancrage & Pivots** | Faible | Réticule interactif direct sur atlas et aperçu, 9 presets cardinaux, enveloppe d'animation anti-jittering, export des margins. |
| **M4** | **Éditeur de Pixels Chirurgical** | Haute | Tracé Bresenham 1px, pipette, seau de remplissage, sélections rectangle/baguette magique, 7 palettes rétro authentiques. |
| **M5** | **Format de Projet Natif `.bento`** | Faible | Archive ZIP compressée (Miniz), verrouillage de session `.session_lock`, crash recovery, time-travel Git intégré (LibGit2). |
| **M6** | **Empaquetage Avancé MaxRects** | Moyenne | Heuristiques BSSF, BAF, BLSF, BottomLeft, ContactPoint, contrainte POT ($2^n$), extrusion de bordure anti-bleeding, déduplication de frames. |
| **M7** | **Filtres Graphiques & Traitement** | Moyenne | 9 plugins de filtres (Chroma BG, Despill, Outline, ColorSwap, ColorAdjust, RetroPalette, Rescale, MaxRects, TightPacking) avec live preview. |
| **M8** | **Empaquetage Polygonal & Maillages Serrés** | Haute | Marching Squares étanche, RDP, triangulation Ear-Clipping, réduction de 60-80% de l'overdraw GPU, export Unity Tight & Godot ArrayMesh. |
| **M9** | **Compression de Textures VRAM (KTX2)** | Haute | Khronos `basis_universal` v2.50, modes UASTC 4x4 et ETC1S, Zstd niveaux 1-22, téléversement direct GPU sans décompression CPU. |
| **M11** | **Architecture de Plugins Qt6 & SDK** | Haute | Externalisation des 9 filtres et 6 extracteurs en modules `.so`/`.dll` dynamiques (`QPluginLoader`), CMake config exportable. |
| **M15** | **Refonte Drag & Drop Filmstrip** | Moyenne | `FilmstripListWidget` dédié, calcul linéaire du drop 1D, indicateur bleu contrasté, découplage transactionnel sans récursion destructrice. |
| **M-CLI** | **Interface CLI & Compatibilité Moteurs** | Moyenne | `bentopack-cli` avec mode drop-in TexturePacker, Aseprite (`-b`) et export natif Godot 4 avec conservation des UIDs. |
| **M-DEVOPS** | **Release CI Duale & Signature** | Moyenne | Approche B cryptographique, pipelines `release-community.yml` et `release-commercial.yml` privés, watermarking discret. |
| **ASSETS** | **Banque d'Échantillons Libres de Droits** | Faible | Élimination de tout asset tiers copyrighté, assets procéduraux et pixel art originaux pour les tests. |
| **AUDIT** | **Dette de Thread-Safety & Modèle Pur** | Moyenne | Modèle 100% `QImage` en mémoire CPU contiguë, zéro appel `QPixmap` hors thread UI, 201 tests CTest à 100% de succès. |
| **POLISH** | **Export Asynchrone & Préférences** | Faible | Export non bloquant via `QtConcurrent`, barre de progression animée, dialogue de réglages à 6 onglets avec hot-reload des plugins. |

---

## 🎯 Chantiers Métier Restants & Planifiés

```
┌────────────────────────────────────────────────────────────────────────┐
│                        FEUILLE DE ROUTE ACTIVE                         │
├───────────────────┬───────────────────────────────────┬────────────────┤
│ Jalon             │ Thématique                        │ Priorité       │
├───────────────────┼───────────────────────────────────┼────────────────┤
│ CH-TECH (1 à 6)   │ Assainissement & Refactoring      │ Haute (Imm.)   │
│ M10               │ Intégrations Moteurs & Stores     │ Haute          │
│ M14               │ Optimisations SIMD & Grands Atlas │ Moyenne        │
│ M12               │ Rigging & Animation Squelettique  │ Moyenne        │
│ M13               │ Micro-Démonstrateur Web (WASM)    │ Basse          │
└───────────────────┴───────────────────────────────────┴────────────────┘
```

---

## 🔧 Chantiers Techniques Issus de l'Audit Critique (Priorité Immédiate)

### CH-TECH-1 : Scission Architecturale de `BentoPackCore` (Découplage UI / Headless) — ✅ **TERMINÉ**
- **Constat d'Audit :** La cible CMake `BentoPackCore` compilait initialement `MainWindow`, l'ensemble des boîtes de dialogue et dépendait de `Qt6::Widgets`. Par conséquent, le CLI headless `bentopack-cli` liait inutilement l'infrastructure graphique.
- **Réalisations Effectuées :**
  1. **Scission effective en deux bibliothèques partagées distinctes :**
     - `BentoPackCore` (Shared Library) : 100% headless (`SpriteDocument`, `AtlasPacker`, `MaxRectsPacker`, `TightPolygonPacker`, `VramTextureCompressor`, `ContourTracer`, `Triangulator`, `PolygonSimplifier`, `PolygonMerger`, `SessionManager`, `ProjectManager`, `ProjectController`, `LicenseManager`, `IntegrityGuard`, pipeline CLI et registres d'I/O). Dépendances strictes : `Qt6::Core`, `Qt6::Gui`, `Qt6::Concurrent`, `Qt6::Network`, `LibGit2`, `basisu_encoder`. **Zéro dépendance vers `Qt6::Widgets`** (validé par `ldd`).
     - `BentoPackWidgets` (Shared Library) : Composants graphiques et interfaces utilisateur (`MainWindow`, `AnimationController`, `AtlasViewController`, `ArrangementModel`, `AtlasBoxItem`, `AboutDialog`, `FilterMenuBuilder`, `FilterDialogBase`, `ExportDialog`, `SettingsDialog`, `PixelEditorDialog`, `PixelCanvas`, `PolygonMeshDialog`, `GitHistoryDock`, `BranchSelectionDialog`, etc.). Dépendances : `BentoPackCore`, `Qt6::Widgets`.
  2. **Refactorisation de `bentopack-cli` :**
     - Transition vers `QGuiApplication` (plus de `QApplication` ni de dépendance à `libQt6Widgets.so`).
     - Lancement headless allégé, parfait pour les runners CI et les serveurs de build sans serveur X11/Wayland.
  3. **Découplage des menus et dialogues :**
     - Extraction de la construction des menus de filtres dans `FilterMenuBuilder` (`BentoPackWidgets`), purgeant `FilterRegistry` (`BentoPackCore`) de toute dépendance vers `QMenu`, `QAction` et `QWidget`.
     - Migration de la boîte de sélection de branche Git dans `MainWindow::onRedoTriggered`, purgeant `ProjectController` de toute inclusion de dialogue.
  4. **Validation des tests :**
     - 8/8 suites de tests CTest validées et passant à 100% (`test_extractors`, `test_controllers`, `test_project`, `test_core`, `test_cli`, `test_mesh`, `test_pixel_editor`, `test_vram_compression`).

### CH-TECH-2 : Finalisation du Rebranding & Élimination des Reliques `SpriteStudio` — ✅ **TERMINÉ**
- **Constat d'Audit :** Des reliques du nom `SpriteStudio` subsistaient dans les macros d'exportation, les variables d'environnement, les macros de configuration de licence et les en-têtes d'intégrité.
- **Réalisations Effectuées :**
  1. **Purge intégrale des macros d'export :**
     - Remplacement de `SPRITESTUDIO_CORE_EXPORT` par `BENTOPACK_CORE_EXPORT` sur l'ensemble des 26 en-têtes publics.
     - Remplacement de `SPRITESTUDIO_WIDGETS_EXPORT` par `BENTOPACK_WIDGETS_EXPORT`.
     - Nettoyage des bibliothèques CMake (`BENTOPACK_CORE_LIBRARY`, `BENTOPACK_WIDGETS_LIBRARY`).
  2. **Renommage de l'infrastructure de licence commerciale :**
     - Variables CMake et macros C++ renommées en `BENTOPACK_COMMERCIAL_BUILD` et `BENTOPACK_LICENSE_TOKEN`.
     - Fichier de template mis à jour : [`license_config.h.in`](file:///home/oktail/Documents/GitHub/BentoPack/BentoPack/src/license_config.h.in).
     - Tags d'intégrité stéganographique et métadonnées mis à jour : `X-SS-Integrity` remplacé par `X-BentoPack-Integrity`.
  3. **Variables d'environnement unifiées :**
     - `BENTOPACK_PLUGIN_PATH` pris en compte dans `ExtractorRegistry` et `FilterRegistry`.
     - Scripts et configuration CTest alignés sur `BENTOPACK_PLUGIN_PATH`.
     - Bridge Godot [`addons/godot/cli_bridge.gd`](file:///home/oktail/Documents/GitHub/BentoPack/addons/godot/cli_bridge.gd) nettoyé avec `BENTOPACK_CLI` et `BENTOPACK_GUI`.
  4. **Documentation & Validation globale :**
     - Documentation mise à jour (`README.md`).
     - Vérification `git grep -i "spritestudio"` : **0 occurrence restante** sur l'ensemble du dépôt.
     - 8/8 suites de tests CTest validées avec 100% de réussite.

### CH-TECH-3 : Correction des Doublons de Code & Scories — ✅ **TERMINÉ**
- **Constat d'Audit :**
  - Dans `BentoPack/src/license/licensemanager.cpp`, les méthodes `complianceMetadata()` et `applyWatermark()` contenaient des insertions dupliquées consécutives à l'identique (lignes 107-135 et 153-180).
  - Dans `wrappers/TexturePacker` et `wrappers/aseprite`, les scripts bash exécutaient la même commande dans les deux branches du `if/else`.
  - Dans `CMakeLists.txt`, la directive CPack RPM déclarait `set(CPACK_RPM_PACKAGE_LICENSE "MIT")` alors que la licence du projet est Apache 2.0.
- **Réalisations Effectuées :**
  1. **Nettoyage de `licensemanager.cpp` :** Suppression des blocs redondants dans `complianceMetadata()` et `applyWatermark()`.
  2. **Fiabilisation des scripts wrappers (`wrappers/TexturePacker` et `wrappers/aseprite`) :** Recherche ordonnée du binaire `bentopack-cli` (`$PATH`, répertoire courant, chemin relatif du build `../build/bin`), message d'erreur clair et code de sortie 127 si l'exécutable est introuvable.
  3. **Alignement CPack RPM :** `CPACK_RPM_PACKAGE_LICENSE` mis à jour en `"Apache-2.0"`.
  4. **Validation :** Compilation complète et validation de l'ensemble des 8 suites de tests CTest (100% Passed).

### CH-TECH-4 : Découpage de la Suite de Test Monolithique (`test_controllers.cpp`) — ✅ **TERMINÉ**
- **Constat d'Audit :** Le fichier `tests/test_controllers.cpp` comptait plus de 3 048 lignes regroupant plus de 60 tests hétérogènes (AppConfig, ProjectController, AnimationController, AtlasViewController, filtres, i18n, Git time-travel).
- **Réalisations Effectuées :**
  1. **Découpage en 5 suites de tests modulaires et spécialisées :**
     - [`test_app_config.cpp`](file:///home/oktail/Documents/GitHub/BentoPack/tests/test_app_config.cpp) : Persistance JSON, fallback fichier corrompu, configuration par défaut.
     - [`test_controller_project.cpp`](file:///home/oktail/Documents/GitHub/BentoPack/tests/test_controller_project.cpp) : Chargement de projets (JSON, GIF), gestion des fichiers récents, suppression de fond asynchrone, intégration Git time-travel & branching.
     - [`test_controller_animation.cpp`](file:///home/oktail/Documents/GitHub/BentoPack/tests/test_controller_animation.cpp) : Playback, création/suppression/duplication/renommage d'animations, timeline filmstrip drag-and-drop, réorganisation de frames, persistance des modes de boucle, alignement de pivots, masquage polygonal.
     - [`test_controller_atlas.cpp`](file:///home/oktail/Documents/GitHub/BentoPack/tests/test_controller_atlas.cpp) : Interactions QGraphicsView, modes d'outils, zoom centré souris, sélection lasso/marquee, manipulation de boîtes et de pivots, translations i18n, persistance des docks.
     - [`test_filters.cpp`](file:///home/oktail/Documents/GitHub/BentoPack/tests/test_filters.cpp) : Registre de filtres & plugins, algorithmes (Despill, Outline, ColorSwap, ColorAdjust, PixelRescale, RetroPalette, AtlasPacking), auto-détection de boîtes, rendu de l'AboutDialog en mode sombre et tarification dynamique multilingue.
  2. **Refonte de [`tests/CMakeLists.txt`](file:///home/oktail/Documents/GitHub/BentoPack/tests/CMakeLists.txt) :**
     - Remplacement de la cible monolithique `test_controllers` par 5 exécutables de test distincts (`test_app_config`, `test_controller_project`, `test_controller_animation`, `test_controller_atlas`, `test_filters`).
     - Configuration des dépendances, plugins DLL Windows en post-build et variables d'environnement (`BENTOPACK_PLUGIN_PATH`).
  3. **Suppression du monolithe :** Suppression définitive de `tests/test_controllers.cpp`.
  4. **Extension majeure de la couverture de tests (3 nouvelles suites) :**
     - [`test_robustness.cpp`](file:///home/oktail/Documents/GitHub/BentoPack/tests/test_robustness.cpp) : Robustesse I/O, tolérance aux fichiers `.bento` corrompus/0-byte, ZIP incomplets, `project.json` manquant/invalide, syntaxes JSON corrompues, image d'atlas manquante, formats Aseprite/TexturePacker malformés, packing 0-slice, sprites surdimensionnés et chemins de répertoires Unicode avec espaces.
     - [`test_gui_integration.cpp`](file:///home/oktail/Documents/GitHub/BentoPack/tests/test_gui_integration.cpp) : Initialisation `MainWindow` headless/offscreen, hiérarchie des menus, cycle de vie du dirty flag, filtrage MIME du drag-and-drop, déclencheurs de lecture d'animation, boîtes de dialogue (`ExportDialog` chemins et configurations par défaut, `BranchSelectionDialog` sélection et checkout de branches Git).
     - [`test_concurrency_and_security.cpp`](file:///home/oktail/Documents/GitHub/BentoPack/tests/test_concurrency_and_security.cpp) : Sécurité multithread des tâches asynchrones (`QFutureWatcher`, détourage IA et ouvertures de fichiers asynchrones sans interblocage), injection du filigrane stéganographique dans les pixels alpha zéro (`IntegrityGuard`), calcul et vérification de la signature HMAC de layout, et détection/rejet des falsifications.
  5. **Validation CTest :** **15/15 suites de test** validées avec **100% de réussite** (temps d'exécution total : ~2.6 secondes).

### CH-TECH-5 : Hygiène Git & Nettoyage des Artefacts de Test — ✅ **TERMINÉ**
- **Constat d'Audit :** L'espace de travail contenait des fichiers `.tres` orphelins, des images d'atlas volumineuses non suivies dans `examples/godot_demo/`, et des fichiers `.uid` de Godot non ignorés.
- **Réalisations Effectuées :**
  1. **Mise à jour exhaustive de `.gitignore` :**
     - Exclusion des artefacts de démo Godot générés (`examples/godot_demo/*.tres`, `examples/godot_demo/*.png`, `examples/godot_demo/*.bento`).
     - Exclusion des métadonnées de cache Godot 4 (`.godot/`, `*.import`, `*.uid`).
     - Exclusion des caches Python (`__pycache__/`, `*.py[cod]`, `.pytest_cache/`).
     - Exclusion des artefacts d'OS et d'éditeurs (`.DS_Store`, `Thumbs.db`, `*~`).
     - Exclusion préventive des clés privées et secrets (`*.key`, `*.secret`, `*.pem`, `license_config.h`).
  2. **Nettoyage du working directory :** Suppression des 86 fichiers orphelins et validation d'un état propre.
  3. **Audit de sécurité des scripts (`scripts/`) :** Vérification de l'absence de fuite de secrets ou de tokens pré-enregistrés dans `audit_asset_compliance.py` et `generate_commercial_key.py`.

### CH-TECH-6 : Synchronisation & Correction de la Documentation — ✅ **TERMINÉ**
- **Constat d'Audit :** Coquilles post-rebranding dans `docs/USER_GUIDE.md`, `docs/README_DOCS.md` et `docs/DEVELOPER_GUIDE.md` (`"anciennement BentoPack"`), chemins d'exemples SDK obsolètes dans `README.md` (`examples/sample_filter_plugin` inexistant), et fuites de chemins absolus Windows dans `benchmarks/REPORT.md`.
- **Réalisations Effectuées :**
  1. **Purge des coquilles de rebranding :** Remplacement de `"BentoPack (anciennement BentoPack)"` par `"BentoPack"` dans `docs/USER_GUIDE.md`, `docs/README_DOCS.md` et `docs/DEVELOPER_GUIDE.md`.
  2. **Complétion du SDK Plugin Developer :**
     - Création de l'exemple de référence manquant [`examples/sample_filter_plugin/`](file:///home/oktail/Documents/GitHub/BentoPack/examples/sample_filter_plugin) (`CMakeLists.txt`, `sample_filter.h`, `sample_filter.cpp`, `README.md`).
     - Renommage cohérent de `examples/sample_plugin/` en [`examples/sample_extractor_plugin/`](file:///home/oktail/Documents/GitHub/BentoPack/examples/sample_extractor_plugin).
     - Alignement de [`README.md`](file:///home/oktail/Documents/GitHub/BentoPack/README.md) sur les deux exemples du SDK et mise à jour du compteur de suites CTest (15 suites au lieu de 8).
  3. **Purge des chemins Windows absolus :** Nettoyage de `benchmarks/REPORT.md` (remplacement de `C:\Users\ec135\...` par des chemins relatifs portables `bin/bentopack-cli` et `history/...`).

---

## 🚀 M10 : Intégration aux Écosystèmes & Marchés Moteurs de Jeu (Godot AssetLib, Unity UPM, Unreal Fab)

### 📌 Contexte & Enjeux d'Adoption
L'adoption en studio et par les créateurs indépendants dépend de la suppression totale des frictions entre BentoPack et le moteur de jeu :
1. **Installation en 1 Clic :** Présence sur les registres officiels (Godot AssetLib, OpenUPM, Epic Games Fab).
2. **Importation Transparente :** Glisser-déposer un fichier `.bento` et générer automatiquement textures, animations, pivots et maillages serrés.
3. **Hot-Reloading Bidirectionnel :** Sauvegarde dans BentoPack $\implies$ rechargement immédiat des ressources dans l'éditeur du moteur via `bentopack-cli --watch`.

### 🏛️ Modules & Livrables Cibles

#### 1. Écosystème Godot Engine 4 (`godot-bentopack-addon`)
- **Nature :** `EditorPlugin` en GDScript pur (zéro dépendance binaire, multiplateforme).
- **Fonctionnalités :**
  - `EditorFileSystemImportPlugin` prenant en charge nativement les fichiers `.bento` et `.ssp`.
  - Génération automatique de `SpriteFrames` (`.tres`) et sous-ressources `AtlasTexture`.
  - Application des décalages de pivots exacts via `margin = Rect2(...)`.
  - Génération de ressources `ArrayMesh` 2D pour le rendu sans overdraw via `MeshInstance2D`.
  - Bouton *"Ouvrir dans BentoPack"* dans l'inspecteur Godot sur les nœuds `AnimatedSprite2D` et `Sprite2D`.
- **Publication :** Soumission officielle sur la [Godot Asset Library](https://godotengine.org/asset-library).

#### 2. Écosystème Unity (`com.bentopack.importer`)
- **Nature :** Package Unity Package Manager (UPM) en C#.
- **Fonctionnalités :**
  - `ScriptedImporter` prenant en charge les fichiers `.bento` et les métadonnées JSON.
  - Configuration automatique du `TextureImporter` en mode `Sprite (2D and UI)` avec filtrage Point.
  - **Injection des maillages serrés M8 :** Appel à `Sprite.OverrideGeometry()` avec les sommets et triangles calculés par BentoPack (`SpriteMeshType.Tight`), économisant 60% à 80% de fillrate GPU.
  - Génération automatique des `AnimationClip` avec courbes de frames cadencées au bon FPS.
- **Publication :** Hébergement OpenUPM et soumission sur l'Unity Asset Store (catégorie *2D Tools*).

#### 3. Écosystème Unreal Engine 5 (`BentoPack UE5 Plugin`)
- **Nature :** Plugin C++ pour Unreal Engine 5.x.
- **Fonctionnalités :**
  - Classes `UFactory` et `FAssetTypeActions` pour l'import par glisser-déposer créant `UPaperSprite` et `UPaperFlipbook`.
  - Interfaçage natif avec **PaperZD** (State Machines d'animation 2D).
  - Définition de la géométrie de rendu personnalisée (`RenderGeometry`) issue du maillage M8 pour minimiser le coût de translucidité.
- **Publication :** Soumission sur la nouvelle marketplace unifiée d'Epic Games : **Fab** (`fab.com`).

---

## ⚡ M14 : Optimisations Hautes Performances SIMD & Gestion Mémoire Grands Atlas

### 📌 Objectifs & Spécifications
Garantir une réactivité totale de l'interface lors de la manipulation de très grandes planches de sprites (4K, 8K, 16K) et stabiliser l'empreinte mémoire vive.

1. **Vectorisation SIMD (AVX2 / NEON) :**
   - Accélération des boucles de traitement scanline contiguës dans les filtres graphiques les plus coûteux :
     - `DespillFilter` (distances chromatiques et clamping de composantes).
     - `OutlineFilter` (dilatations morphologiques 4-connectées et 8-connectées).
     - `ColorSwapFilter` (conversions d'espaces colorimétriques RGB <-> HSV vectorisées).
   - Utilisation d'intrinsèques ou de bibliothèques d'en-tête modernes (ex. `Highway` ou `xsimd`) pour traiter 8 pixels 32 bits par cycle AVX2 et 4 pixels par cycle NEON (Apple Silicon).
   - Gain attendu : Accélération d'un facteur 4x à 8x (filtrage d'une texture 8K en moins de 15 ms).
2. **Gestion Mémoire & Plafonnement `QUndoStack` :**
   - Configuration d'un seuil maximal de mémoire vive allouée à l'historique d'annulation dans `AppConfig`.
   - **Snapshotting différentiel (*dirty rects*) :** Mémoriser uniquement le sous-rectangle modifié lors des retouches dans l'Éditeur de Pixels plutôt que de cloner l'image complète de l'atlas à chaque coup de pinceau.
   - Compression transparente en tâche de fond des états anciens de l'UndoStack via LZ4.

---

## 🦴 M12 : Animation Squelettique & Découpe de Membres 2D (Rigging, Bones, Spine / Godot / Unity)

### 📌 Contexte & Enjeux
L'animation image par image traditionnelle est coûteuse en temps et en VRAM. L'animation squelettique 2D découpe un personnage en éléments anatomiques distincts (tête, buste, membres), les rattache à une hiérarchie d'os (*Bones*), et anime les transformations pour produire des mouvements fluides à 60 ou 120 FPS avec un nombre réduit de textures.

### 🏛️ Modules & Livrables Cibles
1. **Module 1 : Outil de Découpe de Membres (Limb Slicing) :**
   - Détection et isolation des éléments corporels avec marges de recouvrement aux articulations.
2. **Module 2 : Éditeur d'Armature 2D (Bone Rigging) :**
   - Tracé interactif des os sur le canevas avec liaisons parent-enfant hiérarchiques.
   - Définition des pivots de rotation et contraintes angulaires.
   - Solveur analytique 2D de cinématique inverse (IK 2-bones) pour les membres.
3. **Module 3 : Pondération de Sommets (Skinning & Weight Painting) :**
   - Association des influences d'os aux sommets du maillage polygonal M8 avec interpolation douce.
4. **Module 4 : Formats d'Exportation Standards :**
   - **Spine JSON (v3.8 / v4.x) :** Standard mondial supporté par l'ensemble des moteurs via les runtimes Spine officiels.
   - **Godot 4 Skeleton2D :** Génération native d'une scène `.tscn` avec nœuds `Skeleton2D`, `Bone2D` et `Polygon2D`.
   - **Unity 2D Animation :** Export compatible avec le package officiel Unity `2D Animation`.

---

## 🌐 M13 : Micro-Démonstrateur Web Vitrine (WebAssembly Showcase)

### 📌 Cadrage Stratégique
Le portage intégral de l'application de bureau en WebAssembly (Qt for WebAssembly) est **fortement déconseillé** en raison des contraintes techniques inhérentes :
- Poids de téléchargement excessif (25 à 40 Mo non compressé pour le runtime Qt Widgets).
- Sandbox navigateur interdisant l'accès direct au système de fichiers local et le verrouillage atomique de session.
- Nécessité d'en-têtes HTTP de sécurité stricts (COOP/COEP) pour autoriser les threads C++ via `SharedArrayBuffer`.
- Encodage KTX2/Basis Universal bridé en performance dans le navigateur.

### 💡 Solution Retenue :
Concevoir un **micro-démonstrateur web vitrine ultra-léger** (mini-module WebAssembly ou script TypeScript/Canvas) hébergé sur le site officiel de BentoPack. Il permettra aux visiteurs de glisser-déposer un sprite pour tester instantanément en direct la découpe automatique, le despill et l'aperçu d'animation, servant d'entonnoir d'acquisition vers l'application de bureau native.

---

## 📊 Matrice d'Exécution & Plan d'Action Recommandé

| Horizon | Chantier | Actions Clés | Livrables |
|---|---|---|---|
| **Semaine 1 (Immédiat)** | **CH-TECH-3 & CH-TECH-5** | • Supprimer les doublons dans `licensemanager.cpp`<br>• Nettoyer le wrapper TexturePacker<br>• Corriger la licence RPM vers Apache-2.0<br>• Mettre à jour `.gitignore` et purger les 86 `.tres` d'exemples | Code source propre, git status immaculé. |
| **Semaine 2** | **CH-TECH-2 (✅ TERMINÉ) & CH-TECH-6** | • Remplacer `SPRITESTUDIO_CORE_EXPORT` par `BENTOPACK_CORE_EXPORT` (Fait)<br>• Aligner la documentation (`USER_GUIDE.md`, `DEVELOPER_GUIDE.md`, `README.md`)<br>• Purger les chemins absolus locaux dans `benchmarks/REPORT.md` | Rebranding 100% cohérent, documentation irréprochable. |
| **Semaine 3-4** | **CH-TECH-1 & CH-TECH-4** | • Scinder `BentoPackCore` (pur headless) et `BentoPackGUI`<br>• Alléger `bentopack-cli` (dépendance `Qt6Widgets` éliminée)<br>• Découper `test_controllers.cpp` en 4 fichiers de tests ciblés | Architecture saine, CLI prêt pour la CI cloud minimale. |
| **Mois 2** | **M10 (Godot & Stores)** | • Finaliser et publier l'addon Godot 4 sur AssetLib<br>• Lancer la page Steam et la boutique Itch.io pour la version Store Convenience | Premier flux de revenus et visibilité communauté. |
| **Mois 3** | **M10 (Unity & Unreal)** | • Développer le package Unity UPM avec support `SpriteMeshType.Tight`<br>• Développer le plugin UE5 PaperZD pour Fab | Couverture complète des trois moteurs majeurs du marché. |
| **Mois 4-5** | **M14 & M12** | • Vectorisation SIMD des filtres graphiques<br>• Cadrage et développement initial du rigging 2D (Spine JSON) | Performances extrêmes 8K/16K et diversification fonctionnelle. |
