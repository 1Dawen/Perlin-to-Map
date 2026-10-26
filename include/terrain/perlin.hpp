#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <stdexcept>

namespace terrain {

// Deterministic, gradient-based two-dimensional noise. No graphics dependencies.
class PerlinNoise {
public:
    explicit PerlinNoise(std::uint32_t seed = 42u) : seed_(seed) {}

    std::uint32_t seed() const noexcept { return seed_; }

    static float fade(float t) noexcept {
        return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
    }

    static float lerp(float a, float b, float t) noexcept {
        return a + (b - a) * t;
    }

    float sample(float x, float y) const noexcept {
        const int x0 = static_cast<int>(std::floor(x));
        const int y0 = static_cast<int>(std::floor(y));
        const float tx = x - static_cast<float>(x0);
        const float ty = y - static_cast<float>(y0);
        const float sx = fade(tx);
        const float sy = fade(ty);
        const float a = lerp(dotGradient(x0, y0, x, y),
                             dotGradient(x0 + 1, y0, x, y), sx);
        const float b = lerp(dotGradient(x0, y0 + 1, x, y),
                             dotGradient(x0 + 1, y0 + 1, x, y), sx);
        return lerp(a, b, sy);
    }

    float fractal(float x, float y, int octaves, float persistence) const {
        validateOctaves(octaves, persistence);
        float sum = 0.0f;
        float amplitude = 1.0f;
        float frequency = 1.0f;
        float totalAmplitude = 0.0f;
        for (int octave = 0; octave < octaves; ++octave) {
            sum += amplitude * sample(x * frequency, y * frequency);
            totalAmplitude += amplitude;
            amplitude *= persistence;
            frequency *= 2.0f;
        }
        return sum / totalAmplitude;
    }

    // Ridged multifractal: folds every octave into a ridge before combining them.
    float ridged(float x, float y, int octaves, float persistence) const {
        validateOctaves(octaves, persistence);
        float sum = 0.0f;
        float amplitude = 1.0f;
        float frequency = 1.0f;
        float totalAmplitude = 0.0f;
        for (int octave = 0; octave < octaves; ++octave) {
            const float ridge = 1.0f - std::clamp(std::abs(sample(x * frequency, y * frequency)), 0.0f, 1.0f);
            sum += amplitude * ridge;
            totalAmplitude += amplitude;
            amplitude *= persistence;
            frequency *= 2.0f;
        }
        return std::clamp(sum / totalAmplitude, 0.0f, 1.0f);
    }

    static float terrace(float value, int steps) {
        if (steps < 1) throw std::invalid_argument("Terrace steps must be positive");
        if (steps == 1) return std::clamp(value, 0.0f, 1.0f);
        const float scaled = std::clamp(value, 0.0f, 1.0f) * static_cast<float>(steps);
        return std::floor(scaled) / static_cast<float>(steps);
    }

    // Full radial falloff: a circle of radius 1, zero at the boundary and outside.
    static float islandMask(float x, float z) noexcept {
        return std::clamp(1.0f - std::sqrt(x * x + z * z), 0.0f, 1.0f);
    }

private:
    std::uint32_t seed_;

    static void validateOctaves(int octaves, float persistence) {
        if (octaves < 1 || octaves > 16 || !(persistence > 0.0f && persistence <= 1.0f))
            throw std::invalid_argument("Octaves must be 1..16 and persistence must be (0,1]");
    }

    static std::uint32_t hash(int x, int y, std::uint32_t seed) noexcept {
        std::uint32_t h = static_cast<std::uint32_t>(x) * 374761393u;
        h ^= static_cast<std::uint32_t>(y) * 668265263u;
        h ^= seed + 0x9e3779b9u;
        h = (h ^ (h >> 13u)) * 1274126177u;
        return h ^ (h >> 16u);
    }

    float dotGradient(int ix, int iy, float x, float y) const noexcept {
        // 8 normalized gradient directions: low-cost, reproducible and platform independent.
        constexpr float d = 0.7071067811865475f;
        constexpr float gx[8] = {1, -1, 0, 0, d, -d, d, -d};
        constexpr float gy[8] = {0, 0, 1, -1, d, d, -d, -d};
        const unsigned choice = hash(ix, iy, seed_) & 7u;
        return gx[choice] * (x - static_cast<float>(ix)) +
               gy[choice] * (y - static_cast<float>(iy));
    }
};

}  // namespace terrain
