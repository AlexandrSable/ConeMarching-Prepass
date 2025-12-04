#include <iostream>
#include <string>
#include <sstream>
#include <fstream>
#include <cmath>
#include <algorithm>
#include <vector>

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <windows.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <imgui/imgui.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_opengl3.h>

#include "camera.h"
#include "player.h"



const unsigned short OPENGL_MAJOR_VERSION = 4;
const unsigned short OPENGL_MINOR_VERSION = 6;

const unsigned int INIT_WIDTH = 900;
const unsigned int INIT_HEIGHT = 900;

int s_width = INIT_WIDTH;
int s_height = INIT_HEIGHT;

int   disp_fps = 0;
float disp_ms  = 0.0f;

// Fullscreen state
bool isFullscreen = true;
GLFWwindow* g_window = nullptr;
int windowedWidth = 1280;
int windowedHeight = 720;
int windowedX = 100;
int windowedY = 100;

// Cascade cone marching resolution scales
// Much finer scales to prevent edge cutting
int cascadeScale1 = 120;   // pixels per cell (was 40, now much finer)
int cascadeScale2 = 60;    // pixels per cell (was 16, now much finer)
int cascadeScale3 = 2;    // pixels per cell (was 4)

float CameraPosLOCX = 0.0f;
float CameraPosLOCY = 8.0f;
float CameraPosLOCZ = -15.0f;

float CameraDirLocPitch = 0.0f;
float CameraDirLocYaw = 0.0f;

// Anti-aliasing samples
int   aaSamples    = 3;
float maxDistance  = 50;
float epsilon      = 0.01f;
int   maxStepsMain = 32;
int   maxStepsCone = 32;
float coneMultiplier = 1.0f;  // Cone step width: increase to make collisions easier (wider cone)

// ──────────────────────────────────────────────────────────────────────── //
//                             PLANETARY SYSTEM                             //
// ──────────────────────────────────────────────────────────────────────── //

std::vector<Planet> planets;
Player gamePlayer;
GLuint planetSSBO = 0;

// GPU timing
struct PassTiming {
    float cascade1_ms = 0.0f;
    float cascade2_ms = 0.0f;
    float cascade3_ms = 0.0f;
    float mainMarch_ms = 0.0f;
    float screenRender_ms = 0.0f;
    float total_ms = 0.0f;
};
PassTiming passTiming;
GLuint timingQueries[10];  // Query objects for timing

float vertices[] = {
    -1.0f, -1.0f,  0.0f, 0.0f,
    1.0f, -1.0f,  1.0f, 0.0f,
    1.0f,  1.0f,  1.0f, 1.0f,
    -1.0f,  1.0f,  0.0f, 1.0f
};

unsigned int indices[] = { 0, 1, 2,  2, 3, 0 };



void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    glViewport(0, 0, width, height);
}

void getFrameRate(int* disp_fps, float* disp_ms)
{
    static float framesPerSecond = 0.0f;
    static int fps = 0;
    static float ms = 0.0f;
    static float lastTime = 0.0f;
    float currentTime = GetTickCount() * 0.001f;
    framesPerSecond++;

    if (currentTime - lastTime > 1.0f)
    {
        lastTime = currentTime;
        fps = (int)framesPerSecond;
        ms = (fps > 0) ? (1000.0f / fps) : 0.0f;
        framesPerSecond = 0;
    }

    *disp_fps = fps;
    *disp_ms = ms;
}

void Dispatch(bool isConePass) //Deprecated
{
    int dispatchX, dispatchY;
    
    if (isConePass) {
        // Coarse cone pass: Dispatch over coarse grid (width/128 × height/128 for example)
        int coarseResX = std::max(1, s_width / 128);
        int coarseResY = std::max(1, s_height / 128);
        dispatchX = std::max(1, (int)ceil((float)coarseResX / float(8)));
        dispatchY = std::max(1, (int)ceil((float)coarseResY / float(4)));
    } else {
        // Pass 1: Dispatch over full resolution
        dispatchX = std::max(1, (int)ceil((float)s_width / float(8)));
        dispatchY = std::max(1, (int)ceil((float)s_height / float(4)));
    }
    
    glDispatchCompute(dispatchX, dispatchY, 1);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
}

