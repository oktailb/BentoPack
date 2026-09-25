# Documentation de l'API C++ — BentoPack

Bienvenue dans la documentation de l'API de **BentoPack**, un atelier complet de découpe, manipulation géométrique, filtrage et empaquetage d'atlas de sprites 2D pour le jeu vidéo.

## 🏛️ Architecture & Composants Principaux

### 1. Modèle de Données Pur (`BentoPackCore`)
- \ref SpriteDocument : L'unique source de vérité de l'application. Stocke l'atlas de base (`QImage`), la liste des frames découpées (`QList<QImage>`), les boîtes de délimitation (`QList<SpriteBox>`) et les séquences d'animation (`QList<SpriteAnimation>`).
- \ref SpriteBox : Structure représentant une boîte englobante de frame avec rectangle $X, Y, W, H$, configuration de point d'ancrage / pivot (`QPoint`) et maillage polygonal 2D (`QPolygonF vertices`, indices de triangulation, mode de maillage).
- \ref SpriteAnimation : Structure décrivant une séquence d'animation cadencée (nom, FPS, mode de boucle `Loop / Once / PingPong`, liste des index de frames).

### 2. Codecs d'Import / Export (`Extractor`)
- \ref Extractor : Interface abstraite de codec I/O pur.
- \ref ExtractorRegistry : Singleton gérant l'enregistrement et la détection automatique du codec approprié pour un fichier donné.
- Codecs disponibles :
  - \ref SpriteExtractor : Découpage automatique d'images PNG/JPEG/BMP via flood-fill et composantes connexes.
  - \ref GifExtractor : Décodage et ré-assemblage d'animations GIF.
  - \ref JsonExtractor : Import / Export au format standard TexturePacker et Aseprite JSON.
  - \ref GodotExtractor : Génération de ressources Godot 4 `SpriteFrames` (`.tres`) avec marges de pivots précises.
  - \ref UnityExtractor : Export des métadonnées pour Unity 2D (`.unity.json`) et maillages serrés.
  - \ref UnrealExtractor : Export des métadonnées Paper2D pour Unreal Engine (`.paper2d.json`).

### 3. Moteurs d'Empaquetage 2D (`AtlasPacker`)
- \ref AtlasPacker : Orchestrateur principal de packing d'atlas (gestion des options POT, extrusion de bordure, padding anti-saignement).
- \ref MaxRectsPacker : Algorithme de bin-packing 2D avec heuristiques *Best Short Side Fit* (BSSF) et *Best Area Fit* (BAF).
- \ref TightPolygonPacker : Algorithme d'empaquetage polygonal haute densité avec multithreading et optimisation des points d'ancrage.

### 4. Géométrie & Maillage 2D (`BentoPackGeometry`)
- \ref BentoPackGeometry::ContourTracer : Extraction de contours étanches par l'algorithme *Marching Squares* 2D.
- \ref BentoPackGeometry::PolygonSimplifier : Simplification de contours par *Ramer-Douglas-Peucker* (RDP) avec dilatation normale et budget de sommets.
- \ref BentoPackGeometry::Triangulator : Décomposition de polygones en triangles par *Ear-Clipping* et calcul des gains d'overdraw GPU (Shoelace formula).

### 5. Système de Filtres Graphiques (`FilterPlugin`)
- \ref FilterPlugin : Interface abstraite de filtre modulaire.
- \ref FilterRegistry : Registre singleton et générateur dynamique de menus.
- \ref FilterDialogBase : Dialogue flottant non-bloquant avec Live Preview (debounce 80 ms) et rollback fidèle.
- Filtres intégrés : \ref BackgroundRemovalFilter, \ref DespillFilter, \ref OutlineFilter, \ref ColorSwapFilter, \ref ColorAdjustFilter, \ref PixelRescaleFilter, \ref RetroPaletteFilter.

### 6. Atelier d'Édition Pixel par Pixel
- \ref PixelCanvas : Widget de canevas graphique haute précision avec tracé de Bresenham 1px, gomme, pipette, seau, tampon flottant et zoom $100\%-6400\%$.
- \ref PixelEditorDialog : Boîte de dialogue d'édition avec palettes rétro et navigation inter-frames.

### 7. Interface Ligne de Commande Headless (`bentopack-cli`)
- \ref BentoPackCli::CliParser : Dispatcher multi-saveurs détectant l'émulation TexturePacker, Aseprite ou le mode natif.
- \ref BentoPackCli::TexturePackerAdapter : Émulateur 100% compatible avec les flags TexturePacker CLI.
- \ref BentoPackCli::AsepriteAdapter : Émulateur des options de feuilles de sprites d'Aseprite (`-b`, `--sheet`, `--data`).
- \ref BentoPackCli::GodotPipeline : Génération directe de ressources Godot 4 avec préservation des UIDs.

---

## 📚 Guides Associés
- Pour le guide d'extension développeur (nouveaux codecs, plugins de filtres) : voir `docs/DEVELOPER_GUIDE.md`.
- Pour le manuel utilisateur complet illustré : voir `docs/USER_GUIDE.md`.
