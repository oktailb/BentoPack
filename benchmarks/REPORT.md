# Rapport Formel d'Évaluation & de Performance CLI (`bentopack-cli`)

> **Date du Rapport :** 2026-10-25 07:11:16  
> **Version Git :** `62ed31a` (branche `main`)  
> **Binaire Testé :** `build/bin/bentopack-cli`  
> **Environnement :** Linux 7.2.10-arch1-1 (x86_64) | SIMD: AVX2 (256-bit SIMD)  

---

## 📊 Tableau de Bord des KPIs & Métriques Clés

| Indicateur de Performance | Valeur Actuelle | Évolution (vs run précédent) | Statut Qualité |
|---|:---:|:---:|:---:|
| **Taux de Succès Global** | **28 / 28** (100.0%) | - | 🟢 100% SUCCÈS |
| **Temps Total d'Exécution** | **3933.7 ms** | `+182.1 ms (+4.9%)` | ⚡ Ultra-rapide |
| **Débit Massif (Atlas Packing)** | **3747.9 frames / sec** | `-228.9 FPS` | 🚀 Industriel |
| **Accélération SIMD (AVX2 (256-bit SIMD))** | **30.06x (Despill) / 10.06x (Swap)** | Matériel | ⚡ Vectorisation Dédiée |
| **Débit de Traitement SIMD** | **3057.1 Mpx/s (Despill) / 1599.5 Mpx/s (Swap)** | Matériel | 🚀 Ultra High-Throughput |
| **Économie Mémoire Undo (Dirty-Rect)** | **99.9% (64 Ko vs 64 Mo)** | O(Surface) | 💾 Économie >99% RAM (Atlas 4K) |
| **Intégrité des Formats** | **100% Validé** | Baseline | 🟢 JSON, TRES, TSCN, PNG |

---

## 🚀 Chantier M14 : Performances Matérielles SIMD & Mémoire Différentielle

Le **Chantier M14** introduit deux avancées majeures de performances mesurées formellement ci-dessus :

1. **Palier 1 : Dirty-Rects & Direct Scanlines (`PixelHash::computeDirtyRect` & `patchAtlasDirect`)**
   - **Économie RAM :** `99.9%` sur atlas 4K (4096x4096). Au lieu de cloner 64 Mo de texture à chaque action de peinture/filtre, seuls **64 Ko** sont stockés dans la pile Undo/Redo.
   - **Pointeurs Scanlines Directs :** Remplacement des accès `pixel()` / `setPixel()` par des boucles de scanlines directes sans copies défensives Qt.

2. **Palier 2 : Vectorisation SIMD Dédiée AVX2 / NEON (`SimdKernels`)**
   - **Architecture Détectée au Runtime :** `AVX2 (256-bit SIMD)`.
   - **Gain Filtrage Despill Chroma :** Accélération **30.06x** avec un débit de **3057.1 Megapixels / sec**.
   - **Gain Filtrage Color Swap :** Accélération **10.06x** avec un débit de **1599.5 Megapixels / sec**.
   - **Exactitude Bit-à-Bit :** 100% conforme et certifié bit-exact avec l'implémentation de référence scalaire.

---

## 🧪 Matrice Complète des 28 Scénarios d'Évaluation

