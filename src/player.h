#ifndef PLAYER_CLASS_H
#define PLAYER_CLASS_H

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <glm/gtx/vector_angle.hpp>
#include <vector>

#include "camera.h"

// ──────────────────────────────────────────────────────────────────────── //
//                              PLANET CLASS                                //
// ──────────────────────────────────────────────────────────────────────── //

struct Planet {
    glm::vec3 position;
    float radius;
    glm::vec3 color;
    float gravity;
    
    Planet() : position(0.0f), radius(1.0f), color(1.0f), gravity(1.0f) {}
    Planet(glm::vec3 pos, float r, glm::vec3 col, float g) 
        : position(pos), radius(r), color(col), gravity(g) {}
};

// GPU-compatible planet data for shader
struct PlanetData {
    glm::vec4 positionRadius;  // xyz = position, w = radius
    glm::vec4 colorGravity;    // xyz = color, w = gravity strength
};

// ──────────────────────────────────────────────────────────────────────── //
//                              PLAYER CLASS                                //
// ──────────────────────────────────────────────────────────────────────── //

class Player
{   
    public:
        // Position and orientation
        glm::vec3 position;
        glm::vec3 velocity;
        glm::vec3 orientation = glm::vec3(0.0f, 0.0f, -1.0f);

        // Physics parameters
        float mass = 70.0f;           // kg
        float moveSpeed = 15.0f;      // m/s
        float jumpPower = 20.0f;      // m/s initial velocity
        float groundDrag = 0.95f;     // friction coefficient (on ground)
        float airDrag = 0.999f;       // air resistance (much lower, gravity needs to work)

        // State
        bool onGround = false;
        bool jumpPressed = false;
        glm::vec3 currentPlanetSurfaceNormal = glm::vec3(0.0f, 1.0f, 0.0f);
        float distanceToPlanet = 0.0f;

        // Constructor/Destructor
        Player();
        ~Player() = default;

        // Physics and movement
        void UpdatePhysics(float deltaTime, const std::vector<Planet>& planets);
        void ApplyGravity(float deltaTime, const Planet& planet);
        void ApplyMovementInput(GLFWwindow* window, float deltaTime);
        void SetOrientation(const glm::vec3& newOrientation) { orientation = newOrientation; }
        void Jump();
        void CheckGroundCollision(const std::vector<Planet>& planets);
        
        // Helper functions
        glm::vec3 GetLocalUpDirection(const glm::vec3& surfacePoint, const Planet& planet) const;
        bool IsNearPlanetSurface(const Planet& planet, float threshold = 0.5f) const;
};

#endif
