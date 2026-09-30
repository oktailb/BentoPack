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
| **M6** | **Empaquetage Avancé MaxRects** | Moyenne | Heuristiques BSSF, BAF, BLSF, BottomLeft, ContactPoint, contrainte POT ($2^n$), extrusion de bordure anti-bleeding, déduplication de frames. |
| **M7** | **Filtres Graphiques & Traitement** | Moyenne | 9 plugins de filtres (Chroma BG, Despill, Outline, ColorSwap, ColorAdjust, RetroPalette, Rescale, MaxRects, TightPacking) avec live preview. |
| **M8** | **Empaquetage Polygonal & Maillages Serrés** | Haute | Marching Squares étanche, RDP, triangulation Ear-Clipping, réduction de 60-80% de l'overdraw GPU, export Unity Tight, Godot ArrayMesh & Paper2D. |
| **M9** | **Compression de Textures VRAM (KTX2)** | Haute | Khronos `basis_universal` v2.50, modes UASTC 4x4 et ETC1S, Zstd niveaux 1-22, téléversement direct GPU sans décompression CPU. |
| **M10** | **Addons Moteurs (Godot, Unity, Unreal)** | Haute | **Godot 4 :** `godot-bentopack-addon` (AssetLib zip, ArrayMesh, transport interactif).<br>**Unity 6 :** `com.bentopack.importer` (UPM tgz, Tight Mesh, AnimationClips, Dashboard).<br>**Unreal 5 :** `BentoPack UE5 Plugin` (Zip Fab, UFactory, M8 Render/Collision, Slate Dashboard).<br>Cahier des charges : [`addons/ADDONS_GUIDELINES.md`](file:///addons/ADDONS_GUIDELINES.md). Méta-cible `package_all_addons`. |
| **M11** | **Architecture de Plugins Qt6 & SDK** | Haute | Externalisation des 9 filtres et 6 extracteurs en modules `.so`/`.dll` dynamiques (`QPluginLoader`), CMake config exportable. |
| **M15** | **Refonte Drag & Drop Filmstrip** | Moyenne | `FilmstripListWidget` dédié, calcul linéaire du drop 1D, indicateur bleu contrasté, découplage transactionnel sans récursion destructrice. |
| **M-CLI** | **Interface CLI & Compatibilité Moteurs** | Moyenne | `bentopack-cli` avec mode drop-in TexturePacker, Aseprite (`-b`) et export natif Godot 4 avec conservation des UIDs. |
| **M-DEVOPS** | **Release CI Duale & Packaging Windows** | Moyenne | Pipelines `release-community.yml` et `release-commercial.yml`. Empaquetage autonome Windows NSIS/ZIP (`package.cmake`, windeployqt, runtimes MinGW). |
| **CH-TECH (1-8)** | **Audit, Refactor & Protection Légale** | Haute | Scission `BentoPackCore` / `BentoPackWidgets`, rebranding intégral sans relique, suppression des doublons, refonte en 15 suites de tests modulaires CTest, hygiène Git (.gitignore), CMI statutaire et double licence EULA anti-freeriding AAA. |

---

## 🎯 Feuille de Route Active (Chantiers Restants)

```
┌────────────────────────────────────────────────────────────────────────┐
│                        FEUILLE DE ROUTE ACTIVE                         │
├───────────────────┬───────────────────────────────────┬────────────────┤
│ Jalon             │ Thématique                        │ Priorité       │
├───────────────────┼───────────────────────────────────┼────────────────┤
│ STORES            │ Publication & Distribution Stores │ Haute (Imm.)   │
│ FORMATS           │ Formats d'Exportation Post-M10    │ Moyenne        │
│ M14               │ Optimisations SIMD & Grands Atlas │ Moyenne        │
│ M12               │ Rigging & Animation Squelettique  │ Moyenne (Opt.) │
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

### 🚀 Formats Cibles Post-M10 (Élargissement Industriel)

1. **Format Texte LibGDX / Spine (`.atlas`) — Priorité Moyenne :**
   - **Enjeu :** Standard clé-valeur textuel ultra-répandu dans les frameworks indés et légers (Raylib, Bevy Rust, MonoGame, Defold, LibGDX).
   - **Avantage :** Parsing trivial sans dépendance JSON, interopérabilité directe avec les runtimes Spine officiels.
   - **Complexité :** Faible (~150 lignes C++).
2. **Format Binaire Natif Aseprite (`.ase` / `.aseprite`) — Priorité UX & Confort :**
   - **Enjeu :** Ouvrir ou glisser-déposer directement un projet Aseprite dans BentoPack sans étape intermédiaire manuelle d'exportation vers JSON+PNG.
   - **Avantage :** Argument produit majeur pour la communauté des pixel-artists.
   - **Complexité :** Moyenne (décodage du format binaire ouvert Aseprite : calques, chunks d'images, tags, palettes).
3. **Format Apple / Cocos2d-x (`.plist` XML) — Priorité Basse :**
   - **Enjeu :** Compatibilité avec les pipelines historiques de jeux mobiles 2D (Cocos2d-x, SpriteKit iOS).
   - **Complexité :** Faible (sérialisation XML de dictionnaires/rectangles).

---

## ⚡ 3. M14 : Optimisations Hautes Performances SIMD & Gestion Mémoire Grands Atlas

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

## 🦴 4. M12 : Animation Squelettique & Découpe de Membres 2D (Rigging, Bones, Spine / Godot / Unity)

### 📌 Contexte & Enjeux
L'animation squelettique 2D découpe un personnage en éléments anatomiques distincts (tête, buste, membres), les rattache à une hiérarchie d'os (*Bones*), et anime les transformations pour produire des mouvements fluides à 60 ou 120 FPS avec un nombre réduit de textures.

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

> [!NOTE]
> **Orientation d'Architecture (M12) :**  
> Ce chantier doit être conçu comme un **module optionnel avancé / plugin détachable**, afin de préserver l'agilité du cœur de BentoPack centré sur l'empaquetage d'atlas et les maillages anti-overdraw.

---

## 🌐 5. M13 : Micro-Démonstrateur Web Vitrine (WebAssembly Showcase)

### 📌 Cadrage Stratégique
Le portage intégral de l'application de bureau en WebAssembly (Qt for WebAssembly) est écarté en raison du poids de téléchargement excessif (25 à 40 Mo) et du sandboxing navigateur.

### 💡 Solution Retenue :
Concevoir un **micro-démonstrateur web vitrine ultra-léger** (mini-module WebAssembly ou script TypeScript/Canvas) hébergé sur le site officiel de BentoPack. Il permettra aux visiteurs de glisser-déposer un sprite pour tester instantanément en direct la découpe automatique, le despill et l'aperçu d'animation, servant d'entonnoir d'acquisition vers l'application de bureau native.

---

## 📊 Matrice d'Exécution

| Phase | Horizon | Chantiers Clés | Livrables Attendus |
|---|---|---|---|
| **Phase 1** | **Court terme (Immédiat)** | **Stores & Distribution** | • Soumission officielle Godot AssetLib (`godot-bentopack-addon`)<br>• Enregistrement OpenUPM et soumission Unity Asset Store<br>• Soumission Unreal Fab (`unreal-bentopack-plugin`)<br>• Déploiement des pages Steam / Itch.io | Visibilité officielle sur tous les écosystèmes et premiers flux d'acquisition. |
| **Phase 2** | **Court terme** | **Formats Post-M10** | • Extracteur LibGDX / Spine (`.atlas`)<br>• Décodeur binaire direct Aseprite (`.ase` / `.aseprite`)<br>• Export XML `.plist` (Apple/Cocos2d) | Interopérabilité élargie avec l'écosystème indé et les pixel-artists. |
| **Phase 3** | **Moyen terme** | **M14 (Performances)** | • Vectorisation AVX2 / NEON des filtres graphiques les plus lourds<br>• Plafonnement mémoire `QUndoStack` & snapshotting différentiel LZ4 | Traitement temps réel des planches 8K/16K sous les 15 ms. |
| **Phase 4** | **Moyen/Long terme** | **M12 (Rigging) & M13 (Web)** | • Découpe de membres, armature 2D et export Spine JSON / Skeleton2D<br>• Micro-démonstrateur web vitrine sur le site officiel | Diversification fonctionnelle avancée et vitrine d'acquisition en ligne. |
