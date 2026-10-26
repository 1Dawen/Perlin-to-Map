# Original 2025 implementation

This directory preserves the two authored C++ source files from the original Visual Studio project, without modification:

- `original_main.cpp`: OpenGL terrain viewer and ImGui controls.
- `original_perlin.hpp`: GLM-based gradient noise and terrain modifiers.

The source files were compared byte-for-byte with the earlier two-file uploads and match them. They are **historical source**, not part of the CMake build. For the portable, maintained code see `src/` and `include/terrain/`.

The complete original archive also contained a Visual Studio 2022 solution, Dear ImGui 1.91.9b source, `x64/Debug` executable/build outputs, `.vs` caches, and IDE-generated intermediate objects. These are not republished here. Its MSVC project referenced GLFW, GLAD and GLM from absolute paths outside the project archive, so the original Visual Studio solution is not directly buildable on another machine without configuration changes.

See [`docs/original_build_configuration.md`](../docs/original_build_configuration.md) for build provenance and limitations.
