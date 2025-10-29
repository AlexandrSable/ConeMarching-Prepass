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


int   disp_fps = 0;
float disp_ms  = 0.0f;

const unsigned int SCREEN_WIDTH = 900;
const unsigned int SCREEN_HEIGHT = 900;

const unsigned short OPENGL_MAJOR_VERSION = 4;
const unsigned short OPENGL_MINOR_VERSION = 6;

GLfloat vertices[] =
{
	-1.0f, -1.0f , 0.0f, 0.0f, 0.0f,
	-1.0f,  1.0f , 0.0f, 0.0f, 1.0f,
	 1.0f,  1.0f , 0.0f, 1.0f, 1.0f,
	 1.0f, -1.0f , 0.0f, 1.0f, 0.0f,
};

GLuint indices[] =
{
	0, 2, 1,
	0, 3, 2
};

std::vector<glm::vec4> ParticlePositions; 



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

std::string LoadShaderFromPath(const std::string& filename)
{
    std::ifstream file;
    std::stringstream buffer;

    file.open(filename);
    if (file.is_open())
    {
        buffer << file.rdbuf();
        file.close();
        return buffer.str();
    }
    else
    {
        std::cout << "Failed to load shader: " << filename << std::endl;
        return "";
    }
}



int main()
{
    glfwInit();

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, OPENGL_MAJOR_VERSION);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, OPENGL_MINOR_VERSION);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);



    GLFWwindow* window = glfwCreateWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "FractalPS", NULL, NULL);
    if (!window)
    {
        std::cout << "Failed to open GLFW window\n";
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(false); // Disable/Enable V-Sync

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD\n";
        return -1;
    }

    glViewport(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
 


    // Get executable directory
    char exePath[1024];
    GetModuleFileNameA(NULL, exePath, sizeof(exePath));
    std::string exeDir = std::string(exePath).substr(0, std::string(exePath).find_last_of("/\\"));
    replace(exeDir.begin(), exeDir.end(), '\\', '/');
    // Load shaders (store strings so c_str() remains valid)
    std::cout<<"Executable Directory: " << exeDir << "\n";
    std::string vertexShaderSourceStr = LoadShaderFromPath(exeDir + "/src/shd/vertexShader.vert");
    std::string fragmentShaderSourceStr = LoadShaderFromPath(exeDir + "/src/shd/fragmentShader.frag");
    std::string computeShaderSourceStr = LoadShaderFromPath(exeDir + "/src/shd/computeShader.comp");
    const char* vertexShaderSource = vertexShaderSourceStr.c_str();
    const char* fragmentShaderSource = fragmentShaderSourceStr.c_str();
    const char* computeShaderSource = computeShaderSourceStr.c_str();
    
//===================================================================================// Test Starts
    GLuint VAO, VBO, EBO;
	glCreateVertexArrays(1, &VAO);
	glCreateBuffers(1, &VBO);
	glCreateBuffers(1, &EBO);

	glNamedBufferData(VBO, sizeof(vertices), vertices, GL_STATIC_DRAW);
	glNamedBufferData(EBO, sizeof(indices), indices, GL_STATIC_DRAW);

	glEnableVertexArrayAttrib(VAO, 0);
	glVertexArrayAttribBinding(VAO, 0, 0);
	glVertexArrayAttribFormat(VAO, 0, 3, GL_FLOAT, GL_FALSE, 0);

	glEnableVertexArrayAttrib(VAO, 1);
	glVertexArrayAttribBinding(VAO, 1, 0);
	glVertexArrayAttribFormat(VAO, 1, 2, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat));

	glVertexArrayVertexBuffer(VAO, 0, VBO, 0, 5 * sizeof(GLfloat));
	glVertexArrayElementBuffer(VAO, EBO);



    // create screen texture (compute shader output)
    GLuint screenTex;
    glCreateTextures(GL_TEXTURE_2D, 1, &screenTex);
    glTextureParameteri(screenTex, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTextureParameteri(screenTex, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTextureParameteri(screenTex, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(screenTex, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTextureStorage2D(screenTex, 1, GL_RGBA32F, SCREEN_WIDTH, SCREEN_HEIGHT);
    // bind image unit 0 for compute shader write
    glBindImageTexture(0, screenTex, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);

    GLuint screenVertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(screenVertexShader, 1, &vertexShaderSource, NULL);
    glCompileShader(screenVertexShader);

    GLuint screenFragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(screenFragmentShader, 1, &fragmentShaderSource, NULL);
    glCompileShader(screenFragmentShader);

    GLuint screenShaderProgram = glCreateProgram();
    glAttachShader(screenShaderProgram, screenVertexShader);
    glAttachShader(screenShaderProgram, screenFragmentShader);
    glLinkProgram(screenShaderProgram);

    glDeleteShader(screenVertexShader);
    glDeleteShader(screenFragmentShader);



    GLuint computeShader = glCreateShader(GL_COMPUTE_SHADER);
    glShaderSource(computeShader, 1, &computeShaderSource, NULL);
    glCompileShader(computeShader);

    GLuint computeProgram = glCreateProgram();
    glAttachShader(computeProgram, computeShader);
    glLinkProgram(computeProgram);

    glDeleteShader(computeShader);

    // --- create particle SSBO ---
    const int PARTICLE_COUNT = 65536 * 5;
    struct ParticleCPU { float x, y; float vx, vy; float life; unsigned int seed; float pad; };
    std::vector<ParticleCPU> particles(PARTICLE_COUNT);
    // initialize particles
    unsigned int seed = 12345u;
    for (int i = 0; i < PARTICLE_COUNT; ++i) {
        // simple random using linear congruential
        seed = seed * 1664525u + 1013904223u;
        float r1 = ((seed & 0xFFFF) / float(0xFFFF)) * 2.0f - 1.0f;
        seed = seed * 1664525u + 1013904223u;
        float r2 = ((seed & 0xFFFF) / float(0xFFFF)) * 2.0f - 1.0f;
        particles[i].x = r1 * 0.5f;
        particles[i].y = r2 * 0.5f;
        float ang = fmodf((seed & 0xFFFF) / float(0xFFFF) * 6.2831853f, 6.2831853f);
        float spd = 0.1f + ((seed >> 8) & 0xFF) / 255.0f * 0.5f;
        particles[i].vx = cosf(ang) * spd;
        particles[i].vy = sinf(ang) * spd;
        particles[i].life = 1.0f + ((seed >> 16) & 0xFF) / 255.0f * 4.0f;
        particles[i].seed = seed ^ (unsigned int)i;
        particles[i].pad = 0.0f;
    }

    GLuint ssbo;
    glCreateBuffers(1, &ssbo);
    glNamedBufferData(ssbo, PARTICLE_COUNT * sizeof(ParticleCPU), particles.data(), GL_DYNAMIC_COPY);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, ssbo);

    glUseProgram(computeProgram);
    GLint loc_dt = glGetUniformLocation(computeProgram, "u_dt");
    GLint loc_count = glGetUniformLocation(computeProgram, "u_particleCount");
    if (loc_count >= 0) glUniform1i(loc_count, PARTICLE_COUNT);


	int work_grp_cnt[3];
	glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 0, &work_grp_cnt[0]);
	glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 1, &work_grp_cnt[1]);
	glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 2, &work_grp_cnt[2]);
	std::cout << "Max work groups per compute shader" << 
		" x:" << work_grp_cnt[0] <<
		" y:" << work_grp_cnt[1] <<
		" z:" << work_grp_cnt[2] << "\n";

	int work_grp_size[3];
	glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 0, &work_grp_size[0]);
	glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 1, &work_grp_size[1]);
	glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 2, &work_grp_size[2]);
	std::cout << "Max work group sizes" <<
		" x:" << work_grp_size[0] <<
		" y:" << work_grp_size[1] <<
		" z:" << work_grp_size[2] << "\n";

	int work_grp_inv;
	glGetIntegerv(GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS, &work_grp_inv);
	std::cout << "Max invocations count per work group: " << work_grp_inv << "\n";
