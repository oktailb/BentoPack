# Sample Plugin for SpriteStudio (`spritestudio-dev`)

This directory contains a complete, minimal, independent third-party plugin example for SpriteStudio.
It demonstrates how external developers can write and compile dynamic plugins (`.so` on Linux, `.dll` on Windows) without cloning the main SpriteStudio repository, using only the `spritestudio-dev` development package.

## Building the plugin

```bash
# 1. Configure against your installed SpriteStudio or build tree
cmake -B build -DCMAKE_PREFIX_PATH=/path/to/spritestudio/install

# 2. Compile
cmake --build build

# 3. Test loading in SpriteStudio
export SPRITESTUDIO_PLUGIN_PATH=$(pwd)/build
spritestudio
```
