#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <stack>
#include <string>
#include <cstdlib>
#include <ctime>
#include <cmath>
#include <algorithm>

// ============================================================================
// STUDENT & COURSE DETAILS 
// ============================================================================
const std::string PROJECT_TITLE = "NEON HIGHWAY (HEADLINE TECHNIQUE: BLOOM)";
const std::string TEAM_NAMES = "TEAM: Ayauzhan Baurzhanova & Zhanassyl Sherkenov";
const std::string GROUP_NAME = "GROUP: SE-2416";
const std::string INSTRUCTOR_NAME = "COURSE: CG FUNDAMENTALS | INSTRUCTOR: ALMAS ZHANABILOV";

// Window dimensions
int scrWidth = 1280;
int scrHeight = 720;

// Game State Machine (Phase 4 Step 8)
enum GameState {
    STATE_START,
    STATE_PLAYING,
    STATE_GAME_OVER
};
GameState gameState = STATE_START;

// Technique & App Toggles
bool bloomEnabled = true;
bool isPaused = false;
bool freeCameraMode = false;
bool showHelp = false;
float exposure = 1.1f;

// Player Car State (Phase 1 & 2)
float carX = 0.0f;
float targetCarX = 0.0f;
int currentLane = 0; // -1 = Left, 0 = Center, 1 = Right
const float laneWidth = 3.2f;
float wheelRotation = 0.0f;
float highwayScroll = 0.0f;
float gameSpeed = 18.0f;
float score = 0.0f;
float highScore = 0.0f;
int currentFPS = 60;

// Movable Light State (Phase 1 Step 3)
glm::vec3 lightPos(0.0f, 6.0f, -8.0f);
bool autoMoveLight = true;

// Free-Fly / Orbit Camera State (Requirement 2.2)
glm::vec3 freeCamPos(0.0f, 5.0f, 9.0f);
float camYaw = -90.0f;
float camPitch = -20.0f;

// Procedural Entities (Phase 2 Step 4)
struct Entity {
    glm::vec3 pos;
    int type; // 0 = Red Obstacle Barrier, 1 = Green Neon Battery
    bool active;
};
std::vector<Entity> entities;
float spawnTimer = 0.0f;

// FBO Handles (Phase 3 Step 6 & 7)
GLuint hdrFBO = 0;
GLuint colorBuffers[2] = { 0, 0 };
GLuint rboDepth = 0;
GLuint pingpongFBO[2] = { 0, 0 };
GLuint pingpongColorbuffers[2] = { 0, 0 };

// Geometry VAO/VBO Handles
GLuint cubeVAO = 0, cubeVBO = 0;
GLuint quadVAO = 0, quadVBO = 0;
GLuint uiVAO = 0, uiVBO = 0;

// Texture Handles (Phase 2 Step 5 & Phase 4 Font Texture)
GLuint roadTexture = 0;
GLuint obstacleTexture = 0;
GLuint fontTexture = 0;

// Key Press Debouncing
bool keyPressed[1024] = { false };

// ============================================================================
// SHADER LOADER WITH MANDATORY CONSOLE ERROR CHECKING
// ============================================================================
GLuint loadShaderFromFile(const char* vertexPath, const char* fragmentPath) {
    std::string vertexCode, fragmentCode;
    std::ifstream vShaderFile(vertexPath);
    std::ifstream fShaderFile(fragmentPath);

    if (!vShaderFile.is_open()) {
        std::cerr << "[ERROR] Failed to open vertex shader file: " << vertexPath << std::endl;
    }
    else {
        std::stringstream vShaderStream;
        vShaderStream << vShaderFile.rdbuf();
        vertexCode = vShaderStream.str();
        vShaderFile.close();
    }

    if (!fShaderFile.is_open()) {
        std::cerr << "[ERROR] Failed to open fragment shader file: " << fragmentPath << std::endl;
    }
    else {
        std::stringstream fShaderStream;
        fShaderStream << fShaderFile.rdbuf();
        fragmentCode = fShaderStream.str();
        fShaderFile.close();
    }

    const char* vShaderCode = vertexCode.c_str();
    const char* fShaderCode = fragmentCode.c_str();

    GLuint vertex, fragment;
    GLint success;
    GLchar infoLog[1024];

    vertex = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertex, 1, &vShaderCode, NULL);
    glCompileShader(vertex);
    glGetShaderiv(vertex, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(vertex, 1024, NULL, infoLog);
        std::cerr << "[SHADER COMPILE ERROR] Vertex (" << vertexPath << "):\n" << infoLog << std::endl;
    }

    fragment = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragment, 1, &fShaderCode, NULL);
    glCompileShader(fragment);
    glGetShaderiv(fragment, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(fragment, 1024, NULL, infoLog);
        std::cerr << "[SHADER COMPILE ERROR] Fragment (" << fragmentPath << "):\n" << infoLog << std::endl;
    }

    GLuint ID = glCreateProgram();
    glAttachShader(ID, vertex);
    glAttachShader(ID, fragment);
    glLinkProgram(ID);
    glGetProgramiv(ID, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(ID, 1024, NULL, infoLog);
        std::cerr << "[SHADER LINK ERROR] Program (" << vertexPath << " + " << fragmentPath << "):\n" << infoLog << std::endl;
    }

    glDeleteShader(vertex);
    glDeleteShader(fragment);
    return ID;
}

