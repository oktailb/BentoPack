# 📝 TODO — Tâches Courantes, Urgentes & Maintenance Opérationnelle

> **Projet :** BentoPack Studio  
> **Dernière mise à jour :** 2026-10-09  
> **Règle de séparation :** Ce fichier rassemble les finitions immédiates, l'hygiène de code, l'ergonomie et la maintenance continue des travaux en cours. Pour la vision stratégique et les grandes fonctionnalités à venir (Stores, M21 Lighting, M16 Multi-Page, M22 VRAM Dedup, M12 Sur-animation), consulter impérativement [**`ROADMAP.md`**](file:///ROADMAP.md).

---

## 🎨 1. Ergonomie UI & Graphisme (Priorité Immédiate)

- [x] **Remplacement des glyphes / emojis UTF-8 des outils dans le Pixel Editor :**
  - **Livrable :** Intégration des icônes PNG 512x512 dans `BentoPack/icons/` (Crayon, Gomme, Seau, Pipette, Sélection rectangulaire, Baguette magique, Flip H/V, Rotation 90°, Supprimer sélection, Undo/Redo).
  - Embarquement automatique dans le binaire via `qt6_add_resources`.
  - Inversion chromatique dynamique temps réel selon le thème clair/sombre de l'OS (`isSystemDarkMode`), avec génération multi-résolutions anti-crénelage pour écrans High-DPI et état checked contrasté.
- [x] **Harmonisation des bulles d'aide (Tooltips) :**
  - Vérifier que chaque bouton d'outil dans `PixelEditorDialog`, la timeline et le dock de calques dispose d'un tooltip descriptif avec rappel de son raccourci clavier.
- [x] **Feedback visuel sur les sélections d'outils :**
  - Vérifier l'état enfoncé (*checked state*) et le contraste des boutons d'outils maillage et dessin sous le thème sombre.
- [x] **Système modulaire de palettes de couleurs standardisées (`.gpl` / `.hex`) :**
  - Palettes extraites en fichiers indépendants au format standard GIMP/Aseprite (`.gpl`) dans `palettes/`.
  - Embarquement autonome via Qt Resource System (`palettes.qrc`), copie automatique dans le dossier `bin/` et packaging dans les livrables (`install.cmake`).
  - Scan automatique au démarrage dans le dossier de l'application et le dossier de configuration utilisateur (`%APPDATA%/BentoPack/palettes/`).
  - Suppression totale des enums redondants et mappings statiques (`mapIdToPreset`, `presetFileName`, `allPresets`) : seul `Standard` (36 couleurs) est conservé comme secours ultime, tout le reste est piloté dynamiquement par identifiant et métadonnées de fichiers.

---

## 🌐 2. Internationalisation & Localisation (Contrôle Continu i18n)

- [x] **Audit régulier des nouvelles chaînes dans `tr()` :**
  - Passer en revue les textes ajoutés récemment :
    - Panneau Maillage Intelligent CDT (Mode distance RGB, mode Sobel, seuils, couteau laser).
    - Checkboxes d'application ciblée (*« Appliquer à toutes les frames de l'animation »*).
    - Menu contextuel des filtres et typage déclaratif.
- [x] **Synchronisation et mise à jour des catalogues `.ts` :**
  - Exécuter périodiquement `lupdate` pour extraire les nouvelles chaînes :
    - `resources/translations/bentopack_fr.ts` (Français)
    - `resources/translations/bentopack_ja.ts` (Japonais)
    - `resources/translations/bentopack_en.ts` (Anglais)
    - ...
  - Compléter les traductions manquantes et vérifier l'absence d'encodages défectueux.
- [x] **Compilation binaire des traductions :**
  - Générer les fichiers `.qm` via `lrelease` et vérifier le basculement dynamique de langue à chaud dans les Préférences.

---

## 🧪 3. Tests de Robustesse & Corner Cases Immédiats

- [x] **Cas limites géométriques sur le maillage CDT :**
  - [x] Sprite vide ou monochrome 1x1 : vérifier que la détection de contour et CDT ne crashent pas et renvoient un quad minimal propre.
  - [x] Formes concaves avec trous intérieurs (îlots transparents) : vérifier que le rognage hors-frontière de Bowyer-Watson élimine bien les triangles internes sans trouer le contour extérieur.
  - [x] Suppression massive de sommets au couteau ou à la touche Suppr : s'assurer que le garde-fou ($> 3$ sommets de contour) reste inviolable.
- [x] **Pression mémoire sur animations longues :**
  - [x] Tester l'option *« Appliquer le maillage à toutes les frames »* sur une séquence de plus de 60 frames.
  - [x] Vérifier la consommation mémoire de `QUndoStack` et le temps de réponse de l'Undo/Redo après cette opération groupée.
- [x] **Revue des 16 suites de tests CTest :**
  - Maintenir un taux de passage strict de 100% sur l'ensemble de la suite (`ctest --output-on-failure`).

---

## 📖 4. Documentation Utilisateur & Développeur

- [x] **Guide Utilisateur ([`docs/USER_GUIDE.md`](file:///docs/USER_GUIDE.md)) :**
  - Ajouter un chapitre dédié au **Maillage Intelligent CDT & Anti-Overdraw** :
    - Comment utiliser la détection de crêtes (RGB vs Sobel).
    - Comment utiliser l'outil couteau laser pour découper les pliures d'animation.
    - Quand activer l'application automatique sur toutes les frames d'une animation.
- [x] **Guide Développeur ([`docs/DEVELOPER_GUIDE.md`](file:///docs/DEVELOPER_GUIDE.md)) :**
  - Documenter la signature et les contrats d'invariance de `Triangulator::generateSmartMesh()` et `Triangulator::triangulateCDT()`.
  - Documenter le format de stockage des `triangles` et `vertices` dans `SpriteBox` et l'injection dans les extracteurs moteurs.

---

## 🧹 5. Hygiène de Code & Maintenance Continue

- [x] **Contrôle des avertissements de compilation (Warnings) :**
  - Passer le build complet avec `-Wall -Wextra`, élimination des warnings de dépréciation Qt 6 (constructeurs `QDragEnterEvent`, `QDragMoveEvent`, `QMouseEvent`).
- [x] **Contrôle des fuites mémoires et RAII :**
  - S'assurer que les allocations de `QActionGroup`, `QMenu` dynamiques et buffers temporaires de triangulation sont correctement rattachées à leurs parents Qt ou encapsulées dans des pointeurs intelligents.