| # | Scénario d'Évaluation | Catégorie | Temps (ms) | Statut | Résultat & Métadonnées |
|:---:|---|---|:---:|:---:|---|
| 01 | `TP_MaxRects_BSSF` | TexturePacker | 51.96 | 🟢 **PASS** | MaxRects BestShortSideFit + Extrude 1 |
| 02 | `TP_MaxRects_BAF` | TexturePacker | 48.6 | 🟢 **PASS** | MaxRects BestAreaFit heuristic |
| 03 | `TP_MaxRects_BLSF` | TexturePacker | 53.56 | 🟢 **PASS** | MaxRects BestLongSideFit heuristic |
| 04 | `TP_MaxRects_BottomLeft` | TexturePacker | 52.02 | 🟢 **PASS** | MaxRects BottomLeft heuristic |
| 05 | `TP_MaxRects_ContactPoint` | TexturePacker | 52.42 | 🟢 **PASS** | MaxRects ContactPoint heuristic |
| 06 | `TP_SizeConstraints_POT` | TexturePacker | 64.21 | 🟢 **PASS** | Power-Of-Two forced: 512x256 |
| 07 | `TP_Extrude_Border` | TexturePacker | 52.67 | 🟢 **PASS** | Border pixel extrusion 2px anti-bleeding |
| 08 | `TP_Trim_Mode_Trim` | TexturePacker | 59.1 | 🟢 **PASS** | Transparency trimmed (20 frames) |
| 09 | `TP_AutoAlias_Deduplication` | TexturePacker | 64.51 | 🟢 **PASS** | 100 frames compacted to 40 unique atlas regions (60% saved) |
| 10 | `TP_AutoAlias_Disabled` | TexturePacker | 66.1 | 🟢 **PASS** | All 100 frames preserved without deduplication |
| 11 | `TP_CustomPivots` | TexturePacker | 52.89 | 🟢 **PASS** | Normalized pivot point (0.5, 1.0) validated in JSON |
| 12 | `Aseprite_Packed` | Aseprite | 52.66 | 🟢 **PASS** | Aseprite -b batch packed mode |
| 13 | `Aseprite_RowPacker` | Aseprite | 54.07 | 🟢 **PASS** | Horizontal row filmstrip arrangement |
| 14 | `Aseprite_GridPacker` | Aseprite | 53.85 | 🟢 **PASS** | Matrix uniform grid layout |
| 15 | `Aseprite_ListTags` | Aseprite | 1425.08 | 🟢 **PASS** | frameTags exported (1 animation tags found) |
| 16 | `Godot_SpriteFrames` | Godot 4 | 53.68 | 🟢 **PASS** | Native SpriteFrames (.tres) with AtlasTexture sub-resources |
| 17 | `Godot_UID_Preservation` | Godot 4 | 97.52 | 🟢 **PASS** | UID preserved across builds: uid://1a010fbdeee5 |
| 18 | `Godot_SceneGen` | Godot 4 | 53.68 | 🟢 **PASS** | AnimatedSprite2D (.tscn) scene generated & ready to instantiate |
| 19 | `Native_Slice_Raw` | Native Commands | 45.87 | 🟢 **PASS** | Segmented 16 sprites from transparent sheet in O(N) |
| 20 | `Native_Slice_RemoveBg` | Native Commands | 50.94 | 🟢 **PASS** | Dominant background removed + 16 sprites segmented |
| 21 | `Native_Filter_Despill` | Native Commands | 50.6 | 🟢 **PASS** | Despill edge cleanup filter applied headless |
| 22 | `Native_Filter_Outline` | Native Commands | 51.67 | 🟢 **PASS** | 2px red procedural silhouette outline applied |
| 23 | `POSIX_ExitCodes` | Compliance | 102.79 | 🟢 **PASS** | Strict POSIX codes verified: 1 (syntax), 2 (not found), 3 (constraints) |
| 24 | `Massive_500_Stress` | Scalability | 133.41 | 🟢 **PASS** | Packed 500 frames into 718x718 atlas (77.1% occupancy) at 3747.9 frames/sec |
| 25 | `M14_SIMD_ColorSwap_2K` | SIMD Acceleration (M14) | 358.45 | 🟢 **PASS** | Recolored 2048x2048 (4.19 Mpx) atlas at 28.5 Mpx/sec (AVX2 (256-bit SIMD)) |
| 26 | `M14_SIMD_Despill_2K` | SIMD Acceleration (M14) | 272.64 | 🟢 **PASS** | Despilled 2048x2048 (4.19 Mpx) atlas at 68.8 Mpx/sec (AVX2 (256-bit SIMD)) |
| 27 | `M14_SIMD_Microbench_Speedup` | SIMD Acceleration (M14) | 310.45 | 🟢 **PASS** | Hardware acceleration: Despill 30.06x (3057.1 Mpx/s), ColorSwap 10.06x (1599.5 Mpx/s) [AVX2 (256-bit SIMD)] |
| 28 | `M14_DirtyRect_Differential_Undo` | Dirty-Rect Memory (M14) | 148.29 | 🟢 **PASS** | 4K Atlas (64 MB): 99.9% RAM saved via 128x128 patch (64 KB) [diff: 80461.7 µs, patch: 6163.9 µs] |

---

## 📈 Historique & Suivi des Régressions

Chaque exécution enregistre un instantané JSON immuable dans `benchmarks/history/`.
- **Enregistrement actuel :** [`20261025_071116_62ed31a.json`](history/20261025_071116_62ed31a.json)

| Date | Commit | Tests Validés | Temps Total | Débit Packing | Débit SIMD | Gain RAM Undo |
|---|:---:|:---:|:---:|:---:|:---:|:---:|
| 2026-10-24 20:45:41 | `62ed31a` | 24/24 (100.0%) | 2856.46 ms | 3802.2 fps | - | - |
| 2026-10-24 20:48:30 | `62ed31a` | 24/24 (100.0%) | 2728.63 ms | 3923.1 fps | - | - |
| 2026-10-24 20:59:04 | `62ed31a` | 24/28 (85.7%) | 3379.96 ms | 4024.6 fps | 0.0 Mpx/s | 0.0% |
| 2026-10-24 20:59:40 | `62ed31a` | 28/28 (100.0%) | 3751.58 ms | 3976.8 fps | 3241.1 Mpx/s | 99.9% |
| 2026-10-25 07:11:16 | `62ed31a` | 28/28 (100.0%) | 3933.69 ms | 3747.9 fps | 3057.1 Mpx/s | 99.9% |

---

### 💡 Commande de Reproduction Locale :
```bash
python scripts/benchmark_cli.py
```