// ============================================================================
// EMBEDDED 5x7 BITMAP FONT & 2D HUD RENDERER (Phase 4 Step 8)
// ============================================================================
void getCharBitmap(char c, unsigned char out[7]) {
    for (int i = 0; i < 7; ++i) out[i] = 0;
    if (c >= 'a' && c <= 'z') c -= 32; // Auto-uppercase
    switch (c) {
    case 'A': { unsigned char b[7] = { 0x0E,0x11,0x11,0x1F,0x11,0x11,0x11 }; memcpy(out, b, 7); break; }
    case 'B': { unsigned char b[7] = { 0x1E,0x11,0x11,0x1E,0x11,0x11,0x1E }; memcpy(out, b, 7); break; }
    case 'C': { unsigned char b[7] = { 0x0E,0x11,0x10,0x10,0x10,0x11,0x0E }; memcpy(out, b, 7); break; }
    case 'D': { unsigned char b[7] = { 0x1C,0x12,0x11,0x11,0x11,0x12,0x1C }; memcpy(out, b, 7); break; }
    case 'E': { unsigned char b[7] = { 0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F }; memcpy(out, b, 7); break; }
    case 'F': { unsigned char b[7] = { 0x1F,0x10,0x10,0x1E,0x10,0x10,0x10 }; memcpy(out, b, 7); break; }
    case 'G': { unsigned char b[7] = { 0x0E,0x11,0x10,0x17,0x11,0x11,0x0F }; memcpy(out, b, 7); break; }
    case 'H': { unsigned char b[7] = { 0x11,0x11,0x11,0x1F,0x11,0x11,0x11 }; memcpy(out, b, 7); break; }
    case 'I': { unsigned char b[7] = { 0x0E,0x04,0x04,0x04,0x04,0x04,0x0E }; memcpy(out, b, 7); break; }
    case 'J': { unsigned char b[7] = { 0x07,0x02,0x02,0x02,0x02,0x12,0x0C }; memcpy(out, b, 7); break; }
    case 'K': { unsigned char b[7] = { 0x11,0x12,0x14,0x18,0x14,0x12,0x11 }; memcpy(out, b, 7); break; }
    case 'L': { unsigned char b[7] = { 0x10,0x10,0x10,0x10,0x10,0x10,0x1F }; memcpy(out, b, 7); break; }
    case 'M': { unsigned char b[7] = { 0x11,0x1B,0x15,0x15,0x11,0x11,0x11 }; memcpy(out, b, 7); break; }
    case 'N': { unsigned char b[7] = { 0x11,0x19,0x15,0x13,0x11,0x11,0x11 }; memcpy(out, b, 7); break; }
    case 'O': { unsigned char b[7] = { 0x0E,0x11,0x11,0x11,0x11,0x11,0x0E }; memcpy(out, b, 7); break; }
    case 'P': { unsigned char b[7] = { 0x1E,0x11,0x11,0x1E,0x10,0x10,0x10 }; memcpy(out, b, 7); break; }
    case 'Q': { unsigned char b[7] = { 0x0E,0x11,0x11,0x11,0x15,0x12,0x0D }; memcpy(out, b, 7); break; }
    case 'R': { unsigned char b[7] = { 0x1E,0x11,0x11,0x1E,0x14,0x12,0x11 }; memcpy(out, b, 7); break; }
    case 'S': { unsigned char b[7] = { 0x0F,0x10,0x10,0x0E,0x01,0x01,0x1E }; memcpy(out, b, 7); break; }
    case 'T': { unsigned char b[7] = { 0x1F,0x04,0x04,0x04,0x04,0x04,0x04 }; memcpy(out, b, 7); break; }
    case 'U': { unsigned char b[7] = { 0x11,0x11,0x11,0x11,0x11,0x11,0x0E }; memcpy(out, b, 7); break; }
    case 'V': { unsigned char b[7] = { 0x11,0x11,0x11,0x11,0x11,0x0A,0x04 }; memcpy(out, b, 7); break; }
    case 'W': { unsigned char b[7] = { 0x11,0x11,0x11,0x15,0x15,0x15,0x0A }; memcpy(out, b, 7); break; }
    case 'X': { unsigned char b[7] = { 0x11,0x11,0x0A,0x04,0x0A,0x11,0x11 }; memcpy(out, b, 7); break; }
    case 'Y': { unsigned char b[7] = { 0x11,0x11,0x0A,0x04,0x04,0x04,0x04 }; memcpy(out, b, 7); break; }
    case 'Z': { unsigned char b[7] = { 0x1F,0x01,0x02,0x04,0x08,0x10,0x1F }; memcpy(out, b, 7); break; }
    case '0': { unsigned char b[7] = { 0x0E,0x11,0x13,0x15,0x19,0x11,0x0E }; memcpy(out, b, 7); break; }
    case '1': { unsigned char b[7] = { 0x04,0x0C,0x04,0x04,0x04,0x04,0x0E }; memcpy(out, b, 7); break; }
    case '2': { unsigned char b[7] = { 0x0E,0x11,0x01,0x02,0x04,0x08,0x1F }; memcpy(out, b, 7); break; }
    case '3': { unsigned char b[7] = { 0x1F,0x02,0x04,0x02,0x01,0x11,0x0E }; memcpy(out, b, 7); break; }
    case '4': { unsigned char b[7] = { 0x02,0x06,0x0A,0x12,0x1F,0x02,0x02 }; memcpy(out, b, 7); break; }
    case '5': { unsigned char b[7] = { 0x1F,0x10,0x1E,0x01,0x01,0x11,0x0E }; memcpy(out, b, 7); break; }
    case '6': { unsigned char b[7] = { 0x06,0x08,0x10,0x1E,0x11,0x11,0x0E }; memcpy(out, b, 7); break; }
    case '7': { unsigned char b[7] = { 0x1F,0x01,0x02,0x04,0x08,0x08,0x08 }; memcpy(out, b, 7); break; }
    case '8': { unsigned char b[7] = { 0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E }; memcpy(out, b, 7); break; }
    case '9': { unsigned char b[7] = { 0x0E,0x11,0x11,0x0F,0x01,0x02,0x0C }; memcpy(out, b, 7); break; }
    case ':': { unsigned char b[7] = { 0x00,0x0C,0x0C,0x00,0x0C,0x0C,0x00 }; memcpy(out, b, 7); break; }
    case '-': { unsigned char b[7] = { 0x00,0x00,0x00,0x1F,0x00,0x00,0x00 }; memcpy(out, b, 7); break; }
    case '+': { unsigned char b[7] = { 0x00,0x04,0x04,0x1F,0x04,0x04,0x00 }; memcpy(out, b, 7); break; }
    case '/': { unsigned char b[7] = { 0x01,0x01,0x02,0x04,0x08,0x10,0x10 }; memcpy(out, b, 7); break; }
    case '|': { unsigned char b[7] = { 0x04,0x04,0x04,0x04,0x04,0x04,0x04 }; memcpy(out, b, 7); break; }
    case '[': { unsigned char b[7] = { 0x0E,0x08,0x08,0x08,0x08,0x08,0x0E }; memcpy(out, b, 7); break; }
    case ']': { unsigned char b[7] = { 0x0E,0x02,0x02,0x02,0x02,0x02,0x0E }; memcpy(out, b, 7); break; }
    case '(': { unsigned char b[7] = { 0x02,0x04,0x08,0x08,0x08,0x04,0x02 }; memcpy(out, b, 7); break; }
    case ')': { unsigned char b[7] = { 0x08,0x04,0x02,0x02,0x02,0x04,0x08 }; memcpy(out, b, 7); break; }
    case '_': { unsigned char b[7] = { 0x00,0x00,0x00,0x00,0x00,0x00,0x1F }; memcpy(out, b, 7); break; }
    case '&': { unsigned char b[7] = { 0x0C,0x12,0x14,0x08,0x15,0x12,0x0D }; memcpy(out, b, 7); break; }
    case '!': { unsigned char b[7] = { 0x04,0x04,0x04,0x04,0x04,0x00,0x04 }; memcpy(out, b, 7); break; }
    case '.': { unsigned char b[7] = { 0x00,0x00,0x00,0x00,0x00,0x0C,0x0C }; memcpy(out, b, 7); break; }
    case ',': { unsigned char b[7] = { 0x00,0x00,0x00,0x00,0x04,0x04,0x08 }; memcpy(out, b, 7); break; }
    default:  break;
    }
}

