# BentoPack Godot 4 Addon (`godot-bentopack-addon`)

Official Godot 4 integration plugin for **BentoPack** — Native 2D sprite slicing, texture packing, animation editing, and M8 polygonal mesh support.

---

## 🚀 Key Features

* **Zero-Configuration Import (`.bento` / `.ssp`)**: Drag and drop `.bento` project files into your Godot project. Godot automatically imports them into:
  * Optimised `SpriteFrames` resources with full timeline & FPS settings.
  * **Automatic M8 Polygon Clipping**: When tight polygonal packing is used, frames are automatically clipped pixel-perfect to their contours with non-overlapping repacking, completely eliminating neighbouring sprite bleed.
  * **Instant Hitbox / CollisionPolygon2D Generation**: Generates synchronized `CollisionPolygon2D` collider shapes matching the exact contour of every single animation frame. No manual hitbox tracing needed!
  * Ready-to-use companion scenes (`.tscn`) with configured `AnimatedSprite2D`, `BentoMeshSprite`, `Area2D` (Hitbox), and `AnimationPlayer`.
  * **M8 Tight Polygonal Meshes (`ArrayMesh`)**: GPU zero-transparency overdraw rendering via `BentoMeshSprite` / `MeshInstance2D`.
* **1-Click FileSystem Context Menu**: Right-click on any image (`.png`, `.webp`, `.jpg`) or `.bento` file directly in Godot's FileSystem dock for instant Auto-Slice, scene generation, or desktop editing.
* **Polygonal Sprite Player (`BentoMeshSprite`)**: Built-in `@tool` node for playing frame-by-frame polygonal mesh animations with live collision synchronization, horizontal/vertical flipping, and gameplay signals (`frame_changed`, `animation_finished`).
* **Inspector Integration**: Quick-action buttons in the Inspector when selecting `AnimatedSprite2D`, `Sprite2D`, or `SpriteFrames` to open the asset directly in BentoPack.
* **CLI & Automation**: Seamless connection with `bentopack-cli` (or legacy `bentopack-cli`) for live re-packing and watch daemon support, with built-in one-click release downloader when the CLI is missing.

---

## 📦 Installation

1. Copy the `addons/bentopack` directory into your Godot project's `addons/` folder:
   ```
   my_godot_project/
   ├── addons/
   │   └── bentopack/
   │       ├── plugin.cfg
   │       ├── bentopack_plugin.gd
   │       └── ...
   └── project.godot
   ```
2. In Godot 4, open **Project -> Project Settings -> Plugins**.
3. Enable the **BentoPack Integration** plugin.

---

## 🛠️ Testing Locally

You can test the importer directly using the headless Godot test suite:

```bash
# Run standalone unit test:
godot --headless -s tests/test_godot_plugin.gd

# Or run within the demo Godot project:
godot --headless --path examples/godot_demo/ -s res://test_plugin.gd
```