void DispatchPass(int passType, GLuint computeProgram)
{
    int dispatchX, dispatchY;
    int resX, resY;
    
    if (passType == 0) {
        // Cascade1: coarse dispatch
        resX = std::max(1, s_width / cascadeScale1);
        resY = std::max(1, s_height / cascadeScale1);
    }
    else if (passType == 1) {
        // Cascade2: medium dispatch
        resX = std::max(1, s_width / cascadeScale2);
        resY = std::max(1, s_height / cascadeScale2);
    }
    else if (passType == 2) {
        // Cascade3: fine dispatch
        resX = std::max(1, s_width / cascadeScale3);
        resY = std::max(1, s_height / cascadeScale3);
    }
    else {
        // Main raymarch: full resolution
        resX = s_width;
        resY = s_height;
    }
    
    dispatchX = std::max(1, (int)ceil((float)resX / float(8)));
    dispatchY = std::max(1, (int)ceil((float)resY / float(4)));
    
    glUniform1i(glGetUniformLocation(computeProgram, "u_passType"), passType);
    glDispatchCompute(dispatchX, dispatchY, 1);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
}

std::string LoadShaderFromPath(const std::string& filename) //Deprecated
{
    // std::ifstream file;
    // std::stringstream buffer;

    // file.open(filename);
    // if (file.is_open())
    // {
    //     buffer << file.rdbuf();
    //     file.close();
    //     return buffer.str();
    // }
    // else
    // {
    //     std::cout << "Failed to load shader: " << filename << std::endl;
    //     return "";
    // }
    return "";
}

std::string LoadShaderWithIncludes(const std::string& filename, int depth = 0) 
{
    if (depth > 8) {
        std::cerr << "ERROR: Shader include depth too large: " << filename << std::endl;
        return "";
    }
    std::ifstream file(filename.c_str());
    if (!file.is_open()) {
        std::cerr << "ERROR: Could not open shader file: " << filename << std::endl;
        return "";
    }
    std::stringstream result;
    std::string line;
    while (std::getline(file, line)) {
        // Remove leading/trailing whitespace
        size_t first = line.find_first_not_of(" \t");
        if (first != std::string::npos) line = line.substr(first);
        size_t last = line.find_last_not_of(" \t");
        if (last != std::string::npos) line = line.substr(0, last+1);
        if (line.find("#include") == 0) {
            std::string includeFile = line.substr(8);
            includeFile.erase(0, includeFile.find_first_not_of(" \t\"<"));
            includeFile.erase(includeFile.find_last_not_of(" \t\">\"") + 1);
            std::string includePath = filename.substr(0, filename.find_last_of("/\\") + 1) + includeFile;
            result << LoadShaderWithIncludes(includePath, depth + 1);
        } else {
            result << line << "\n";
        }
    }
    return result.str();
}

GLuint compileShader(GLenum type, const char* src)
{
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, NULL);
    glCompileShader(shader);

    // print compile log if any
    GLint status = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    GLint logLen = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLen);
    if (logLen > 1) {
        std::string log(logLen, '\0');
        glGetShaderInfoLog(shader, logLen, NULL, &log[0]);
        std::cerr << "Shader compile log (type=" << type << "):\n" << log << std::endl;
    }
    if (status != GL_TRUE) {
        std::cerr << "Shader compile FAILED (type=" << type << ")\n";
        std::cerr << "Source length: " << strlen(src) << " characters\n";
    } else {
        std::cout << "Shader compiled successfully (type=" << type << ")\n";
    }

    return shader;
}

GLuint linkShaderProgram(std::initializer_list<GLuint> shaders)
{
    GLuint shaderProg = glCreateProgram();
    for (auto s: shaders) 
    {
        glAttachShader(shaderProg, s);
        glDeleteShader(s);
    }
    glLinkProgram(shaderProg);

    // print link log if any
    GLint status = GL_FALSE;
    glGetProgramiv(shaderProg, GL_LINK_STATUS, &status);
    GLint logLen = 0;
    glGetProgramiv(shaderProg, GL_INFO_LOG_LENGTH, &logLen);
    if (logLen > 1) {
        std::string log(logLen, '\0');
        glGetProgramInfoLog(shaderProg, logLen, NULL, &log[0]);
        std::cerr << "Program link log:\n" << log << std::endl;
    }
    if (status != GL_TRUE) {
        std::cerr << "Program link FAILED" << std::endl;
    } else {
        std::cout << "Shader program linked successfully\n";
    }

    return shaderProg;
}

