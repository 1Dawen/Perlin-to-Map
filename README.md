# Procedural Terrain Generation with Perlin Noise

**C++17 · Gradient noise · Fractal and ridged terrain · Biomes · OpenGL 3.3 · Dear ImGui**

An interactive 3D terrain generator. The terrain starts with a custom gradient-noise implementation and combines elevation and moisture maps to produce different biomes. An optional OpenGL viewer adds real-time parameter controls, terrain lighting, animated wireframe rendering, and simple snow/rain particles.

The code has been refactored from the complete original Visual Studio project (including the authored two-file implementation). The **headless terrain generator and its tests compile and run independently of OpenGL**. The graphical viewer is included but has not yet been built and verified independently of the historic Windows development environment; see [Project status](#project-status).

## Features

- Deterministic 2D gradient noise with a quintic fade curve, interpolation, and configurable seed.
- Normalized multi-octave fractal noise and multi-octave ridged noise.
- Adjustable noise scale, octaves, persistence, ridge blending, and elevation terraces.
- Island-shaped radial falloff with submerged coastlines.
- Separate moisture noise for biome classification and vertex colors.
- CPU mesh generation with correctly oriented indices and calculated vertex normals.
- Optional OpenGL 3.3 rendering with orbit camera, diffuse lighting, purple wireframe ripple, and snow/rain particles.
- OBJ mesh export and a PPM top-down biome preview without a graphics environment.

## Build the standalone generator (no graphics libraries needed)

Requirements: C++17 compiler and CMake 3.20 or later.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure

# Export the colored biome map and a 3D OBJ mesh.
./build/terrain_cli --grid 128 --seed 42 --island \
  --output outputs/terrain.ppm --obj outputs/terrain.obj
```

On Windows, use `build/Release/terrain_cli.exe` if your CMake generator produces configuration-specific subdirectories. The `.ppm` file can be opened with common image viewers or converted to PNG; the `.obj` mesh can be inspected in Blender or another 3D viewer.

Available CLI options: `--grid`, `--seed`, `--island`, `--terrace`, `--ridged-mix`, `--output`, `--obj` and `--help`.

## Build the interactive OpenGL viewer (optional)

The viewer requires OpenGL 3.3, GLFW, GLAD (1.x headers), GLM, and Dear ImGui with GLFW and OpenGL3 backends. A `vcpkg.json` manifest records the intended dependencies. With [vcpkg](https://github.com/microsoft/vcpkg) configured and installed, build with:

```bash
cmake -S . -B build-viewer \
  -DCMAKE_TOOLCHAIN_FILE=/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake \
  -DTERRAIN_BUILD_VIEWER=ON
cmake --build build-viewer --parallel
./build-viewer/terrain_viewer
```

On Windows, adapt the shell syntax and use the generated executable location for your build configuration. Linux may also require system packages for a windowing system and OpenGL development drivers. This viewer integration **has not yet been compiled or smoke-tested** here because the OpenGL dependencies were not available in the review environment.

### Viewer controls

| Control | Effect |
|---|---|
| Left mouse drag | Orbit the camera |
| Mouse wheel | Zoom in or out |
| Seed | Reproduce or change the generated terrain |
| Grid size, scale, octaves, persistence | Control terrain resolution and detail |
| Ridged blend, terraces, island mask | Change the terrain shape |
| Moisture and biome thresholds | Adjust generated biome regions |
| Purple wireframe | Animate a shader-based ripple |
| Snow / Rain | Toggle simple frame-rate-independent precipitation |

## How the terrain is generated

1. Hash integer lattice coordinates with a fixed seed to select gradients.
2. Interpolate gradient dot-products with the quintic fade curve to obtain smooth noise.
3. Combine octaves into a normalized fractal map, optionally blending in ridged noise.
4. Quantize elevations into terraces, or apply a radial island falloff.
5. Sample a separate seeded noise field for moisture and classify each vertex's biome.
6. Build a triangle grid, compute upward-facing vertex normals, and upload it for rendering or export it as an OBJ mesh.

The mathematical terrain generator has **no dependency on GLFW, OpenGL, or GLM**, allowing deterministic automated tests.

## Repository layout

```text
.
├── CMakeLists.txt
├── vcpkg.json
├── include/terrain/
│   ├── perlin.hpp          # Seeded 2D gradient noise and terrain modifiers
│   └── terrain.hpp         # Settings, terrain generator and mesh types
├── src/
│   ├── terrain.cpp         # Heightmap, moisture, biomes, normals and mesh
│   ├── terrain_cli.cpp     # Portable PPM/OBJ exporter
│   └── viewer.cpp          # Optional OpenGL/GLFW/ImGui application
├── tests/test_terrain.cpp
├── docs/
│   ├── academic_report.pdf
│   ├── generated_biome_preview.png
│   └── implementation_notes.md
└── .github/workflows/tests.yml
```

The [academic report](docs/academic_report.pdf) describes the original project and includes mathematical background. The [Visual Studio build notes](docs/original_build_configuration.md) explain why the historic project requires external library paths and how the refactored build differs. It is preserved as originally submitted; several algorithms and implementation details have been clarified or improved in this refactor. See [implementation notes](docs/implementation_notes.md) for specifics.

## Scope

The terrain is generated by deterministic mathematical noise and rule-based biome selection. The included preview demonstrates code output, not measured real-time rendering performance.
