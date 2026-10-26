#include "terrain/terrain.hpp"

#include <cassert>
#include <cmath>
#include <iostream>
#include <stdexcept>

using terrain::PerlinNoise;
using terrain::Settings;
using terrain::TerrainGenerator;

int main() {
    int checks = 0;
    auto check = [&](bool ok, const char* name) {
        if (!ok) throw std::runtime_error(name);
        ++checks;
    };
    const PerlinNoise n1(42), n2(42), n3(43);
    check(n1.sample(1.23f, -4.56f) == n2.sample(1.23f, -4.56f), "seed repeatability");
    check(std::abs(n1.sample(1.23f, -4.56f) - n3.sample(1.23f, -4.56f)) > 1e-6,
          "different seeds should change samples");
    check(PerlinNoise::fade(0) == 0 && PerlinNoise::fade(1) == 1, "fade endpoints");
    check(std::abs(n1.sample(2.0f, 3.0f)) < 1e-6, "lattice point zero");
    check(std::isfinite(n1.fractal(0.13f, 0.54f, 4, 0.5f)), "finite fractal");
    check(n1.ridged(0.5f, 0.5f, 4, 0.5f) >= 0 && n1.ridged(0.5f, 0.5f, 4, 0.5f) <= 1,
          "bounded ridge");
    check(PerlinNoise::islandMask(0, 0) == 1 && PerlinNoise::islandMask(1, 0) == 0,
          "island boundary");
    check(PerlinNoise::terrace(0.67f, 4) == 0.5f, "terrace quantization");
    Settings settings;
    settings.gridSize = 20;
    TerrainGenerator generator(settings);
    const auto mesh = generator.generate();
    check(mesh.vertices.size() == 21u * 21u, "mesh vertices");
    check(mesh.indices.size() == 6u * 20u * 20u, "mesh indices");
    bool validNormals = true;
    for (const auto& vertex : mesh.vertices) {
        const float len = std::sqrt(vertex.normal.x * vertex.normal.x +
                                    vertex.normal.y * vertex.normal.y +
                                    vertex.normal.z * vertex.normal.z);
        validNormals &= std::isfinite(len) && std::abs(len - 1.0f) < 1e-4f && vertex.normal.y > 0;
    }
    check(validNormals, "unit upward vertex normals");
    Settings islandSettings = settings;
    islandSettings.islandEnabled = true;
    TerrainGenerator island(islandSettings);
    check(island.elevation(1, 0) < islandSettings.waterLevel, "submerged island shoreline");
    check(island.elevation(0, 0) >= islandSettings.waterLevel, "island center above water");
    check(generator.elevation(0.21f, -0.67f) == TerrainGenerator(settings).elevation(0.21f, -0.67f),
          "terrain repeatability");
    check(generator.classify(0.0f, 0.5f) == terrain::Biome::Water, "water biome");
    check(generator.classify(0.99f, 0.5f) == terrain::Biome::Snow, "snow biome");
    bool rejectsInvalid = false;
    try { Settings bad = settings; bad.gridSize = 0; TerrainGenerator t(bad); }
    catch (const std::invalid_argument&) { rejectsInvalid = true; }
    check(rejectsInvalid, "invalid configuration rejected");
    std::cout << checks << " checks passed\n";
}
