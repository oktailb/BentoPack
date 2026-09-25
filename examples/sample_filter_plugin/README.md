# BentoPack Sample Filter Plugin

This example demonstrates how to write, build, and package an external filter plugin for **BentoPack** using the `bentopack-dev` SDK and Qt 6.

## Structure

- `sample_filter.h`: Declares the filter implementing `FilterPlugin` and `Q_PLUGIN_METADATA(IID FilterPlugin_iid)`.
- `sample_filter.cpp`: Implements the direct image processing algorithm (`applyImage()`).
- `CMakeLists.txt`: Uses `find_package(BentoPack REQUIRED)` and links against `BentoPack::BentoPackCore`.

## Building and Installation

```bash
mkdir build && cd build
cmake .. -DCMAKE_PREFIX_PATH="/path/to/bentopack/install"
cmake --build .
```

To load the compiled plugin into BentoPack, copy the resulting shared library (`.so` / `.dll` / `.dylib`) into BentoPack's `plugins/filters/` directory or point `BENTOPACK_PLUGIN_PATH` to its location.
