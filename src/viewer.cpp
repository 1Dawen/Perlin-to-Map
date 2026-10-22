// Interactive OpenGL viewer. Optional: build with -DTERRAIN_BUILD_VIEWER=ON.
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include "terrain/terrain.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
constexpr int WINDOW_WIDTH = 1200;
constexpr int WINDOW_HEIGHT = 780;

const char* kTerrainVertex = R"GLSL(
#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;
layout(location=2) in vec3 aColor;
uniform mat4 uMVP;
out vec3 vNormal;
out vec3 vColor;
void main() {
    vNormal = aNormal;
    vColor = aColor;
    gl_Position = uMVP * vec4(aPos, 1.0);
}
)GLSL";

const char* kTerrainFragment = R"GLSL(
#version 330 core
in vec3 vNormal;
in vec3 vColor;
out vec4 FragColor;
uniform vec3 uLightDir;
void main() {
    float diffuse = max(dot(normalize(vNormal), normalize(-uLightDir)), 0.0);
    FragColor = vec4(vColor * (0.38 + 0.62 * diffuse), 1.0);
}
)GLSL";

const char* kWireframeVertex = R"GLSL(
#version 330 core
layout(location=0) in vec3 aPos;
uniform mat4 uMVP;
uniform float uTime;
void main() {
    float distanceFromCenter = length(aPos.xz);
    float ripple = 0.04 * sin(distanceFromCenter * 12.0 - uTime * 2.0);
    gl_Position = uMVP * vec4(aPos.x, aPos.y + ripple, aPos.z, 1.0);
}
)GLSL";

const char* kWireframeGeometry = R"GLSL(
#version 330 core
layout(triangles) in;
layout(line_strip, max_vertices=6) out;
void main() {
    for (int edge = 0; edge < 3; ++edge) {
        gl_Position = gl_in[edge].gl_Position;
        EmitVertex();
        gl_Position = gl_in[(edge + 1) % 3].gl_Position;
        EmitVertex();
        EndPrimitive();
    }
}
)GLSL";

const char* kWireframeFragment = R"GLSL(
#version 330 core
out vec4 FragColor;
void main() { FragColor = vec4(0.68, 0.40, 0.89, 1.0); }
)GLSL";

const char* kParticleVertex = R"GLSL(
#version 330 core
layout(location=0) in vec3 aPos;
uniform mat4 uMVP;
void main() {
    gl_Position = uMVP * vec4(aPos, 1.0);
    gl_PointSize = 3.0;
}
)GLSL";

const char* kParticleFragment = R"GLSL(
#version 330 core
uniform vec4 uColor;
out vec4 FragColor;
void main() { FragColor = uColor; }
)GLSL";

GLuint compileShader(GLenum type, const char* source) {
    const GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    GLint compiled = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (compiled != GL_TRUE) {
        char info[2048] = {};
        glGetShaderInfoLog(shader, sizeof(info), nullptr, info);
        glDeleteShader(shader);
        throw std::runtime_error(std::string("GLSL compilation error: ") + info);
    }
    return shader;
}

GLuint createProgram(const char* vertex, const char* fragment, const char* geometry = nullptr) {
    const GLuint program = glCreateProgram();
    std::vector<GLuint> shaders;
    try {
        shaders.push_back(compileShader(GL_VERTEX_SHADER, vertex));
        if (geometry) shaders.push_back(compileShader(GL_GEOMETRY_SHADER, geometry));
        shaders.push_back(compileShader(GL_FRAGMENT_SHADER, fragment));
        for (auto shader : shaders) glAttachShader(program, shader);
        glLinkProgram(program);
        GLint linked = GL_FALSE;
        glGetProgramiv(program, GL_LINK_STATUS, &linked);
        if (linked != GL_TRUE) {
            char info[2048] = {};
            glGetProgramInfoLog(program, sizeof(info), nullptr, info);
            throw std::runtime_error(std::string("Shader linking error: ") + info);
        }
    } catch (...) {
        for (auto shader : shaders) glDeleteShader(shader);
        glDeleteProgram(program);
        throw;
    }
    for (auto shader : shaders) glDeleteShader(shader);
    return program;
}

