# Rapport Formel d'Évaluation & de Performance CLI (`spritestudio-cli`)

> **Date du Rapport :** 2026-09-17 15:45:49  
> **Version Git :** `135737d` (branche `main`)  
> **Binaire Testé :** `C:\Users\ec135\Documents\GitHub\SpriteStudio\build\Desktop_Qt_6_10_2_MinGW_64_bit-Debug\bin\spritestudio-cli.exe`  
> **Environnement :** Windows 10 (AMD64)  

---

## 📊 Tableau de Bord des KPIs & Métriques Clés

| Indicateur de Performance | Valeur Actuelle | Évolution (vs run précédent) | Statut Qualité |
|---|:---:|:---:|:---:|
| **Taux de Succès Global** | **24 / 24** (100.0%) | - | 🟢 100% SUCCÈS |
| **Temps Total d'Exécution** | **1600.9 ms** | `-43.7 ms (-2.7%)` | ⚡ Ultra-rapide |
| **Débit Massif (Throughput)** | **1965.1 frames / sec** | `+38.7 FPS` | 🚀 Industriel |
| **Intégrité des Formats** | **100% Validé** | Baseline | 🟢 JSON, TRES, TSCN, PNG |

---

## 🧪 Matrice Complète des 24 Scénarios d'Évaluation

| # | Scénario d'Évaluation | Catégorie | Temps (ms) | Statut | Résultat & Métadonnées |
|:---:|---|---|:---:|:---:|---|
| 01 | `TP_MaxRects_BSSF` | TexturePacker | 43.71 | 🟢 **PASS** | MaxRects BestShortSideFit + Extrude 1 |
| 02 | `TP_MaxRects_BAF` | TexturePacker | 42.24 | 🟢 **PASS** | MaxRects BestAreaFit heuristic |
| 03 | `TP_MaxRects_BLSF` | TexturePacker | 43.37 | 🟢 **PASS** | MaxRects BestLongSideFit heuristic |
| 04 | `TP_MaxRects_BottomLeft` | TexturePacker | 42.76 | 🟢 **PASS** | MaxRects BottomLeft heuristic |
| 05 | `TP_MaxRects_ContactPoint` | TexturePacker | 45.73 | 🟢 **PASS** | MaxRects ContactPoint heuristic |
| 06 | `TP_SizeConstraints_POT` | TexturePacker | 52.69 | 🟢 **PASS** | Power-Of-Two forced: 512x256 |
| 07 | `TP_Extrude_Border` | TexturePacker | 44.47 | 🟢 **PASS** | Border pixel extrusion 2px anti-bleeding |
| 08 | `TP_Trim_Mode_Trim` | TexturePacker | 48.57 | 🟢 **PASS** | Transparency trimmed (20 frames) |
| 09 | `TP_AutoAlias_Deduplication` | TexturePacker | 71.13 | 🟢 **PASS** | 100 frames compacted to 40 unique atlas regions (60% saved) |
| 10 | `TP_AutoAlias_Disabled` | TexturePacker | 67.09 | 🟢 **PASS** | All 100 frames preserved without deduplication |
| 11 | `TP_CustomPivots` | TexturePacker | 42.58 | 🟢 **PASS** | Normalized pivot point (0.5, 1.0) validated in JSON |
| 12 | `Aseprite_Packed` | Aseprite | 43.06 | 🟢 **PASS** | Aseprite -b batch packed mode |
| 13 | `Aseprite_RowPacker` | Aseprite | 40.92 | 🟢 **PASS** | Horizontal row filmstrip arrangement |
| 14 | `Aseprite_GridPacker` | Aseprite | 43.34 | 🟢 **PASS** | Matrix uniform grid layout |
| 15 | `Aseprite_ListTags` | Aseprite | 194.68 | 🟢 **PASS** | frameTags exported (3 animation tags found) |
| 16 | `Godot_SpriteFrames` | Godot 4 | 45.61 | 🟢 **PASS** | Native SpriteFrames (.tres) with AtlasTexture sub-resources |
| 17 | `Godot_UID_Preservation` | Godot 4 | 89.98 | 🟢 **PASS** | UID preserved across builds: uid://1a010fbdeee5 |
| 18 | `Godot_SceneGen` | Godot 4 | 45.8 | 🟢 **PASS** | AnimatedSprite2D (.tscn) scene generated & ready to instantiate |
| 19 | `Native_Slice_Raw` | Native Commands | 66.73 | 🟢 **PASS** | Segmented 16 sprites from transparent sheet in O(N) |
| 20 | `Native_Slice_RemoveBg` | Native Commands | 66.9 | 🟢 **PASS** | Dominant background removed + 16 sprites segmented |
| 21 | `Native_Filter_Despill` | Native Commands | 35.81 | 🟢 **PASS** | Despill edge cleanup filter applied headless |
| 22 | `Native_Filter_Outline` | Native Commands | 35.46 | 🟢 **PASS** | 2px red procedural silhouette outline applied |
| 23 | `POSIX_ExitCodes` | Compliance | 93.81 | 🟢 **PASS** | Strict POSIX codes verified: 1 (syntax), 2 (not found), 3 (constraints) |
| 24 | `Massive_500_Stress` | Scalability | 254.44 | 🟢 **PASS** | Packed 500 frames into 816x816 atlas (78.1% occupancy) at 1965.1 frames/sec |

---

## 📈 Historique & Suivi des Régressions

Chaque exécution enregistre un instantané JSON immuable dans `benchmarks/history/`.
- **Enregistrement actuel :** [`20260917_154549_135737d.json`](file:///C:/Users/ec135/Documents/GitHub/SpriteStudio/benchmarks/history/20260917_154549_135737d.json)

| Date | Commit | Tests Validés | Temps Total | Débit (FPS) |
|---|:---:|:---:|:---:|:---:|
| 2026-09-17 14:39:28 | `135737d` | 23/24 (95.8%) | 7205.47 ms | 107.8 fps |
| 2026-09-17 14:40:34 | `135737d` | 24/24 (100.0%) | 3165.38 ms | 357.0 fps |
| 2026-09-17 15:45:08 | `135737d` | 24/24 (100.0%) | 1644.57 ms | 1926.4 fps |
| 2026-09-17 15:45:49 | `135737d` | 24/24 (100.0%) | 1600.88 ms | 1965.1 fps |

---

### 💡 Commande de Reproduction Locale :
```bash
python scripts/benchmark_cli.py
```
