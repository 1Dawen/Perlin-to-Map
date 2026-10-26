#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <cmath>
#include <ctime>
#include <algorithm>

namespace perlin {
    // Seed for pseudo-random gradients
    static const unsigned int SEED = static_cast<unsigned int>(std::time(nullptr));

    // 1. Linear interpolation
    inline float lerp(float a, float b, float t) {
        return a + t * (b - a);
    }

    // 2. Smooth fade curve (6t^5 − 15t^4 + 10t^3)
    inline float fade(float t) {
        return t * t * t * (t * (t * 6.f - 15.f) + 10.f);
    }

    // 3. Pseudorandom gradient at integer lattice points
    inline glm::vec2 grad2d(int ix, int iy) {
        unsigned int h = ix * 374761393u + iy * 668265263u + SEED;
        h = (h ^ (h >> 13u)) * 1274126177u;
        h = h ^ (h >> 16u);
        float angle = (h % 360u) * (3.14159265359f / 180.f);
        return glm::vec2(std::cos(angle), std::sin(angle));
    }

    // 4. Dot product of gradient with distance vector
    inline float dotGridGradient(int ix, int iy, float x, float y) {
        glm::vec2 g = grad2d(ix, iy);
        return g.x * (x - float(ix)) + g.y * (y - float(iy));
    }

    // 5. Single-point 2D Perlin noise sample
    inline float perlin2d(float x, float y) {
        int x0 = int(std::floor(x)), x1 = x0 + 1;
        int y0 = int(std::floor(y)), y1 = y0 + 1;

        float tx = x - float(x0);
        float ty = y - float(y0);
        float sx = fade(tx);
        float sy = fade(ty);

        float n0 = dotGridGradient(x0, y0, x, y);
        float n1 = dotGridGradient(x1, y0, x, y);
        float ix0 = lerp(n0, n1, sx);

        n0 = dotGridGradient(x0, y1, x, y);
        n1 = dotGridGradient(x1, y1, x, y);
        float ix1 = lerp(n0, n1, sx);

        return lerp(ix0, ix1, sy);
    }

    // 6. Fractal (multi-octave) Perlin noise
    inline float fractalNoise2d(float x, float y,
        int octaves = 4,
        float persistence = 0.5f) {
        float total = 0.f;
        float amplitude = 1.f;
        float frequency = 1.f;
        float maxAmp = 0.f;

        for (int i = 0; i < octaves; ++i) {
            total += perlin2d(x * frequency, y * frequency) * amplitude;
            maxAmp += amplitude;
            amplitude *= persistence;
            frequency *= 2.f;
        }
        return total / maxAmp;  // normalized to [-1,1]
    }

    // 7. Ridge noise
    inline float ridgedNoise(float x, float y) {
        float n = perlin2d(x, y) * 0.5f + 0.5f;  // normalize to [0,1]
        return 1.f - std::abs(2.f * n - 1.f);
    }

    // 8. Terracing
    inline float terrace(float e, int steps) {
        return std::floor(e * float(steps)) / float(steps);
    }

    // 9. Island masks (square bump & radial)
    inline float squareBump(float nx, float ny) {
        return 1.f - (1.f - nx * nx) * (1.f - ny * ny);
    }

    inline float radialMask(float nx, float ny) {
        return std::min(1.f, (nx * nx + ny * ny) / std::sqrt(2.f));
    }

    inline float applyIsland(float e, float d, float waterLevel = 0.5f) {
        return lerp(e, waterLevel, d);
    }

    // 10. Generate an RGB noise texture using multi-octave noise
    inline std::vector<unsigned char> GenerateNoise(int width,
        int height,
        float scale,
        int octaves = 4,
        float persistence = 0.5f) {
        std::vector<unsigned char> data(width * height * 3);
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                float nx = float(x) / float(width) * scale;
                float ny = float(y) / float(height) * scale;
                float e = fractalNoise2d(nx, ny, octaves, persistence);
                unsigned char v = static_cast<unsigned char>((e * 0.5f + 0.5f) * 255.f);
                int idx = (y * width + x) * 3;
                data[idx + 0] = v;
                data[idx + 1] = v;
                data[idx + 2] = v;
            }
        }
        return data;
    }
} // namespace perlin