void generateFontTexture() {
    // 128 ASCII characters, each stored in an 8x8 cell = 1024 x 8 texture
    const int atlasWidth = 128 * 8;
    const int atlasHeight = 8;
    std::vector<unsigned char> fontData(atlasWidth * atlasHeight, 0);

    for (int c = 32; c < 128; ++c) {
        unsigned char rows[7];
        getCharBitmap((char)c, rows);
        for (int r = 0; r < 7; ++r) {
            for (int col = 0; col < 5; ++col) {
                bool pixelOn = (rows[r] >> (4 - col)) & 1;
                if (pixelOn) {
                    int x = c * 8 + col + 1;
                    int y = r;
                    fontData[y * atlasWidth + x] = 255;
                }
            }
        }
    }

    glGenTextures(1, &fontTexture);
    glBindTexture(GL_TEXTURE_2D, fontTexture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, atlasWidth, atlasHeight, 0, GL_RED, GL_UNSIGNED_BYTE, fontData.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glGenVertexArrays(1, &uiVAO);
    glGenBuffers(1, &uiVBO);
    glBindVertexArray(uiVAO);
    glBindBuffer(GL_ARRAY_BUFFER, uiVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 6 * 4 * 512, NULL, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

void drawUIPanel(GLuint uiShader, float x, float y, float w, float h, glm::vec4 color) {
    glUniform1i(glGetUniformLocation(uiShader, "isPanel"), 1);
    glUniform4fv(glGetUniformLocation(uiShader, "textColor"), 1, glm::value_ptr(color));

    float vertices[24] = {
        x,     y + h,   0.0f, 1.0f,
        x,     y,       0.0f, 0.0f,
        x + w, y,       1.0f, 0.0f,

        x,     y + h,   0.0f, 1.0f,
        x + w, y,       1.0f, 0.0f,
        x + w, y + h,   1.0f, 1.0f
    };

    glBindVertexArray(uiVAO);
    glBindBuffer(GL_ARRAY_BUFFER, uiVBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}

void drawUIText(GLuint uiShader, const std::string& text, float x, float y, float scale, glm::vec4 color) {
    if (text.empty()) return;
    glUniform1i(glGetUniformLocation(uiShader, "isPanel"), 0);
    glUniform4fv(glGetUniformLocation(uiShader, "textColor"), 1, glm::value_ptr(color));

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, fontTexture);

    std::vector<float> vertices;
    vertices.reserve(text.size() * 24);

    float charW = 6.0f * scale;
    float charH = 7.0f * scale;
    float curX = x;

    for (char c : text) {
        int ascii = (unsigned char)c;
        if (ascii < 32 || ascii >= 128) ascii = 32;

        float u0 = (ascii * 8.0f + 0.5f) / 1024.0f;
        float u1 = (ascii * 8.0f + 6.5f) / 1024.0f;
        float v0 = 0.0f;
        float v1 = 7.0f / 8.0f;

        float x0 = curX;
        float y0 = y;
        float x1 = curX + charW;
        float y1 = y + charH;

        float quad[24] = {
            x0, y1, u0, v1,
            x0, y0, u0, v0,
            x1, y0, u1, v0,

            x0, y1, u0, v1,
            x1, y0, u1, v0,
            x1, y1, u1, v1
        };
        vertices.insert(vertices.end(), quad, quad + 24);
        curX += charW;
    }

    glBindVertexArray(uiVAO);
    glBindBuffer(GL_ARRAY_BUFFER, uiVBO);
    int maxFloats = 6 * 4 * 512;
    int countFloats = std::min((int)vertices.size(), maxFloats);
    glBufferSubData(GL_ARRAY_BUFFER, 0, countFloats * sizeof(float), vertices.data());
    glDrawArrays(GL_TRIANGLES, 0, countFloats / 4);
    glBindVertexArray(0);
}

void renderHUD(GLuint uiShader) {
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glUseProgram(uiShader);
    glm::mat4 orthoProj = glm::ortho(0.0f, (float)scrWidth, (float)scrHeight, 0.0f, -1.0f, 1.0f);
    glUniformMatrix4fv(glGetUniformLocation(uiShader, "projection"), 1, GL_FALSE, glm::value_ptr(orthoProj));
    glUniform1i(glGetUniformLocation(uiShader, "fontAtlas"), 0);

    // 1. TOP-LEFT HUD PANEL: Team Names, Group, Course, Instructor, Project Title
    drawUIPanel(uiShader, 14.0f, 14.0f, 670.0f, 92.0f, glm::vec4(0.04f, 0.02f, 0.10f, 0.80f));
    drawUIPanel(uiShader, 14.0f, 14.0f, 4.0f, 92.0f, glm::vec4(0.0f, 0.95f, 1.0f, 1.0f)); // Cyan accent bar

    drawUIText(uiShader, PROJECT_TITLE, 28.0f, 24.0f, 2.2f, glm::vec4(0.0f, 0.95f, 1.0f, 1.0f));
    drawUIText(uiShader, TEAM_NAMES + " | " + GROUP_NAME, 28.0f, 52.0f, 2.0f, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
    drawUIText(uiShader, INSTRUCTOR_NAME, 28.0f, 78.0f, 1.7f, glm::vec4(0.85f, 0.70f, 1.0f, 1.0f));

    // 2. TOP-RIGHT HUD PANEL: Score, High Score, Bloom State, Camera, FPS
    float rightPanelW = 360.0f;
    float rightPanelX = scrWidth - rightPanelW - 14.0f;
    drawUIPanel(uiShader, rightPanelX, 14.0f, rightPanelW, 92.0f, glm::vec4(0.04f, 0.02f, 0.10f, 0.80f));
    drawUIPanel(uiShader, rightPanelX + rightPanelW - 4.0f, 14.0f, 4.0f, 92.0f, glm::vec4(1.0f, 0.1f, 0.75f, 1.0f));

    std::string scoreStr = "SCORE: " + std::to_string((int)score) + "  |  BEST: " + std::to_string((int)highScore);
    drawUIText(uiShader, scoreStr, rightPanelX + 16.0f, 24.0f, 2.2f, glm::vec4(0.2f, 1.0f, 0.4f, 1.0f));

    std::string bloomStr = std::string("BLOOM [B]: ") + (bloomEnabled ? "ON" : "OFF");
    glm::vec4 bloomColor = bloomEnabled ? glm::vec4(0.0f, 0.95f, 1.0f, 1.0f) : glm::vec4(1.0f, 0.3f, 0.3f, 1.0f);
    drawUIText(uiShader, bloomStr, rightPanelX + 16.0f, 52.0f, 2.0f, bloomColor);

    std::string statusStr = "FPS: " + std::to_string(currentFPS) + " | CAM [C]: " + (freeCameraMode ? "FREE" : "GAME");
    drawUIText(uiShader, statusStr, rightPanelX + 16.0f, 78.0f, 1.7f, glm::vec4(0.85f, 0.85f, 0.9f, 1.0f));

    // 3. BOTTOM BAR: Quick Controls Reminder
    drawUIPanel(uiShader, 14.0f, scrHeight - 36.0f, scrWidth - 28.0f, 24.0f, glm::vec4(0.04f, 0.02f, 0.10f, 0.75f));
    drawUIText(uiShader, "[A/D] STEER   [B] TOGGLE BLOOM   [P] PAUSE   [C] CAMERA   [ARROWS/U/O] MOVE LIGHT   [R] RESET   [H/F1] HELP",
        24.0f, scrHeight - 29.0f, 1.6f, glm::vec4(0.75f, 0.85f, 1.0f, 1.0f));

    // 4. CENTER BANNER: Start State / Pause State / Game Over State
    if (gameState == STATE_START) {
        float bw = 640.0f, bh = 115.0f;
        float bx = (scrWidth - bw) * 0.5f, by = (scrHeight - bh) * 0.5f - 30.0f;
        drawUIPanel(uiShader, bx, by, bw, bh, glm::vec4(0.03f, 0.01f, 0.08f, 0.90f));
        drawUIPanel(uiShader, bx, by, bw, 4.0f, glm::vec4(0.0f, 0.95f, 1.0f, 1.0f));
        drawUIText(uiShader, "NEON HIGHWAY - READY TO LAUNCH", bx + 75.0f, by + 22.0f, 2.6f, glm::vec4(0.0f, 0.95f, 1.0f, 1.0f));
        drawUIText(uiShader, "DODGE RED BARRIERS & COLLECT GREEN NEON BATTERIES (+50)", bx + 42.0f, by + 56.0f, 1.8f, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
        drawUIText(uiShader, "PRESS [SPACE] OR [A / D] TO START DRIVING!", bx + 85.0f, by + 82.0f, 2.1f, glm::vec4(0.2f, 1.0f, 0.4f, 1.0f));
    }
    else if (gameState == STATE_GAME_OVER) {
        float bw = 580.0f, bh = 120.0f;
        float bx = (scrWidth - bw) * 0.5f, by = (scrHeight - bh) * 0.5f - 30.0f;
        drawUIPanel(uiShader, bx, by, bw, bh, glm::vec4(0.12f, 0.01f, 0.04f, 0.92f));
        drawUIPanel(uiShader, bx, by, bw, 4.0f, glm::vec4(1.0f, 0.1f, 0.3f, 1.0f));
        drawUIText(uiShader, "GAME OVER - SYSTEM CRASH!", bx + 95.0f, by + 22.0f, 2.8f, glm::vec4(1.0f, 0.2f, 0.3f, 1.0f));
        drawUIText(uiShader, "FINAL SCORE: " + std::to_string((int)score) + "   |   HIGH SCORE: " + std::to_string((int)highScore),
            bx + 90.0f, by + 58.0f, 2.0f, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
        drawUIText(uiShader, "PRESS [R] OR [SPACE] TO PLAY AGAIN", bx + 85.0f, by + 86.0f, 2.1f, glm::vec4(0.0f, 0.95f, 1.0f, 1.0f));
    }
    else if (isPaused) {
        float bw = 420.0f, bh = 75.0f;
        float bx = (scrWidth - bw) * 0.5f, by = (scrHeight - bh) * 0.5f - 30.0f;
        drawUIPanel(uiShader, bx, by, bw, bh, glm::vec4(0.04f, 0.02f, 0.10f, 0.88f));
        drawUIText(uiShader, "SIMULATION PAUSED", bx + 75.0f, by + 18.0f, 2.6f, glm::vec4(1.0f, 0.9f, 0.2f, 1.0f));
        drawUIText(uiShader, "PRESS [P] TO RESUME", bx + 100.0f, by + 48.0f, 2.0f, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
    }

    // 5. HELP POPUP OVERLAY (When H or F1 is toggled)
    if (showHelp) {
        float hw = 540.0f, hh = 210.0f;
        float hx = (scrWidth - hw) * 0.5f, hy = 125.0f;
        drawUIPanel(uiShader, hx, hy, hw, hh, glm::vec4(0.02f, 0.02f, 0.08f, 0.94f));
        drawUIPanel(uiShader, hx, hy, hw, 4.0f, glm::vec4(0.0f, 0.95f, 1.0f, 1.0f));
        drawUIText(uiShader, "CONTROLS & DEFENSE SHORTCUTS [H / F1]", hx + 45.0f, hy + 16.0f, 2.1f, glm::vec4(0.0f, 0.95f, 1.0f, 1.0f));
        drawUIText(uiShader, "[A / D]          : STEER CAR LEFT / RIGHT", hx + 30.0f, hy + 48.0f, 1.8f, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
        drawUIText(uiShader, "[B]              : TOGGLE BLOOM SHADER ON / OFF", hx + 30.0f, hy + 72.0f, 1.8f, glm::vec4(0.2f, 1.0f, 0.4f, 1.0f));
        drawUIText(uiShader, "[P]              : PAUSE / RESUME ANIMATION", hx + 30.0f, hy + 96.0f, 1.8f, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
        drawUIText(uiShader, "[C]              : TOGGLE GAME CAM VS FREE-FLY CAM", hx + 30.0f, hy + 120.0f, 1.8f, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
        drawUIText(uiShader, "[W/A/S/D/Q/E]    : MOVE FREE-FLY CAMERA", hx + 30.0f, hy + 144.0f, 1.8f, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
        drawUIText(uiShader, "[ARROWS / U / O] : MOVE 3D LIGHT SOURCE POSITION", hx + 30.0f, hy + 168.0f, 1.8f, glm::vec4(1.0f, 0.85f, 0.3f, 1.0f));
    }

    glDisable(GL_BLEND);
    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
}

// ============================================================================
// PROCEDURAL 2D TEXTURE GENERATION (Phase 2 Step 5)
// ============================================================================
void generateTextures() {
    const int texSize = 256;
    std::vector<unsigned char> roadData(texSize * texSize * 3);
    for (int y = 0; y < texSize; ++y) {
        for (int x = 0; x < texSize; ++x) {
            int idx = (y * texSize + x) * 3;
            bool isGridLine = (x < 6 || x > texSize - 6 || y < 6 || y > texSize - 6);
            if (isGridLine) {
                roadData[idx + 0] = 180;
                roadData[idx + 1] = 30;
                roadData[idx + 2] = 220;
            }
            else {
                unsigned char noise = (unsigned char)((x * 17 + y * 31) % 12);
                roadData[idx + 0] = 18 + noise;
                roadData[idx + 1] = 18 + noise;
                roadData[idx + 2] = 28 + noise;
            }
        }
    }

    glGenTextures(1, &roadTexture);
    glBindTexture(GL_TEXTURE_2D, roadTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, texSize, texSize, 0, GL_RGB, GL_UNSIGNED_BYTE, roadData.data());
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    std::vector<unsigned char> obsData(texSize * texSize * 3);
    for (int y = 0; y < texSize; ++y) {
        for (int x = 0; x < texSize; ++x) {
            int idx = (y * texSize + x) * 3;
            bool stripe = ((x + y) / 32) % 2 == 0;
            bool border = (x < 8 || x > texSize - 8 || y < 8 || y > texSize - 8);
            if (border || stripe) {
                obsData[idx + 0] = 255;
                obsData[idx + 1] = 255;
                obsData[idx + 2] = 255;
            }
            else {
                obsData[idx + 0] = 55;
                obsData[idx + 1] = 55;
                obsData[idx + 2] = 65;
            }
        }
    }

    glGenTextures(1, &obstacleTexture);
    glBindTexture(GL_TEXTURE_2D, obstacleTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, texSize, texSize, 0, GL_RGB, GL_UNSIGNED_BYTE, obsData.data());
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}

// ============================================================================
// FRAMEBUFFER OBJECT (FBO) SETUP FOR BLOOM (Phase 3 Step 6 & 7)
// ============================================================================
void setupFramebuffers(int width, int height) {
    if (width <= 0 || height <= 0) return;

    if (hdrFBO != 0) {
        glDeleteFramebuffers(1, &hdrFBO);
        glDeleteTextures(2, colorBuffers);
        glDeleteRenderbuffers(1, &rboDepth);
        glDeleteFramebuffers(2, pingpongFBO);
        glDeleteTextures(2, pingpongColorbuffers);
    }

    glGenFramebuffers(1, &hdrFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, hdrFBO);

    glGenTextures(2, colorBuffers);
    for (unsigned int i = 0; i < 2; i++) {
        glBindTexture(GL_TEXTURE_2D, colorBuffers[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_TEXTURE_2D, colorBuffers[i], 0);
    }

    glGenRenderbuffers(1, &rboDepth);
    glBindRenderbuffer(GL_RENDERBUFFER, rboDepth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, width, height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rboDepth);

    unsigned int attachments[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
    glDrawBuffers(2, attachments);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "[ERROR] HDR Framebuffer is not complete!" << std::endl;
    }

    glGenFramebuffers(2, pingpongFBO);
    glGenTextures(2, pingpongColorbuffers);
    for (unsigned int i = 0; i < 2; i++) {
        glBindFramebuffer(GL_FRAMEBUFFER, pingpongFBO[i]);
        glBindTexture(GL_TEXTURE_2D, pingpongColorbuffers[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, pingpongColorbuffers[i], 0);

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            std::cerr << "[ERROR] Ping-Pong Framebuffer " << i << " is not complete!" << std::endl;
        }
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

// ============================================================================
// 3D CUBE & 2D QUAD GEOMETRY INITIALIZATION
// ============================================================================
void renderCube() {
    if (cubeVAO == 0) {
        float vertices[] = {
            // Back face (-Z)
            -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f, 0.0f, 0.0f,
             0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f, 1.0f, 1.0f,
             0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f, 1.0f, 0.0f,
             0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f, 1.0f, 1.0f,
            -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f, 0.0f, 0.0f,
            -0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f, 0.0f, 1.0f,
            // Front face (+Z)
            -0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f, 0.0f, 0.0f,
             0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f, 1.0f, 0.0f,
             0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f, 1.0f, 1.0f,
             0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f, 1.0f, 1.0f,
            -0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f, 0.0f, 1.0f,
            -0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f, 0.0f, 0.0f,
            // Left face (-X)
            -0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f, 1.0f, 0.0f,
            -0.5f,  0.5f, -0.5f, -1.0f,  0.0f,  0.0f, 1.0f, 1.0f,
            -0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f, 0.0f, 1.0f,
            -0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f, 0.0f, 1.0f,
            -0.5f, -0.5f,  0.5f, -1.0f,  0.0f,  0.0f, 0.0f, 0.0f,
            -0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f, 1.0f, 0.0f,
            // Right face (+X)
             0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f, 1.0f, 0.0f,
             0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f, 0.0f, 1.0f,
             0.5f,  0.5f, -0.5f,  1.0f,  0.0f,  0.0f, 1.0f, 1.0f,
             0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f, 0.0f, 1.0f,
             0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f, 1.0f, 0.0f,
             0.5f, -0.5f,  0.5f,  1.0f,  0.0f,  0.0f, 0.0f, 0.0f,
             // Bottom face (-Y)
             -0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f, 0.0f, 1.0f,
              0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f, 1.0f, 1.0f,
              0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f, 1.0f, 0.0f,
              0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f, 1.0f, 0.0f,
             -0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f, 0.0f, 0.0f,
             -0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f, 0.0f, 1.0f,
             // Top face (+Y)
             -0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f, 0.0f, 1.0f,
              0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f, 1.0f, 0.0f,
              0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f, 1.0f, 1.0f,
              0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f, 1.0f, 0.0f,
             -0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f, 0.0f, 1.0f,
             -0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f, 0.0f, 0.0f
        };
        glGenVertexArrays(1, &cubeVAO);
        glGenBuffers(1, &cubeVBO);
        glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
        glBindVertexArray(cubeVAO);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
    }
    glBindVertexArray(cubeVAO);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);
}

void renderQuad() {
    if (quadVAO == 0) {
        float quadVertices[] = {
            -1.0f,  1.0f, 0.0f, 1.0f,
            -1.0f, -1.0f, 0.0f, 0.0f,
             1.0f,  1.0f, 1.0f, 1.0f,
             1.0f, -1.0f, 1.0f, 0.0f,
        };
        glGenVertexArrays(1, &quadVAO);
        glGenBuffers(1, &quadVBO);
        glBindVertexArray(quadVAO);
        glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    }
    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);
}

// ============================================================================
// HIERARCHICAL CAR MODEL USING MATRIX STACK (Phase 1 Step 2)
// ============================================================================
void drawBox(GLuint shader, glm::mat4 model, glm::vec3 color, float emission, int useTex, glm::vec2 uvScale = glm::vec2(1.0f), glm::vec2 uvOffset = glm::vec2(0.0f)) {
    glUniformMatrix4fv(glGetUniformLocation(shader, "model"), 1, GL_FALSE, glm::value_ptr(model));
    glUniform3fv(glGetUniformLocation(shader, "objectColor"), 1, glm::value_ptr(color));
    glUniform1f(glGetUniformLocation(shader, "emissionStrength"), emission);
    glUniform1i(glGetUniformLocation(shader, "useTexture"), useTex);
    glUniform2fv(glGetUniformLocation(shader, "uvScale"), 1, glm::value_ptr(uvScale));
    glUniform2fv(glGetUniformLocation(shader, "uvOffset"), 1, glm::value_ptr(uvOffset));
    renderCube();
}

void renderHierarchicalCar(GLuint shader) {
    std::stack<glm::mat4> matrixStack;

    glm::mat4 rootMatrix = glm::mat4(1.0f);
    rootMatrix = glm::translate(rootMatrix, glm::vec3(carX, 0.45f, 0.0f));
    float steeringTilt = (carX - targetCarX) * 6.0f;
    rootMatrix = glm::rotate(rootMatrix, glm::radians(steeringTilt), glm::vec3(0.0f, 0.0f, 1.0f));

    matrixStack.push(rootMatrix);

    // 1. Main Car Chassis (Parent)
    {
        matrixStack.push(matrixStack.top());
        glm::mat4 chassisModel = glm::scale(matrixStack.top(), glm::vec3(1.3f, 0.4f, 2.6f));
        drawBox(shader, chassisModel, glm::vec3(0.12f, 0.14f, 0.25f), 0.0f, 0);
        matrixStack.pop();
    }

    // 2. Cockpit Cabin (Child of Chassis)
    {
        matrixStack.push(matrixStack.top());
        glm::mat4 cabinModel = glm::translate(matrixStack.top(), glm::vec3(0.0f, 0.32f, -0.1f));
        cabinModel = glm::scale(cabinModel, glm::vec3(0.95f, 0.32f, 1.3f));
        drawBox(shader, cabinModel, glm::vec3(0.05f, 0.85f, 1.0f), 1.8f, 0);
        matrixStack.pop();
    }

    // 3. Twin Glowing Tail Lights (Child of Chassis)
    for (int side = -1; side <= 1; side += 2) {
        matrixStack.push(matrixStack.top());
        glm::mat4 tailLight = glm::translate(matrixStack.top(), glm::vec3(side * 0.42f, 0.08f, 1.31f));
        tailLight = glm::scale(tailLight, glm::vec3(0.35f, 0.15f, 0.08f));
        drawBox(shader, tailLight, glm::vec3(1.0f, 0.05f, 0.35f), 5.5f, 0);
        matrixStack.pop();
    }

    // 4. Four Hierarchical Wheels (Children of Chassis)
    glm::vec3 wheelOffsets[4] = {
        glm::vec3(-0.72f, -0.15f, -0.85f),
        glm::vec3(0.72f, -0.15f, -0.85f),
        glm::vec3(-0.72f, -0.15f,  0.85f),
        glm::vec3(0.72f, -0.15f,  0.85f)
    };

    for (int i = 0; i < 4; ++i) {
        matrixStack.push(matrixStack.top());
        glm::mat4 wheelModel = glm::translate(matrixStack.top(), wheelOffsets[i]);
        wheelModel = glm::rotate(wheelModel, glm::radians(wheelRotation), glm::vec3(1.0f, 0.0f, 0.0f));
        wheelModel = glm::scale(wheelModel, glm::vec3(0.22f, 0.52f, 0.52f));
        drawBox(shader, wheelModel, glm::vec3(0.0f, 0.95f, 1.0f), 2.5f, 0);
        matrixStack.pop();
    }

    matrixStack.pop();
}

// ============================================================================
// WINDOW RESIZE CALLBACK & INPUT HANDLING (Phase 4 Step 8)
// ============================================================================
void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    if (width <= 0 || height <= 0) return;
    scrWidth = width;
    scrHeight = height;
    glViewport(0, 0, width, height);
    setupFramebuffers(width, height);
}

void resetGame() {
    entities.clear();
    score = 0.0f;
    gameSpeed = 18.0f;
    currentLane = 0;
    carX = 0.0f;
    targetCarX = 0.0f;
    gameState = STATE_PLAYING;
    isPaused = false;
    std::cout << "[GAMEPLAY] Game started/restarted! Good luck!" << std::endl;
}

void processInput(GLFWwindow* window, float dt) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    // Start or Restart with SPACE
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS && !keyPressed[GLFW_KEY_SPACE]) {
        if (gameState == STATE_START || gameState == STATE_GAME_OVER) {
            resetGame();
        }
        keyPressed[GLFW_KEY_SPACE] = true;
    }
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_RELEASE) keyPressed[GLFW_KEY_SPACE] = false;

    // Toggle Bloom (B Key)
    if (glfwGetKey(window, GLFW_KEY_B) == GLFW_PRESS && !keyPressed[GLFW_KEY_B]) {
        bloomEnabled = !bloomEnabled;
        keyPressed[GLFW_KEY_B] = true;
        std::cout << "[TOGGLE] Bloom Effect: " << (bloomEnabled ? "ON" : "OFF") << std::endl;
    }
    if (glfwGetKey(window, GLFW_KEY_B) == GLFW_RELEASE) keyPressed[GLFW_KEY_B] = false;

    // Pause Animation (P Key)
    if (glfwGetKey(window, GLFW_KEY_P) == GLFW_PRESS && !keyPressed[GLFW_KEY_P]) {
        if (gameState == STATE_PLAYING) {
            isPaused = !isPaused;
            std::cout << "[TOGGLE] Animation Paused: " << (isPaused ? "YES" : "NO") << std::endl;
        }
        keyPressed[GLFW_KEY_P] = true;
    }
    if (glfwGetKey(window, GLFW_KEY_P) == GLFW_RELEASE) keyPressed[GLFW_KEY_P] = false;

    // Toggle Camera Mode (C Key)
    if (glfwGetKey(window, GLFW_KEY_C) == GLFW_PRESS && !keyPressed[GLFW_KEY_C]) {
        freeCameraMode = !freeCameraMode;
        keyPressed[GLFW_KEY_C] = true;
        std::cout << "[TOGGLE] Camera Mode: " << (freeCameraMode ? "FREE-FLY CAMERA" : "GAME FOLLOW CAMERA") << std::endl;
    }
    if (glfwGetKey(window, GLFW_KEY_C) == GLFW_RELEASE) keyPressed[GLFW_KEY_C] = false;

    // Help Menu (H or F1 Key)
    if ((glfwGetKey(window, GLFW_KEY_H) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_F1) == GLFW_PRESS) && !keyPressed[GLFW_KEY_H]) {
        showHelp = !showHelp;
        keyPressed[GLFW_KEY_H] = true;
    }
    if (glfwGetKey(window, GLFW_KEY_H) == GLFW_RELEASE && glfwGetKey(window, GLFW_KEY_F1) == GLFW_RELEASE) keyPressed[GLFW_KEY_H] = false;

    // Reset Game (R Key)
    if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS && !keyPressed[GLFW_KEY_R]) {
        resetGame();
        keyPressed[GLFW_KEY_R] = true;
    }
    if (glfwGetKey(window, GLFW_KEY_R) == GLFW_RELEASE) keyPressed[GLFW_KEY_R] = false;

    // Toggle Auto Light Movement (L Key)
    if (glfwGetKey(window, GLFW_KEY_L) == GLFW_PRESS && !keyPressed[GLFW_KEY_L]) {
        autoMoveLight = !autoMoveLight;
        keyPressed[GLFW_KEY_L] = true;
    }
    if (glfwGetKey(window, GLFW_KEY_L) == GLFW_RELEASE) keyPressed[GLFW_KEY_L] = false;

    // Manual 3D Light Movement (Arrow Keys + U/O)
    float lightSpeed = 12.0f * dt;
    if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS) { lightPos.x -= lightSpeed; autoMoveLight = false; }
    if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) { lightPos.x += lightSpeed; autoMoveLight = false; }
    if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS) { lightPos.z -= lightSpeed; autoMoveLight = false; }
    if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS) { lightPos.z += lightSpeed; autoMoveLight = false; }
    if (glfwGetKey(window, GLFW_KEY_U) == GLFW_PRESS) { lightPos.y += lightSpeed; autoMoveLight = false; }
    if (glfwGetKey(window, GLFW_KEY_O) == GLFW_PRESS) { lightPos.y -= lightSpeed; autoMoveLight = false; }

    // Camera or Car Controls
    if (freeCameraMode) {
        float camSpeed = 12.0f * dt;
        glm::vec3 front;
        front.x = cos(glm::radians(camYaw)) * cos(glm::radians(camPitch));
        front.y = sin(glm::radians(camPitch));
        front.z = sin(glm::radians(camYaw)) * cos(glm::radians(camPitch));
        front = glm::normalize(front);
        glm::vec3 right = glm::normalize(glm::cross(front, glm::vec3(0.0f, 1.0f, 0.0f)));

        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) freeCamPos += front * camSpeed;
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) freeCamPos -= front * camSpeed;
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) freeCamPos -= right * camSpeed;
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) freeCamPos += right * camSpeed;
        if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) freeCamPos.y += camSpeed;
        if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) freeCamPos.y -= camSpeed;
    }
    else {
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS && !keyPressed[GLFW_KEY_A]) {
            if (gameState == STATE_START) gameState = STATE_PLAYING;
            if (gameState == STATE_PLAYING && !isPaused) {
                if (currentLane > -1) currentLane--;
                targetCarX = currentLane * laneWidth;
            }
            keyPressed[GLFW_KEY_A] = true;
        }
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_RELEASE) keyPressed[GLFW_KEY_A] = false;

        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS && !keyPressed[GLFW_KEY_D]) {
            if (gameState == STATE_START) gameState = STATE_PLAYING;
            if (gameState == STATE_PLAYING && !isPaused) {
                if (currentLane < 1) currentLane++;
                targetCarX = currentLane * laneWidth;
            }
            keyPressed[GLFW_KEY_D] = true;
        }
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_RELEASE) keyPressed[GLFW_KEY_D] = false;
    }
}

