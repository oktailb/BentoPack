# 🍱 BentoPack — Cahier des Charges & Guidelines des Addons Moteurs
### Spécifications Fonctionnelles, Architecturales et Standards d'Intégration

Ce document définit les exigences, comportements, architecture technique et livrables attendus pour tout plugin/addon d'intégration de **BentoPack** dans un moteur de jeu (Godot 4, Unity, Unreal Engine 5, Defold, Bevy, Raylib, Phaser, etc.).

Il sert de **cahier des charges de référence** pour garantir l'uniformité ergonomique, la conformité technique et le respect de la proposition de valeur de BentoPack sur tous les écosystèmes cibles.

---

## 1. 🎯 Vision Produit & Philosophie d'Intégration

### 1.1. Le Rôle de l'Addon : Consommateur Optimal & Passerelle
Un addon BentoPack a deux rôles fondamentaux et complémentaires :
1. **Être le Consommateur Optimal :** Fournir une expérience d'importation sans aucune friction (*zéro configuration*) et injecter les optimisations exclusives de BentoPack (notamment les **maillages serrés M8** réduisant de 60% à 80% l'overdraw GPU, et la synchronisation des hitboxes 2D).
2. **Être la Passerelle vers BentoPack Studio :** Permettre en un clic d'ouvrir l'asset dans l'application de bureau officielle et recharger instantanément les modifications dans le moteur via le démon de surveillance (*Watch Daemon*).

> [!IMPORTANT]
> **Anti-Pattern Absolu :** Un addon de moteur ne doit **JAMAIS** tenter de réimplémenter un logiciel d'animation 2D concurrent dans l'inspecteur. BentoPack Studio est l'outil de création et de cadencement officiel. L'addon se concentre sur l'intégration transparente, la prévisualisation dans le niveau et la performance moteur.

### 1.2. Le Principe de la Source Unique de Vérité (*Single Source of Truth*)
Le fichier projet `.bento` (ou le descripteur exporté) constitue l'**autorité absolue**.
- Toute modification de structure, de découpage, de cadence (FPS), de bouclage ou d'ordre de frames doit être effectuée et persistée dans BentoPack Studio.
- L'addon ne doit jamais diverger silencieusement de la source `.bento` afin d'éviter tout écrasement destructeur lors d'une sauvegarde externe.

---

## 2. 📦 Format d'Entrée : L'Archive `.bento`

Chaque addon doit savoir parser nativement le format conteneur `.bento` (archive ZIP standard décompressée en mémoire ou sur disque temporaire) comprenant :

1. **`project.json`** : Métadonnées complètes du projet :
   - Dimensions du canvas et métriques d'empaquetage.
   - Format de l'atlas texture (`webp`, `png`, `ktx2`, `basis`).
   - Tableau `sprites` :
     - `name` : Identifiant unique du sprite.
     - `rect` : Rectangle source dans l'atlas (`x`, `y`, `w`, `h`).
     - `pivot` : Point de pivot normalisé (`x`, `y` entre `0.0` et `1.0`).
     - `tight_mesh` : Maillage polygonal M8 (sommets `vertices`, triangles `indices`, coordonnées de texture `uvs`).
     - `collision_polygon` : Polygone de contour de la frame pour la physique 2D.
   - Tableau `animations` :
     - `name` : Nom de l'animation (`idle`, `walk`, `attack`, etc.).
     - `fps` : Cadence d'échantillonnage en images par seconde.
     - `loop` : Booléen de répétition cyclique.
     - `frames` : Liste ordonnée des indices de sprites composant l'animation.
2. **Image Atlas (`atlas.webp` ou `atlas.png`)** : Texture compilée contenant l'ensemble des sprites du projet.

---

## 3. ⚙️ Exigences Fonctionnelles Obligatoires (Core Requirements)

Tout addon BentoPack doit implémenter les 8 piliers fonctionnels suivants :

