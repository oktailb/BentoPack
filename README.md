# 🌟 SpriteStudio

[![CI](https://github.com/oktailb/SpriteStudio/actions/workflows/ci.yml/badge.svg)](https://github.com/oktailb/SpriteStudio/actions/workflows/ci.yml)
[![License](https://img.shields.io/badge/License-Apache_2.0-blue.svg)](https://opensource.org/licenses/Apache-2.0)
[![C++17](https://img.shields.io/badge/C%2B%2B-17-00599C.svg?logo=c%2B%2B)](https://en.cppreference.com/w/cpp/17)
[![Qt 6](https://img.shields.io/badge/Qt-6.5+-41CD52.svg?logo=qt)](https://www.qt.io/)
[![CMake](https://img.shields.io/badge/CMake-3.20+-064F8C.svg?logo=cmake)](https://cmake.org/)

> **SpriteStudio** is a fast, modular, and modern desktop application tailored for game developers, pixel artists, and 2D animators to extract, clean, arrange, and export 2D sprite sheets and animated textures.

---

![SpriteStudio Interface Demo](SpriteStudio.gif)

---

## ✨ Key Features

### 📥 Intelligent Extraction & Import
* **Smart Sprite Sheet Slicing:** Automatic sprite detection and extraction with configurable alpha and color tolerance.
* **Animated GIF Decompilation:** Instant extraction of multi-frame animated GIFs into ordered frame sequences.
* **Format Parsers:** Import existing sprite atlases with associated **JSON** (TexturePacker / Aseprite) or **Godot 4 `.tres`** metadata.
* **Non-Destructive Background Removal:** Pick transparent chroma colors and remove background colors with instant visual feedback.

![Background Removal Demo](RemoveBackground.gif)

### 🎬 Timeline & Animation Filmstrip
* **Filmstrip Dock:** Intuitive bottom timeline with thumbnail filmstrip, scrub bar, and drag-and-drop frame reordering.
* **Frame Merging & Compositing:** Drag and drop one frame onto another to fuse them into a single layered sprite.
* **Batch Editing:** Multi-selection operations including group deletion, invert selection, and reverse frame ordering (ideal for ping-pong loops).
* **Real-time Playback:** High-precision preview engine with configurable FPS (1 to 60 FPS), loop controls, and aspect-ratio stabilization.

![Frame Fusion Demo](Fusion.gif)

### 💾 Native `.ssp` Project Architecture
* **All-in-One Compressed Format:** `.ssp` project files package project metadata, frame descriptors, and source assets into a structured ZIP archive.
* **Atomic Transactions & Crash Recovery:** Atomic file writing with journaled crash-recovery safeguard prevents project corruption.
* **Embedded Git Time-Travel:** Integrated non-destructive versioning engine powered by LibGit2. Browse commit history, inspect visual diffs, and revert to previous states without leaving the app.

### 📤 Multi-Engine Export
* **PNG Sprite Atlas:** Optimized packing of extracted frames into a consolidated texture sheet.
* **Godot 4 Engine Exporter:** Generates ready-to-use Godot 4 `SpriteFrames` (`.tres`) resources with embedded `AtlasTexture` definitions and animations.
* **TexturePacker / Aseprite JSON:** Universal JSON metadata mapping frame bounds `(x, y, w, h)` and animation tags.

---

## 🛠️ Tech Stack & Architecture

* **Language:** C++17
* **Framework:** Qt 6 (Core, Gui, Widgets, Multimedia, Concurrent, Test, LinguistTools)
* **Build System:** CMake 3.20+ with modular architecture (`SpriteStudioCore` static engine + `SpriteStudio` app + automated CTest test suite)
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

---

## 🗺️ Roadmap

- [x] Native `.ssp` compressed project format with atomic saves
- [x] Embedded Git time-travel dock
- [x] Godot 4 `SpriteFrames` exporter
- [x] Factorized CMake architecture (`SpriteStudioCore`) & automated CTest suites
- [ ] **M3 — Interactive Pivots & Alignment:** Visual crosshair gizmo, batch alignment presets (Bottom-Center, Top-Left), bounding box stabilization
- [ ] **M4 — In-App Pixel Art & Cleanup Editor:** 1-bit pencil, eraser, color picker, onion skinning, flood fill, transparency mask brush
- [ ] **M5 — Advanced Exporters:** Unity `.anim` / `SpriteSheet`, Defold atlas, Unreal Engine paper2D metadata

---

## 📄 License

This project is licensed under the **Apache License 2.0**. See the [LICENSE](LICENSE) file for details.

**Developer:** Vincent LECOQ
