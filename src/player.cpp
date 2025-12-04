#include "player.h"
#include <iostream>
#include <algorithm>
#include <limits>

Player::Player()
{
    position = glm::vec3(0.0f, 3.5f, 0.0f);
    velocity = glm::vec3(0.0f);
    orientation = glm::vec3(0.0f, 0.0f, -1.0f);
}

// ──────────────────────────────────────────────────────────────────────── //
//                          GRAVITY & PHYSICS                               //
// ──────────────────────────────────────────────────────────────────────── //

glm::vec3 Player::GetLocalUpDirection(const glm::vec3& surfacePoint, const Planet& planet) const
{
    return glm::normalize(surfacePoint - planet.position);
}

bool Player::IsNearPlanetSurface(const Planet& planet, float threshold) const
{
    float dist = glm::distance(position, planet.position);
    float surfaceDistance = dist - planet.radius;
    return surfaceDistance >= 0.0f && surfaceDistance <= threshold;
}

void Player::ApplyGravity(float deltaTime, const Planet& planet)
{
    glm::vec3 dirToPlanet = planet.position - position;
    float distance = glm::length(dirToPlanet);
    
    if (distance < 0.1f) return;
    
    // Gravity force: F = G * (m1 * m2) / r^2
    // Simplified: use planet gravity strength and distance
    float gravityMagnitude = (planet.gravity * 10.0f) / (distance * distance);
    glm::vec3 gravityDir = glm::normalize(dirToPlanet);
    
    // Apply acceleration
    glm::vec3 acceleration = gravityDir * gravityMagnitude;
    velocity += acceleration * deltaTime;
    
    // Clamp terminal velocity
    float speed = glm::length(velocity);
    if (speed > 100.0f) {
        velocity = glm::normalize(velocity) * 100.0f;
    }
}

void Player::CheckGroundCollision(const std::vector<Planet>& planets)
{
    onGround = false;
    
    for (const auto& planet : planets) {
        if (IsNearPlanetSurface(planet, 0.5f)) {
            float dist = glm::distance(position, planet.position);
            float targetHeight = planet.radius + 0.25f;  // Slightly above surface
            
            // Snap to surface and zero out downward velocity
            glm::vec3 dirFromPlanet = glm::normalize(position - planet.position);
            position = planet.position + dirFromPlanet * targetHeight;
            
            // Remove velocity component pointing into planet
            float velTowardsPlanet = glm::dot(velocity, -dirFromPlanet);
            if (velTowardsPlanet > 0.0f) {
                velocity -= dirFromPlanet * velTowardsPlanet;
            }
            
            onGround = true;
            currentPlanetSurfaceNormal = dirFromPlanet;
            distanceToPlanet = glm::distance(position, planet.position);
            break;
        }
    }
}

void Player::ApplyMovementInput(GLFWwindow* window, float deltaTime)
{
    if (!onGround || window == nullptr) return;
    
    // Get local coordinate system relative to planet surface
    glm::vec3 localUp = currentPlanetSurfaceNormal;
    glm::vec3 localForward = glm::normalize(orientation);
    glm::vec3 localRight = glm::cross(localForward, localUp);
    localRight = glm::normalize(localRight);
    localForward = glm::cross(localUp, localRight);
    
    glm::vec3 moveDirection(0.0f);
    
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        moveDirection += localForward;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        moveDirection -= localForward;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        moveDirection -= localRight;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        moveDirection += localRight;
    
    if (glm::length(moveDirection) > 0.0f) {
        moveDirection = glm::normalize(moveDirection);
        velocity += moveDirection * moveSpeed * deltaTime;
    }
    
    // Apply friction
    glm::vec3 flatVelocity = velocity - localUp * glm::dot(velocity, localUp);
    flatVelocity *= groundDrag;
    velocity = flatVelocity + localUp * glm::dot(velocity, localUp);
    
    // Jump
    if (window != nullptr && glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS && !jumpPressed) {
        Jump();
        jumpPressed = true;
    }
    if (window != nullptr && glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_RELEASE) {
        jumpPressed = false;
    }
}

void Player::Jump()
{
    if (!onGround) return;
    velocity += currentPlanetSurfaceNormal * jumpPower;
    onGround = false;
}

void Player::UpdatePhysics(float deltaTime, const std::vector<Planet>& planets)
{
    // Find closest planet
    float closestDist = FLT_MAX;
    const Planet* closestPlanet = nullptr;
    
    for (const auto& planet : planets) {
        float dist = glm::distance(position, planet.position);
        if (dist < closestDist) {
            closestDist = dist;
            closestPlanet = &planet;
        }
    }
    
    if (closestPlanet != nullptr) {
        // ALWAYS apply gravity towards closest planet
        ApplyGravity(deltaTime, *closestPlanet);
    }
    
    // Apply air resistance
    velocity *= airDrag;
    
    // Update position
    position += velocity * deltaTime;
    
    // Check ground collision with all planets
    CheckGroundCollision(planets);
    
    // If on ground, clamp to surface and zero out radial velocity
    if (onGround && closestPlanet != nullptr) {
        float distToCenter = glm::distance(position, closestPlanet->position);
        float targetHeight = closestPlanet->radius + 0.25f;
        glm::vec3 dirFromPlanet = glm::normalize(position - closestPlanet->position);
        
        // Always snap to target height when on ground (sticky contact)
        position = closestPlanet->position + dirFromPlanet * targetHeight;
        
        // ZERO OUT ALL velocity component in radial direction (towards/away from planet)
        // This prevents velocity from accumulating radially while stuck on surface
        float radialVelocity = glm::dot(velocity, dirFromPlanet);
        velocity -= dirFromPlanet * radialVelocity;
    }
}