void setMatrix(GLuint program, const glm::mat4& mvp) {
    glUseProgram(program);
    glUniformMatrix4fv(glGetUniformLocation(program, "uMVP"), 1, GL_FALSE, &mvp[0][0]);
}

struct Camera {
    float yaw = -45.0f;
    float pitch = 30.0f;
    float radius = 3.7f;
    bool dragging = false;
    double previousX = 0.0;
    double previousY = 0.0;
};

Camera& camera(GLFWwindow* window) {
    return *static_cast<Camera*>(glfwGetWindowUserPointer(window));
}

bool imguiCapturesMouse() {
    return ImGui::GetCurrentContext() && ImGui::GetIO().WantCaptureMouse;
}

void mouseButton(GLFWwindow* window, int button, int action, int) {
    if (button != GLFW_MOUSE_BUTTON_LEFT || imguiCapturesMouse()) return;
    auto& c = camera(window);
    c.dragging = action == GLFW_PRESS;
    if (c.dragging) glfwGetCursorPos(window, &c.previousX, &c.previousY);
}

void cursorPosition(GLFWwindow* window, double x, double y) {
    auto& c = camera(window);
    if (!c.dragging) return;
    if (imguiCapturesMouse()) { c.dragging = false; return; }
    c.yaw += static_cast<float>(x - c.previousX) * 0.25f;
    c.pitch = std::clamp(c.pitch - static_cast<float>(y - c.previousY) * 0.25f, -80.0f, 80.0f);
    c.previousX = x;
    c.previousY = y;
}

void scroll(GLFWwindow* window, double, double y) {
    if (!imguiCapturesMouse()) {
        auto& c = camera(window);
        c.radius = std::clamp(c.radius - static_cast<float>(y) * 0.25f, 1.5f, 10.0f);
    }
}

struct GpuBuffers {
    GLuint vao = 0;
    GLuint vertex = 0;
    GLuint index = 0;
    GLsizei triangleIndices = 0;

    void upload(const terrain::Mesh& mesh) {
        if (!vao) glGenVertexArrays(1, &vao);
        if (!vertex) glGenBuffers(1, &vertex);
        if (!index) glGenBuffers(1, &index);
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vertex);
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(mesh.vertices.size() * sizeof(terrain::Vertex)),
                     mesh.vertices.data(), GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(mesh.indices.size() * sizeof(std::uint32_t)),
                     mesh.indices.data(), GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(terrain::Vertex),
                              reinterpret_cast<void*>(offsetof(terrain::Vertex, position)));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(terrain::Vertex),
                              reinterpret_cast<void*>(offsetof(terrain::Vertex, normal)));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(terrain::Vertex),
                              reinterpret_cast<void*>(offsetof(terrain::Vertex, color)));
        glBindVertexArray(0);
        triangleIndices = static_cast<GLsizei>(mesh.indices.size());
    }

    void draw() const {
        glBindVertexArray(vao);
        glDrawElements(GL_TRIANGLES, triangleIndices, GL_UNSIGNED_INT, nullptr);
    }

    void clear() {
        if (index) glDeleteBuffers(1, &index);
        if (vertex) glDeleteBuffers(1, &vertex);
        if (vao) glDeleteVertexArrays(1, &vao);
        index = vertex = vao = 0;
    }
};

struct ParticleField {
    std::vector<glm::vec3> positions;
    GLuint vao = 0;
    GLuint vbo = 0;
    float carry = 0.0f;
    std::mt19937 generator{93u};
    std::uniform_real_distribution<float> xy{-1.0f, 1.0f};

    void initialize() {
        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), nullptr);
        glBindVertexArray(0);
    }

    void update(float dt, float rate, float speed, const terrain::TerrainGenerator& terrainGenerator) {
        carry += dt * rate;
        const int toSpawn = std::min(static_cast<int>(carry), 5000 - static_cast<int>(positions.size()));
        for (int i = 0; i < toSpawn; ++i) positions.push_back({xy(generator), 1.6f, xy(generator)});
        carry -= static_cast<float>(toSpawn);
        carry = std::min(carry, rate);  // avoid unlimited backlog when the cap is reached
        positions.erase(std::remove_if(positions.begin(), positions.end(), [&](glm::vec3& p) {
            p.y -= dt * speed;
            const float surface = terrainGenerator.elevation(p.x, p.z) * terrainGenerator.settings().heightScale;
            return p.y <= surface;
        }), positions.end());
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(positions.size() * sizeof(glm::vec3)),
                     positions.empty() ? nullptr : positions.data(), GL_DYNAMIC_DRAW);
    }

    void draw() const {
        glBindVertexArray(vao);
        glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(positions.size()));
    }

    void clear() {
        if (vbo) glDeleteBuffers(1, &vbo);
        if (vao) glDeleteVertexArrays(1, &vao);
        vbo = vao = 0;
    }
};