//===================================================================================// Test End


    // Do while program is running 
    {
        double lastTime = GetTickCount() * 0.001;
    }
    while(!glfwWindowShouldClose(window))
    {
        // delta time
        double currentTime = GetTickCount() * 0.001;
        static double prevTime = currentTime;
        float dt = float(currentTime - prevTime);
        prevTime = currentTime;

        getFrameRate(&disp_fps, &disp_ms);
        glfwSetWindowTitle(window, ("FractalPS - fps: " + std::to_string(disp_fps) + " | ms: "+ std::to_string(disp_ms)).c_str());

        // clear the compute output texture to black each frame
        float clearColor[4] = {0.0f, 0.0f, 0.0f, 1.0f};
        glClearTexImage(screenTex, 0, GL_RGBA, GL_FLOAT, clearColor);

        // dispatch compute for particles
        const int local_size = 256;
        const int num_groups = ( (PARTICLE_COUNT) + local_size - 1) / local_size;
        glUseProgram(computeProgram);
        // set dt uniform
        if (loc_dt >= 0) glUniform1f(loc_dt, dt);
        glDispatchCompute(num_groups, 1, 1);
        // ensure writes are visible to subsequent rendering
        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_SHADER_STORAGE_BARRIER_BIT);

        // render fullscreen textured quad
        glUseProgram(screenShaderProgram);
        glBindTextureUnit(0, screenTex);
        glUniform1i(glGetUniformLocation(screenShaderProgram, "screen"), 0);
        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, sizeof(indices) / sizeof(indices[0]), GL_UNSIGNED_INT, 0);

        glfwSwapBuffers(window);
        glfwPollEvents();    
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}