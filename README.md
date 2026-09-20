# 🌟 SpriteStudio

[![CI](https://github.com/oktailb/SpriteStudio/actions/workflows/ci.yml/badge.svg)](https://github.com/oktailb/SpriteStudio/actions/workflows/ci.yml)
[![License](https://img.shields.io/badge/License-Apache_2.0-blue.svg)](https://opensource.org/licenses/Apache-2.0)
[![C++17](https://img.shields.io/badge/C%2B%2B-17-00599C.svg?logo=c%2B%2B)](https://en.cppreference.com/w/cpp/17)
[![Qt 6](https://img.shields.io/badge/Qt-6.5+-41CD52.svg?logo=qt)](https://www.qt.io/)
[![CMake](https://img.shields.io/badge/CMake-3.20+-064F8C.svg?logo=cmake)](https://cmake.org/)

> **SpriteStudio** is a fast, modular, and modern desktop application tailored for game developers, pixel artists, and 2D animators to extract, clean, arrange, and export 2D sprite sheets and animated textures.
The UI and CLI are licensed under Apache 2.0. Specific engine integration plugins (Godot, Unity, Unreal) and advanced filters are located in the /plugins directory and are provided under a Source-Available License (free to compile for entities under $1M revenue). Pre-compiled binaries are available for purchase on Stores.

---

![SpriteStudio Interface Demo](SpriteStudio.gif)

---

## ✨ Key Features

### 📥 Intelligent Extraction & Import
* **Smart Sprite Sheet Slicing:** Automatic sprite detection and extraction with configurable alpha and color tolerance.
* **Animated GIF Decompilation:** Instant extraction of multi-frame animated GIFs into ordered frame sequences.
* **Format Parsers:** Import existing sprite atlases with associated **JSON** (TexturePacker / Aseprite) or **Godot 4 `.tres`** metadata.
* **Non-Destructive Filter & Cleanup Suite:** Extensible filter plugin system featuring real-time debounced live preview, non-destructive rollback, and full `QUndoStack` integration across 9 specialized filters:
  * **Chroma Background Removal (`Ctrl+Shift+B`):** Dominant color auto-detection, alpha thresholding, and smart crop.
  * **Despill / Anti-Halo:** Clean up 1-pixel colored fringes left by antialiased edges using *Color Clamping* or strict removal.
  * **Outline & Silhouette Generator:** 1-4px customizable stroke (4-connected or 8-connected) and solid hit-flash silhouette masks.
  * **Color Swap (Alt-Skins):** Instant palette replacement preserving pixel art shading gradients (HSV).
  * **Color Adjustment:** Real-time Hue, Saturation, Value, Brightness, and Contrast grading.
  * **Retro Palette & Dithering:** Color quantization and Floyd-Steinberg dithering to authentic retro systems (NES, SNES, Game Boy, Amiga, Pico-8, C64).
  * **Pixel Art Rescale:** Pixel-perfect integer rescaling without bilinear blurring.
  * **Atlas Bin-Packing (MaxRects) (`Ctrl+Shift+P`):** Multi-heuristic 2D box packing (Best Short Side Fit, Best Area Fit, Best Long Side Fit, Bottom Left, Contact Point).
  * **Tight Polygon Packing (Nesting) (`Ctrl+Shift+T`):** Multi-threaded high-density concave/convex polygon packing allowing bounding boxes to overlap.

![Background Removal Demo](RemoveBackground.gif)

### 🎬 Timeline & Animation Filmstrip
* **Filmstrip Dock:** Intuitive bottom timeline with thumbnail filmstrip, scrub bar, and drag-and-drop frame reordering.
* **Frame Merging & Compositing:** Drag and drop one frame onto another to fuse them into a single layered sprite.
* **Batch Editing:** Multi-selection operations including group deletion, invert selection, and reverse frame ordering (ideal for ping-pong loops).
* **Real-time Playback:** High-precision preview engine with configurable FPS (1 to 60 FPS), loop controls, and aspect-ratio stabilization.

![Frame Fusion Demo](Fusion.gif)

### 🎯 Interactive Anchor Points & Pivots
* **Zero-Jittering Animation Stabilization:** Shared animation envelope alignment guarantees rock-solid anchored characters during motion, attacks, or jumps without visual shaking or sliding.
* **Dual-Surface Direct Editing:** Move the high-contrast reticle directly within the atlas slices or right inside the live animation preview with real-time coordinate updates and Shift+Click snapping.
* **Instant Cardinal Presets & Batch Actions:** One-click shortcuts for Ground/Bottom-Center, Center, and Top-Left (UI), with batch propagation to entire animations or the full project.
* **Intelligent Preview Navigation:** Automatic optimal framing (*Fit In View*), 5000% razor-sharp pixel art wheel zoom, and pan controls.

### 💾 Native `.ssp` Project Architecture
* **All-in-One Compressed Format:** `.ssp` project files package project metadata, frame descriptors, and source assets into a structured ZIP archive.
* **Atomic Transactions & Crash Recovery:** Atomic file writing with journaled crash-recovery safeguard prevents project corruption.
* **Embedded Git Time-Travel:** Integrated non-destructive versioning engine powered by LibGit2. Browse commit history, inspect visual diffs, and revert to previous states without leaving the app.

### 📐 2D Polygon & Tight Mesh Packing (M8)
* **Alpha Contour Extraction:** Automatic watertight Marching Squares contouring on sprite alpha silhouettes.
* **Intelligent Simplification:** Ramer-Douglas-Peucker (RDP) boundary reduction with outward normal dilation (0–8px padding) to prevent edge pixel clipping, with customizable vertex budget (3–48 vertices).
* **Non-Convex Triangulation:** Robust Ear-Clipping triangulation generating GPU index buffers and slashing up to 60–80% of GPU transparent pixel overdraw.
* **HMI Live Visualization & Controls:** Real-time wireframe view directly on the atlas canvas (cyan mesh lines, neon green boundary, and vertex handles). Toggleable in the View menu. Dedicated tuning dialog (`Ctrl+M` / right-click) with 400% zoomed interactive preview, slider controls, and live fillrate savings telemetry.
* **Direct Canvas Vertex Editing:** Click-drag individual vertices, multi-select vertices with `Shift` or `Ctrl`, nudge with arrow keys, double-click edge to insert a vertex, and hit `Delete` to remove vertices with automatic re-triangulation.
* **High-Density Tight Polygon Packing:** Compaction allowing bounding boxes to overlap without pixel collisions. Multi-threaded candidate evaluation with configurable thread count (up to hardware cores) and sub-20ms instant calculation.
* **Reversible & Persistent:** Full `QUndoStack` integration (`SetPolygonMeshCommand`) and lossless `.ssp` project serialization.

### 🖌️ Surgical Pixel-by-Pixel Editor (M4)
* **High-Precision Canvas (`Ctrl+E`):** Fast, non-interpolated pixel art editor (`SmoothPixmapTransform = false`) for surgical touch-ups without switching to GIMP or Photoshop.
* **Continuous Bresenham Drawing:** 1px pencil and 1px eraser guarantee continuous unbroken lines even during fast mouse sweeps.
* **Instant Sampling & Flood Fill:** Eyedropper (`Alt+Click` or `I`) and 4-way flood fill bucket bounded by sprite limits and active selection.
* **Flexible Selections & Floating Stamp:** Rectangular marquee and magic wand color selection; Cut (`Ctrl+X`), Copy (`Ctrl+C`), and Paste (`Ctrl+V`) with draggable floating stamp preview.
* **Authentic Retro & Dynamic Palettes:** Real-time auto-extraction of unique sprite colors, alongside authentic historical palettes: **NES / Famicom** (54), **SNES / Super Famicom** (32), **Amiga OCS** (32), **NEC PC-Engine** (32), **Game Boy DMG** (4), **Pico-8** (16), and **Commodore 64** (16).
* **Inter-Frame Navigation:** `[◀ Previous]` and `[Next ▶]` buttons (`Page Up` / `Page Down`) allow touching up entire walk cycles frame by frame in a single session.
* **Two-Tier Undo/Redo:** Local stack for fine brush strokes and compound `EditSpritePixelsCommand` synchronizing frame and atlas texture seamlessly with `CompositionMode_Source`.

### 📤 Multi-Engine Export
* **PNG Sprite Atlas:** Optimized packing of extracted frames into a consolidated texture sheet.
* **Godot 4 Engine Exporter:** Generates ready-to-use Godot 4 `SpriteFrames` (`.tres`) resources with embedded `AtlasTexture` definitions, animations, and companion `_mesh.tres` 2D `ArrayMesh` resources.
* **Unity 2D SpriteSheet Exporter:** Generates `.unity.json` metadata compatible with Unity's `SpriteMeshType.Tight`, including normalized UVs, inverted Y-axis coordinates, and face indices.
* **Unreal Engine Paper2D Exporter:** Generates `.paper2d.json` descriptors containing both render and collision polygon geometries.
* **TexturePacker / Aseprite JSON:** Universal JSON metadata mapping frame bounds `(x, y, w, h)`, animation tags, and optional mesh geometry (`vertices`, `verticesUV`, `triangles`).

### 🚀 GPU VRAM Hardware Texture Compression (M9)
* **Direct GPU Memory Upload (Zero CPU Decompression Overhead):** Bypasses CPU software raster decompression at game runtime. Uploads blocks straight into VRAM, slashing memory bus bandwidth and reducing game asset load times to near-zero.
* **Universal KTX2 Containers & Khronos Basis Universal:** Powered by Khronos `basis_universal` (v2.50) with multi-threaded C++ encoding.
* **UASTC 4x4 Mode (Recommended for Pixel Art):** High-fidelity 8 bpp block compression delivering **75.0% VRAM memory savings** while preserving crisp sprite pixel contours and delicate alpha antialiasing. Transcodes on-the-fly to native GPU formats: **ASTC** (iOS, Android, Switch, Apple Silicon), **BC7** (PC DirectX 11/12, Vulkan, PS5, Xbox), or **ETC2**.
* **ETC1S Ultra-Compact Mode:** Global codebook vector quantization yielding up to **87.5% VRAM footprint reduction** for lightweight UI and mass sprite textures.
* **Lossless Zstandard Supercompression:** Integrated Zstd compression (levels 1 to 22) shrinking file size on disk while retaining GPU block layout.
* **Live VRAM Telemetry in ExportDialog:** Real-time feedback comparing raw RGBA8888 vs UASTC/ETC1S memory footprint with instant percentage savings.
* **Integrated Game Engine Companions:** Emits `.ktx2` companion atlas files natively referenced by Godot 4 (`res://atlas.ktx2`), Unity (`KtxUnity`), Unreal Engine, and WebGL/WebGPU.

---

## 🛠️ Tech Stack & Architecture

* **Language:** C++17
* **Framework:** Qt 6 (Core, Gui, Widgets, Multimedia, Concurrent, Test, LinguistTools)
* **Core Engine:** `libSpriteStudioCore` shared library with clean exported API symbols (`SPRITESTUDIOCORE_EXPORT`)
* **Dynamic Plugin System:** Hot-loadable Qt 6 plugins (`QPluginLoader`) for both **Filters** (`plugins/filters/`) and **Codecs / Extractors** (`plugins/extractors/`)
* **Plugin Developer SDK:** Installed public headers, CMake package configuration (`SpriteStudioConfig.cmake`), and reference examples (`examples/sample_filter_plugin`, `examples/sample_extractor_plugin`)
* **Hardware Texture Compression:** Khronos `basis_universal` (v2.50) integration for direct GPU memory upload (KTX2, UASTC, ETC1S, Zstd)
* **Build System:** CMake 3.20+ with modular architecture (`SpriteStudioCore` + `SpriteStudio` GUI + `spritestudio-cli` + 8 automated CTest suites)
* **Versioning Engine:** LibGit2 (optional, enabled when detected)

---

## 💻 Building and Running

### Prerequisites

* **C++17 compliant compiler:** GCC 11+, Clang 13+, or MSVC 2019/2022
* **CMake:** Version 3.20 or newer
* **Qt 6:** Version 6.5 or newer (Core, Gui, Widgets, Multimedia, MultimediaWidgets, Concurrent, Test, LinguistTools)
* **Ninja** or **Make** (recommended build generators)
* *(Optional)* **LibGit2** (for embedded `.ssp` time-travel versioning)

### Build Instructions

#### Linux (Ubuntu / Debian)

```bash
# 1. Install prerequisites
sudo apt-get update
sudo apt-get install -y build-essential cmake ninja-build \
  qt6-base-dev qt6-multimedia-dev qt6-tools-dev libgit2-dev

# 2. Clone repository
git clone https://github.com/oktailb/SpriteStudio.git
cd SpriteStudio

# 3. Configure and build
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build

# 4. Run tests
ctest --test-dir build --output-on-failure

# 5. Launch
./build/bin/SpriteStudio
```

#### Windows (MinGW 64-bit or MSVC)

```powershell
# 1. Clone repository
git clone https://github.com/oktailb/SpriteStudio.git
cd SpriteStudio

# 2. Configure with CMake
cmake -B build -DCMAKE_BUILD_TYPE=Release

# 3. Build project
cmake --build build --config Release --parallel

# 4. Run automated test suite
ctest --test-dir build --output-on-failure -C Release

# 5. Launch
.\build\bin\SpriteStudio.exe
```

---

## 🧪 Automated Testing

SpriteStudio includes a modular test suite using `QtTest` and `CTest`, validating core models, extractors, controllers, and project serialization:

```bash
ctest --test-dir build --output-on-failure --verbose
```

| Test Suite | Description |
| :--- | :--- |
| `test_core` | Frame data structures, color detection, and algorithm utilities |
| `test_project` | `.ssp` serialization, atomic saves, journal recovery, and LibGit2 versioning |
| `test_extractors` | GIF, JSON, Godot 4 `.tres`, and Sprite Sheet detectors using sample assets |
| `test_controllers` | Undo/Redo commands, frame merging, selection, and timeline controller logic |
| `test_cli` | Headless CLI parser, TexturePacker & Aseprite emulation, Godot 4 UID/scene generation, native commands, and POSIX exit codes |
| `test_mesh` | Watertight Marching Squares contouring, RDP boundary reduction, Ear-Clipping triangulation, canvas vertex manipulation, multithreaded tight polygon packing, and Unity/Unreal/Godot mesh exports (22 tests) |
| `test_vram_compression` | Khronos KTX2 containers, UASTC 4x4 & ETC1S compression, Zstd supercompression ratio, RGBA transcoding roundtrip, and CLI KTX2 export pipelines (12 tests) |

---

## ⚡ Command-Line Interface (`spritestudio-cli`)

SpriteStudio includes an autonomous, 100% headless console binary **`spritestudio-cli`** designed for game studio build pipelines and CI/CD runners (zero GUI/display required):

### TexturePacker Drop-In Mode (with KTX2 VRAM Compression)
Replaces `TexturePacker` directly in existing build scripts without modifying Makefile or CMake commands:
```bash
# Standard PNG packing
spritestudio-cli --sheet atlas.png --data atlas.json \
  --format json-array --algorithm MaxRects --maxrects-heuristics BestShortSideFit \
  --padding 2 --extrude 1 --trim-mode Trim --size-constraints POT \
  --enable-auto-alias assets/sprites/*.png

# Direct GPU Hardware Texture Compression (KTX2 UASTC 4x4 with Zstd)
spritestudio-cli --sheet atlas.ktx2 --data atlas.json \
  --texture-format ktx2 --opt ASTC_4x4 --zstd-level 9 assets/sprites/*.png
```

### Aseprite Batch Mode
```bash
spritestudio-cli -b sprites/*.png --sheet atlas.png --data atlas.json \
  --sheet-type packed --list-tags
```

### Godot 4 Native Resource & Scene Generation
Generates complete `.tres` `SpriteFrames` with jitter-free margins, UID stability across builds, companion `.ktx2` texture, and an instantiable `.tscn` scene:
```bash
spritestudio-cli pack --format godot4 \
  --sheet res/player_atlas.ktx2 --data res/player_frames.tres \
  --godot-scene res/player.tscn assets/player/*.png
```

### Native Headless Slicing & Filters
```bash
# Auto-slice raw sheet into individual sprites with background removal
spritestudio-cli slice --remove-bg --tolerance 15 --smart-crop --output-dir out/ sheet.png

# Apply procedural outline headless
spritestudio-cli filter --outline 2 red --output-dir out/ sprites/*.png
```

### Reactive Watch & Multi-Instance Daemon (`--watch`)
Live background surveillance with intelligent burst debouncing and directory-isolated locking (`QLockFile`):
```bash
# Interactive watch with 300ms burst debouncing
spritestudio-cli pack --sheet atlas.png --data atlas.json --watch assets/sprites/

# Multi-instance daemon mode (isolated per directory)
spritestudio-cli pack --sheet char_atlas.png --data char.json --watch --daemon assets/characters/
spritestudio-cli pack --sheet ui_atlas.png --data ui.json --watch --daemon assets/ui/

# Stop active daemon guarding a specific folder
spritestudio-cli --stop-watch assets/characters/
```

### Transparent Drop-In Wrappers
Ready-to-use scripts located in `wrappers/` allow legacy makefiles to call `TexturePacker` or `aseprite` seamlessly:
```bash
# Windows
wrappers\TexturePacker.bat --sheet atlas.png --data atlas.json sprites/*.png

# Linux / macOS
wrappers/TexturePacker --sheet atlas.png --data atlas.json sprites/*.png
```

### Benchmarks & Regression Tracking
Execute the autonomous 24-scenario benchmark suite with performance diffing and KPI reporting:
```bash
python scripts/benchmark_cli.py
```
Reports are automatically written to [`benchmarks/REPORT.md`](benchmarks/REPORT.md) with historical tracking snapshots in `benchmarks/history/`.

---

## 📚 Documentation

Comprehensive documentation is available in the [`docs/`](docs/) directory:
* **[User Guide](docs/USER_GUIDE.md):** Complete, step-by-step illustrated manual covering atlas slicing, timeline animations, anchor pivots, filters, MaxRects & tight mesh packing, surgical pixel editing, and multi-engine exports.
* **[Developer & Extension Guide](docs/DEVELOPER_GUIDE.md):** Architectural deep-dive, step-by-step tutorial for writing custom I/O codecs (`Extractor`) and image filter plugins (`FilterPlugin`), memory scanline best practices, and headless unit testing.
* **API Reference (Doxygen):** Build the complete C++ API reference with interactive inheritance graphs by running `cmake --build build --target doxygen` (outputs to `build/docs/html/index.html`).
* **UNIX Manual Pages:** Traditional troff/groff manpages for [`spritestudio(1)`](docs/man/spritestudio.1) and [`spritestudio-cli(1)`](docs/man/spritestudio-cli.1).

---

## ⚔️ Comparison & Market Positioning

SpriteStudio bridges the gap between raw asset extraction/cleanup (historically handled by tools like *ShoeBox*), sprite atlas packing (*TexturePacker*), and animation sequencing (*Aseprite / Pixelorama / Godot SpriteFrames*).

| Feature / Criterion | **SpriteStudio** | **TexturePacker** | **Aseprite** | **Pixelorama** | **ShoeBox** *(Discontinued)* | **Free Texture Packer** | **Godot 4 Editor** *(Built-in)* |
| :--- | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| **License & Pricing** | **Open-Source (Apache 2.0)** | Commercial (~40€) | Commercial (~20€) / Source | **Open-Source (MIT)** | Free (Abandoned) | Open-Source (MIT) | Integrated (MIT) |
| **Core Technology** | C++17 / Qt 6 | C++ / Qt | C++ / Skia | GDScript / Godot Engine | Adobe AIR / Flash | Electron / Web | C++ / Godot Core |
| **Smart Atlas Slicing** | 🟢 **Advanced (O(N) SpatialGrid)** | 🔴 None (requires loose images) | 🟡 Basic | 🔴 None (canvas drawing) | 🟢 Historic pioneer | 🔴 None (requires loose files) | 🟡 Basic (Grid / Alpha) |
| **Live Filters & Background Cleanup** | 🟢 **Yes (Plugin Registry, Despill, Outline, Color Swap)** | 🔴 None | 🔴 Manual | 🟡 Basic image effects | 🟢 Historic (BG only) | 🔴 None | 🔴 None |
| **Timeline & Filmstrip** | 🟢 **Yes (Filmstrip, Ping-Pong)** | 🔴 None (Static preview) | 🟢 **Full animation studio** | 🟢 **Full animation studio** | 🔴 None | 🔴 None | 🟢 Engine-integrated |
| **Packing Algorithms** | 🟢 **MaxRects (5 heuristics), Tight Polygon Nesting (Multithreaded), Shelf, Grid, Auto-Alias, Extrude** | 🟢 **Industry Leader (MaxRects, Polygon)** | 🟡 Basic Sprite Sheet | 🟡 Basic Sprite Sheet | 🟡 Basic Shelf | 🟢 MaxRects | 🔴 Manual atlas |
| **2D Mesh & Tight Polygon Slicing** | 🟢 **Yes (Marching Squares, Ear-Clipping, Vertex Editor, Godot/Unity/Unreal)** | 🟢 Commercial Feature | 🔴 None | 🔴 None | 🔴 None | 🔴 None | 🟡 Collision Polygon only |
| **Surgical Pixel Editor** | 🟢 **Yes (Bresenham 1px, Wand, Retro Palettes, Stamp)** | 🔴 None | 🟢 **Full illustration editor** | 🟢 **Full illustration editor** | 🔴 None | 🔴 None | 🔴 None |
| **Anchor Points / Pivots** | 🟢 **Yes (Interactive Reticle, Zero-Jittering, Godot 4 / JSON)** | 🟢 Yes (All presets) | 🟢 Yes (Canvas origin) | 🟡 Canvas origin | 🟡 Basic | 🟢 Yes | 🟢 Yes |
| **Embedded Time-Travel** | 🟢 **Unique (Git / LibGit2 dock)** | 🔴 None | 🔴 Local undo only | 🔴 Local undo only | 🔴 None | 🔴 None | 🟡 External Git |
| **Godot 4 Integration** | 🟢 **Native (`.tres` SpriteFrames & `.tscn`)** | 🟢 Supported | 🟡 Via community plugins | 🟢 **Native (Built in Godot)** | 🔴 None | 🟡 JSON export | 🟢 Native |
| **Headless CLI for CI/CD** | 🟢 **Yes (`spritestudio-cli`, TexturePacker, Aseprite, Godot 4)** | 🟢 **Industry standard** | 🟢 Full CLI | 🟡 Basic Godot CLI flags | 🔴 None | 🟢 npm CLI | 🟢 Headless Godot |
| **VRAM Texture Compression** | 🟢 **Native KTX2, UASTC, ETC1S, Zstd, Direct GPU Zero-Decompress, CLI** | 🟢 **ASTC, ETC2, KTX2, Basis (Commercial)** | 🔴 PNG / GIF | 🔴 PNG | 🔴 PNG | 🟡 TinyPNG API | 🟢 Engine import |

> [!TIP]
> **Why SpriteStudio?** While *TexturePacker* excels at packing clean loose PNGs for AAA pipelines and *Aseprite* / *Pixelorama* are dedicated pixel art authoring tools, **SpriteStudio** is uniquely built to **rescue, decompile, clean, organize, and bridge existing 2D sheets** into production-ready game engine resources without external dependencies.

---

## 🗺️ Roadmap

- [x] **M0 — Architecture & Decoupling:** Standalone stateless codecs, `SpriteDocument` single source of truth, autonomous controllers
- [x] **M1 — Interactive Atlas Slicing:** 8 cosmetic handles, mouse-centered zoom, group drag, pixel-perfect nudge, alpha trim, frame merging
- [x] **M2 — Timeline & Animation Manager:** Filmstrip ribbon, scrubber with milliseconds counter, loop modes (Loop, Once, Ping-Pong), auto-play
- [x] **M5 — Native `.ssp` Project Format & Time-Travel:** ZIP container atomic saves, crash detection & recovery lock, LibGit2 continuous Git history dock
- [x] **M7 — Advanced Filter System & Cleanup:** Plugin registry (`FilterPlugin` / `FilterRegistry`), universal Undo (`ApplyFilterCommand`), live preview (`FilterDialogBase`), Despill/Anti-Halo (color clamping), Outline & Silhouettes, Color Swap (HSV shading)
- [x] **M3 — Interactive Pivots & Alignment:** High-contrast double-ring reticle, interactive pivot drag in atlas & live preview, Shift+Click snapping, zero-jittering animation envelope stabilization, ground line, cardinal presets (Bottom-Center, Center, Top-Left, UI), batch application, optimal fit & 5000% zoom, engine offset export (Godot 4 margin Rect2 / JSON / .ssp)
- [x] **M6 — Advanced Bin-Packing:** MaxRects (5 heuristics: BestShortSideFit, BestAreaFit, BestLongSideFit, BottomLeft, ContactPoint), padding, 1-2px extrusion anti-bleeding, Power-Of-Two / AnySize, auto-alias visual frame deduplication
- [x] **M-CLI — Headless Command-Line Interface:** `spritestudio-cli` with multi-flavor dispatch (TexturePacker drop-in, Aseprite batch, Godot 4 pipeline, native slice/filter/ssp), POSIX codes, JSON output, automated benchmarks & regression tracking
- [x] **M8 — Polygon & Tight Mesh Packing:** Watertight Marching Squares, Ramer-Douglas-Peucker boundary reduction with outward dilation, Ear-Clipping triangulation, interactive canvas vertex editor (drag, multi-select, insert, delete), multithreaded tight polygon nesting (configurable CPU threads), and multi-engine exports (Godot 4 `_mesh.tres`, Unity `.unity.json`, Unreal Paper2D `.paper2d.json`, TexturePacker JSON)
- [x] **M9 — VRAM Texture Compression & GPU Formats:** Universal Khronos KTX2 & Basis Universal (v2.50) integration, UASTC 4x4 (75.0% VRAM reduction) & ETC1S (87.5% VRAM reduction), lossless Zstandard supercompression, live VRAM telemetry in ExportDialog, CLI automation flags, 12 automated unit tests (100% CTest).
- [x] **M4 — Surgical Pixel Art Cleanup Editor:** Continuous 1px Bresenham pencil, 1px eraser (alpha 0), eyedropper, flood fill, rectangular/color wand selection, floating stamp clipboard, retro palettes (NES, SNES, Amiga, NEC, GB, Pico-8, C64) & dynamic sprite colors, pixel grid (≥400%), inter-frame navigation, reversible atlas synchronization
- [x] **M11 — Dynamic Plugin Architecture & Third-Party SDK:** Fully modular Qt6 dynamic plugin ecosystem (`QPluginLoader`), shared core library (`libSpriteStudioCore`), standalone external filter (`plugins/filters/`) and extractor (`plugins/extractors/`) modules, exported CMake package config (`SpriteStudioConfig.cmake`), and developer SDK templates (`examples/`).

---

---

## 📄 License

This project GUI and CLI are licensed under the **Apache License 2.0**. See the [LICENSE](LICENSE) file for details.
The plugins are licensed under defined [EULA](plugins/LICENSE-PLUGINS.md). Precompiled binaries are available for purchase on Stores.

**Developer:** Vincent LECOQ
