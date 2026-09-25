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

### CH-TECH-1 : Scission Architecturale de `BentoPackCore` (Découplage UI / Headless)
- **Constat d'Audit :** La cible CMake `BentoPackCore` compile `MainWindow`, l'ensemble des boîtes de dialogue et dépend de `Qt6::Widgets`. Par conséquent, le CLI headless `bentopack-cli` lie inutilement l'infrastructure graphique et impose `qputenv("QT_QPA_PLATFORM", "offscreen")` avec des dépendances X11/Wayland/libGL sur les serveurs de build CI. De plus, les plugins de filtres et d'extracteurs lient `MainWindow` via le Core.
- **Plan d'Action :**
  1. **Scinder la bibliothèque en deux cibles distinctes :**
     - `BentoPackCore` (Shared Library) : Modèles de données (`SpriteDocument`), algorithmes d'empaquetage (`AtlasPacker`, `MaxRectsPacker`, `TightPolygonPacker`), géométrie (`ContourTracer`, `Triangulator`), compression VRAM (`VramTextureCompressor`), gestionnaire de licence (`LicenseManager`, `IntegrityGuard`), sessions et codecs d'I/O headless. Dépendances strictes : `Qt6::Core`, `Qt6::Gui`, `Qt6::Concurrent`. **Zéro dépendance vers `Qt6::Widgets`**.
     - `BentoPackGUI` (ou inclus directement dans l'exécutable `bentopack`) : Contrôleurs d'interface, `MainWindow`, vues `QGraphicsView`, boîtes de dialogue (`ExportDialog`, `SettingsDialog`, `PixelEditorDialog`) et délégués.
  2. **Refactoriser `bentopack-cli` :**
     - Remplacer `QApplication` par `QGuiApplication` (ou `QCoreApplication` si les opérations graphiques le permettent).
     - Lier exclusivement `BentoPackCore`.
     - Supprimer le hack `qputenv("QT_QPA_PLATFORM", "offscreen")`.

### CH-TECH-2 : Finalisation du Rebranding & Élimination des Reliques `SpriteStudio`
- **Constat d'Audit :** Le renommage de `SpriteStudio` en `BentoPack` est incomplet au niveau du code source C++ et des scripts :
  - La macro d'exportation d'API s'appelle toujours `SPRITESTUDIO_CORE_EXPORT` dans `BentoPack/include/bentopackcore_export.h`.
  - La variable d'environnement de recherche des plugins est `SPRITESTUDIO_PLUGIN_PATH`.
  - Les macros de build commercial s'appellent `SPRITESTUDIO_COMMERCIAL_BUILD` et `SPRITESTUDIO_LICENSE_TOKEN`.
  - Les tags d'intégrité utilisent le préfixe `X-SS-Integrity`.
- **Plan d'Action :**
  1. Remplacer `SPRITESTUDIO_CORE_EXPORT` par `BENTOPACK_CORE_EXPORT` dans tous les en-têtes publics.
  2. Prendre en compte `BENTOPACK_PLUGIN_PATH` en priorité dans `ExtractorRegistry` et `FilterRegistry` (avec fallback transparent sur `SPRITESTUDIO_PLUGIN_PATH` pour rétrocompatibilité).
  3. Renommer les variables CMake et macros de configuration en `BENTOPACK_COMMERCIAL_BUILD` et `BENTOPACK_LICENSE_TOKEN`.

### CH-TECH-3 : Correction des Doublons de Code & Scories
- **Constat d'Audit :**
  - Dans `BentoPack/src/license/licensemanager.cpp`, les méthodes `complianceMetadata()` et `applyWatermark()` contiennent des insertions dupliquées consécutives à l'identique (lignes 107-135 et 153-180).
  - Dans `wrappers/TexturePacker`, la structure `if command -v bentopack-cli; then ... else ... fi` exécute la même commande dans les deux branches.
  - Dans `CMakeLists.txt`, la directive CPack RPM déclare `set(CPACK_RPM_PACKAGE_LICENSE "MIT")` alors que la licence du projet est Apache 2.0.
- **Plan d'Action :**
  1. Supprimer les lignes d'insertion dupliquées dans `licensemanager.cpp`.
  2. Nettoyer et fiabiliser les scripts de wrapper dans `wrappers/`.
  3. Corriger la licence du paquet RPM dans `CMakeLists.txt` vers `"Apache-2.0"`.

### CH-TECH-4 : Découpage de la Suite de Test Monolithique (`test_controllers.cpp`)
- **Constat d'Audit :** Le fichier `tests/test_controllers.cpp` compte plus de 3 048 lignes regroupant 32 tests hétérogènes (AppConfig, ProjectController, AnimationController, AtlasViewController, filtres, i18n, Git time-travel).
- **Plan d'Action :**
  - Découper ce fichier en suites thématiques spécialisées :
    - `test_controller_project.cpp` : Chargement, sauvegarde asynchrone, sessions, Git time-travel.
    - `test_controller_animation.cpp` : Timeline, scrubber, lecture, modes de boucle.
    - `test_controller_atlas.cpp` : Interactions QGraphicsView, zoom au curseur, sélection par lasso, manipulation des boîtes.
    - `test_app_config.cpp` : Persistance JSON, fallback en cas de fichier corrompu, synchronisation i18n.

### CH-TECH-5 : Hygiène Git & Nettoyage des Artefacts de Test
- **Constat d'Audit :** L'espace de travail contient 86 fichiers `.tres` orphelins, des images d'atlas volumineuses non suivies dans `examples/godot_demo/`, et des fichiers `.uid` de Godot non ignorés. Les scripts d'audit et de génération de clés dans `scripts/` ne sont pas versionnés.
- **Plan d'Action :**
  1. Mettre à jour `.gitignore` pour ignorer systématiquement `examples/godot_demo/*.tres`, `examples/godot_demo/*.png`, `examples/godot_demo/*.bento` et `*.uid`.
  2. Nettoyer les fichiers générés orphelins du working directory.
  3. Versionner proprement `scripts/audit_asset_compliance.py` et `scripts/generate_commercial_key.py`.

### CH-TECH-6 : Synchronisation & Correction de la Documentation
- **Constat d'Audit :** Coquilles post-rebranding dans `docs/USER_GUIDE.md` et `docs/DEVELOPER_GUIDE.md` (`"anciennement BentoPack"`), chemins d'exemples SDK obsolètes dans `README.md` (`examples/sample_filter_plugin` inexistant), et fuites de chemins absolus Windows dans `benchmarks/REPORT.md`.
- **Plan d'Action :**
  1. Corriger les mentions d'historique dans les guides.
  2. Créer l'exemple manquant `examples/sample_filter_plugin` ou aligner le `README.md` sur `examples/sample_plugin`.
  3. Purger les chemins absolus locaux dans `benchmarks/REPORT.md` au profit de chemins relatifs portables.

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
| **Semaine 2** | **CH-TECH-2 & CH-TECH-6** | • Remplacer `SPRITESTUDIO_CORE_EXPORT` par `BENTOPACK_CORE_EXPORT`<br>• Aligner la documentation (`USER_GUIDE.md`, `DEVELOPER_GUIDE.md`, `README.md`)<br>• Purger les chemins absolus locaux dans `benchmarks/REPORT.md` | Rebranding 100% cohérent, documentation irréprochable. |
| **Semaine 3-4** | **CH-TECH-1 & CH-TECH-4** | • Scinder `BentoPackCore` (pur headless) et `BentoPackGUI`<br>• Alléger `bentopack-cli` (dépendance `Qt6Widgets` éliminée)<br>• Découper `test_controllers.cpp` en 4 fichiers de tests ciblés | Architecture saine, CLI prêt pour la CI cloud minimale. |
| **Mois 2** | **M10 (Godot & Stores)** | • Finaliser et publier l'addon Godot 4 sur AssetLib<br>• Lancer la page Steam et la boutique Itch.io pour la version Store Convenience | Premier flux de revenus et visibilité communauté. |
| **Mois 3** | **M10 (Unity & Unreal)** | • Développer le package Unity UPM avec support `SpriteMeshType.Tight`<br>• Développer le plugin UE5 PaperZD pour Fab | Couverture complète des trois moteurs majeurs du marché. |
| **Mois 4-5** | **M14 & M12** | • Vectorisation SIMD des filtres graphiques<br>• Cadrage et développement initial du rigging 2D (Spine JSON) | Performances extrêmes 8K/16K et diversification fonctionnelle. |
