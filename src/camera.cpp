#include "camera.h"
#include <iostream>

using namespace std;

Camera::Camera(int width, int height, glm::vec3 position, bool controlledByPlayer)
{
    Camera::width = width;
    Camera::height = height;
    Position = position;
    Camera::contolledByPlayer = controlledByPlayer;
}

void Camera::ProcessInputs(GLFWwindow *window, int width, int height)
{
    // Check current owner
    if(!contolledByPlayer)
        return;

    // When controlled by player, only handle mouse rotation (position is set externally)
    // Don't process WASD movement - that's handled by player physics

    if(glfwGetKey(window, GLFW_KEY_F1) == GLFW_PRESS)
        activeBuffer = FINAL;
    if(glfwGetKey(window, GLFW_KEY_F2) == GLFW_PRESS)
        activeBuffer = NORMAL;
    if(glfwGetKey(window, GLFW_KEY_F3) == GLFW_PRESS)
        activeBuffer = DISTANCE;
    if(glfwGetKey(window, GLFW_KEY_F4) == GLFW_PRESS)
        activeBuffer = ID;
    if(glfwGetKey(window, GLFW_KEY_F5) == GLFW_PRESS)
        activeBuffer = STEPCOUNT;


    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS)
    {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);

        if (firstClick)
        {
            glfwSetCursorPos(window, (width / 2), (height / 2));
            firstClick = false;
        }

        double mouseX, mouseY;
        glfwGetCursorPos(window, &mouseX, &mouseY);

        float xOffset = sensitivity * (float)(mouseX - (width / 2));
        float yOffset = sensitivity * (float)((height / 2) - mouseY);

        yaw -= xOffset;
        pitch = glm::clamp(pitch + yOffset, -89.5f, 89.5f);

    Orientation = glm::normalize(glm::vec3(-sin(glm::radians(yaw)), 
                                            sin(glm::radians(pitch)), 
                                            cos(glm::radians(pitch)) * cos(glm::radians(yaw))));

        glfwSetCursorPos(window, (width / 2), (height / 2));
    }
    else if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_RELEASE)
    {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        firstClick = true;
    }
}