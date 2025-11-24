#include <iostream>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <windows.h>
#include <string>
#include <sstream>
#include <fstream>
#include <cmath>
#include <algorithm>
#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "camera.h"
#include "camera.cpp"



const unsigned short OPENGL_MAJOR_VERSION = 4;
const unsigned short OPENGL_MINOR_VERSION = 6;

const unsigned int INIT_WIDTH = 900;
const unsigned int INIT_HEIGHT = 900;

int s_width = INIT_WIDTH;
int s_height = INIT_HEIGHT;

int   disp_fps = 0;
float disp_ms  = 0.0f;

// Cascade cone marching resolution scales
int cascadeScale1 = 120;  // pixels per cell
int cascadeScale2 = 60;   // pixels per cell
int cascadeScale3 = 2;    // pixels per cell

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

std::string LoadShaderWithIncludes(const string& filename, int depth = 0) 
{
    if (depth > 8) {
        cerr << "ERROR: Shader include depth too large: " << filename << endl;
        return "";
    }
    ifstream file(filename.c_str());
    if (!file.is_open()) {
        cerr << "ERROR: Could not open shader file: " << filename << endl;
        return "";
    }
    stringstream result;
    string line;
    while (getline(file, line)) {
        // Remove leading/trailing whitespace
        size_t first = line.find_first_not_of(" \t");
        if (first != string::npos) line = line.substr(first);
        size_t last = line.find_last_not_of(" \t");
        if (last != string::npos) line = line.substr(0, last+1);
        if (line.find("#include") == 0) {
            string includeFile = line.substr(8);
            includeFile.erase(0, includeFile.find_first_not_of(" \t\"<"));
            includeFile.erase(includeFile.find_last_not_of(" \t\">\"") + 1);
            string includePath = filename.substr(0, filename.find_last_of("/\\") + 1) + includeFile;
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
    }

    return shaderProg;
}



int main()
{
    glfwInit();

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, OPENGL_MAJOR_VERSION);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, OPENGL_MINOR_VERSION);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(monitor);
    // Create fullscreen window (borderless fullscreen)
    GLFWwindow* window = glfwCreateWindow(mode->width, mode->height, "Cone Marching Fractal", monitor, NULL);
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

    Camera camera(s_width, s_height, glm::vec3(0.0f, 0.0f, -5.0f));

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



    while(!glfwWindowShouldClose(window))
    {
        static float startTime = glfwGetTime();
        float currentTime = glfwGetTime() - startTime;
        
        getFrameRate(&disp_fps, &disp_ms);
        glfwGetWindowSize(window, &s_width, &s_height);
        glfwSetWindowTitle(window, ("ConeMarching Prepass test - fps: " + std::to_string(disp_fps) + " | ms: "+ std::to_string(disp_ms)).c_str());
        cout<<"FPS: " << disp_fps << " | MS: " << disp_ms << "\r";

        camera.ProcessInputs(window, s_width, s_height);

        glUseProgram(computeProgram);
        glUniform1f(glGetUniformLocation(computeProgram, "u_time"),     currentTime);
        glUniform3f(glGetUniformLocation(computeProgram, "u_camPos"),   camera.Position.x, camera.Position.y, camera.Position.z);
        glUniform3f(glGetUniformLocation(computeProgram, "u_camRot"),   glm::radians(camera.pitch), glm::radians(camera.yaw), 0.0f);
        glUniform2i(glGetUniformLocation(computeProgram, "u_fullRes"),  s_width, s_height);
        glUniform2i(glGetUniformLocation(computeProgram, "u_cascade1Res"), std::max(1, s_width / cascadeScale1), std::max(1, s_height / cascadeScale1));
        glUniform2i(glGetUniformLocation(computeProgram, "u_cascade2Res"), std::max(1, s_width / cascadeScale2), std::max(1, s_height / cascadeScale2));
        glUniform2i(glGetUniformLocation(computeProgram, "u_cascade3Res"), std::max(1, s_width / cascadeScale3), std::max(1, s_height / cascadeScale3));
        glUniform1f(glGetUniformLocation(computeProgram, "u_fov"),      glm::radians(60.0f));
        glUniform1i(glGetUniformLocation(computeProgram, "u_buffer"),   camera.activeBuffer);

        // ──────────────────────────── Cascading Cone Prepass ──────────────────────────── //
        // Pass 0: Cascade1 (coarse)
        glBindImageTexture(0, cascade1DepthTex, 0, GL_FALSE, 0, GL_READ_WRITE, GL_R32F);
        glBindImageTexture(1, cascade2DepthTex, 0, GL_FALSE, 0, GL_READ_WRITE, GL_R32F);
        glBindImageTexture(2, cascade3DepthTex, 0, GL_FALSE, 0, GL_READ_WRITE, GL_R32F);
        glBindImageTexture(3, outputTex,        0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F);
        DispatchPass(0, computeProgram);

        // Pass 1: Cascade2 (medium, refines cascade1 hits)
        DispatchPass(1, computeProgram);

        // Pass 2: Cascade3 (fine, refines cascade2 hits)
        DispatchPass(2, computeProgram);

        // Pass 3: Main raymarch (uses cascade3 depth)
        glBindImageTexture(3, outputTex, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);
        DispatchPass(3, computeProgram);

        // ─────────────────────────────── Render to screen ────────────────────────────── //
        glUseProgram(shaderProgram);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, outputTex);
        glUniform1i(glGetUniformLocation(shaderProgram, "screen"), 0);

        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

        glfwSwapBuffers(window);
        glfwPollEvents();    
    }

    glDeleteProgram(shaderProgram);
    glDeleteProgram(computeProgram);
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}