// ============================================================================
// MAIN APPLICATION ENTRY POINT
// ============================================================================
int main() {
    srand(static_cast<unsigned int>(time(0)));

    if (!glfwInit()) {
        std::cerr << "[FATAL ERROR] Failed to initialize GLFW!" << std::endl;
        return -1;
    }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(scrWidth, scrHeight, "Neon Highway | Bloom: ON | Score: 0", NULL, NULL);
    if (!window) {
        std::cerr << "[FATAL ERROR] Failed to create GLFW window!" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) {
        std::cerr << "[FATAL ERROR] Failed to initialize GLEW!" << std::endl;
        return -1;
    }

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    // Load 3D Scene, Bloom Blur, and 2D UI Shaders
    GLuint sceneShader = loadShaderFromFile("scene.vert", "scene.frag");
    GLuint blurShader = loadShaderFromFile("blur.vert", "blur.frag");
    GLuint uiShader = loadShaderFromFile("ui.vert", "ui.frag");

    generateTextures();
    generateFontTexture();
    setupFramebuffers(scrWidth, scrHeight);

    glUseProgram(sceneShader);
    glUniform1i(glGetUniformLocation(sceneShader, "diffuseTexture"), 0);

    glUseProgram(blurShader);
    glUniform1i(glGetUniformLocation(blurShader, "image"), 0);
    glUniform1i(glGetUniformLocation(blurShader, "bloomBlur"), 1);

    std::cout << "=======================================================\n"
        << "  NEON HIGHWAY (Headline Technique: Bloom) Initialized!\n"
        << "  Press [SPACE] or [A/D] to Start Driving.\n"
        << "  Press [H] or [F1] to toggle on-screen Help UI.\n"
        << "  Press [B] to toggle Bloom ON/OFF.\n"
        << "=======================================================\n" << std::endl;

    float lastFrame = static_cast<float>(glfwGetTime());
    float fpsTimer = 0.0f;
    int frameCount = 0;

    // ========================================================================
    // MAIN GAME & RENDER LOOP
    // ========================================================================
    while (!glfwWindowShouldClose(window)) {
        float currentFrame = static_cast<float>(glfwGetTime());
        float dt = currentFrame - lastFrame;
        lastFrame = currentFrame;

        // Calculate FPS
        frameCount++;
        fpsTimer += dt;
        if (fpsTimer >= 0.5f) {
            currentFPS = (int)(frameCount / fpsTimer);
            frameCount = 0;
            fpsTimer = 0.0f;
        }

        processInput(window, dt);

        // Ambient animation in STATE_START so Bloom is visible in first 15 seconds
        if (gameState == STATE_START && !isPaused) {
            wheelRotation -= 10.0f * 25.0f * dt;
            highwayScroll -= (10.0f * 0.2f) * dt;
            if (autoMoveLight) {
                lightPos.z += 8.0f * dt;
                if (lightPos.z > 10.0f) lightPos.z = -50.0f;
                lightPos.x = sin(currentFrame * 1.5f) * 4.5f;
            }
        }

        // Active Gameplay Logic (Phase 4 Step 8)
        if (gameState == STATE_PLAYING && !isPaused) {
            score += dt * 10.0f;
            if (score > highScore) highScore = score;
            gameSpeed = 18.0f + (score * 0.04f);

            carX += (targetCarX - carX) * 10.0f * dt;
            wheelRotation -= gameSpeed * 25.0f * dt;
            highwayScroll -= (gameSpeed * 0.2f) * dt;

            if (autoMoveLight) {
                lightPos.z += gameSpeed * 0.7f * dt;
                if (lightPos.z > 10.0f) lightPos.z = -50.0f;
                lightPos.x = sin(currentFrame * 1.5f) * 4.5f;
            }

            spawnTimer += dt;
            float spawnInterval = glm::max(0.35f, 1.1f - (score * 0.002f));
            if (spawnTimer >= spawnInterval) {
                spawnTimer = 0.0f;
                int randomLane = (rand() % 3) - 1;
                int type = (rand() % 100 < 30) ? 1 : 0;
                entities.push_back({ glm::vec3(randomLane * laneWidth, 0.6f, -75.0f), type, true });
            }

            // AABB Bounding-Box Collision Detection (Phase 4 Step 8)
            for (auto& e : entities) {
                if (!e.active) continue;
                e.pos.z += gameSpeed * dt;

                if (std::abs(e.pos.z - 0.0f) < 1.35f && std::abs(e.pos.x - carX) < 1.15f) {
                    if (e.type == 0) {
                        gameState = STATE_GAME_OVER;
                        std::cout << "[GAME OVER] You hit a barrier! Final Score: " << (int)score
                            << " | High Score: " << (int)highScore << " | Press [R] to Reset!" << std::endl;
                    }
                    else {
                        score += 50.0f;
                        if (score > highScore) highScore = score;
                        e.active = false;
                        std::cout << "[BATTERY COLLECTED] +50 Bonus! Score: " << (int)score << std::endl;
                    }
                }
            }

            entities.erase(
                std::remove_if(entities.begin(), entities.end(), [](const Entity& e) { return !e.active || e.pos.z > 12.0f; }),
                entities.end()
            );
        }

        // Update Window Title Bar as well
        std::string title = "Neon Highway | " + TEAM_NAMES + " (" + GROUP_NAME + ") | Bloom: " +
            std::string(bloomEnabled ? "ON" : "OFF") + " | Score: " + std::to_string((int)score);
        glfwSetWindowTitle(window, title.c_str());

        // ====================================================================
        // PASS 1: RENDER 3D SCENE INTO FLOATING-POINT HDR FBO
        // ====================================================================
        glBindFramebuffer(GL_FRAMEBUFFER, hdrFBO);
        glClearColor(0.02f, 0.01f, 0.05f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(sceneShader);

        glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float)scrWidth / (float)scrHeight, 0.1f, 150.0f);
        glm::mat4 view;
        glm::vec3 currentViewPos;

        if (freeCameraMode) {
            glm::vec3 front;
            front.x = cos(glm::radians(camYaw)) * cos(glm::radians(camPitch));
            front.y = sin(glm::radians(camPitch));
            front.z = sin(glm::radians(camYaw)) * cos(glm::radians(camPitch));
            view = glm::lookAt(freeCamPos, freeCamPos + glm::normalize(front), glm::vec3(0.0f, 1.0f, 0.0f));
            currentViewPos = freeCamPos;
        }
        else {
            glm::vec3 camPos(carX * 0.35f, 3.8f, 6.8f);
            glm::vec3 camTarget(carX * 0.2f, 0.7f, -6.0f);
            view = glm::lookAt(camPos, camTarget, glm::vec3(0.0f, 1.0f, 0.0f));
            currentViewPos = camPos;
        }

        glUniformMatrix4fv(glGetUniformLocation(sceneShader, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
        glUniformMatrix4fv(glGetUniformLocation(sceneShader, "view"), 1, GL_FALSE, glm::value_ptr(view));
        glUniform3fv(glGetUniformLocation(sceneShader, "viewPos"), 1, glm::value_ptr(currentViewPos));
        glUniform3fv(glGetUniformLocation(sceneShader, "lightPos"), 1, glm::value_ptr(lightPos));
        glUniform3f(glGetUniformLocation(sceneShader, "lightColor"), 1.2f, 1.1f, 1.5f);

        // 1. Highway Floor
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, roadTexture);
        glm::mat4 floorModel = glm::mat4(1.0f);
        floorModel = glm::translate(floorModel, glm::vec3(0.0f, -0.1f, -35.0f));
        floorModel = glm::scale(floorModel, glm::vec3(11.5f, 0.2f, 95.0f));
        drawBox(sceneShader, floorModel, glm::vec3(1.0f, 1.0f, 1.0f), 0.15f, 1, glm::vec2(6.0f, 45.0f), glm::vec2(0.0f, highwayScroll));

        // 2. Neon Side Rails
        for (int side = -1; side <= 1; side += 2) {
            glm::mat4 railModel = glm::mat4(1.0f);
            railModel = glm::translate(railModel, glm::vec3(side * 5.8f, 0.25f, -35.0f));
            railModel = glm::scale(railModel, glm::vec3(0.25f, 0.4f, 95.0f));
            drawBox(sceneShader, railModel, glm::vec3(1.0f, 0.05f, 0.8f), 3.8f, 0);
        }

        // 3. Cyberpunk Pillars
        for (int i = 0; i < 10; ++i) {
            float zPos = fmod(-i * 10.0f - highwayScroll * 5.0f, 90.0f);
            if (zPos > 10.0f) zPos -= 90.0f;
            for (int side = -1; side <= 1; side += 2) {
                glm::mat4 pillarModel = glm::mat4(1.0f);
                pillarModel = glm::translate(pillarModel, glm::vec3(side * 6.8f, 3.0f, zPos));
                pillarModel = glm::scale(pillarModel, glm::vec3(0.5f, 6.0f, 0.5f));
                glm::vec3 archColor = (i % 2 == 0) ? glm::vec3(0.0f, 0.9f, 1.0f) : glm::vec3(1.0f, 0.1f, 0.7f);
                drawBox(sceneShader, pillarModel, archColor, 3.2f, 0);
            }
        }

        // 4. Movable 3D Light Source Indicator
        {
            glm::mat4 lightModel = glm::mat4(1.0f);
            lightModel = glm::translate(lightModel, lightPos);
            lightModel = glm::scale(lightModel, glm::vec3(0.7f, 0.7f, 0.7f));
            drawBox(sceneShader, lightModel, glm::vec3(1.0f, 0.95f, 0.8f), 6.0f, 0);
        }

        // 5. Hierarchical Player Car
        renderHierarchicalCar(sceneShader);

        // 6. Obstacles & Neon Batteries
        glBindTexture(GL_TEXTURE_2D, obstacleTexture);
        for (const auto& e : entities) {
            if (!e.active) continue;
            glm::mat4 entModel = glm::mat4(1.0f);
            entModel = glm::translate(entModel, e.pos);
            if (e.type == 0) {
                entModel = glm::scale(entModel, glm::vec3(2.1f, 1.1f, 0.8f));
                drawBox(sceneShader, entModel, glm::vec3(1.0f, 0.15f, 0.05f), 2.8f, 1, glm::vec2(2.0f, 1.0f));
            }
            else {
                entModel = glm::rotate(entModel, currentFrame * 3.0f, glm::vec3(0.0f, 1.0f, 0.0f));
                entModel = glm::scale(entModel, glm::vec3(0.8f, 1.1f, 0.8f));
                drawBox(sceneShader, entModel, glm::vec3(0.1f, 1.0f, 0.3f), 4.8f, 0);
            }
        }

        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        // ====================================================================
        // PASS 2: TWO-PASS GAUSSIAN BLUR ON BRIGHT PIXELS
        // ====================================================================
        bool horizontal = true, first_iteration = true;
        unsigned int amount = 10;
        glUseProgram(blurShader);
        glActiveTexture(GL_TEXTURE0);

        for (unsigned int i = 0; i < amount; i++) {
            glBindFramebuffer(GL_FRAMEBUFFER, pingpongFBO[horizontal]);
            glUniform1i(glGetUniformLocation(blurShader, "passType"), horizontal ? 0 : 1);
            glBindTexture(GL_TEXTURE_2D, first_iteration ? colorBuffers[1] : pingpongColorbuffers[!horizontal]);
            renderQuad();
            horizontal = !horizontal;
            if (first_iteration) first_iteration = false;
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        // ====================================================================
        // PASS 3: FINAL COMPOSITE & BLOOM ADDITIVE BLENDING
        // ====================================================================
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glUseProgram(blurShader);
        glUniform1i(glGetUniformLocation(blurShader, "passType"), 2);
        glUniform1i(glGetUniformLocation(blurShader, "bloomEnabled"), bloomEnabled ? 1 : 0);
        glUniform1f(glGetUniformLocation(blurShader, "exposure"), exposure);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, colorBuffers[0]);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, pingpongColorbuffers[!horizontal]);
        renderQuad();

        // ====================================================================
        // PASS 4: 2D ON-SCREEN UI / HUD OVERLAY (Phase 4 Step 8)
        // ====================================================================
        renderHUD(uiShader);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}