bool terrainControls(terrain::Settings& s, bool& wireframe, bool& snow, bool& rain) {
    bool dirty = false;
    ImGui::Begin("Terrain Controls");
    ImGui::TextUnformatted("Custom gradient / fractal noise");
    int seed = static_cast<int>(s.seed);
    if (ImGui::SliderInt("Seed", &seed, 0, 100000)) { s.seed = static_cast<std::uint32_t>(seed); dirty = true; }
    dirty |= ImGui::SliderInt("Grid size", &s.gridSize, 16, 256);
    dirty |= ImGui::SliderFloat("Noise scale", &s.noiseScale, 0.1f, 10.0f);
    dirty |= ImGui::SliderInt("Octaves", &s.octaves, 1, 8);
    dirty |= ImGui::SliderFloat("Persistence", &s.persistence, 0.1f, 1.0f);
    dirty |= ImGui::SliderFloat("Ridged blend", &s.ridgedMix, 0.0f, 1.0f);
    dirty |= ImGui::Checkbox("Terraces", &s.terraceEnabled);
    if (s.terraceEnabled) dirty |= ImGui::SliderInt("Terrace steps", &s.terraceSteps, 2, 30);
    dirty |= ImGui::Checkbox("Island mask", &s.islandEnabled);
    ImGui::Separator();
    ImGui::TextUnformatted("Biomes and moisture");
    dirty |= ImGui::SliderFloat("Moisture scale", &s.moistureScale, 0.1f, 10.0f);
    dirty |= ImGui::SliderInt("Moisture octaves", &s.moistureOctaves, 1, 8);
    dirty |= ImGui::SliderFloat("Moisture persistence", &s.moisturePersistence, 0.1f, 1.0f);
    dirty |= ImGui::SliderFloat("Water level", &s.waterLevel, 0.0f, 0.85f);
    dirty |= ImGui::SliderFloat("Beach level", &s.beachLevel, 0.05f, 0.95f);
    dirty |= ImGui::SliderFloat("Snow level", &s.snowLevel, 0.10f, 1.0f);
    // Maintain a valid ordering even when independently adjusted sliders cross.
    s.beachLevel = std::max(s.beachLevel, s.waterLevel + 0.01f);
    s.snowLevel = std::max(s.snowLevel, s.beachLevel + 0.01f);
    s.snowLevel = std::min(s.snowLevel, 1.0f);
    ImGui::Separator();
    ImGui::Checkbox("Purple wireframe + ripple", &wireframe);
    ImGui::Checkbox("Snow", &snow);
    ImGui::Checkbox("Rain", &rain);
    ImGui::TextUnformatted("Drag to rotate | Scroll to zoom");
    ImGui::End();
    return dirty;
}

