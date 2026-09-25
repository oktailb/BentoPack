# Rapport Formel d'Évaluation & de Performance CLI (`bentopack-cli`)

> **Date du Rapport :** 2026-09-26 08:33:50  
> **Version Git :** `21e90d3` (branche `main`)  
> **Binaire Testé :** `build/Desktop-Debug/bin/bentopack-cli`  
> **Environnement :** Linux 7.2.6-arch2-1 (x86_64)  

---

## 📊 Tableau de Bord des KPIs & Métriques Clés

| Indicateur de Performance | Valeur Actuelle | Évolution (vs run précédent) | Statut Qualité |
|---|:---:|:---:|:---:|
| **Taux de Succès Global** | **24 / 24** (100.0%) | - | 🟢 100% SUCCÈS |
| **Temps Total d'Exécution** | **1286.9 ms** | `-17.9 ms (-1.4%)` | ⚡ Ultra-rapide |
| **Débit Massif (Throughput)** | **2888.0 frames / sec** | `+65.3 FPS` | 🚀 Industriel |
| **Intégrité des Formats** | **100% Validé** | Baseline | 🟢 JSON, TRES, TSCN, PNG |

---

## 🧪 Matrice Complète des 24 Scénarios d'Évaluation

| # | Scénario d'Évaluation | Catégorie | Temps (ms) | Statut | Résultat & Métadonnées |
|:---:|---|---|:---:|:---:|---|
| 01 | `TP_MaxRects_BSSF` | TexturePacker | 43.16 | 🟢 **PASS** | MaxRects BestShortSideFit + Extrude 1 |
| 02 | `TP_MaxRects_BAF` | TexturePacker | 46.98 | 🟢 **PASS** | MaxRects BestAreaFit heuristic |
| 03 | `TP_MaxRects_BLSF` | TexturePacker | 42.68 | 🟢 **PASS** | MaxRects BestLongSideFit heuristic |
| 04 | `TP_MaxRects_BottomLeft` | TexturePacker | 43.73 | 🟢 **PASS** | MaxRects BottomLeft heuristic |
| 05 | `TP_MaxRects_ContactPoint` | TexturePacker | 41.82 | 🟢 **PASS** | MaxRects ContactPoint heuristic |
| 06 | `TP_SizeConstraints_POT` | TexturePacker | 53.57 | 🟢 **PASS** | Power-Of-Two forced: 512x256 |
| 07 | `TP_Extrude_Border` | TexturePacker | 41.11 | 🟢 **PASS** | Border pixel extrusion 2px anti-bleeding |
| 08 | `TP_Trim_Mode_Trim` | TexturePacker | 47.71 | 🟢 **PASS** | Transparency trimmed (20 frames) |
| 09 | `TP_AutoAlias_Deduplication` | TexturePacker | 48.78 | 🟢 **PASS** | 100 frames compacted to 40 unique atlas regions (60% saved) |
| 10 | `TP_AutoAlias_Disabled` | TexturePacker | 51.89 | 🟢 **PASS** | All 100 frames preserved without deduplication |
| 11 | `TP_CustomPivots` | TexturePacker | 43.83 | 🟢 **PASS** | Normalized pivot point (0.5, 1.0) validated in JSON |
| 12 | `Aseprite_Packed` | Aseprite | 41.92 | 🟢 **PASS** | Aseprite -b batch packed mode |
| 13 | `Aseprite_RowPacker` | Aseprite | 40.92 | 🟢 **PASS** | Horizontal row filmstrip arrangement |
| 14 | `Aseprite_GridPacker` | Aseprite | 42.07 | 🟢 **PASS** | Matrix uniform grid layout |
| 15 | `Aseprite_ListTags` | Aseprite | 55.05 | 🟢 **PASS** | frameTags exported (3 animation tags found) |
| 16 | `Godot_SpriteFrames` | Godot 4 | 43.05 | 🟢 **PASS** | Native SpriteFrames (.tres) with AtlasTexture sub-resources |
| 17 | `Godot_UID_Preservation` | Godot 4 | 83.1 | 🟢 **PASS** | UID preserved across builds: uid://1a010fbdeee5 |
| 18 | `Godot_SceneGen` | Godot 4 | 44.22 | 🟢 **PASS** | AnimatedSprite2D (.tscn) scene generated & ready to instantiate |
| 19 | `Native_Slice_Raw` | Native Commands | 37.84 | 🟢 **PASS** | Segmented 16 sprites from transparent sheet in O(N) |
| 20 | `Native_Slice_RemoveBg` | Native Commands | 40.46 | 🟢 **PASS** | Dominant background removed + 16 sprites segmented |
| 21 | `Native_Filter_Despill` | Native Commands | 39.84 | 🟢 **PASS** | Despill edge cleanup filter applied headless |
| 22 | `Native_Filter_Outline` | Native Commands | 41.99 | 🟢 **PASS** | 2px red procedural silhouette outline applied |
| 23 | `POSIX_ExitCodes` | Compliance | 98.05 | 🟢 **PASS** | Strict POSIX codes verified: 1 (syntax), 2 (not found), 3 (constraints) |
| 24 | `Massive_500_Stress` | Scalability | 173.13 | 🟢 **PASS** | Packed 500 frames into 718x718 atlas (77.1% occupancy) at 2888.0 frames/sec |

---

## 📈 Historique & Suivi des Régressions

Chaque exécution enregistre un instantané JSON immuable dans `benchmarks/history/`.
- **Enregistrement actuel :** [`20260926_083350_21e90d3.json`](history/20260926_083350_21e90d3.json)

| Date | Commit | Tests Validés | Temps Total | Débit (FPS) |
|---|:---:|:---:|:---:|:---:|
| 2026-09-17 15:45:08 | `135737d` | 24/24 (100.0%) | 1644.57 ms | 1926.4 fps |
| 2026-09-17 15:45:49 | `135737d` | 24/24 (100.0%) | 1600.88 ms | 1965.1 fps |
| 2026-09-17 16:24:47 | `0ea40f5` | 24/24 (100.0%) | 1743.95 ms | 2221.8 fps |
| 2026-09-26 08:30:43 | `21e90d3` | 23/24 (95.8%) | 1304.81 ms | 2822.7 fps |
| 2026-09-26 08:33:50 | `21e90d3` | 24/24 (100.0%) | 1286.9 ms | 2888.0 fps |

---

### 💡 Commande de Reproduction Locale :
```bash
python scripts/benchmark_cli.py
```
