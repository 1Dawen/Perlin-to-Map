
// main3d_dynamic.cpp
// 3D procedural terrain with Perlin noise, terraces, islands, biomes, falling snow and rain,
// plus an optional geometry-shader wireframe mode with pulsating effect

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include "perlin.hpp"
#include <vector>
#include <cstdlib>
#include <ctime>
#include <cmath>
#include <iostream>

// Window dimensions
static const int WINDOW_W = 1024;
static const int WINDOW_H = 768;

// Terrain parameters (adjustable in GUI)
static int   gridSize = 128;
static float noiseScale = 2.0f;
static int   octaves = 4;
static float persistence = 0.5f;
static float ridgedMix = 0.0f;
static bool  useTerrace = false;
static int   terraceSteps = 5;
static bool  useIsland = false;
static float moistureScale = 2.5f;
static int   moistureOctaves = 3;
static float moisturePersistence = 0.5f;
static float waterLevel = 0.1f;
static float beachLevel = 0.12f;
static float snowLevel = 0.8f;
static bool  dirty = true;

// Particle parameters
static bool  enableSnow = false;
static std::vector<glm::vec3> snowflakes;
static GLuint snowVAO = 0, snowVBO = 0;
static bool  enableRain = false;
static std::vector<glm::vec3> raindrops;
static GLuint rainVAO = 0, rainVBO = 0;

// Toggle geometric wireframe mode
static bool  useGeo = false;

// Camera state
static float yaw = -45.0f;
static float pitch = 30.0f;
static float distanceToCenter = 3.0f;
static double lastX = WINDOW_W * 0.5;
static double lastY = WINDOW_H * 0.5;
static bool rotating = false;

// OpenGL objects
static GLuint terrainVAO = 0, terrainVBO = 0, terrainEBO = 0;
static size_t terrainIndexCount = 0;
static GLuint terrainShader = 0;
static GLuint particleShader = 0;
static GLuint geoProgram = 0;

// Vertex format
struct Vertex {
    glm::vec3 Position;
    glm::vec3 Normal;
    glm::vec3 Color;
};

// Biome enumeration
enum Biome { WATER, BEACH, DESERT, SAVANNAH, FOREST, TAIGA, GRASSLAND, RAINFOREST, SNOW };

// Classify biome by elevation & moisture
Biome classifyBiome(float e, float m) {
    if (e < waterLevel) return WATER;
    if (e < beachLevel) return BEACH;
    if (e > snowLevel) {
        if (m < 0.1f) return DESERT;
        if (m < 0.4f) return SAVANNAH;
        return SNOW;
    }
    if (e > 0.6f) {
        if (m < 0.3f) return DESERT;
        if (m < 0.7f) return FOREST;
        return TAIGA;
    }
    if (m < 0.3f) return DESERT;
    if (m < 0.7f) return GRASSLAND;
    return RAINFOREST;
}

// Compute noise-based elevation at (x,z)
double getElevation(double x, double z) {
    double f = perlin::fractalNoise2d(x * noiseScale, z * noiseScale, octaves, persistence) * 0.5 + 0.5;
    double r = perlin::ridgedNoise(x * noiseScale, z * noiseScale);
    double e = f * (1.0 - ridgedMix) + r * ridgedMix;
    if (useTerrace) e = perlin::terrace(e, terraceSteps);
    if (useIsland) e = perlin::applyIsland(e, perlin::radialMask(x, z));
    return e;
}

// Shader sources
static const char* vertSrc = R"(
#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;
layout(location=2) in vec3 aColor;
uniform mat4 uMVP;
out vec3 vNormal;
out vec3 vColor;
void main() {
    vNormal = aNormal;
    vColor  = aColor;
    gl_Position = uMVP * vec4(aPos,1.0);
}
)";

static const char* fragSrc = R"(
#version 330 core
in vec3 vNormal;
in vec3 vColor;
out vec4 FragColor;
uniform vec3 uLightDir;
void main() {
    float diff = max(dot(normalize(vNormal), -uLightDir), 0.0);
    FragColor = vec4(vColor * diff, 1.0);
}
)";

// Particle shader
static const char* particleVert = R"(
#version 330 core
layout(location=0) in vec3 aPos;
uniform mat4 uMVP;
void main() {
    gl_Position = uMVP * vec4(aPos,1.0);
    gl_PointSize = 3.0;
}
)";

static const char* particleFrag = R"(
#version 330 core
out vec4 FragColor;
uniform vec4 uColor;
void main() {
    FragColor = uColor;
}
)";

