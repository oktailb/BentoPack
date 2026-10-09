# 🗺️ ROADMAP Stratégique & Spécifications Métier — BentoPack

> **Version de référence :** BentoPack Studio 0.11.x  
> **Statut global :** Socle logiciel complet (M0 à M11, M15, M17, M18, M19, M20 validés avec 100% de tests unitaires réussis). Addons moteurs Godot 4, Unity 6 et Unreal Engine 5 packagés et prêts à la distribution.  
> **Ligne directrice :** Focalisation sur la valeur ajoutée concrète pour les studios indépendants et pixel-artists, préservation de performances extrêmes, et refus catégorique des usines à gaz hors-scope.

---

## 🏛️ Historique des Jalons Clôturés & Validés (M0 — M20)

| Jalon | Intitulé | Portée & Livrables Validés |
|---|---|---|
| **M0** | **Assainissement Architectural** | Modèle unique `SpriteDocument`, découplage en 3 contrôleurs, RAII strict, scanlines contiguës, i18n trilingue (FR/EN/JA). |
| **M1** | **Édition Interactive Bounding Boxes** | 8 poignées interactives, déplacement groupé, outil découpe rapide (`Shift`), trim alpha automatique, effacement destructif. |
| **M2** | **Timeline & Gestionnaire d'Animations** | Ruban filmstrip fluide, modes Loop/Once/Ping-Pong, scrubber interactif, persistance `QSettings` des docks. |
| **M3** | **Points d'Ancrage & Pivots** | Réticule direct sur atlas/aperçu, 9 presets cardinaux, enveloppe d'animation anti-jittering, export des offsets. |
| **M4** | **Éditeur de Pixels Chirurgical** | Tracé Bresenham 1px, pipette, seau de remplissage, sélections rectangle/baguette magique, 7 palettes rétro authentiques. |
| **M5** | **Format Projet Natif `.bento`** | Archive ZIP compressée (Miniz), verrouillage `.session_lock`, crash recovery, time-travel Git intégré (LibGit2). |
| **M6** | **Empaquetage Avancé MaxRects** | Heuristiques BSSF, BAF, BLSF, BottomLeft, ContactPoint, contrainte POT ($2^n$), extrusion bordure anti-bleeding, déduplication de frames. |
| **M7** | **Filtres Graphiques & Traitement** | 9 plugins modulaires (Chroma BG, Despill, Outline, ColorSwap, ColorAdjust, RetroPalette, Rescale, MaxRects, TightPacking). |
| **M8** | **Empaquetage Polygonal & Maillages Serrés** | Marching Squares étanche, RDP, triangulation Ear-Clipping, réduction de 60-80% d'overdraw GPU, export Unity Tight, Godot ArrayMesh & Paper2D. |
| **M9** | **Compression de Textures VRAM (KTX2)** | Khronos `basis_universal` v2.50, modes UASTC 4x4 et ETC1S, Zstd niveaux 1-22, téléversement direct GPU sans décompression CPU. |
| **M10** | **Addons Moteurs (Godot, Unity, Unreal)** | **Godot 4 :** `godot-bentopack-addon` (AssetLib zip).<br>**Unity 6 :** `com.bentopack.importer` (UPM tgz, Tight Mesh, AnimationClips).<br>**Unreal 5 :** `BentoPack UE5 Plugin` (Fab zip, Render/Collision Geometry). |
| **M11** | **Architecture de Plugins Qt6 & SDK** | Externalisation des filtres et extracteurs en bibliothèques dynamiques (`QPluginLoader`), CMake config exportable. |
| **M15** | **Refonte Drag & Drop Filmstrip** | `FilmstripListWidget` dédié, drop linéaire 1D, indicateur bleu contrasté, découplage transactionnel sans récursion. |
| **M17** | **Calques Aseprite & Variantes Skin** | Structures `SpriteLayer` et `SpriteCel`, décodage/encodage binaire Aseprite complet (`CHUNK_LAYER`, `CHUNK_CEL` raw/zlib), profils de skins modulaires. |
| **M18** | **Édition de Pixels Multi-Calques** | Dock `LayerStackWidget` complet dans `PixelEditorDialog` (Z-order, opacité, modes de composition, merge, flatten), dessin multi-couches, onion skinning. |
| **M19** | **Maillage Intelligent CDT & Relief** | Triangulation Delaunay avec contraintes (CDT Bowyer-Watson), détection de crêtes chromatiques internes (RGB / Sobel), outils interactifs de sommets & couteau laser, application à toutes les frames d'animation. |
| **M20** | **Taxonomie Filtres & Pixel Editor** | Typage déclaratif `FilterModifierFlags` (`PixelModifier`, `GeometryModifier`, `AtlasModifier`), intégration directe des filtres dans le Pixel Editor avec live preview et Undo/Redo. |
| **M-CLI** | **CLI & Mode Watch Daemon** | `bentopack-cli` avec mode daemon de surveillance en arrière-plan (`--watch`, détection récursive, debouncing, isolation `.lock`), drop-in TexturePacker. |
| **M-DEVOPS**| **CI/CD Multi-Plateforme** | Pipelines automatisés Windows (NSIS & ZIP), Linux (AppImage, DEB, RPM), macOS (ARM64/Intel DMG & .app), Haiku OS (`.hpkg`). |

