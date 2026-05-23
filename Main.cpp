#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <cmath>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// ─────────────────────────────────────────────
// Dimensões da janela
// ─────────────────────────────────────────────
const float SCREEN_W = 800.0f;
const float SCREEN_H = 600.0f;

// ─────────────────────────────────────────────
// Personagem
// ─────────────────────────────────────────────
float playerX = SCREEN_W / 2.0f;
float playerY = 120.0f;          // altura do chão
const float PLAYER_W = 96.0f;  // tamanho exibido na tela
const float PLAYER_H = 96.0f;
const float PLAYER_SPEED = 4.0f;

// Spritesheet: 7 frames de 128x128 por linha
const int   SPRITE_FRAMES_IDLE = 7;
const int   SPRITE_FRAMES_WALK = 7;
const float SPRITE_FRAME_W = 128.0f;
const float SPRITE_FRAME_H = 128.0f;
const float SPRITE_SHEET_W = 896.0f; // 7 * 128

// Animação
int   currentFrame = 0;
float frameTimer = 0.0f;
const float FRAME_DT = 0.12f;  // segundos por frame
bool  isMoving = false;
bool  facingLeft = false;

// ─────────────────────────────────────────────
// Câmera / parallax
// ─────────────────────────────────────────────
float cameraX = 0.0f;
float cameraY = 0.0f;

// ─────────────────────────────────────────────
// Camadas de fundo
// parallaxX: 0 = estático, 1 = move igual ao player
// ─────────────────────────────────────────────
struct Layer {
    const char* path;
    float parallaxX;
    float parallaxY;
    float yOffset;   // posição Y da camada na tela (pixels)
    float height;    // altura da camada na tela
    GLuint texture;
};

Layer layers[] = {
    { "wall.png",   0.15f, 0.0f, 0.0f,   SCREEN_H, 0 },  // parede ao fundo
    { "ground.png", 1.0f,  0.0f, 0.0f,   120.0f,   0 },  // chão na base
};
const int NUM_LAYERS = sizeof(layers) / sizeof(layers[0]);

// ─────────────────────────────────────────────
// Shader sources
// ─────────────────────────────────────────────
const char* VS_SRC = R"(
    #version 330 core
    layout (location = 0) in vec2 aPos;
    layout (location = 1) in vec2 aTex;

    out vec2 TexCoord;

    uniform mat4 projection;
    uniform vec2 texOffset;

    void main() {
        gl_Position = projection * vec4(aPos, 0.0, 1.0);
        TexCoord = aTex + texOffset;
    }
)";

const char* FS_SRC = R"(
    #version 330 core
    out vec4 FragColor;
    in vec2 TexCoord;

    uniform sampler2D tex;
    uniform bool useAlpha;

    void main() {
        vec4 color = texture(tex, TexCoord);
        if (useAlpha && color.a < 0.1)
            discard;
        FragColor = color;
    }
)";

// ─────────────────────────────────────────────
// Auxiliares OpenGL
// ─────────────────────────────────────────────
GLuint compileShader(const char* src, GLenum type) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, NULL);
    glCompileShader(s);
    GLint ok; glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512]; glGetShaderInfoLog(s, 512, NULL, log);
        std::cout << "Shader error: " << log << "\n";
    }
    return s;
}

GLuint createProgram(const char* vs, const char* fs) {
    GLuint prog = glCreateProgram();
    GLuint v = compileShader(vs, GL_VERTEX_SHADER);
    GLuint f = compileShader(fs, GL_FRAGMENT_SHADER);
    glAttachShader(prog, v); glAttachShader(prog, f);
    glLinkProgram(prog);
    glDeleteShader(v); glDeleteShader(f);
    return prog;
}

GLuint loadTexture(const char* path, bool repeat = false) {
    GLuint tex;
    glGenTextures(1, &tex);
    int w, h, ch;
    stbi_set_flip_vertically_on_load(true);
    unsigned char* data = stbi_load(path, &w, &h, &ch, 0);
    if (!data) { std::cout << "Falha ao carregar: " << path << "\n"; return 0; }
    GLenum fmt = (ch == 4) ? GL_RGBA : GL_RGB;
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, fmt, w, h, 0, fmt, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);
    GLint wrap = repeat ? GL_REPEAT : GL_CLAMP_TO_EDGE;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    stbi_image_free(data);
    return tex;
}

// Matriz ortográfica column-major
void ortho(float* m, float l, float r, float b, float t) {
    for (int i = 0; i < 16; i++) m[i] = 0.0f;
    m[0] = 2.0f / (r - l);
    m[5] = 2.0f / (t - b);
    m[10] = -1.0f;
    m[12] = -(r + l) / (r - l);
    m[13] = -(t + b) / (t - b);
    m[15] = 1.0f;
}

// Cria VAO/VBO para quad em pixels com UV customizado
void buildQuad(GLuint& VAO, GLuint& VBO,
    float x, float y, float w, float h,
    float u0 = 0, float v0 = 0, float u1 = 1, float v1 = 1) {
    float verts[] = {
        x,   y + h, u0, v1,
        x,   y,   u0, v0,
        x + w, y,   u1, v0,
        x,   y + h, u0, v1,
        x + w, y,   u1, v0,
        x + w, y + h, u1, v1,
    };
    glGenVertexArrays(1, &VAO); glGenBuffers(1, &VBO);
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
}

