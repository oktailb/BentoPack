# Sample Plugin for BentoPack (`bentopack-dev`)

This directory contains a complete, minimal, independent third-party plugin example for BentoPack.
It demonstrates how external developers can write and compile dynamic plugins (`.so` on Linux, `.dll` on Windows) without cloning the main BentoPack repository, using only the `bentopack-dev` development package.

## Building the plugin

```bash
# 1. Configure against your installed BentoPack or build tree
cmake -B build -DCMAKE_PREFIX_PATH=/path/to/bentopack/install

# 2. Compile
cmake --build build

# 3. Test loading in BentoPack
export BENTOPACK_PLUGIN_PATH=$(pwd)/build
bentopack
```