---

## 🎯 Ordre de Priorité des Chantiers Futurs

```
┌────────────────────────────────────────────────────────────────────────┐
│                      FEUILLE DE ROUTE STRATÉGIQUE                      │
├─────┬───────────────────┬───────────────────────────────────┬──────────┤
│ Rang│ Jalon             │ Thématique                        │ Priorité │
├─────┼───────────────────┼───────────────────────────────────┼──────────┤
│ 1   │ STORES            │ Publication & Distribution Stores │ P0 - Imm.│
│ 2   │ M21 (LIGHTING)    │ Multi-Atlas Normal/Emissive Maps  │ P1 - Trés│
│ 3   │ M16 (MULTI-PAGE)  │ Multi-Page Atlas (Spanning VRAM)  │ P1 - Fort│
│ 4   │ M22 (VRAM-DEDUP)  │ Déduplication Cels & Delta-Patches│ P2 - Moy.│
│ 5   │ GUI-WATCH         │ Mode Hot-Reload Zéro-Clic (GUI)   │ P2 - Moy.│
│ 6   │ FILTERS-POLISH    │ Fignolage Extrusions & Filtres    │ P2 - Moy.│
│ 7   │ M12 (SUR-ANIM)    │ Sur-Animation (Weights & Sockets) │ P3 - Moy.│
│ 8   │ M14 (SIMD)        │ Vectorisation AVX2/NEON & Undo RAM│ P3 - Tech│
│ 9   │ FORMATS & SHOWCASE│ Export Apple .plist & Démo Web    │ P4 - Bas │
└─────┴───────────────────┴───────────────────────────────────┴──────────┘
```

---

## 🏪 Priorité 1 (P0) : Publication & Distribution sur les Stores Officiels

### 📌 Contexte & Objectif
Le code source, les binaires multi-plateformes et les trois packages d'addons moteurs sont terminés et validés par les tests d'intégration. La priorité absolue est de rendre BentoPack accessible là où travaillent les développeurs de jeux.

### 💰 Valeur Ajoutée Produit
* Passage du statut de « projet GitHub » à celui de **solution de référence packagée et reconnue**.
* Adoption organique immédiate par les créateurs Godot, Unity et Unreal via leurs gestionnaires de paquets natifs.

### 🛠️ Spécifications d'Exécution
1. **Godot Asset Library (`godot-bentopack-addon`) :**
   - Dépôt de l'archive ZIP sur l'AssetLib officielle (catégorie *2D Tools*, tags : `spritesheet`, `mesh`, `anti-overdraw`).
   - Guide d'intégration 1-clic pour Godot 4.3+.
2. **Unity Package Manager (UPM) & Unity Asset Store :**
   - Publication du package `com.bentopack.importer` sur le registre communautaire OpenUPM.
   - Soumission de la fiche Asset Store (catégorie *2D Tools / Textures & Sprites*).
3. **Epic Games Fab (`fab.com`) :**
   - Soumission du plugin Unreal Engine 5 `unreal-bentopack-plugin` compatible UE 5.3 à 5.5+.
4. **Distribution Desktop Autonome (Steam & Itch.io) :**
   - Fiches produit prêtes à l'emploi basées sur [`docs/STORE_DESCRIPTIONS.md`](file:///docs/STORE_DESCRIPTIONS.md).
   - Packaging NSIS/ZIP (Windows), AppImage autonome (Linux/SteamDeck), DMG universel (macOS).

