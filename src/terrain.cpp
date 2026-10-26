#include "terrain/terrain.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace terrain {
namespace {
Vec3 subtract(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
Vec3 cross(Vec3 a, Vec3 b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
void add(Vec3& a, Vec3 b) { a.x += b.x; a.y += b.y; a.z += b.z; }
Vec3 normalized(Vec3 a) {
    const float length = std::sqrt(a.x * a.x + a.y * a.y + a.z * a.z);
    return length > 1e-9f ? Vec3{a.x / length, a.y / length, a.z / length} : Vec3{0.0f, 1.0f, 0.0f};
}
} // namespace

void Settings::validate() const {
    if (gridSize < 2 || gridSize > 1024) throw std::invalid_argument("Grid size must be 2..1024");
    if (!(noiseScale > 0.0f) || !(moistureScale > 0.0f))
        throw std::invalid_argument("Noise scales must be positive");
    if (octaves < 1 || octaves > 16 || moistureOctaves < 1 || moistureOctaves > 16)
        throw std::invalid_argument("Octaves must be 1..16");
    if (!(persistence > 0.0f && persistence <= 1.0f) ||
        !(moisturePersistence > 0.0f && moisturePersistence <= 1.0f))
        throw std::invalid_argument("Persistence must be in (0,1]");
    if (!(ridgedMix >= 0.0f && ridgedMix <= 1.0f)) throw std::invalid_argument("Ridged blend must be 0..1");
    if (terraceSteps < 1) throw std::invalid_argument("Terrace steps must be positive");
    if (!(0.0f <= waterLevel && waterLevel < beachLevel && beachLevel < snowLevel && snowLevel <= 1.0f))
        throw std::invalid_argument("Biome thresholds must satisfy 0 <= water < beach < snow <= 1");
    if (!(heightScale > 0.0f)) throw std::invalid_argument("Height scale must be positive");
}

TerrainGenerator::TerrainGenerator(Settings settings)
    : config_(settings), elevationNoise_(settings.seed), moistureNoise_(settings.seed ^ 0x85ebca6bu) {
    config_.validate();
}

float TerrainGenerator::elevation(float x, float z) const {
    const float nx = x * config_.noiseScale;
    const float nz = z * config_.noiseScale;
    const float base = 0.5f + 0.5f * elevationNoise_.fractal(nx, nz, config_.octaves, config_.persistence);
    const float ridge = elevationNoise_.ridged(nx, nz, config_.octaves, config_.persistence);
    float e = PerlinNoise::lerp(base, ridge, config_.ridgedMix);
    e = std::clamp(e, 0.0f, 1.0f);
    if (config_.terraceEnabled) e = PerlinNoise::terrace(e, config_.terraceSteps);
    if (config_.islandEnabled) {
        // At the coastline, blend down to a level *below* water to expose water biomes.
        const float underwater = std::max(0.0f, config_.waterLevel - 0.05f);
        e = PerlinNoise::lerp(underwater, e, PerlinNoise::islandMask(x, z));
    }
    return std::clamp(e, 0.0f, 1.0f);
}

float TerrainGenerator::moisture(float x, float z) const {
    const float raw = moistureNoise_.fractal(x * config_.moistureScale, z * config_.moistureScale,
                                            config_.moistureOctaves, config_.moisturePersistence);
    return std::clamp(0.5f + 0.5f * raw, 0.0f, 1.0f);
}

Biome TerrainGenerator::classify(float e, float m) const noexcept {
    if (e < config_.waterLevel) return Biome::Water;
    if (e < config_.beachLevel) return Biome::Beach;
    if (e >= config_.snowLevel) return Biome::Snow;
    if (e > 0.65f) return m < 0.30f ? Biome::Desert : m < 0.70f ? Biome::Forest : Biome::Taiga;
    if (m < 0.30f) return Biome::Desert;
    if (m < 0.45f) return Biome::Savannah;
    if (m < 0.70f) return Biome::Grassland;
    return Biome::Rainforest;
}

Vec3 TerrainGenerator::biomeColor(Biome biome) noexcept {
    switch (biome) {
        case Biome::Water:      return {0.08f, 0.24f, 0.72f};
        case Biome::Beach:      return {0.85f, 0.79f, 0.54f};
        case Biome::Desert:     return {0.91f, 0.73f, 0.37f};
        case Biome::Savannah:   return {0.70f, 0.67f, 0.30f};
        case Biome::Forest:     return {0.20f, 0.49f, 0.25f};
        case Biome::Taiga:      return {0.23f, 0.41f, 0.32f};
        case Biome::Grassland:  return {0.29f, 0.67f, 0.33f};
        case Biome::Rainforest: return {0.09f, 0.37f, 0.19f};
        case Biome::Snow:       return {0.94f, 0.96f, 0.97f};
    }
    return {1.0f, 0.0f, 1.0f};
}

Mesh TerrainGenerator::generate() const {
    const auto n = static_cast<std::size_t>(config_.gridSize);
    Mesh mesh;
    mesh.vertices.reserve((n + 1) * (n + 1));
    mesh.indices.reserve(6 * n * n);
    for (std::size_t j = 0; j <= n; ++j) {
        for (std::size_t i = 0; i <= n; ++i) {
            const float x = -1.0f + (2.0f * static_cast<float>(i) / static_cast<float>(n));
            const float z = -1.0f + (2.0f * static_cast<float>(j) / static_cast<float>(n));
            const float e = elevation(x, z);
            mesh.vertices.push_back({{x, e * config_.heightScale, z}, {0, 0, 0},
                                      biomeColor(classify(e, moisture(x, z)))});
        }
    }
    for (std::size_t j = 0; j < n; ++j) {
        for (std::size_t i = 0; i < n; ++i) {
            const auto a = static_cast<std::uint32_t>(j * (n + 1) + i);
            const auto b = a + 1;
            const auto c = a + static_cast<std::uint32_t>(n + 1);
            const auto d = c + 1;
            // Winding is counterclockwise from above: positive Y face normals.
            mesh.indices.insert(mesh.indices.end(), {a, c, b, b, c, d});
        }
    }
    for (std::size_t i = 0; i < mesh.indices.size(); i += 3) {
        auto& a = mesh.vertices[mesh.indices[i]];
        auto& b = mesh.vertices[mesh.indices[i + 1]];
        auto& c = mesh.vertices[mesh.indices[i + 2]];
        const Vec3 nrm = cross(subtract(b.position, a.position), subtract(c.position, a.position));
        add(a.normal, nrm); add(b.normal, nrm); add(c.normal, nrm);
    }
    for (auto& vertex : mesh.vertices) vertex.normal = normalized(vertex.normal);
    return mesh;
}

} // namespace terrain
