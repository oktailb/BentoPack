# BentoPack Unreal Engine 5 Integration (`BentoPack`)

Official Unreal Engine 5 integration plugin for **BentoPack** — Native 2D sprite slicing, texture packing, Flipbook generation, and M8 tight polygonal mesh support (`RenderGeometry`).

---

## 🚀 Key Features

* **Zero-Configuration Import (`.bento` & `.paper2d.json`)**: Drag and drop BentoPack archives or Paper2D descriptors directly into your Unreal Engine Content Browser. Unreal automatically generates:
  * Packed `UTexture2D` atlas configured for crisp pixel art (`TextureFilter = TF_Nearest`, `Address = TA_Clamp`).
  * Sliced `UPaperSprite` assets with normalized custom pivot points.
  * **M8 Tight Polygonal Meshes (`RenderGeometry`)**: Eliminates transparent fillrate overdraw on the GPU (60% to 80% bandwidth savings) directly on standard `UPaperSprite` / `UPaperFlipbook` without custom shaders.
  * Ready-to-use `UPaperFlipbook` animation assets sampled at exact FPS and loop settings.
* **PaperZD Native Interoperability**: Generated `UPaperFlipbook` assets interface seamlessly with PaperZD animation state machines.
* **1-Click Content Browser Context Menu**: Right-click on any BentoPack flipbook or sprite to open directly in BentoPack Studio.
* **Interactive Dashboard (`Window -> BentoPack Dashboard`)**: In-editor Slate control center to pack raw texture atlases, configure Auto-Slice vs. Fixed Grid, select packing algorithms, and manage background watch daemons.
* **Automatic CLI Bridge**: Communicates with `bentopack-cli` for automated packing and watch-mode hot-reload workflows.

---

## 📦 Installation

### Method 1: Project Plugins Folder (Recommended)
1. Copy or extract the `BentoPack/` plugin directory into your Unreal project's `Plugins/` folder:
   ```text
   YourProject/
   ├── Content/
   ├── Plugins/
   │   └── BentoPack/
   │       ├── BentoPack.uplugin
   │       ├── Source/
   │       └── Resources/
   └── YourProject.uproject
   ```
2. Open your project in Unreal Engine 5.
3. In **Edit -> Plugins**, verify that **BentoPack** and **Paper2D** are enabled.
4. Restart the editor if prompted.

### Method 2: Fab Marketplace
1. Acquire the plugin from **Fab** (`fab.com`).
2. Install via the Epic Games Launcher to your target Unreal Engine 5 engine version.

---

## 🛠️ Usage Workflow

1. In BentoPack Studio, export or save your project as a `.bento` archive or choose **Unreal Engine Paper2D (*.paper2d.json)**.
2. Drag the `.bento` or `.paper2d.json` file into your Unreal Engine Content Browser.
3. BentoPack's `UBentoFactory` automatically generates textures, tight-mesh `UPaperSprite` assets, and `UPaperFlipbook` animations.
4. Drag generated Flipbooks directly into your 2D/3D level or use in your PaperZD animation blueprints!
