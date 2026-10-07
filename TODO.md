# 📋 Feuille de Route & Spécifications Métier — BentoPack

> **Dernière mise à jour :** 2026-09-30  
> **Statut global :** Socle logiciel et addons moteurs finalisés (Godot 4, Unity 6, Unreal Engine 5 packagés et validés, 15 suites CTest à 100% de réussite). Focus actif sur la publication sur les stores officiels, les formats d'exportation post-M10, les optimisations SIMD (M14) et l'animation squelettique (M12).

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
| **M6** | **Empaquetage Avancé MaxRects** | Moyenne | Heuristiques BSSF, BAF, BLSF, BottomLeft, ContactPoint, contrainte POT ($2^n$), extrusion de bordure anti-bleeding (1-4px), déduplication de frames. |
| **M7** | **Filtres Graphiques & Traitement** | Moyenne | 9 plugins de filtres (Chroma BG, Despill, Outline, ColorSwap, ColorAdjust, RetroPalette, Rescale, MaxRects, TightPacking) avec live preview. |
| **M8** | **Empaquetage Polygonal & Maillages Serrés** | Haute | Marching Squares étanche, RDP, triangulation Ear-Clipping, réduction de 60-80% de l'overdraw GPU, export Unity Tight, Godot ArrayMesh & Paper2D. |
| **M9** | **Compression de Textures VRAM (KTX2)** | Haute | Khronos `basis_universal` v2.50, modes UASTC 4x4 et ETC1S, Zstd niveaux 1-22, téléversement direct GPU sans décompression CPU. |
| **M10** | **Addons Moteurs (Godot, Unity, Unreal)** | Haute | **Godot 4 :** `godot-bentopack-addon` (AssetLib zip, ArrayMesh, transport interactif).<br>**Unity 6 :** `com.bentopack.importer` (UPM tgz, Tight Mesh, AnimationClips, Dashboard).<br>**Unreal 5 :** `BentoPack UE5 Plugin` (Zip Fab, UFactory, M8 Render/Collision, Slate Dashboard).<br>Cahier des charges : [`addons/ADDONS_GUIDELINES.md`](file:///addons/ADDONS_GUIDELINES.md). Méta-cible `package_all_addons`. |
| **M11** | **Architecture de Plugins Qt6 & SDK** | Haute | Externalisation des 9 filtres et 6 extracteurs en modules `.so`/`.dll` dynamiques (`QPluginLoader`), CMake config exportable. |
| **M15** | **Refonte Drag & Drop Filmstrip** | Moyenne | `FilmstripListWidget` dédié, calcul linéaire du drop 1D, indicateur bleu contrasté, découplage transactionnel sans récursion destructrice. |
| **M17** | **Calques Aseprite & Variantes Skin** | Haute | Structures `SpriteLayer` et `SpriteCel` dans `SpriteDocument`, décodage/encodage binaire Aseprite complet (`CHUNK_LAYER`, `CHUNK_CEL` raw/zlib, tags, blending), profils de skins/variantes combinatoires et découplés, compatibilité 100% Aseprite bidirectionnelle. |
| **M18** | **Édition de Pixels Multi-Calques & Pile de Calques** | Haute | Dock `LayerStackWidget` complet dans `PixelEditorDialog` (Z-order, visibilité, verrou, opacité 0-100%, modes de composition QPainter, duplication, suppression, merge down, flatten), dessin multi-couches dans `PixelCanvas`, échantillonnage multi-calques (pipette, remplissage, baguette magique), onion skinning ciblé, synchronisation bidirectionnelle avec `SpriteDocument` et préservation des documents plats. |
| **M20** | **Taxonomie Filtres & Pixel Editor** | Moyenne | Typage déclaratif `FilterModifierFlags` (`PixelModifier`, `GeometryModifier`, `AtlasModifier`). Intégration complète des filtres non-atlas dans `PixelEditorDialog` avec bouton menu dédié, gestion ciblée frame unique vs animation complète via checkbox dynamique, et Undo/Redo transactionnel. |
| **M-CLI** | **Interface CLI & Mode Watch Daemon** | Moyenne | `bentopack-cli` avec mode daemon de surveillance en arrière-plan (`--watch`, détection QFileSystemWatcher, debouncing, isolation par verrou `.lock`, protection contre l'auto-déclenchement), drop-in TexturePacker / Aseprite (`-b`) et export Godot 4 avec conservation des UIDs. |
| **M-DEVOPS** | **Release CI Multi-Plateforme & Packaging** | Haute | Pipelines CI/CD automatisés sur tous les OS cibles :<br>• **Windows :** NSIS installer & ZIP portable autonome (résolution transitives MinGW par `ldd` 3 passes, `qt.conf` relatif).<br>• **Linux :** Ubuntu 22.04+ (paquets DEB, RPM, AppImage autonome, Tarball).<br>• **macOS :** Puces Apple Silicon (ARM64) et Intel (x86_64), bundles `.app` et archives DMG.<br>• **Haiku OS :** Support natif BeAPI/HaikuDepot via `haiku.PackageInfo.in` et `package.cmake` (génération `.hpkg`). |

---

## 🎯 Feuille de Route Active (Chantiers Restants)

```
┌────────────────────────────────────────────────────────────────────────┐
│                        FEUILLE DE ROUTE ACTIVE                         │
├───────────────────┬───────────────────────────────────┬────────────────┤
│ Jalon             │ Thématique                        │ Priorité       │
├───────────────────┼───────────────────────────────────┼────────────────┤
│ STORES            │ Publication & Distribution Stores │ Haute (Imm.)   │
│ M19 (SMART-MESH)  │ Maillage Intelligent CDT & Relief │ Haute / Moyenne│
│ M16               │ Multi-Page Atlas (Atlas Spanning) │ Haute / Moyenne│
│ FORMATS           │ Formats d'Exportation Post-M10    │ Moyenne        │
│ FILTERS-POLISH    │ Fignolage Extrusions & Filtres    │ Moyenne        │
│ CLI-DOCS          │ Clarification Watch Daemon CLI    │ Moyenne        │
│ M14               │ Optimisations SIMD & Grands Atlas │ Moyenne        │
│ M12               │ Rigging 2D (Cadrage Anti-Creep)   │ Basse (Opt.)   │
│ M13               │ Micro-Démonstrateur Web (WASM)    │ Basse          │
└───────────────────┴───────────────────────────────────┴────────────────┘
```

---

## 🏪 1. Publication & Distribution Stores (Finalisation M10 & Commercialisation)

### 📌 Objectifs
Les packages d'addons moteur et l'application autonome BentoPack sont entièrement générés par CMake (`package_all_addons` et `cpack`). Il reste à réaliser la distribution publique et la mise en ligne sur les marketplaces officielles :

1. **Godot Asset Library :**
   - Soumission de l'archive `godot-bentopack-addon-0.11.0.zip`.
   - Création de la fiche AssetLib (catégorie *2D Tools*, tags `spritesheet`, `animation`, `mesh`, `anti-overdraw`).
2. **Unity Package Manager & Asset Store :**
   - Hébergement OpenUPM du package `com.bentopack.importer`.
   - Soumission du package sur l'Unity Asset Store (catégorie *2D Tools / Texture & Sprites*).
3. **Epic Games Fab (`fab.com`) :**
   - Soumission du plugin Unreal Engine 5 `unreal-bentopack-plugin-0.11.0.zip` (support UE 5.3+).
4. **Boutiques Desktop (Store Convenience) :**
   - Lancement de la page Steam et de la boutique Itch.io pour la distribution binaire packagée de BentoPack Studio (Windows NSIS/ZIP, Linux AppImage/Tarball).

> [!TIP]
> **Kit Marketing & Listings Prêts à l'Emploi :**
> L'ensemble des descriptions, spécifications techniques, tags et la checklist des captures d'écran (avec noms de fichiers et résolutions recommandées) sont centralisés dans [`docs/STORE_DESCRIPTIONS.md`](file:///docs/STORE_DESCRIPTIONS.md) et consultables interactivement via [`docs/STORE_DESCRIPTIONS.html`](file:///docs/STORE_DESCRIPTIONS.html).

---

## 🔌 2. Écosystème des Formats & Plugins Extracteurs (Évolutions Post-M10)

### 📌 État Actuel des Plugins Extracteurs (Socle Validé)
| Plugin | Extensions | Import | Export | Spécificités & Rôle dans le Pipeline |
|---|---|:---:|:---:|---|
| **`spritesheet`** | `.png`, `.webp`, `.jpg`, `.jpeg`, `.bmp`, `.ktx2`, `.basis` | ✅ | ✅ | Découpe automatique par seuillage alpha & tolérance, smart crop, compression VRAM GPU matérielle (KTX2 UASTC/ETC1S, Basis). |
| **`gif`** | `.gif` | ✅ | ✅ | Import et export de séquences animées multi-animations (`projectname_animationname.gif`) avec timings FPS, loop modes (Loop, Once, Ping-Pong) et transparence. |
| **`json`** | `.json` | ✅ | ✅ | Standard TexturePacker (Hash & Array) + Aseprite (`frameTags` convertis nativement en animations BentoPack avec leurs `loop_mode`). Interopérable d'emblée avec Phaser 3, PixiJS, Bevy, Raylib, Defold. |
| **`godot`** | `.tres` | ✅ | ✅ | Format texte natif Godot 4.x (`SpriteFrames`) avec sous-ressources `AtlasTexture`, animations et atlas compagnon. |
| **`unity`** | `.unity.json`, `.json` | ✅ | ✅ | Descripteur de maillage serré (*Tight Sprite Mesh*) injectant sommets, UVs et triangles dans `Sprite.OverrideGeometry`. |
| **`unreal`** | `.paper2d.json`, `.json` | ✅ | ✅ | Format dédié UE5 Paper2D / PaperZD avec géométrie de rendu polygonale M8 (`RenderGeometry`) et `CollisionGeometry`. |
| **`libgdx_spine`** | `.atlas`, `.atlas.txt` | ✅ | ✅ | Format textuel clé-valeur standard LibGDX et Spine 2D (pages, régions, index d'animations, pivots, texture compagnon). |
| **`aseprite`** | `.ase`, `.aseprite` | ✅ | ✅ | Import et export binaires directs Aseprite (.ase, .aseprite) : 32bpp RGBA, compression zlib, cels, tags d'animations convertis et préservés. |

### 🚀 Formats Cibles Post-M10 (Élargissement Industriel)

1. **Format Texte LibGDX / Spine (`.atlas`) — ✅ Terminé & Testé :**
   - **Enjeu :** Standard clé-valeur textuel ultra-répandu dans les frameworks indés et légers (Raylib, Bevy Rust, MonoGame, Defold, LibGDX).
   - **Avantage :** Parsing trivial sans dépendance JSON, interopérabilité directe avec les runtimes Spine officiels.
2. **Format Binaire Natif Aseprite (`.ase` / `.aseprite`) — ✅ Terminé & Testé :**
   - **Enjeu :** Ouvrir ou glisser-déposer directement un projet Aseprite dans BentoPack sans étape intermédiaire manuelle d'exportation vers JSON+PNG.
   - **Avantage :** Argument produit majeur pour la communauté des pixel-artists. Décodage binaire 100% natif Qt (zlib, palettes, calques, frames, tags d'animations convertis).
3. **Refonte de la Boîte de Dialogue d'Export (`ExportDialog`) — ✅ Terminée & Testée :**
   - **Workflow par étapes intuitives :**
     - **Étape 1 (Destination & Nom) :** Dossier de sortie (`txtOutputDir` + bouton Parcourir), nom du projet/fichier (`txtBaseName`), aperçu temps réel du chemin de destination (`txtFilePath`).
     - **Étape 2 (Format cible) :** Sélection du moteur / format d'export avec mise à jour automatique de l'extension et description d'usage.
     - **Étape 3 (Options contextuelles dynamiques) :**
       - Pour les formats Atlas (Godot, Unity, Unreal, LibGDX/Spine, JSON) : Algorithmes de packing, géométrie/marges, compression VRAM KTX2/Zstd et télémétrie GPU en direct.
       - Pour le format GIF animé : Masquage total de l'atlas et de la VRAM, options de lecture (boucle, une fois, ping-pong), cadence FPS, seuil alpha et export multi-animations.
       - Pour le format Aseprite natif : Masquage de l'atlas et de la VRAM, compression zlib des cels et tags d'animations.
4. **Format Apple / Cocos2d-x (`.plist` XML) — Priorité Basse :**
   - **Enjeu :** Compatibilité avec les pipelines historiques de jeux mobiles 2D (Cocos2d-x, SpriteKit iOS).
   - **Complexité :** Faible (sérialisation XML de dictionnaires/rectangles).

---

## 🗂️ 3. M16 : Multi-Page Atlas & Texture Spanning (Gestion du Débordement VRAM)

### 📌 Contexte & Problématique Métier
En production réelle (RPG, metroidvanias, jeux de combat), un personnage ou une collection de sprites peut comporter des centaines voire des milliers de frames d'animation et de variantes cosmétiques (skins).
* Si le projet impose une taille maximale de texture stricte (ex. $2048 \times 2048$ pour compatibilité mobile bas de gamme OpenGL ES 3.0 / WebGL ou $4096 \times 4096$ pour Nintendo Switch), l'ensemble des sprites ne peut pas tenir sur une seule texture sheet.
* Actuellement, le packing échoue ou tronque les frames si la surface totale dépasse la capacité de l'atlas unique.

### 🏛️ Spécifications Techniques d'Implémentation
1. **Algorithme de Partitionnement Multi-Pages :**
   - Heuristique de tri décroissant (Area / Max Dimension) et découpage glouton en pages d'atlas successives (`Page 0`, `Page 1`, ..., `Page N`).
   - Respect strict des contraintes par page : dimensions maximales ($W_{max}, H_{max}$), contrainte Power of Two (POT $2^n$), force square, marges intérieures et extrusions.
   - Heuristique de regroupement sémantique : regrouper en priorité les frames d'une même animation sur la même page d'atlas pour minimiser les changements de texture GPU (*texture binds / state switches*) à l'exécution.
2. **Nomenclature & Génération d'Artefacts :**
   - Export d'un ensemble ordonné de textures compagnons : `nom_atlas_0.png`, `nom_atlas_1.png` (ou `.ktx2`).
3. **Mise à Jour des Descripteurs Moteurs :**
   - **Format JSON (TexturePacker / Aseprite) :** Ajout de la clé `"pages"` ou de la propriété `"image": "nom_atlas_X.png"` au niveau de chaque descripteur de sprite.
   - **Godot 4 (`.tres`) :** Génération de plusieurs sous-ressources `CompressedTexture2D` et référencement explicite de la page adéquate dans chaque `AtlasTexture`.
   - **Unity 2D (`.unity.json`) :** Indexation de la texture source par sprite dans la structure de métadonnées pour le bridge `BentoImporter`.
   - **Unreal Engine 5 Paper2D (`.paper2d.json`) :** Association de la texture parente pour chaque `RenderGeometry`.
   - **LibGDX / Spine 2D (`.atlas`) :** Standard textuel natif multi-pages (chaque bloc de page déclare sa texture, ses dimensions et la liste de ses régions associées).

---

## 🎨 4. Fignolage des Extrusions de Bordure & Filtres Plugins

### 📌 État Actuel & Objectifs de Raffinement
L'extrusion de bordure est déjà opérationnelle dans `atlaspacker.cpp`, `tightpolygonpacker.cpp` et `atlaspackingdialog.cpp`, et 9 plugins de filtres dynamiques sont intégrés via l'architecture modulaire `QPluginLoader`. Plusieurs raffinements techniques doivent être finalisés pour garantir un rendu parfait sous tous les filtres GPU :

1. **Raffinements de l'Extrusion Anti-Bleeding :**
   - **Angles 8-connectés (Corners Clamping) :** Compléter la duplication des pixels sur les diagonales extérieures des coins pour éviter les pixels noirs ou transparents lors d'un échantillonnage bilinéaire ou sous fort mipmapping à angles obliques.
   - **Épaisseur paramétrable :** Autoriser un paramétrage fin de l'extrusion de 1 à 4 pixels selon le niveau de mipmapping visé.
2. **Perfectionnement des Filtres Plugins :**
   - **Despill Filter :** Ajout d'une option de calcul de distance colorimétrique dans l'espace perceptuel CIELAB pour éliminer les halos verts/bleus sur les contours antialiasés sans altérer les teintes voisines.
   - **Outline Filter :** Lissage des contours à sous-pixel pour les contours circulaires ou fins.
   - **Support des ratios d'aspect non-carrés :** Prise en compte explicite des pixels anamorphiques ou des mises à l'échelle asymétriques.

---

## ⏱️ 5. Mode Daemon Watch CLI : Clarification & Documentation

### 📌 État Actuel
Le mode daemon est déjà entièrement implémenté et opérationnel au cœur de `bentopack-cli` via la classe `WatchDaemon` ([`BentoPack/include/cli/watch_daemon.h`](file:///BentoPack/include/cli/watch_daemon.h), [`watch_daemon.cpp`](file:///BentoPack/src/cli/watch_daemon.cpp)) :
* Surveillance récursive via `QFileSystemWatcher`.
* Filtrage strict par extensions (`.png`, `.webp`, `.ase`, `.aseprite`, etc.).
* Débouncing paramétrable (délai anti-rebond) pour éviter les recompilations multiples lors d'écritures en rafale.
* Protection active contre l'auto-déclenchement (*self-trigger protection*) et isolation par fichier verrou (`.lock`).

### 📌 Actions Requises :
1. **Clarification de la documentation utilisateur et développeur :**
   - Mettre en avant la commande `bentopack-cli --watch <dossier_source> --output <dossier_cible>` dans `docs/USER_GUIDE.md` et `docs/DEVELOPER_GUIDE.md`.
   - Fournir des scripts d'exemple d'intégration pour pipelines de studio (ex. tâche npm/gulp, script shell d'arrière-plan, intégration Makefile/CMake).
2. **Indicateur d'état GUI (Optionnel) :**
   - Intégrer une icône de notification discrète dans la barre d'état de l'application de bureau indiquant si un watcher CLI est actif sur le projet courant.

---

## 🌍 6. Matrice Multi-Plateforme Validée & CI/CD (Windows, Linux, macOS, Haiku)

### 📌 Architecture de Déploiement Standalone
L'infrastructure de compilation et de livraison automatisée (`.github/workflows/release.yml` et `package.cmake`) couvre l'ensemble des systèmes d'exploitation modernes, sans dépendance externe au runtime :

| Plateforme | Architectures | Format de Package | Spécificités Techniques |
|---|---|---|---|
| **Windows** | x86_64 | • Installateur NSIS (`.exe`)<br>• Archive ZIP autonome | Résolution automatique des DLLs transitives MinGW par `ldd` en 3 passes, injection locale de `qt.conf` (`Prefix = .\nPlugins = .`), zéro installation dans le registre. |
| **Linux** | x86_64 | • AppImage standalone<br>• Paquets `.deb` (Debian/Ubuntu)<br>• Paquets `.rpm` (Fedora/RHEL)<br>• Tarball `.tar.gz` | Support natif Wayland et X11, binaire AppImage portable 1-clic compatible SteamOS / Steam Deck. |
| **macOS** | ARM64 (Apple Silicon)<br>+ Intel (x86_64) | • Fichier Image disque `.dmg`<br>• Bundle `.app` dans `.tar.gz` | Binaires natifs M1/M2/M3/M4 et Intel, configuration Info.plist, rpath dynamique pour les frameworks Qt6. |
| **Haiku OS** | x86_64 | • Paquet natif HaikuDepot (`.hpkg`) | Support par conviction et affinité communautaire. Intégration via `haiku.PackageInfo.in` et `package.cmake`, compatibilité BeAPI et libroot. |

---

## ⚡ 7. M14 : Optimisations Hautes Performances SIMD & Gestion Mémoire Grands Atlas

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
   - **Snapshotting différentiel (*dirty rects*) :** Mémoriser uniquement le sous-rectangle modifié lors des retouches dans l'Éditeur de Pixels plutôt que de cloner l'image complète de l'atlas à chaque coup de pinceau (évite de saturer plusieurs gigaoctets de RAM lors d'éditions sur planches 8K).
   - Compression transparente en tâche de fond des états anciens de l'UndoStack via LZ4.

---

## 🦴 8. M12 : Animation Squelettique 2D (Cadrage & Prévention du Scope Creep)

### 📌 Arbitrage de Production Senior
> [!WARNING]
> **Alerte Risque Dérive Périmètre (Scope Creep) :**  
> BentoPack tire son excellence et sa réputation de son positionnement chirurgical : **être le meilleur processeur d'atlas 2D, de compression VRAM et de maillage polygonal anti-overdraw**.  
> Réimplémenter un solveur d'animation squelettique complet (IK, skinning, hiérarchies d'os) risquerait d'alourdir inutilement le projet et de concurrencer frontalement des outils spécialisés matures comme Spine 2D ou DragonBones.

### 💡 Stratégie Retenue :
* **Priorité Basse / Plugin optionnel séparé :** Si ce module est développé, il doit se limiter à un **outil de découpe de membres (Limb Slicing)** et un **exporteur de métadonnées** vers les moteurs cibles :
  - Génération de hiérarchies `Skeleton2D` / `Bone2D` prêtes pour Godot 4.
  - Export compatible Unity `2D Animation`.
  - Export de maillages polygonaux vers Spine JSON.
* Aucun solveur IK lourd ne sera intégré dans le noyau de BentoPack.

---

## 🌐 9. M13 : Micro-Démonstrateur Web Vitrine (WebAssembly Showcase)

### 📌 Cadrage Stratégique
Le portage intégral de l'application de bureau en WebAssembly (Qt for WebAssembly) est écarté en raison du poids de téléchargement excessif (25 à 40 Mo) et du sandboxing navigateur.

### 💡 Solution Retenue :
Concevoir un **micro-démonstrateur web vitrine ultra-léger** (mini-module WebAssembly ou script TypeScript/Canvas) hébergé sur le site officiel de BentoPack. Il permettra aux visiteurs de glisser-déposer un sprite pour tester instantanément en direct la découpe automatique, le despill et l'aperçu d'animation, servant d'entonnoir d'acquisition vers l'application de bureau native.

---

## 🎨 10. M17 : Calques Aseprite & Variantes d'Animations (Skins / Équipements Modulaires)

### 📌 Contexte & Problématique Métier
Dans les jeux vidéo (RPG, action, beat'em up), un personnage effectue les mêmes mouvements corporels (ex. course, coup d'estoc, saut), mais l'élément tenu en main ou équipé change (épée en acier, gourdin, baguette magique ou bouquet de fleurs ; chapeaux, armures, boucliers).
* **Dans Aseprite :** Cette logique est couramment modélisée via les **calques (*layers*)** : un calque pour le corps animé, et plusieurs calques distincts pour les accessoires alternatifs.
* **Limitation actuelle de BentoPack :** L'extracteur `asepriteextractor.cpp` aplatit aveuglément tous les calques visibles lors du décodage binaire des cels. Il est impossible d'importer la structure hiérarchique, de basculer la visibilité d'un accessoire, ou de générer des variantes d'atlas sans exporter manuellement chaque combinaison depuis Aseprite.

### 🏛️ Spécifications Techniques d'Implémentation
1. **Évolution du Modèle de Données (`SpriteDocument` & `SpriteAnimation`) :**
   - Introduction d'une structure de calque de premier ordre :
     ```cpp
     struct SpriteLayer {
         QString id;
         QString name;
         bool visible = true;
         bool locked = false;
         quint8 opacity = 255;
         int zOrder = 0;
         QPainter::CompositionMode blendMode = QPainter::CompositionMode_SourceOver;
     };
     ```
   - Chaque frame / cel stocke son image par calque plutôt qu'une image unique pré-aplatie (`QMap<QString, QImage> m_layerCels`).
2. **Décodage Binaire Aseprite Avancé (`asepriteextractor.cpp`) :**
   - Préservation de l'arbre des calques Aseprite (`CHUNK_LAYER`, type normal/groupe, hiérarchie `childLevel`, drapeaux de visibilité).
   - Décodage cel par cel (`CHUNK_CEL`) sans aplatissement immédiat, avec association directe au calque parent (`layerIndex`).
3. **Gestion des Profils de Variantes (Skins / Équipements) :**
   - Définition de profils de visibilité : un profil déclare quels calques sont actifs (ex. Profil *« Épée »* = Base + Main_Epée ; Profil *« Fleur »* = Base + Main_Fleurs).
   - **Mode Combinatoire / Baking :** Génération automatique des séquences d'animations combinées pour l'atlas (`hero_walk_sword`, `hero_walk_flower`).
   - **Mode Modulaire / Découplé :** Export séparé de la base et des overlays d'équipements calés sur le **même point de pivot**, permettant aux moteurs (Godot, Unity) d'assembler les sprites dynamiquement à l'exécution avec zéro duplication de texture pour le corps.
4. **Corpus de Test & Fichiers d'Exemple :**
   - Intégration de fixtures `.aseprite` multi-calques sous licence libre (CC0 / MIT, ex. spritesheets modulaires de Kenney ou Daniel Linssen) dans `tests/data/aseprite/`.
   - Tests automatisés dans `test_extractors.cpp` validant la conservation des calques, des cels indépendants et des tags.

---

## 🖌️ 11. M18 : Édition de Pixels Multi-Calques & Pile de Calques (Layer Stack) — ✅ Terminé & Validé (42/42 Tests Pixel Editor, 15/15 CTest)

### 📌 Contexte & Problématique Métier
Avec l'avènement des animations multi-calques, l'Éditeur de Pixels ([`PixelEditorDialog`](file:///BentoPack/src/widgets/pixeleditordialog.cpp), [`PixelCanvas`](file:///BentoPack/src/widgets/pixelcanvas.cpp)) ne peut plus se contenter d'éditer une simple image aplatie. L'utilisateur doit pouvoir dessiner sur l'accessoire sans écraser les pixels du corps du personnage situé dessous.

### 🏛️ Spécifications Techniques d'Implémentation
1. **Dock « Pile de Calques » (*Layer Stack*) dans `PixelEditorDialog` :**
   - Liste ordonnée de bas en haut respectant le Z-order.
   - Sélection du **calque actif** (sur lequel tous les outils de dessin opèrent).
   - Contrôles directs par calque :
     - 👁️ Bascule de visibilité.
     - 🔒 Verrouillage en écriture (protection anti-peinture accidentelle).
     - Curseur d'opacité (0 à 100%).
     - Menu déroulant des modes de fusion (Normal, Multiplier, Écran, Incrustation, etc.).
   - Actions contextuelles : *Nouveau calque*, *Dupliquer*, *Supprimer*, *Monter/Descendre*, *Fusionner vers le bas (Merge Down)*, *Aplatir l'image (Flatten)*.
2. **Rendu Composite & Moteur de Tracé (`PixelCanvas`) :**
   - Composition en temps réel multi-couches accélérée par `QPainter` avec buffer de prévisualisation du trait en cours.
   - **Outils sensibles aux calques :**
     - *Crayon / Pinceau / Gomme :* Altère strictement les pixels du calque actif.
     - *Pipette :* Option commutable *« Échantillonner calque actif »* vs *« Échantillonner tous les calques visibles »*.
     - *Seau de remplissage & Baguette magique :* Option *« Détection de contour multi-calques »* (permet de remplir une zone sur un calque transparent en s'appuyant sur les traits d'encrage d'un autre calque).
3. **Synergie avec la Pelure d'Oignon (Onion Skinning) :**
   - Option d'affichage : projeter l'onion skin sur l'ensemble de l'image composite, OU isoler l'onion skin au calque actif uniquement (idéal pour régler la trajectoire d'un coup d'épée sans être distrait par le corps).
4. **Application Multi-Frames Transactionnelle :**
   - La propagation de traits à travers plusieurs frames (`CanvasActionData`) applique la modification sur le calque sélectionné de chaque frame cible.

---

## 📐 12. M19 : Maillage Polygonal Intelligent (Triangulation CDT, Points de Steiner & Contraste Interne)

### 📌 Contexte & Problématique Métier
La triangulation actuelle par Ear-Clipping ([`triangulator.cpp`](file:///c:/Users/ec135/Documents/GitHub/SpriteStudio/BentoPack/src/geometry/triangulator.cpp)) opère exclusivement sur les sommets du contour extérieur du sprite.
* **Défaut majeur :** Sur des formes concaves ou allongées (bras, épées, capes, jambes), l'algorithme génère des triangles étirés et très effilés (*sliver triangles* avec des angles très aigus $< 10^\circ$).
* **Conséquences GPU & Artistiques :**
  1. *Pénalité Quad Overdraw :* Les petits triangles étirés traversent de multiples quads de pixels $2 \times 2$ sur les GPUs, annulant une partie des gains de fillrate.
  2. *Inutilisable en déformation 2.5D / Squelettique :* Les artistes 3D/2D dans Unity (2D Animation), Godot (Skeleton2D / Bone2D) ou Spine ne peuvent pas déformer proprement le maillage (rigging, bending). Les membres se tordent avec des artefacts d'interpolation hideux.

### 🏛️ Spécifications Techniques d'Implémentation
1. **Conservation Stricte de la Frontière Extérieure :**
   - Le contour extérieur simplifié (Marching Squares + RDP + normal padding) reste la frontière rigide (*rigid boundary constraint*). Aucun pixel opaque n'est exclu, la découpe anti-overdraw reste à 100% garantie.
2. **Détection d'Arêtes Internes à Fort Contraste (*Feature Edge Detection*) :**
   - Analyse locale du gradient sur les canaux RGB/Luminance (filtre Sobel / Scharr ou dérivée morphologique) au sein de la zone opaque.
   - Détection des lignes de rupture fortes : séparation nette entre chevelure et visage, col de vêtement, contours d'yeux, plis de tissu, limite bras/buste.
   - Vectorisation et simplification RDP de ces arêtes intérieures sous forme de polylignes de contrainte.
3. **Triangulation de Delaunay Contrainte (CDT) & Points de Steiner :**
   - Remplacement / évolution du Ear-Clipping basique par une **Triangulation de Delaunay Contrainte (Constrained Delaunay Triangulation - CDT)** (ex. algorithme de Chew / Ruppert).
   - Les segments du contour extérieur ET les arêtes internes de contraste sont injectés comme arêtes obligatoires (*constrained edges*).
   - **Génération de points intérieurs (Points de Steiner) :**
     - Insertion de points au barycentre des triangles trop grands ou trop effilés.
     - Garantie d'un angle minimal (ex. $\theta_{min} \ge 25^\circ$ à $30^\circ$) assurant des triangles bien proportionnés (*Delaunay quality mesh*).
4. **Bénéfice Moteur & Prise en Main dans l'UI :**
   - Les sous-parties du sprite (ex. les mèches de cheveux, le visage, la manche) forment des groupes de triangles cohérents. Un artiste 2D/3D peut pondérer un os (*bone weight*) sur le cluster de cheveux pour les faire bouger indépendamment du visage au vent !
   - Contrôles interactifs dans [`PolygonMeshDialog`](file:///c:/Users/ec135/Documents/GitHub/SpriteStudio/BentoPack/src/widgets/polygonmeshdialog.cpp) :
     - Curseur *« Densité du maillage intérieur »* (faible, équilibrée, dense).
     - Curseur *« Sensibilité au contraste »* (seuil d'arêtes internes).
     - Curseur *« Angle minimal garanti »* (15° à 35°).

---

## 🧩 13. M20 : Taxonomie & Déclencheurs Intelligents des Filtres (Pipeline Déclaratif)

### 📌 Contexte & Problématique Métier
À ce jour, les 9 plugins de filtres implémentent `FilterPlugin` sans déclarer la nature exacte de leurs mutations. Le moteur ignore si un filtre ne fait que changer une teinte (ex. *ColorSwap*), altère la silhouette alpha (ex. *Outline*, *BackgroundRemoval*), ou bouscule l'emplacement de tous les sprites sur la feuille (ex. *AtlasPacking*).
* **Conséquence :** L'utilisateur applique un filtre modifiant le contour (ex. contour de 2px) mais doit penser manuellement à rouvrir le dialogue de maillage polygonal pour recalculer les triangles devenus obsolètes.

### 🏛️ Spécifications Techniques d'Implémentation
1. **Typage Déclaratif des Filtres (`FilterModifierFlags`) :**
   - Extension de l'interface [`FilterPlugin`](file:///c:/Users/ec135/Documents/GitHub/SpriteStudio/BentoPack/include/filters/filterplugin.h) :
     ```cpp
     enum FilterModifierFlag {
         None               = 0x0,
         PixelModifier      = 0x1, // Teinte, palette, saturation, despill (silhouette et taille 100% invariantes)
         GeometryModifier   = 0x2, // Modifie le contour alpha, la taille ou la silhouette (Outline, Rescale, Chroma BG)
         AtlasModifier      = 0x4  // Réorganise l'empaquetage ou les rectangles de l'atlas (MaxRects, TightPacking)
     };
     Q_DECLARE_FLAGS(FilterModifierFlags, FilterModifierFlag)
     virtual FilterModifierFlags modifierFlags() const = 0;
     ```
   - Classification des filtres existants :
     - `PixelModifier` : `ColorSwapFilter`, `ColorAdjustFilter`, `RetroPaletteFilter`, `DespillFilter`.
     - `GeometryModifier` : `BackgroundRemovalFilter` (efface des pixels de fond), `OutlineFilter` (élargit le sprite), `PixelRescaleFilter` (change les dimensions).
     - `AtlasModifier` : `AtlasPackingFilter`, `TightPolygonPackingFilter`.
2. **Système de Déclencheurs Intelligents & Invalidation de Cache :**
   - **Lors d'un `GeometryModifier` :**
     - Invalidation automatique du cache des polygones simplifiés et des boîtes englobantes ajustées.
     - Notification contextuelle discrète (barre d'action ou toast) : *« La géométrie des sprites a été modifiée. Recalculer le maillage polygonal anti-overdraw ? »* avec bouton d'exécution immédiat `[Recalculer les Maillages]`.
   - **Lors d'un `PixelModifier` :**
     - Préservation stricte de la géométrie polygonale, des coordonnées UV et du placement d'atlas. Aucun calcul lourd superflu n'est déclenché.
   - **Lors d'un `AtlasModifier` :**
     - Mise à jour des coordonnées UV et avertissement si l'atlas dépasse les limites matérielles de la cible.
3. **Organisation Ergonomique dans les Menus & Docks :**
   - Regroupement des actions dans le menu *Filtres* selon leur nature :
     - 🎨 *Filtres de Couleur & Traitement Pixel* (`PixelModifier`)
     - ✂️ *Filtres Géométriques & Découpe* (`GeometryModifier`)
     - 📦 *Organisation & Empaquetage d'Atlas* (`AtlasModifier`)

---

## 📊 Matrice d'Exécution Révisée

| Phase | Horizon | Chantiers Clés | Livrables Attendus |
|---|---|---|---|
| **Phase 1** | **Court terme (Immédiat)** | **Stores & Distribution** | • Soumission officielle Godot AssetLib (`godot-bentopack-addon`)<br>• Enregistrement OpenUPM et soumission Unity Asset Store<br>• Soumission Unreal Fab (`unreal-bentopack-plugin`)<br>• Déploiement des pages Steam / Itch.io (Windows, Linux, macOS, Haiku) | Visibilité officielle sur tous les écosystèmes et premiers flux d'acquisition. |
| **Phase 2** | **Court terme** | **Calques & Pipeline Intelligent** | • **M20 :** Taxonomie des filtres (`PixelModifier` / `GeometryModifier` / `AtlasModifier`) et relance intelligente du maillage<br>• **M17 :** Import calques Aseprite & gestion des variantes d'équipements/skins<br>• **M18 :** Dock Calques et dessin multi-couches dans le Pixel Editor | Prise en charge des assets professionnels modulaires et confort de travail fluide. |
| **Phase 3** | **Moyen terme** | **Maillage Avancé & Grands Atlas** | • **M19 :** Maillage polygonal intelligent (CDT, détection de fort contraste, clusters de déformation 2D/3D)<br>• **M16 :** Multi-Page Atlas (Atlas Spanning & débordement VRAM)<br>• **Filtres/Extrusions :** Clamping angles 8-connectés, despill perceptuel CIELAB | Géométrie haut de gamme pour animateurs et gestion des gros projets RPG/metroidvanias. |
| **Phase 4** | **Moyen/Long terme** | **Performances & Écosystème** | • **M14 :** Vectorisation AVX2 / NEON des filtres graphiques & dirty rects undo<br>• Formats Post-M10 (LibGDX/Spine `.atlas`, export `.plist`)<br>• Micro-démonstrateur web vitrine sur le site officiel<br>• Plugin découpe de membres / export squelettique léger (M12 cadré) | Traitement ultra-rapide des planches 8K/16K et rayonnement produit. |