### ⚠️ Risques & Écueils
* Rejet lors des revues éditeurs pour métadonnées incomplètes ou absence de captures d'écran calibrées.
* Divergence de versionnement entre l'application de bureau et les addons moteurs.

### 🧭 Conseil Senior : Must-Haves vs Barrières à Ne Pas Franchir
> [!IMPORTANT]
> **Must-Have :** Valider l'installation depuis une machine vierge en suivant rigoureusement la checklist de [`docs/STORE_DESCRIPTIONS.md`](file:///docs/STORE_DESCRIPTIONS.md).  
> 🚫 **Barrière à ne pas franchir :** Ne pas tenter de créer un compte éditeur payant personnalisé sur des plateformes exotiques avant d'avoir validé l'afflux d'utilisateurs sur Godot AssetLib, OpenUPM et GitHub Releases.

---

## 💡 Priorité 2 (P1) : M21 — Éclairage 2D & Multi-Atlas Synchronisé (Normal, Emissive, Specular)

### 📌 Contexte & Problématique Métier
La 2D contemporaine (Unity URP 2D, Godot 4 `CanvasTexture`, Unreal Paper2D) s'appuie massivement sur l'éclairage dynamique (point lights, torches, ombres en temps réel).
* Les artistes créent plusieurs calques par sprite : Albedo (diffuse), Normal Map (tangent-space), Emissive (glow) et Specular (rugosité).
* **Le point de friction majeur :** Si les planches de normales ou d'émissif sont packagées séparément, le rognage (*trim*), la rotation ou l'optimisation MaxRects modifient le positionnement d'une texture à l'autre. Les coordonnées UV entre la couleur et la normal map se désalignent complètement, détruisant l'éclairage dans le moteur.

### 💰 Valeur Ajoutée Produit
* Économise des heures de travail méticuleux et fastidieux à un Technical Artist.
* Argument de vente déterminant auprès des studios professionnels réalisant des metroidvanias ou RPGs modernes.

### 🛠️ Spécifications Techniques
1. **Convention de Naming Automatique :**
   - Détection des calques Aseprite ou fichiers suffixés selon les conventions de l'industrie :
     - `_n` ou `_normal` : Texture de normales tangent-space.
     - `_e` ou `_emissive` : Texture émissive (Auto-illumination / Glow).
     - `_s` ou `_specular` : Texture de spécularité / rugosité.
2. **Empaquetage Maître-Esclave (*Master-Slave Atlas Packing*) :**
   - **Atlas Maître (Albedo) :** Calcule l'arrangement optimal (MaxRects ou maillage polygonal M8/M19, rotation 90°, trimming alpha et marges d'extrusion).
   - **Atlas Esclaves (Normal, Emissive, Specular) :** Instanciés aux mêmes dimensions. BentoPack réplique **strictement les mêmes rectangles, découpes, rotations et triangles UV**.
   - **Remplissage neutre des paddings :**
     - Normal Maps : Bleu neutre tangent-space `RGBA(128, 128, 255, 255)` (vecteur $(0, 0, 1)$).
     - Emissive & Specular : Noir transparent `RGBA(0, 0, 0, 0)`.
3. **Liaison Moteur Automatique :**
   - **Godot 4 (`.tres`) :** Génération d'une ressource native `CanvasTexture` associant automatiquement `diffuse_texture`, `normal_texture` et `specular_texture`.
   - **Unity 6 (`.unity.json`) :** Enregistrement des textures secondaires dans les métadonnées pour liaison directe au shader URP Sprite-Lit.
   - **Unreal 5 (`.paper2d.json`) :** Injection des slots de textures dans l'instance de matériau Paper2D.

### ⚠️ Risques & Écueils
* Bords de sprites étirés produisant des artefacts de normale sur les zones de padding extrudé.
* Normal maps inversées selon l'orientation de l'axe Y (Godot vs Unity/Unreal).

### 🧭 Conseil Senior : Must-Haves vs Barrières à Ne Pas Franchir
> [!TIP]
> **Must-Have :** Fournir une option de bascule claire pour inverser le canal vert (Y-Flip) de la Normal Map selon le moteur cible.  
> 🚫 **Barrière à ne pas franchir (Anti-Usine à Gaz) :** **Ne PAS coder de générateur procédural de normal maps** (estimations de reliefs par filtres Sobel ou conversion hauteur-vers-normales dans BentoPack). Des logiciels tiers spécialisés (Laigter, Sprite Illuminator) font déjà cela très bien. BentoPack doit rester le roi incontesté du packaging, de la synchronisation géométrique et de l'export moteur sans perte.