void run() {
    if (glfwInit() != GLFW_TRUE) throw std::runtime_error("Failed to initialize GLFW");
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
    GLFWwindow* window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "Perlin Terrain Generator", nullptr, nullptr);
    if (!window) { glfwTerminate(); throw std::runtime_error("Failed to create GLFW window"); }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);
    if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress))) {
        glfwDestroyWindow(window);
        glfwTerminate();
        throw std::runtime_error("Failed to initialize GLAD");
    }
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_PROGRAM_POINT_SIZE);
    Camera viewCamera;
    glfwSetWindowUserPointer(window, &viewCamera);
    glfwSetMouseButtonCallback(window, mouseButton);
    glfwSetCursorPosCallback(window, cursorPosition);
    glfwSetScrollCallback(window, scroll);

    bool imguiStarted = false;
    GLuint terrainProgram = 0, wireframeProgram = 0, particleProgram = 0;
    GpuBuffers gpu;
    ParticleField snowField, rainField;
    try {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGui::StyleColorsDark();
        if (!ImGui_ImplGlfw_InitForOpenGL(window, true)) throw std::runtime_error("ImGui GLFW initialization failed");
        if (!ImGui_ImplOpenGL3_Init("#version 330 core")) throw std::runtime_error("ImGui OpenGL initialization failed");
        imguiStarted = true;
        terrainProgram = createProgram(kTerrainVertex, kTerrainFragment);
        wireframeProgram = createProgram(kWireframeVertex, kWireframeFragment, kWireframeGeometry);
        particleProgram = createProgram(kParticleVertex, kParticleFragment);
        snowField.initialize();
        rainField.initialize();
        terrain::Settings settings;
        terrain::TerrainGenerator generator(settings);
        gpu.upload(generator.generate());
        bool wireframe = false, snow = false, rain = false;
        float previousTime = static_cast<float>(glfwGetTime());
        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();
            const float time = static_cast<float>(glfwGetTime());
            const float dt = std::clamp(time - previousTime, 0.0f, 0.05f);
            previousTime = time;
            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();
            if (terrainControls(settings, wireframe, snow, rain)) {
                generator = terrain::TerrainGenerator(settings);
                gpu.upload(generator.generate());
            }
            if (snow) snowField.update(dt, 120.0f, 0.50f, generator);
            if (rain) rainField.update(dt, 300.0f, 1.50f, generator);
            int width = 0, height = 0;
            glfwGetFramebufferSize(window, &width, &height);
            if (width <= 0 || height <= 0) { ImGui::Render(); continue; } // minimized window
            glViewport(0, 0, width, height);
            glClearColor(0.55f, 0.70f, 0.87f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            const auto& c = viewCamera;
            const float radiansYaw = glm::radians(c.yaw);
            const float radiansPitch = glm::radians(c.pitch);
            const glm::vec3 eye(
                c.radius * std::cos(radiansPitch) * std::cos(radiansYaw),
                c.radius * std::sin(radiansPitch) + 0.3f,
                c.radius * std::cos(radiansPitch) * std::sin(radiansYaw));
            const glm::mat4 view = glm::lookAt(eye, glm::vec3(0, 0.25f, 0), glm::vec3(0, 1, 0));
            const glm::mat4 projection = glm::perspective(glm::radians(45.0f),
                static_cast<float>(width) / static_cast<float>(height), 0.1f, 100.0f);
            const glm::mat4 mvp = projection * view;
            if (wireframe) {
                setMatrix(wireframeProgram, mvp);
                glUniform1f(glGetUniformLocation(wireframeProgram, "uTime"), time);
            } else {
                setMatrix(terrainProgram, mvp);
                glUniform3f(glGetUniformLocation(terrainProgram, "uLightDir"), -1, 1, -1);
            }
            gpu.draw();
            if (snow || rain) {
                setMatrix(particleProgram, mvp);
                if (snow) {
                    glUniform4f(glGetUniformLocation(particleProgram, "uColor"), 1, 1, 1, 1);
                    snowField.draw();
                }
                if (rain) {
                    glUniform4f(glGetUniformLocation(particleProgram, "uColor"), 0.20f, 0.42f, 1, 1);
                    rainField.draw();
                }
            }
            ImGui::Render();
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
            glfwSwapBuffers(window);
        }
    } catch (...) {
        gpu.clear();
        snowField.clear(); rainField.clear();
        if (terrainProgram) glDeleteProgram(terrainProgram);
        if (wireframeProgram) glDeleteProgram(wireframeProgram);
        if (particleProgram) glDeleteProgram(particleProgram);
        if (imguiStarted) { ImGui_ImplOpenGL3_Shutdown(); ImGui_ImplGlfw_Shutdown(); }
        if (ImGui::GetCurrentContext()) ImGui::DestroyContext();
        glfwDestroyWindow(window);
        glfwTerminate();
        throw;
    }
    gpu.clear();
    snowField.clear(); rainField.clear();
    glDeleteProgram(terrainProgram);
    glDeleteProgram(wireframeProgram);
    glDeleteProgram(particleProgram);
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
}
} // namespace

int main() {
    try { run(); }
    catch (const std::exception& error) {
        std::cerr << "Terrain viewer error: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