void InitializePlanets()
{
    planets.clear();
    
    // Create a few test planets
    planets.push_back(Planet(glm::vec3(0.0f, 0.0f, 0.0f), 3.0f, glm::vec3(0.3f, 0.6f, 1.0f), 9.81f));
    planets.push_back(Planet(glm::vec3(15.0f, 5.0f, 0.0f), 2.5f, glm::vec3(1.0f, 0.5f, 0.3f), 5.0f));
    planets.push_back(Planet(glm::vec3(-12.0f, 3.0f, 10.0f), 2.0f, glm::vec3(0.3f, 1.0f, 0.5f), 7.0f));
    planets.push_back(Planet(glm::vec3(5.0f, -8.0f, 15.0f), 2.2f, glm::vec3(1.0f, 0.8f, 0.2f), 6.5f));
}

void UpdatePlanetBuffer()
{
    std::vector<PlanetData> planetData;
    for (const auto& planet : planets) {
        PlanetData pd;
        pd.positionRadius = glm::vec4(planet.position, planet.radius);
        pd.colorGravity = glm::vec4(planet.color, planet.gravity);
        planetData.push_back(pd);
    }
    
    if (planetSSBO == 0) {
        glGenBuffers(1, &planetSSBO);
    }
    
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, planetSSBO);
    glBufferData(GL_SHADER_STORAGE_BUFFER, planetData.size() * sizeof(PlanetData), planetData.data(), GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, planetSSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void ToggleFullscreen()
{
    if (g_window == nullptr) return;
    
    if (isFullscreen) {
        // Switch to windowed mode
        glfwSetWindowMonitor(g_window, nullptr, windowedX, windowedY, windowedWidth, windowedHeight, 0);
        isFullscreen = false;
    } else {
        // Switch to fullscreen
        GLFWmonitor* monitor = glfwGetPrimaryMonitor();
        const GLFWvidmode* mode = glfwGetVideoMode(monitor);
        glfwSetWindowMonitor(g_window, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
        isFullscreen = true;
    }
}



int main()
{
    // Initialize GLFW 
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, OPENGL_MAJOR_VERSION);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, OPENGL_MINOR_VERSION);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(monitor);
    // Create fullscreen window (borderless fullscreen)
    GLFWwindow* window = glfwCreateWindow(mode->width, mode->height, "CMT", monitor, NULL);
    g_window = window;  // Store global reference for fullscreen toggle
    s_width = mode->width;
    s_height = mode->height;
    
    glfwMakeContextCurrent(window);
    glfwSwapInterval(false); // Disable/Enable V-Sync
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD\n";
        return -1;
    }

    // Initialize IMGUI
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 460");

    Camera camera(s_width, s_height, glm::vec3(0.0f, 8.0f, -15.0f), true);

    char exePath[1024];
    GetModuleFileNameA(NULL, exePath, sizeof(exePath));
    std::string exeDir = std::string(exePath).substr(0, std::string(exePath).find_last_of("/\\"));
    replace(exeDir.begin(), exeDir.end(), '\\', '/');

    std::cout<<"Executable Directory: " << exeDir << "\n";
    std::string computeShaderSourceStr = LoadShaderWithIncludes(exeDir + "/src/ShaderFiles/computeShader.comp");
    std::string vertexShaderSourceStr = LoadShaderWithIncludes(exeDir + "/src/ShaderFiles/vertexShader.vert");
    std::string fragmentShaderSourceStr = LoadShaderWithIncludes(exeDir + "/src/ShaderFiles/fragmentShader.frag");

    const char* computeShaderSource = computeShaderSourceStr.c_str();
    const char* vertexShaderSource = vertexShaderSourceStr.c_str();
    const char* fragmentShaderSource = fragmentShaderSourceStr.c_str();

    

    GLuint computeShader = compileShader(GL_COMPUTE_SHADER, computeShaderSource);
    GLuint computeProgram = linkShaderProgram({ computeShader });

    GLuint screenVertexShader = compileShader(GL_VERTEX_SHADER, vertexShaderSource);
    GLuint screenFragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentShaderSource);
    GLuint shaderProgram = linkShaderProgram({ screenVertexShader, screenFragmentShader });



    unsigned int VBO, VAO, EBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);
    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    // position
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    // texcoord
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    // Cascade1 cone depth texture (coarse, cascadeScale1 pixels per cell)
    GLuint cascade1DepthTex;
    glGenTextures(1, &cascade1DepthTex);
    glBindTexture(GL_TEXTURE_2D, cascade1DepthTex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, std::max(1, s_width / cascadeScale1), std::max(1, s_height / cascadeScale1), 0, GL_RED, GL_FLOAT, NULL);
    glBindImageTexture(0, cascade1DepthTex, 0, GL_FALSE, 0, GL_READ_WRITE, GL_R32F);

    // Cascade2 cone depth texture (medium, cascadeScale2 pixels per cell)
    GLuint cascade2DepthTex;
    glGenTextures(1, &cascade2DepthTex);
    glBindTexture(GL_TEXTURE_2D, cascade2DepthTex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, std::max(1, s_width / cascadeScale2), std::max(1, s_height / cascadeScale2), 0, GL_RED, GL_FLOAT, NULL);
    glBindImageTexture(1, cascade2DepthTex, 0, GL_FALSE, 0, GL_READ_WRITE, GL_R32F);

    // Cascade3 cone depth texture (fine, cascadeScale3 pixels per cell)
    GLuint cascade3DepthTex;
    glGenTextures(1, &cascade3DepthTex);
    glBindTexture(GL_TEXTURE_2D, cascade3DepthTex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, std::max(1, s_width / cascadeScale3), std::max(1, s_height / cascadeScale3), 0, GL_RED, GL_FLOAT, NULL);
    glBindImageTexture(2, cascade3DepthTex, 0, GL_FALSE, 0, GL_READ_WRITE, GL_R32F);

    GLuint outputTex;
    glGenTextures(1, &outputTex);
    glBindTexture(GL_TEXTURE_2D, outputTex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, s_width, s_height, 0, GL_RGBA, GL_FLOAT, NULL);
    glBindImageTexture(3, outputTex, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);



    // Initialize GPU timing queries
    glGenQueries(10, timingQueries);

    // Initialize planetary system
    InitializePlanets();
    UpdatePlanetBuffer();

    float lastFrameTime = glfwGetTime();

    while(!glfwWindowShouldClose(window))
    {
        static float startTime = glfwGetTime();
        float currentTime = glfwGetTime() - startTime;
        
        // Calculate delta time for physics
        float currentFrameTime = glfwGetTime();
        float deltaTime = currentFrameTime - lastFrameTime;
        lastFrameTime = currentFrameTime;
        
        getFrameRate(&disp_fps, &disp_ms);
        glfwGetWindowSize(window, &s_width, &s_height);
        
        // Build title with detailed timing breakdown
        std::string GPUtiming = "FPS: "       + std::to_string(disp_fps) + 
                                " | Total: "  + std::to_string((int)disp_ms) + "ms" +
                                " | C1: "     + std::to_string((int)passTiming.cascade1_ms) + "ms" +
                                " | C2: "     + std::to_string((int)passTiming.cascade2_ms) + "ms" +
                                " | C3: "     + std::to_string((int)passTiming.cascade3_ms) + "ms" +
                                " | Main: "   + std::to_string((int)passTiming.mainMarch_ms) + "ms";
        glfwSetWindowTitle(window, GPUtiming.c_str());
        std::cout << GPUtiming << "\r";

        // ───────────────────────────── Gameplay related shit ──────────────────────────── //
        gamePlayer.ApplyMovementInput(window, deltaTime);
        gamePlayer.UpdatePhysics(deltaTime, planets);
        
        // Update camera to follow player with orientation based on planet surface
        // Camera offset is behind and above the player in local coordinates
        glm::vec3 surfaceNormal = gamePlayer.currentPlanetSurfaceNormal;  // "up" relative to planet
        glm::vec3 cameraLookDir = camera.Orientation;  // Where camera is facing
        glm::vec3 cameraRight = glm::cross(cameraLookDir, surfaceNormal);
        cameraRight = glm::normalize(cameraRight);
        glm::vec3 cameraUp = glm::cross(cameraRight, cameraLookDir);
        
        // Position camera behind player (in camera's local space)
        glm::vec3 cameraOffset = glm::vec3(0.0, 1.0, 0.0);
        camera.Position = gamePlayer.position + cameraOffset;
        
        // Update player orientation to match camera (so WASD moves in camera direction)
        gamePlayer.SetOrientation(cameraLookDir);
        
        // Process mouse input for camera rotation
        camera.ProcessInputs(window, s_width, s_height);
        
        UpdatePlanetBuffer();
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);  // Ensure planet buffer is visible to shaders

        // ──────────────────────────────── Uniform Buffers ─────────────────────────────── //
        glUseProgram(computeProgram);
        glUniform1f(glGetUniformLocation(computeProgram, "u_time"),         currentTime);
        glUniform3f(glGetUniformLocation(computeProgram, "u_camPos"),       camera.Position.x, camera.Position.y, camera.Position.z);
        glUniform3f(glGetUniformLocation(computeProgram, "u_camRot"),       glm::radians(camera.pitch), glm::radians(camera.yaw), 0.0f);
        glUniform2i(glGetUniformLocation(computeProgram, "u_fullRes"),      s_width, s_height);
        glUniform2i(glGetUniformLocation(computeProgram, "u_cascade1Res"),  std::max(1, s_width / cascadeScale1), std::max(1, s_height / cascadeScale1));
        glUniform2i(glGetUniformLocation(computeProgram, "u_cascade2Res"),  std::max(1, s_width / cascadeScale2), std::max(1, s_height / cascadeScale2));
        glUniform2i(glGetUniformLocation(computeProgram, "u_cascade3Res"),  std::max(1, s_width / cascadeScale3), std::max(1, s_height / cascadeScale3));
        glUniform1f(glGetUniformLocation(computeProgram, "u_fov"),          glm::radians(60.0f));
        glUniform1i(glGetUniformLocation(computeProgram, "u_buffer"),       camera.activeBuffer);
        glUniform1i(glGetUniformLocation(computeProgram, "u_aaSamples"),    aaSamples);
        glUniform1f(glGetUniformLocation(computeProgram, "u_maxDist"),      maxDistance);
        glUniform1f(glGetUniformLocation(computeProgram, "u_epsilon"),      epsilon);
        glUniform1i(glGetUniformLocation(computeProgram, "u_maxStepsCone"), maxStepsCone);
        glUniform1i(glGetUniformLocation(computeProgram, "u_maxStepsMain"), maxStepsMain);
        glUniform1f(glGetUniformLocation(computeProgram, "u_coneMultiplier"), coneMultiplier);
        glUniform1i(glGetUniformLocation(computeProgram, "u_numPlanets"),   (int)planets.size());

        // ──────────────────────────── Cascading Cone Prepass ──────────────────────────── //
        // Pass 0: Cascade1 (coarse)
        glBeginQuery(GL_TIME_ELAPSED, timingQueries[0]);
        glBindImageTexture(0, cascade1DepthTex, 0, GL_FALSE, 0, GL_READ_WRITE, GL_R32F);
        DispatchPass(0, computeProgram);
        glEndQuery(GL_TIME_ELAPSED);

        // Pass 1: Cascade2 (medium, refines cascade1 hits)
        glBeginQuery(GL_TIME_ELAPSED, timingQueries[1]);
        glBindImageTexture(1, cascade2DepthTex, 0, GL_FALSE, 0, GL_READ_WRITE, GL_R32F);
        DispatchPass(1, computeProgram);
        glEndQuery(GL_TIME_ELAPSED);

        // Pass 2: Cascade3 (fine, refines cascade2 hits)
        glBeginQuery(GL_TIME_ELAPSED, timingQueries[2]);
        glBindImageTexture(2, cascade3DepthTex, 0, GL_FALSE, 0, GL_READ_WRITE, GL_R32F);
        DispatchPass(2, computeProgram);
        glEndQuery(GL_TIME_ELAPSED);

        // Pass 3: Main raymarch (uses cascade3 depth)
        glBindImageTexture(3, outputTex, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);
        glBeginQuery(GL_TIME_ELAPSED, timingQueries[3]);
        DispatchPass(3, computeProgram);
        glEndQuery(GL_TIME_ELAPSED);

        // ─────────────────────────────── Render to screen ─────────────────────────────── //
        glUseProgram(shaderProgram);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, outputTex);
        glUniform1i(glGetUniformLocation(shaderProgram, "screen"), 0);

        glBeginQuery(GL_TIME_ELAPSED, timingQueries[4]);
        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
        glEndQuery(GL_TIME_ELAPSED);
        
        // ───────────────────────────────── GPU TIMINGS ────────────────────────────────── //
        // Retrieve timing results from previous frame (GPU queries are async)
        if (glfwGetTime() > 0.1) {  // Wait a bit to avoid stalls
            GLuint64 result;
            glGetQueryObjectui64v(timingQueries[0], GL_QUERY_RESULT, &result);
            passTiming.cascade1_ms = result / 1000000.0f;
            glGetQueryObjectui64v(timingQueries[1], GL_QUERY_RESULT, &result);
            passTiming.cascade2_ms = result / 1000000.0f;
            glGetQueryObjectui64v(timingQueries[2], GL_QUERY_RESULT, &result);
            passTiming.cascade3_ms = result / 1000000.0f;
            glGetQueryObjectui64v(timingQueries[3], GL_QUERY_RESULT, &result);
            passTiming.mainMarch_ms = result / 1000000.0f;
            glGetQueryObjectui64v(timingQueries[4], GL_QUERY_RESULT, &result);
            passTiming.screenRender_ms = result / 1000000.0f;
            passTiming.total_ms =   passTiming.cascade1_ms + passTiming.cascade2_ms  + 
                                    passTiming.cascade3_ms + passTiming.mainMarch_ms + 
                                    passTiming.screenRender_ms;
        }

        // ────────────────────────────────── IMGUI RENDER ────────────────────────────────── //
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::Begin("Settings");
        ImGui::Text ("Frame Time:         %.2f ms/frame (%.1f FPS)", 1000.0f / disp_fps, (float)disp_fps);
        ImGui::Text ("GPU Total Time:     %.2f ms", passTiming.total_ms);
        ImGui::Text ("Cascade1 Time:      %.2f ms", passTiming.cascade1_ms);
        ImGui::Text ("Cascade2 Time:      %.2f ms", passTiming.cascade2_ms);
        ImGui::Text ("Cascade3 Time:      %.2f ms", passTiming.cascade3_ms);
        ImGui::Text ("Main Raymarch Time: %.2f ms", passTiming.mainMarch_ms);
        ImGui::Separator();

        ImGui::Text ("Camera Position:   (%.2f, %.2f, %.2f)", camera.Position.x, camera.Position.y, camera.Position.z);
        ImGui::Text ("Camera Pitch/Yaw:  (%.2f, %.2f)", camera.pitch, camera.yaw);
        ImGui::Text ("Active Buffer:      %d", camera.activeBuffer);
        ImGui::Separator();
        
        // Player debug info
        ImGui::Text ("Player Position:   (%.2f, %.2f, %.2f)", gamePlayer.position.x, gamePlayer.position.y, gamePlayer.position.z);
        ImGui::Text ("Player Velocity:   (%.2f, %.2f, %.2f)", gamePlayer.velocity.x, gamePlayer.velocity.y, gamePlayer.velocity.z);
        ImGui::Text ("Player On Ground:  %s", gamePlayer.onGround ? "YES" : "NO");
        ImGui::Text ("Dist to Planet:    %.2f", gamePlayer.distanceToPlanet);
        ImGui::Separator();
        
        if (ImGui::Button(isFullscreen ? "Switch to Windowed" : "Switch to Fullscreen", ImVec2(-1, 0))) {
            ToggleFullscreen();
        }
        ImGui::End();

        ImGui::Begin       ("Camera Controls");
        ImGui::SliderFloat ("Camera X",       &CameraPosLOCX, -10.0f, 10.0f);
        ImGui::SliderFloat ("Camera Y",       &CameraPosLOCY, -10.0f, 10.0f);
        ImGui::SliderFloat ("Camera Z",       &CameraPosLOCZ, -10.0f, 10.0f);
        ImGui::Separator   ();

        ImGui::SliderFloat ("Camera Pitch",   &CameraDirLocPitch, -180.0f, 180.0f);
        ImGui::SliderFloat ("Camera Yaw",     &CameraDirLocYaw, -180.0f, 180.0f);
        ImGui::End         ();

        ImGui::Begin       ("Rendering Settings");
        ImGui::SliderInt   ("AA Samples",     &aaSamples, 1, 5);
        ImGui::SliderFloat ("Max Distance",   &maxDistance, 1.0f, 100.0f);
        ImGui::SliderFloat ("Epsilon",        &epsilon, 0.0001f, 0.1f);
        ImGui::SliderInt   ("Max Steps Cone", &maxStepsCone, 8, 256);
        ImGui::SliderInt   ("Max Steps Main", &maxStepsMain, 8, 256);
        ImGui::SliderFloat ("Cone Multiplier", &coneMultiplier, 0.1f, 10.0f);
        ImGui::End         ();
        
        // camera.Position.x = CameraPosLOCX;
        // camera.Position.y = CameraPosLOCY;
        // camera.Position.z = CameraPosLOCZ;

        // camera.pitch = CameraDirLocPitch;
        // camera.yaw = CameraDirLocYaw;

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
        glfwPollEvents();    
    }

    glDeleteProgram(shaderProgram);
    glDeleteProgram(computeProgram);
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}