// Geometry shader for purple wireframe with pulsation
static const char* geoVert = R"(
#version 330 core
layout(location = 0) in vec3 aPos;
uniform mat4 uMVP;
uniform float uTime;
void main() {
    // Circular ripple: wave expands from center
    float amplitude = 0.05;
    float speed = 2.0;
    float frequency = 12.0;
    float dist = length(aPos.xz);
    float wave = amplitude * sin(dist * frequency - uTime * speed);
    vec3 pos = vec3(aPos.x, aPos.y + wave, aPos.z);
    gl_Position = uMVP * vec4(pos, 1.0);
}
)";

static const char* geoGeom = R"(
#version 330 core
layout(triangles) in;
layout(line_strip, max_vertices=4) out;
void main() {
    for(int i=0;i<3;i++){
        gl_Position = gl_in[i].gl_Position; EmitVertex();
        gl_Position = gl_in[(i+1)%3].gl_Position; EmitVertex();
        EndPrimitive();
    }
}
)";

static const char* geoFrag = R"(
#version 330 core
out vec4 FragColor;
void main() {
    FragColor = vec4(0.5,0.0,0.5,1.0);
}
)";

// Shader helpers
static GLuint compileShader(GLenum t, const char* src) { GLuint s = glCreateShader(t); glShaderSource(s, 1, &src, nullptr); glCompileShader(s); GLint ok;glGetShaderiv(s, GL_COMPILE_STATUS, &ok); if (!ok) { char b[512];glGetShaderInfoLog(s, 512, nullptr, b);std::cerr << b; }return s; }
static GLuint createProgram(const char* vs, const char* fs) { GLuint v = compileShader(GL_VERTEX_SHADER, vs), f = compileShader(GL_FRAGMENT_SHADER, fs), p = glCreateProgram(); glAttachShader(p, v);glAttachShader(p, f); glLinkProgram(p); GLint ok;glGetProgramiv(p, GL_LINK_STATUS, &ok); if (!ok) { char b[512];glGetProgramInfoLog(p, 512, nullptr, b);std::cerr << b; } glDeleteShader(v);glDeleteShader(f); return p; }

// Build terrain mesh
void buildTerrain() {
    std::vector<Vertex> verts;
    std::vector<GLuint> idx;
    int N = gridSize; float h = N / 2.0f;
    verts.reserve((N + 1) * (N + 1));
    for (int j = 0;j <= N;j++) for (int i = 0;i <= N;i++) {
        float x = (i - h) / h, z = (j - h) / h;
        float e = (float)getElevation(x, z);
        float m = (float)(perlin::fractalNoise2d(x * moistureScale, z * moistureScale, moistureOctaves, moisturePersistence) * 0.5 + 0.5);
        Biome b = classifyBiome(e, m); glm::vec3 c;
        switch (b) { case WATER:c = { 0,0,0.8f };break;case BEACH:c = { 0.8f,0.8f,0.5f };break;case DESERT:c = { 0.9f,0.8f,0.4f };break;case SAVANNAH:c = { 0.8f,0.7f,0.3f };break;case FOREST:c = { 0.2f,0.6f,0.2f };break;case TAIGA:c = { 0.3f,0.5f,0.4f };break;case GRASSLAND:c = { 0.4f,0.8f,0.4f };break;case RAINFOREST:c = { 0.1f,0.5f,0.1f };break;default:c = { 1,1,1 }; }
                               verts.push_back({ {x,e,z},{0,0,0},c });
    }
    idx.reserve(N * N * 6);
    for (int j = 0;j < N;j++) for (int i = 0;i < N;i++) {
        GLuint b = j * (N + 1) + i;
        idx.push_back(b); idx.push_back(b + 1); idx.push_back(b + N + 1);
        idx.push_back(b + 1); idx.push_back(b + N + 2); idx.push_back(b + N + 1);
    }
    terrainIndexCount = idx.size();
    for (size_t k = 0;k < idx.size();k += 3) { auto& A = verts[idx[k + 0]], & B = verts[idx[k + 1]], & C = verts[idx[k + 2]];glm::vec3 n = glm::normalize(glm::cross(B.Position - A.Position, C.Position - A.Position));A.Normal += n;B.Normal += n;C.Normal += n; }
    for (auto& v : verts) v.Normal = glm::normalize(v.Normal);
    if (!terrainVAO) glGenVertexArrays(1, &terrainVAO);
    glBindVertexArray(terrainVAO);
    if (!terrainVBO) glGenBuffers(1, &terrainVBO);
    glBindBuffer(GL_ARRAY_BUFFER, terrainVBO); glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(Vertex), verts.data(), GL_STATIC_DRAW);
    if (!terrainEBO) glGenBuffers(1, &terrainEBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, terrainEBO); glBufferData(GL_ELEMENT_ARRAY_BUFFER, idx.size() * sizeof(GLuint), idx.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, Position));
    glEnableVertexAttribArray(1);glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, Normal));
    glEnableVertexAttribArray(2);glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, Color));
}

