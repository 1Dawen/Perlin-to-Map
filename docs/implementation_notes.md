# Implementation notes and verification boundaries

## Material available during this refactor

- The original `main.cpp` and `perlin.hpp`, both previously uploaded individually and now verified against the full `finalproject.rar` source files.
- The five-page academic report, *3D Map Generation from Perlin Noise*.
- The original Visual Studio 2022 project/solution and bundled Dear ImGui 1.91.9b sources.

The complete RAR preserves a historical Windows build but references external graphics libraries outside the archive. Consequently, this package is a **cleaned implementation based on the original code**, not a claim that the original application was independently reproduced in its IDE environment. See [original build provenance](original_build_configuration.md).

## Differences from the earlier prototype and paper

| Area | Earlier sources | Current implementation |
|---|---|---|
| Noise seed | Original header used the current time; paper described a constant | Explicit, configurable `uint32_t` seed (42 by default) |
| Gradient selection | Original generated angles using cosine/sine | Eight fixed unit-gradient directions with a seeded hash; output shapes therefore differ |
| Fractal noise | Original header normalized by summed amplitude; report illustrated an unnormalized sum | Explicit normalized, multi-octave sum |
| Ridged noise | Original header performed one ridge transform; report described a multi-octave blend | Normalized multi-octave ridged noise |
| Island | Original mask leaned toward `0.5` elevation and disagreed with report's radial falloff | Circular falloff blending toward a value below `waterLevel` |
| Mesh normals | Original winding could produce negative-Y normals | Corrected triangle winding and tested upward normals |
| Particles | Original weather movement and spawn counts were frame-dependent | Viewer implementation uses delta-time scaling |
| Headless tests | None provided in original files | CMake core tests, CLI smoke test, OBJ/PPM exports |
| Graphics deps | Original Visual Studio x64 Debug setup referenced GLFW, GLAD and GLM through absolute paths outside the archive; bundled Dear ImGui 1.91.9b | Optional viewer with vcpkg manifest; build pending verification |

## Design choices

The pure C++ terrain core owns only noise evaluation, biome selection, settings validation, and mesh generation. The CLI exercises the same code as the viewer, while the viewer owns the GPU upload and ImGui controls.

The generated top-down PNG is produced by running the C++ CLI and converting the output PPM file to PNG. It is not an invented screenshot and should not be described as an OpenGL rendering.

The GUI's wireframe geometry shader now declares space for six emitted vertices (two per triangle edge). The original declared four, which was insufficient for three independent edges. Shader compilation and link status are checked rather than ignored.

The redesigned noise hash and ridge calculation change the output relative to the original. A fixed seed does **not** mean this implementation reproduces the 2025 project's exact landscapes.

## Checks performed in this release

- Configured and compiled the terrain core and CLI with CMake, C++17, and GCC.
- Ran tests covering repeatable seeds, seed diversity, fade curve endpoints, lattice behavior, normalized ridges, terraces, island coastline, mesh dimensions, upward unit normals, and invalid settings.
- Exercised the PPM/OBJ export path with CTest.
- Generated the included top-down preview using the C++ CLI.

The OpenGL viewer has **not** been compiled or run. The original project folder and its platform-specific configuration have now been inspected; independent GUI build verification and visual validation remain outstanding.