void updateQuad(GLuint VBO,
    float x, float y, float w, float h,
    float u0 = 0, float v0 = 0, float u1 = 1, float v1 = 1) {
    float verts[] = {
        x,   y + h, u0, v1,
        x,   y,   u0, v0,
        x + w, y,   u1, v0,
        x,   y + h, u0, v1,
        x + w, y,   u1, v0,
        x + w, y + h, u1, v1,
    };
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(verts), verts);
}

// ─────────────────────────────────────────────
// Input + atualização
// ─────────────────────────────────────────────
void processInput(GLFWwindow* window, float dt) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    float dx = 0.0f, dy = 0.0f;
    isMoving = false;

    if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS ||
        glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
        dx = -PLAYER_SPEED; facingLeft = true;  isMoving = true;
    }
    if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS ||
        glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
        dx = +PLAYER_SPEED; facingLeft = false; isMoving = true;
    }
    if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS ||
        glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
        dy = +PLAYER_SPEED; isMoving = true;
    }
    if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS ||
        glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
        dy = -PLAYER_SPEED; isMoving = true;
    }

    playerX += dx;
    playerY += dy;
    cameraX += dx;
    cameraY += dy;

    // Animação
    frameTimer += dt;
    int totalFrames = isMoving ? SPRITE_FRAMES_WALK : SPRITE_FRAMES_IDLE;
    if (frameTimer >= FRAME_DT) {
        frameTimer = 0.0f;
        currentFrame = (currentFrame + 1) % totalFrames;
    }
}

// ─────────────────────────────────────────────
// Main
// ─────────────────────────────────────────────
int main() {
    if (!glfwInit()) return -1;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow((int)SCREEN_W, (int)SCREEN_H,
        "Parallax Scrolling", NULL, NULL);
    if (!window) { glfwTerminate(); return -1; }
    glfwMakeContextCurrent(window);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) return -1;
    glViewport(0, 0, (int)SCREEN_W, (int)SCREEN_H);

    GLuint shader = createProgram(VS_SRC, FS_SRC);

    // Projeção ortográfica: (0,0) = canto inferior-esquerdo
    float proj[16];
    ortho(proj, 0.0f, SCREEN_W, 0.0f, SCREEN_H);
    glUseProgram(shader);
    glUniformMatrix4fv(glGetUniformLocation(shader, "projection"), 1, GL_FALSE, proj);
    glUniform1i(glGetUniformLocation(shader, "tex"), 0);

    // Carrega camadas 
    for (int i = 0; i < NUM_LAYERS; i++)
        layers[i].texture = loadTexture(layers[i].path, true);

    // VAO das camadas
    GLuint layerVAO[NUM_LAYERS], layerVBO[NUM_LAYERS];
    for (int i = 0; i < NUM_LAYERS; i++)
        buildQuad(layerVAO[i], layerVBO[i],
            0.0f, layers[i].yOffset, SCREEN_W, layers[i].height,
            0.0f, 0.0f, 3.0f, 1.0f);  // uvW=3 repete 3x horizontalmente

    // Spritesheets do soldado
    // Walk.png é usado para caminhar, Idle.png para parado
    GLuint texIdle = loadTexture("Arquivos de Recurso/Idle.png");
    GLuint texWalk = loadTexture("Arquivos de Recurso/Walk.png");

    // VAO do player
    GLuint playerVAO, playerVBO;
    buildQuad(playerVAO, playerVBO, 0, 0, PLAYER_W, PLAYER_H);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    double lastTime = glfwGetTime();

    while (!glfwWindowShouldClose(window)) {
        double now = glfwGetTime();
        float dt = (float)(now - lastTime);
        lastTime = now;

        processInput(window, dt);

        glClearColor(0.08f, 0.08f, 0.15f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        glUseProgram(shader);

        // ── Renderiza camadas ──
        for (int i = 0; i < NUM_LAYERS; i++) {
            float offU = (cameraX * layers[i].parallaxX) / SCREEN_W;
            float offV = (cameraY * layers[i].parallaxY) / SCREEN_H;

            glBindVertexArray(layerVAO[i]);
            glBindTexture(GL_TEXTURE_2D, layers[i].texture);
            glUniform2f(glGetUniformLocation(shader, "texOffset"), offU, offV);
            glUniform1i(glGetUniformLocation(shader, "useAlpha"), 0);
            glDrawArrays(GL_TRIANGLES, 0, 6);
        }

        // ── Renderiza player (spritesheet) ──
        // Calcula UV do frame atual
        // Cada frame ocupa (1/7) da largura total do sheet
        float frameW_uv = SPRITE_FRAME_W / SPRITE_SHEET_W;  // ≈ 0.142857
        float u0 = currentFrame * frameW_uv;
        float u1 = u0 + frameW_uv;

        // Espelha UV horizontalmente se estiver indo para a esquerda
        if (facingLeft) { float tmp = u0; u0 = u1; u1 = tmp; }

        float px = playerX - PLAYER_W / 2.0f;
        float py = playerY;

        updateQuad(playerVBO, px, py, PLAYER_W, PLAYER_H, u0, 0.0f, u1, 1.0f);

        GLuint texPlayer = isMoving ? texWalk : texIdle;
        glBindVertexArray(playerVAO);
        glBindTexture(GL_TEXTURE_2D, texPlayer);
        glUniform2f(glGetUniformLocation(shader, "texOffset"), 0.0f, 0.0f);
        glUniform1i(glGetUniformLocation(shader, "useAlpha"), 1);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}