int main() {
    // GLFW + GLAD init
    glfwInit(); glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3); glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3); glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    GLFWwindow* window = glfwCreateWindow(WINDOW_W, WINDOW_H, "Terrain", NULL, NULL);
    glfwMakeContextCurrent(window); gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
    glEnable(GL_DEPTH_TEST); glEnable(GL_PROGRAM_POINT_SIZE);

    // Input callbacks
    glfwSetMouseButtonCallback(window, [](GLFWwindow* w, int b, int a, int m) {if (ImGui::GetIO().WantCaptureMouse)return; if (b == GLFW_MOUSE_BUTTON_LEFT) { rotating = (a == GLFW_PRESS); if (rotating)glfwGetCursorPos(w, &lastX, &lastY); } });
    glfwSetCursorPosCallback(window, [](GLFWwindow* w, double x, double y) {if (ImGui::GetIO().WantCaptureMouse)return; if (rotating) { float dx = float(x - lastX), dy = float(y - lastY);yaw += dx * 0.3f;pitch = glm::clamp(pitch + dy * 0.3f, -89.0f, 89.0f);lastX = x;lastY = y; } });
    glfwSetScrollCallback(window, [](GLFWwindow* w, double, double y) {if (ImGui::GetIO().WantCaptureMouse)return; distanceToCenter = glm::clamp(distanceToCenter - float(y) * 0.3f, 1.0f, 10.0f);});

    // ImGui init
    IMGUI_CHECKVERSION(); ImGui::CreateContext(); ImGui_ImplGlfw_InitForOpenGL(window, true); ImGui_ImplOpenGL3_Init("#version 330 core"); ImGui::StyleColorsDark();

    // Compile shaders
    terrainShader = createProgram(vertSrc, fragSrc);
    particleShader = createProgram(particleVert, particleFrag);
    geoProgram = glCreateProgram(); {
        GLuint vs = compileShader(GL_VERTEX_SHADER, geoVert);
        GLuint gs = compileShader(GL_GEOMETRY_SHADER, geoGeom);
        GLuint fs = compileShader(GL_FRAGMENT_SHADER, geoFrag);
        glAttachShader(geoProgram, vs);glAttachShader(geoProgram, gs);glAttachShader(geoProgram, fs);
        glLinkProgram(geoProgram);
        glDeleteShader(vs);glDeleteShader(gs);glDeleteShader(fs);
    }

    // Particle VAOs
    glGenVertexArrays(1, &snowVAO); glGenBuffers(1, &snowVBO);
    glBindVertexArray(snowVAO); glBindBuffer(GL_ARRAY_BUFFER, snowVBO);
    glEnableVertexAttribArray(0); glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);
    glGenVertexArrays(1, &rainVAO); glGenBuffers(1, &rainVBO);
    glBindVertexArray(rainVAO); glBindBuffer(GL_ARRAY_BUFFER, rainVBO);
    glEnableVertexAttribArray(0); glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);

    // Build initial terrain
    buildTerrain();

    // Main loop
    while (!glfwWindowShouldClose(window)) {
        float time = float(glfwGetTime());
        glfwPollEvents();
        ImGui_ImplOpenGL3_NewFrame(); ImGui_ImplGlfw_NewFrame(); ImGui::NewFrame();

        // GUI
        ImGui::Begin("Terrain Controls");
        if (ImGui::Checkbox("Geometry Terrain", &useGeo)) {
            // Rebuild to rebind VAO/EBO correctly for toggled mode
            dirty = true;
        }
        if (ImGui::SliderInt("Grid Size", &gridSize, 16, 256)) dirty = true;
        if (ImGui::SliderFloat("Noise Scale", &noiseScale, 0.1f, 10.0f)) dirty = true;
        if (ImGui::SliderInt("Octaves", &octaves, 1, 8)) dirty = true;
        if (ImGui::SliderFloat("Persistence", &persistence, 0.1f, 1.0f)) dirty = true;
        if (ImGui::SliderFloat("Ridged Blend", &ridgedMix, 0.0f, 1.0f)) dirty = true;
        if (ImGui::Checkbox("Terrace", &useTerrace)) dirty = true;
        if (ImGui::SliderInt("Terrace Steps", &terraceSteps, 1, 50)) dirty = true;
        if (ImGui::Checkbox("Island Mask", &useIsland)) dirty = true;
        if (ImGui::SliderFloat("Moisture Scale", &moistureScale, 0.1f, 10.0f)) dirty = true;
        if (ImGui::SliderInt("Moisture Octaves", &moistureOctaves, 1, 8)) dirty = true;
        if (ImGui::SliderFloat("Moisture Persistence", &moisturePersistence, 0.1f, 1.0f)) dirty = true;
        if (ImGui::SliderFloat("Water Level", &waterLevel, 0.0f, 1.0f)) dirty = true;
        if (ImGui::SliderFloat("Beach Level", &beachLevel, 0.0f, 1.0f)) dirty = true;
        if (ImGui::SliderFloat("Snow Level", &snowLevel, 0.0f, 1.0f)) dirty = true;
        ImGui::Separator();
        ImGui::Checkbox("Falling Snow", &enableSnow);
        ImGui::Checkbox("Falling Rain", &enableRain);
        ImGui::End();

        if (dirty) { buildTerrain(); dirty = false; }

        // Update snowflakes
        if (enableSnow) {
            for (int i = 0;i < 5 && snowflakes.size() < 5000;i++) snowflakes.emplace_back(rand() / float(RAND_MAX) * 2.f - 1.f, 2.f, rand() / float(RAND_MAX) * 2.f - 1.f);
            for (auto it = snowflakes.begin(); it != snowflakes.end();) { it->y -= 0.002f; if (it->y <= getElevation(it->x, it->z)) it = snowflakes.erase(it); else ++it; }
            glBindBuffer(GL_ARRAY_BUFFER, snowVBO); glBufferData(GL_ARRAY_BUFFER, snowflakes.size() * sizeof(glm::vec3), snowflakes.data(), GL_DYNAMIC_DRAW);
        }
        // Update raindrops
        if (enableRain) {
            for (int i = 0;i < 10 && raindrops.size() < 5000;i++) raindrops.emplace_back(rand() / float(RAND_MAX) * 2.f - 1.f, 2.f, rand() / float(RAND_MAX) * 2.f - 1.f);
            for (auto it = raindrops.begin(); it != raindrops.end();) { it->y -= 0.005f; if (it->y <= getElevation(it->x, it->z)) it = raindrops.erase(it); else ++it; }
            glBindBuffer(GL_ARRAY_BUFFER, rainVBO); glBufferData(GL_ARRAY_BUFFER, raindrops.size() * sizeof(glm::vec3), raindrops.data(), GL_DYNAMIC_DRAW);
        }

        // Compute camera matrices
        glm::vec3 center(0, 0, 0);
        glm::vec3 eye(
            center.x + distanceToCenter * cos(glm::radians(pitch)) * cos(glm::radians(yaw)),
            center.y + distanceToCenter * sin(glm::radians(pitch)),
            center.z + distanceToCenter * cos(glm::radians(pitch)) * sin(glm::radians(yaw))
        );
        glm::mat4 view = glm::lookAt(eye, center, glm::vec3(0, 1, 0));
        glm::mat4 proj = glm::perspective(glm::radians(45.f), float(WINDOW_W) / WINDOW_H, 0.1f, 100.f);
        glm::mat4 vp = proj * view;

        // Render
        glViewport(0, 0, WINDOW_W, WINDOW_H);
        glClearColor(0.6f, 0.8f, 1.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Draw terrain
        if (useGeo) {
            glUseProgram(geoProgram);
            glUniformMatrix4fv(glGetUniformLocation(geoProgram, "uMVP"), 1, GL_FALSE, &vp[0][0]);
            glUniform1f(glGetUniformLocation(geoProgram, "uTime"), time);
            glBindVertexArray(terrainVAO);
            glDrawElements(GL_TRIANGLES, (GLsizei)terrainIndexCount, GL_UNSIGNED_INT, 0);
        }
        else {
            glUseProgram(terrainShader);
            glUniformMatrix4fv(glGetUniformLocation(terrainShader, "uMVP"), 1, GL_FALSE, &vp[0][0]);
            glUniform3f(glGetUniformLocation(terrainShader, "uLightDir"), -1.f, 1.f, -1.f);
            glBindVertexArray(terrainVAO);
            glDrawElements(GL_TRIANGLES, (GLsizei)terrainIndexCount, GL_UNSIGNED_INT, 0);
        }

        // Draw particles
        if (enableSnow || enableRain) {
            glUseProgram(particleShader);
            // set MVP for particles
            glUniformMatrix4fv(glGetUniformLocation(particleShader, "uMVP"), 1, GL_FALSE, &vp[0][0]);
            if (enableSnow) {
                glUniform4f(glGetUniformLocation(particleShader, "uColor"), 1, 1, 1, 1);
                glBindVertexArray(snowVAO);
                glDrawArrays(GL_POINTS, 0, (GLsizei)snowflakes.size());
            }
            if (enableRain) {
                glUniform4f(glGetUniformLocation(particleShader, "uColor"), 0, 0, 1, 1);
                glBindVertexArray(rainVAO);
                glDrawArrays(GL_POINTS, 0, (GLsizei)raindrops.size());
            }
        }

        // Render UI
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    return 0;
}


