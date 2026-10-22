#pragma once

#include <cstdint>
#include <vector>

#include "terrain/perlin.hpp"

namespace terrain {

struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct Vertex {
    Vec3 position;
    Vec3 normal;
    Vec3 color;
};

enum class Biome {
    Water, Beach, Desert, Savannah, Forest, Taiga, Grassland, Rainforest, Snow
};

struct Settings {
    int gridSize = 128;
    std::uint32_t seed = 42u;
    float noiseScale = 2.0f;
    int octaves = 4;
    float persistence = 0.5f;
    float ridgedMix = 0.0f;
    bool terraceEnabled = false;
    int terraceSteps = 5;
    bool islandEnabled = false;
    float moistureScale = 2.5f;
    int moistureOctaves = 3;
    float moisturePersistence = 0.5f;
    float waterLevel = 0.20f;
    float beachLevel = 0.26f;
    float snowLevel = 0.82f;
    float heightScale = 0.9f;

    void validate() const;
};

struct Mesh {
    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices;
};

class TerrainGenerator {
public:
    explicit TerrainGenerator(Settings settings = {});

    const Settings& settings() const noexcept { return config_; }
    float elevation(float x, float z) const;
    float moisture(float x, float z) const;
    Biome classify(float elevation, float moisture) const noexcept;
    Mesh generate() const;

    static Vec3 biomeColor(Biome biome) noexcept;

private:
    Settings config_;
    PerlinNoise elevationNoise_;
    PerlinNoise moistureNoise_;
};

}  // namespace terrain