---

## 🗂️ Priorité 3 (P1) : M16 — Multi-Page Atlas & Spanning (Gestion du Débordement VRAM)

### 📌 Contexte & Problématique Métier
En production réelle (RPG avec des centaines de monstres, personnages avec des dizaines d'animations), un projet dépasse rapidement les limites de taille d'une seule texture (ex. contrainte stricte de $2048 \times 2048$ sur mobile bas de gamme ou $4096 \times 4096$ sur Switch).
* À ce jour, le packing échoue ou tronque les sprites si la surface totale excède la texture maximale.

### 💰 Valeur Ajoutée Produit
* Permet d'importer et d'empaqueter de gigantesques banques d'animations sans devoir découper manuellement son projet en plusieurs fichiers `.bento`.

### 🛠️ Spécifications Techniques
1. **Algorithme de Partitionnement Multi-Pages :**
   - Découpage ordonné en pages successives (`page_0.png`, `page_1.png`, etc.) respectant la taille maximale par page et la contrainte Power of Two ($2^n$).
   - **Heuristique de cohésion sémantique :** Regrouper en priorité les frames d'une même animation sur la même page d'atlas afin d'éviter les changements de texture GPU (*texture swaps / state switches*) en plein milieu d'une séquence.
2. **Mise à Jour des Descripteurs Moteurs :**
   - **Godot 4 :** Plusieurs `CompressedTexture2D` déclarées dans le `.tres`, chaque `AtlasTexture` pointant vers l'ID de sa page.
   - **Unity :** Indexation de la texture source par sprite dans `.unity.json`.
   - **Unreal Engine 5 :** Référencement de la texture parente pour chaque `RenderGeometry`.
   - **LibGDX / Spine (`.atlas`) :** Déclaration native des blocs de pages successifs.

### ⚠️ Risques & Écueils
* Explosion du nombre de pages si l'algorithme glouton découpe mal les grands sprites.
* Risque de texture thrashing si les frames d'une même animation sont éparpillées sur plusieurs pages.

### 🧭 Conseil Senior : Must-Haves vs Barrières à Ne Pas Franchir
> [!IMPORTANT]
> **Must-Have :** Règle stricte dans le partitionneur : « Une animation complète ne doit jamais être coupée sur deux pages sauf si elle dépasse physiquement la surface d'une page entière ».  
> 🚫 **Barrière à ne pas franchir :** Ne pas concevoir un gestionnaire d'atlas virtuel avec streaming dynamique de sous-tuiles. La gestion du chargement mémoire en jeu appartient au moteur de jeu (Godot/Unity/UE), pas à l'outil d'atlas.

---

## ✂️ Priorité 4 (P2) : M22 — Déduplication Modulaire par Cels & Deltas (Trim & Link VRAM)

### 📌 Contexte & Problématique Métier
Dans les animations de personnages (idle, respiration, clignement des yeux) ou d'objets (torche allumée), plus de 80% des pixels de l'image restent immobiles entre les frames. Les stocker intégralement à chaque frame dévore de la surface d'atlas et de la mémoire GPU inutilement.

### 💰 Valeur Ajoutée Produit
* Réduction de 30% à 50% de la surface d'atlas requise sur les longues boucles d'animation.
* Double argument commercial : réduction drastique de l'overdraw GPU et compression mémoire VRAM.

### 🛠️ Spécifications Techniques
1. **Déduplication des Cels Liés Aseprite (*Linked Cels*) :**
   - Exploitation de la structure de calques M17/M18.
   - Hachage 64 bits ultra-rapide (`XXH3`) de chaque cel au chargement.
   - Si un cel est répété (ex. corps immobile pendant que seuls les yeux changent sur un calque supérieur), le cel est écrit **une seule fois** dans l'atlas. Les UVs des différentes frames pointent vers cette région partagée.
2. **Extraction Différentielle de Sous-Rectangles (*Delta-Patch Slicing*) :**
   - Détection des pixels modifiés entre une frame et sa référence.
   - Si la surface modifiée est faible ($\le 25\%$ de l'enveloppe du sprite) :
     - Isolation de la *dirty bounding box* sous forme de patch léger.
     - Sauvegarde d'un décalage de pivot relatif $(dx, dy)$ pour reconstitution exacte dans le moteur.
3. **Déduplication de Frames Complètes :**
   - Mutualisation complète du rectangle d'atlas et du maillage polygonal pour les frames strictement identiques.

### ⚠️ Risques & Écueils
* Complexité accrue du runtime si les moteurs doivent gérer des compositions multi-sprites par frame.

### 🧭 Conseil Senior : Must-Haves vs Barrières à Ne Pas Franchir
> [!WARNING]
> **Arbitrage Senior Critique — Le Piège de la Découpe en Micro-Tuiles :**  
> ❌ **À REJETER ABSOLUMENT :** Découper arbitrairement chaque frame en une mosaïque de micro-blocs (ex. tuiles 8x8 px). En théorie, la VRAM diminue légèrement. En pratique, le nombre de polygones, de quads et de **draw calls GPU explose**, anéantissant les performances d'affichage du jeu vidéo.  
>  **La Voie Royale :** Dédupliquer au niveau sémantique des **Cels Aseprite** et des **Delta-Patches rectangulaires**. Zéro surcharge de draw calls inutiles.

---

## ⏱️ Priorité 5 (P2) : GUI-WATCH — Mode Hot-Reload Zéro-Clic Intégré dans l'Interface

### 📌 Contexte & Spécifications
Le mode daemon CLI (`bentopack-cli --watch`) est déjà opérationnel. Pour les artistes qui n'utilisent pas le terminal de commande, intégrer ce mécanisme directement dans l'interface de bureau transforme radicalement l'expérience utilisateur.

### 💰 Valeur Ajoutée Produit
* Ergonomie « zéro clic » : l'artiste fait `Ctrl+S` dans Aseprite, et voit son animation se rafraîchir en quelques millisecondes directement dans la fenêtre de test de son jeu sous Godot ou Unity.

### 🛠️ Spécifications Techniques
1. Action à bascule dans la barre d'outils et le menu principal : *« Mode Surveillance / Live Watch (Ctrl+Alt+W) »*.
2. Exploitation de `QFileSystemWatcher` avec filtre sur les extensions surveillées (`.ase`, `.aseprite`, `.png`).
3. Délai anti-rebond (*debouncing*) de 250 ms pour laisser Aseprite terminer d'écrire son binaire sur disque.
4. Rechargement transparent, re-packing, recalcul du maillage et écriture des descripteurs moteurs en tâche de fond.
5. Icône d'état discrète dans la barre d'état (En écoute / Recompilation / Prêt).

### 🧭 Conseil Senior : Must-Haves vs Barrières à Ne Pas Franchir
> [!TIP]
> **Must-Have :** Vérifier l'existence d'un verrou ou d'une absence d'erreur d'ouverture de fichier avant de lire pour éviter de lire un binaire à moitié écrit par l'OS.  
> 🚫 **Barrière à ne pas franchir :** Ne pas bloquer le thread principal de rendu Qt pendant la recompilation. Tout recalcul doit s'exécuter dans un worker thread sans figer l'interface.

---

## 🎨 Priorité 6 (P2) : FILTERS-POLISH — Fignolage des Extrusions & Filtres

### 📌 Contexte & Spécifications
L'extrusion de bordure (padding) évite le *bleeding* (bavure de pixels voisins sous filtrage bilinéaire ou mipmapping).
1. **Angles 8-connectés (Corners Clamping) :** Dupliquer les pixels sur les diagonales extérieures des coins pour éviter les pixels noirs ou transparents lors d'un échantillonnage oblique.
2. **Épaisseur paramétrable :** 1 à 4 pixels selon le niveau de mipmapping ciblé.
3. **Filtre Despill CIELAB :** Calcul de distance colorimétrique dans l'espace perceptuel CIELAB pour éliminer les halos de fond vert/bleu sans décolorer les teintes voisines.

---

## 🦴 Priorité 7 (P3) : M12 — Sur-Animation 2D & Métadonnées de Déformation

### 📌 Contexte & Arbitrage Senior
Les développeurs souhaitent apporter de la vie à leurs sprites 2D en les animant avec des **Vertex Shaders** (mouvement de vent dans les cheveux, flottaison d'une cape, respiration, secousse d'impact).
* BentoPack ne doit pas devenir un éditeur d'animation squelettique concurrent de Spine ou DragonBones (hors-scope complet).
* En revanche, BentoPack est l'outil idéal pour générer et injecter les **métadonnées géométriques** nécessaires aux shaders.

### 🛠️ Spécifications Techniques
1. **Poids de Sommets & Peinture d'Influence (*Vertex Colors / Wind Weights*) :**
   - Attribut de poids normalisé $[0.0, 1.0]$ par sommet dans `SpriteBox`.
   - Outil pinceau d'influence dans le Pixel Editor avec aperçu thermique (bleu = fixe, rouge = mobile).
   - Export immédiat dans les attributs de sommets : `ArrayMesh::ARRAY_COLOR` (Godot), `Sprite.OverrideGeometry.colors` (Unity), Vertex Data Paper2D (Unreal).
   - *Consommation immédiate :* Un shader de vent Godot fonctionne sans aucune configuration supplémentaire.
2. **Points d'Accroche (*Sockets / Attach Pins*) :**
   - Coordonnées nommées (ex. `socket_hand`, `socket_weapon`, `socket_hat`) synchronisées par frame d'animation pour épingler des armes ou des émetteurs de particules.
3. **Grilles Régulières Déformables :**
   - Génération d'une triangulation régulière $M \times N$ pour bannières, drapeaux et surfaces liquides.

### 🧭 Conseil Senior : Must-Haves vs Barrières à Ne Pas Franchir
> [!WARNING]
> 🚫 **Barrière à ne pas franchir :** **Ne JAMAIS implémenter de système de squelette 2D interne avec rigging IK et déformation matricielle complexe.** Ce serait une dérive colossale de scope. Restons concentrés sur les métadonnées géométriques consommables par les shaders des moteurs.

---

## ⚡ Priorité 8 (P3) : M14 — Optimisations SIMD (AVX2 / NEON) & Mémoire Grands Atlas

### 📌 Contexte & Spécifications
Pour garantir la fluidité sur des atlas de très haute résolution (8K, 16K) :
1. **Vectorisation SIMD (AVX2 / NEON) :** Accélération des filtres graphiques les plus coûteux (`DespillFilter`, `OutlineFilter`, `ColorSwapFilter`) en traitant 8 pixels 32 bits par cycle AVX2 et 4 pixels par cycle NEON.
2. **Snapshotting Différentiel Undo (*Dirty Rects*) :** Mémoriser uniquement le sous-rectangle modifié dans l'Éditeur de Pixels plutôt que de cloner l'image entière de l'atlas à chaque coup de pinceau (évite de saturer la mémoire vive).

---

## 🌐 Priorité 9 (P4) : Formats Secondaires & Démonstrateur Web Vitrine

1. **Format Apple / Cocos2d-x (`.plist` XML) :** Pour les pipelines historiques iOS / Cocos2d-x (complexité faible).
2. **Micro-Démonstrateur Web Vitrine (Showcase WASM) :** Petit module WebAssembly ultra-léger sur le site officiel pour tester le rognage et le despill directement dans le navigateur, servant d'entonnoir d'acquisition vers l'application de bureau.

---

## 🛡️ Synthèse des Garde-Fous & Règles de Décision Senior

| Domaine |  À Réaliser en Priorité (Must-Have) | 🚫 À Rejeter Absolument (Usine à Gaz) |
|---|---|---|
| **Éclairage 2D** | Multi-atlas synchronisé maître-esclave, liaison `CanvasTexture` / URP. | Générateur procédural interne de normal maps. |
| **Optimisation VRAM** | Déduplication sémantique des *Linked Cels* et des *Delta-Patches*. | Découpage aveugle en micro-tuiles (explose les draw calls GPU). |
| **Sur-Animation** | Poids de sommets pour vertex shaders et sockets d'attache d'accessoires. | Moteur d'animation squelettique 2D concurrent de Spine. |
| **Grands Atlas** | Partitionnement multi-pages avec respect de la cohésion des animations. | Moteur de streaming virtuel de sous-tuiles. |
| **Hot-Reload** | Watcher en tâche de fond avec debouncing et ré-export sans clic. | Modification directe de la mémoire vive interne du moteur de jeu. |
