# Original Visual Studio project: build provenance

## New material received

The uploaded `finalproject.rar` contains 99 RAR entries, including:

- `finalproject/main.cpp`, `finalproject/perlin.hpp`: original application source, **identical** to the earlier individually uploaded C++ files.
- `finalproject/finalproject.sln`, `finalproject/finalproject.vcxproj`: Visual Studio 2022 solution and MSVC C++ project (`v143`), with `Win32` and `x64` configurations.
- `finalproject/imgui/`: Dear ImGui **1.91.9b** source with GLFW and OpenGL3 backends.
- A Windows x64 Debug executable and compiler/linker output (`x64/Debug`) as well as `.vs` IDE databases and caches.

The original `.vcxproj` lists the external GLAD source `..\..\librerias opengl\GL\glad\src\glad.c`. In its Debug x64 configuration, header and library search paths point to a machine-specific `C:\Estudio\...` installation. It explicitly references `glfw3.lib`, `glfw3dll.lib`, `freeglut.lib`, and `opengl32.lib`. **GLFW, GLAD and GLM development trees/binaries are absent from the uploaded RAR**. The presence of an executable shows a past local build artifact; it does not verify the reproducibility of that binary or its runtime behavior in this environment.

### Why the GitHub version uses CMake

The maintained source is organized around a standalone, headless C++ terrain library, a command-line exporter, and an **optional** interactive OpenGL viewer. The `vcpkg.json` manifest specifies the graphics dependencies without machine-specific paths. This avoids bundling a large third-party GUI library, IDE caches, proprietary toolchain artifacts, or a historical executable into the public repository.

The original ImGui version is documented for reference but **not vendored** into the refactor; the optional graphics viewer uses the version supplied by the user's dependency manager.

### Status of the GUI

The original project source has been cross-checked with the refactored viewer. The refactor retains interactive terrain controls, camera orbit/zoom, biome shading, animated wireframe and precipitation; it also corrects geometry-shader vertex limits and makes precipitation time-based. The headless components compile and pass tests. The refactored OpenGL GUI has not been built or visually exercised here because the required graphics development packages/context are unavailable; this remains a separate verification step.
