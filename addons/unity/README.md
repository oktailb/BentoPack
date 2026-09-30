# BentoPack Unity Integration (`com.bentopack.importer`)

Official Unity Package Manager (UPM) integration plugin for **BentoPack** — Native 2D sprite slicing, texture packing, AnimationClip generation, and M8 tight polygonal mesh support (`Sprite.OverrideGeometry`).

---

## 🚀 Key Features

* **Zero-Configuration Import (`.bento`)**: Drag and drop `.bento` project files into your Unity project's `Assets/` directory. Unity automatically generates:
  * Packed `Texture2D` atlas configured for crisp pixel art (`FilterMode.Point`, `WrapMode.Clamp`).
  * Sliced `Sprite` sub-assets with normalized custom pivot points.
  * **M8 Tight Polygonal Meshes (`Sprite.OverrideGeometry`)**: GPU overdraw reduction (60% to 80% fillrate savings) directly on standard `SpriteRenderer` without custom shaders.
  * Ready-to-use `AnimationClip` assets with frame curves sampled at exact FPS and loop settings.
* **1-Click Project Window Context Menu**: Right-click on any `.bento` archive or sprite sheet (`.png`, `.webp`, `.jpg`) in Unity's Project Window to open directly in BentoPack Studio or re-pack instantly via CLI.
* **Tight Mesh Sprite Player (`BentoMeshSprite`)**: Runtime component for frame-by-frame polygonal mesh animation with optional synchronized `PolygonCollider2D` hitbox tracing.
* **Interactive Dashboard (`Window -> BentoPack Dashboard`)**: In-editor control center to pack raw texture atlases, configure Auto-Slice vs. Fixed Grid (Tile W/H), select packing algorithms (MaxRects / M8 Tight Polygon), and manage background watch daemons.
* **Automatic CLI Bridge**: Communicates with `bentopack-cli` for automated packing and watch-mode workflows.

---

## 📦 Installation via Unity Package Manager (UPM)

### Method 1: Git URL (Recommended)
1. In Unity, open **Window -> Package Manager**.
2. Click the **+** button in the top-left corner and select **Add package from git URL...**.
3. Enter the repository URL with the unity subpath:
   ```
   https://github.com/oktailb/BentoPack.git?path=addons/unity
   ```

### Method 2: Local Package
1. Clone or copy `addons/unity` into your Unity project's `Packages/` folder or a local directory.
2. In Package Manager, select **Add package from disk...** and choose `addons/unity/package.json`.

### Method 3: Tarball Package (`.tgz`)
1. Download `com.bentopack.importer-<version>.tgz` from the BentoPack release artifacts.
2. In Unity, open **Window -> Package Manager**.
3. Click the **+** button in the top-left corner and select **Add package from tarball...**.
4. Select the downloaded `com.bentopack.importer-<version>.tgz` file.

---

## 🛠️ Usage Workflow

1. In BentoPack Studio, export or save your project as a `.bento` archive (or export via `Unity 2D Sprite Mesh (*.unity.json)`).
2. Drag the `.bento` file into your Unity `Assets/` folder.
3. Unity's `BentoImporter` automatically processes the atlas, injects tight geometry, and generates animation clips.
4. Drag generated sprites or animation clips directly into your 2D Scene!