### REQ-1 : Importation Zéro-Configuration (*Zero-Config Importer*)
- **Détection Automatique :** L'addon enregistre un importateur natif auprès du moteur (`ScriptedImporter` Unity, `EditorImportPlugin` Godot, `UFactory` Unreal) associé à l'extension `.bento`.
- **Extraction des Sous-Assets :** Dès qu'un fichier `.bento` est glissé dans le projet, l'addon génère automatiquement :
  - La texture d'atlas paramétrée pour le pixel art (`FilterMode.Point` / `Nearest`, `WrapMode.Clamp`).
  - L'ensemble des sprites individuels avec découpe précise et point de pivot exact.
  - La ressource ou scène compagnon prête à l'emploi.

### REQ-2 : Injection des Maillages Serrés M8 (*Tight Mesh Anti-Overdraw*)
- **Objectif :** Réduire le coût de fillrate GPU causé par les pixels transparents (typiquement 60% à 80% de bande passante VRAM économisée).
- **Implémentation :**
  - **Unity :** Injection directe dans le `Sprite` standard via `Sprite.OverrideGeometry(vertices, triangles, uvs)`. Fonctionne nativement sur `SpriteRenderer` sans shader sur mesure.
  - **Godot :** Génération de ressources `ArrayMesh` 2D natives appliquées sur un nœud dédié `BentoMeshSprite` (`MeshInstance2D`).
  - **Unreal :** Définition de la `RenderGeometry` polygonale sur le `UPaperSprite` / `UPaperFlipbook`.
