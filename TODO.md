# 📝 TODO — Tâches Courantes, Urgentes & Maintenance Opérationnelle

> **Projet :** BentoPack Studio  
> **Dernière mise à jour :** 2026-10-09  
> **Règle de séparation :** Ce fichier rassemble les finitions immédiates, l'hygiène de code, l'ergonomie et la maintenance continue des travaux en cours. Pour la vision stratégique et les grandes fonctionnalités à venir (Stores, M21 Lighting, M16 Multi-Page, M22 VRAM Dedup, M12 Sur-animation), consulter impérativement [**`ROADMAP.md`**](file:///ROADMAP.md).

---

## 🎨 1. Ergonomie UI & Graphisme (Priorité Immédiate)

- [ ] **Remplacement des glyphes / emojis UTF-8 des outils :**
  - **Constat :** Les emojis textuels (`🔲`, `✖`, `✂️`, drapeaux) souffrent d'un rendu variable ou dégradé (noir & blanc, crénelage) selon la version de Windows et les polices système de rendu.
  - **Action :** Créer de vraies icônes vectorielles SVG ou pixmaps haute résolution stables :
    - Outil d'ajout de sommet CDT (`point_add.svg` ou pixmap pixel-art)
    - Outil de suppression de sommet (`point_del.svg`)
    - Outil de couteau / trait de coupe laser (`mesh_knife.svg`)
    - Boutons des drapeaux de langues et actions de filtres
  - *Note créative :* Le fin du fin sera de dessiner et peaufiner ces icônes pixel-art directement dans l'Éditeur de Pixels de BentoPack !
- [ ] **Harmonisation des bulles d'aide (Tooltips) :**
  - Vérifier que chaque bouton d'outil dans `PixelEditorDialog`, la timeline et le dock de calques dispose d'un tooltip descriptif avec rappel de son raccourci clavier.
- [ ] **Feedback visuel sur les sélections d'outils :**
  - Vérifier l'état enfoncé (*checked state*) et le contraste des boutons d'outils maillage et dessin sous le thème sombre.

---

## 🌐 2. Internationalisation & Localisation (Contrôle Continu i18n)

- [ ] **Audit régulier des nouvelles chaînes dans `tr()` :**
  - Passer en revue les textes ajoutés récemment :
    - Panneau Maillage Intelligent CDT (Mode distance RGB, mode Sobel, seuils, couteau laser).
    - Checkboxes d'application ciblée (*« Appliquer à toutes les frames de l'animation »*).
    - Menu contextuel des filtres et typage déclaratif.
- [ ] **Synchronisation et mise à jour des catalogues `.ts` :**
  - Exécuter périodiquement `lupdate` pour extraire les nouvelles chaînes :
    - `resources/translations/bentopack_fr.ts` (Français)
    - `resources/translations/bentopack_ja.ts` (Japonais)
    - `resources/translations/bentopack_en.ts` (Anglais)
  - Compléter les traductions manquantes et vérifier l'absence d'encodages défectueux.
- [ ] **Compilation binaire des traductions :**
  - Générer les fichiers `.qm` via `lrelease` et vérifier le basculement dynamique de langue à chaud dans les Préférences.

---

## 🧪 3. Tests de Robustesse & Corner Cases Immédiats

- [ ] **Cas limites géométriques sur le maillage CDT :**
  - [ ] Sprite vide ou monochrome 1x1 : vérifier que la détection de contour et CDT ne crashent pas et renvoient un quad minimal propre.
  - [ ] Formes concaves avec trous intérieurs (îlots transparents) : vérifier que le rognage hors-frontière de Bowyer-Watson élimine bien les triangles internes sans trouer le contour extérieur.
  - [ ] Suppression massive de sommets au couteau ou à la touche Suppr : s'assurer que le garde-fou ($> 3$ sommets de contour) reste inviolable.
- [ ] **Pression mémoire sur animations longues :**
  - [ ] Tester l'option *« Appliquer le maillage à toutes les frames »* sur une séquence de plus de 60 frames.
  - [ ] Vérifier la consommation mémoire de `QUndoStack` et le temps de réponse de l'Undo/Redo après cette opération groupée.
- [ ] **Revue des 15 suites de tests CTest :**
  - Maintenir un taux de passage strict de 100% sur l'ensemble de la suite (`ctest --output-on-failure`).

---

## 📖 4. Documentation Utilisateur & Développeur

- [ ] **Guide Utilisateur ([`docs/USER_GUIDE.md`](file:///docs/USER_GUIDE.md)) :**
  - Ajouter un chapitre dédié au **Maillage Intelligent CDT & Anti-Overdraw** :
    - Comment utiliser la détection de crêtes (RGB vs Sobel).
    - Comment utiliser l'outil couteau laser pour découper les pliures d'animation.
    - Quand activer l'application automatique sur toutes les frames d'une animation.
- [ ] **Guide Développeur ([`docs/DEVELOPER_GUIDE.md`](file:///docs/DEVELOPER_GUIDE.md)) :**
  - Documenter la signature et les contrats d'invariance de `Triangulator::generateSmartMesh()` et `Triangulator::triangulateCDT()`.
  - Documenter le format de stockage des `triangles` et `vertices` dans `SpriteBox` et l'injection dans les extracteurs moteurs.

---

## 🧹 5. Hygiène de Code & Maintenance Continue

- [ ] **Contrôle des avertissements de compilation (Warnings) :**
  - Passer le build complet sous MinGW GCC avec `-Wall -Wextra` et s'assurer de l'absence de variables inutilisées ou de conversions de types implicites douteuses.
- [ ] **Contrôle des fuites mémoires et RAII :**
  - S'assurer que les allocations de `QActionGroup`, `QMenu` dynamiques et buffers temporaires de triangulation sont correctement rattachées à leurs parents Qt ou encapsulées dans des pointeurs intelligents.