- **Repères Spatiaux :** Conversion scrupuleuse des repères (ex: inversion de l'axe Y pour Unity/DirectX vs Godot/OpenGL).

### REQ-3 : Hitboxes & Collisions 2D Dynamiques
- L'addon doit générer une forme de collision adaptée à chaque frame à partir de `collision_polygon`.
- **Unity :** Injection via `Sprite.OverridePhysicsShape()` sur le sprite et/ou synchronisation temps réel sur un composant `PolygonCollider2D`.
- **Godot :** Création d'un nœud `Area2D` / `CollisionPolygon2D` mis à jour dynamiquement ou tracé via une piste d'animation `AnimationPlayer`.
- Prise en charge des miroirs horizontaux et verticaux (`flip_h`, `flip_v`) avec inversion géométrique correcte du polygone de collision.

### REQ-4 : Animations & Cadence Moteur
- **Génération Native :**
  - **Unity :** Création d'`AnimationClip` discrets échantillonnés avec le FPS exact et loop-time configuré.
  - **Godot :** Génération d'une ressource `SpriteFrames` ET d'une `AnimationLibrary` pour `AnimationPlayer`.
- Respect absolu des propriétés `loop: false` avec émission de signaux/événements d'achèvement (`animation_finished`).

### REQ-5 : Tableau de Bord Éditeur Dédié (*Dashboard / Dock*)
L'addon doit fournir une interface centrale accessible en permanence (`Window -> BentoPack Dashboard` ou panneau dock inférieur) permettant :
1. De sélectionner une image/atlas source ou un dossier de frames, et un fichier `.bento` cible.
2. De choisir le mode de découpe :
   - *Découpe Automatique (Silhouettes & Transparence)*.
   - *Grille Fixe (Tuiles régulières avec dimensions W / H en pixels)*.
   - *Dossier d'images individuelles*.
3. De sélectionner l'algorithme d'empaquetage (*MaxRects BSSF* ou *Maillage Serré M8*).
4. D'afficher l'état de détection de la CLI BentoPack (vert si présente, rouge avec lien direct de téléchargement si absente).
5. D'ouvrir BentoPack Studio en 1 clic via `BentoCliBridge`.

### REQ-6 : Démon de Surveillance (*Watch Daemon Hot-Reload*)
- L'addon doit permettre de démarrer et stopper le processus d'arrière-plan `bentopack-cli --watch`.
- Lorsqu'une modification est enregistrée dans BentoPack Studio :
  - La CLI re-compile l'archive `.bento`.
  - L'addon détecte la mise à jour sur le disque et réimporte immédiatement les assets dans la scène ouverte sans redémarrage de l'éditeur.

### REQ-7 : Contrôleur Interactif de Scène & Inspecteur
Quand un nœud ou composant de personnage BentoPack est sélectionné dans la scène du moteur, l'inspecteur doit afficher :
- Un **sélecteur déroulant** pour changer d'animation à la volée.
- Une **barre de transport interactive** :
  - Bouton **▶ Lecture / ⏸ Pause** fonctionnant en direct dans l'éditeur (*sans avoir à lancer le mode jeu*).
  - Bouton **⏹ Arrêt** (remise à zéro de la frame).
  - Boutons **⏮ Image Précédente** et **⏭ Image Suivante**.
- Un **curseur de défilement (Frame Scrubber)** permettant de balayer manuellement l'animation et de constater la synchronisation visuelle du maillage serré et de la hitbox.
- Un badge confirmant l'état de l'anti-overdraw et de la synchronisation de collision.
- Un bouton d'accès rapide **"Ouvrir dans BentoPack Studio"**.

### REQ-8 : Internationalisation Native (i18n)
- L'addon doit intégrer un système de traduction complet sans dépendance externe.
- **Langues obligatoires :** Anglais (EN), Français (FR), Japonais (JA), Chinois Simplifié (ZH), Coréen (KO), Portugais Brésilien (PT-BR), Espagnol (ES), Allemand (DE).
- Détection automatique de la langue de l'éditeur du moteur avec sélecteur de langue manuel dans l'interface.

---

## 4. 🏗️ Architecture Modulaire Recommandée

Chaque addon doit structurer son arborescence de manière standardisée :

```text
addons/<moteur>/
├── Runtime/               # Composants exécutés en jeu (doit être ultra-léger)
│   ├── BentoMeshSprite    # Lecteur de maillage M8 & synchronisation hitbox
│   └── BentoData          # Structures de données sérialisables
├── Editor/                # Outils et extensions de l'éditeur
│   ├── BentoImporter      # ScriptedImporter / EditorImportPlugin (.bento)
│   ├── BentoMeshBuilder   # Algorithmes de reconstruction géométrique M8
│   ├── BentoAnimation     # Générateur d'AnimationClips / AnimationPlayer
│   ├── BentoCliBridge     # Détection et exécution de bentopack-cli
│   ├── BentoDashboard     # EditorWindow / Dock principal
│   ├── BentoInspector     # Inspecteur interactif avec contrôleur Play/Scrub
│   ├── BentoDragDrop      # Instanciation par glisser-déposer dans la vue Scène
│   └── BentoI18n          # Dictionnaire et sélecteur multilingue
├── README.md              # Documentation d'installation et tutoriel
├── LICENSE.md             # Addon License
└── package.json / cfg     # Métadonnées officielles du package moteur
```

---

## 5. 📦 Spécifications de Packaging et de Distribution

| Moteur | Format de Distribution | Règles de Conformité Strictes |
|---|---|---|
| **Unity** | Tarball UPM (`.tgz`) | Racine d'archive **obligatoirement nommée `package/`** (contenant `package/package.json`). Fichiers `.meta` systématiques pour chaque script et dossier. Version synchronisée avec `PROJECT_VERSION`. Champ licence `"SEE LICENSE IN LICENSE.md"`. |
| **Godot 4** | Archive ZIP (`.zip`) | Arborescence racine `addons/bentopack/`. Exclusion stricte de tout artefact temporaire (`.uid`, `.import`). Prêt pour extraction directe dans `res://`. |
| **Unreal 5** | Archive Plugin (`.zip`) | Format standard `Plugins/BentoPack/` contenant `BentoPack.uplugin`, dossiers `Source/` et `Resources/`. |

---

## 6. 🛡️ Règles Déontologiques & Respect de Licence

1. **Intégrité des Métadonnées :** L'addon ne doit en aucun cas altérer ou contourner les mécanismes d'identification ou les tags stéganographiques incorporés par BentoPack.
2. **Licence des Addons :** Les plugins d'intégration font partie des composants sous licence **Apache 2.0** ([`LICENSE`](../LICENSE)).