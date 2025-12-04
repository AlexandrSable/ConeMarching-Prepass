# Code Examples - Planetary System

## Example 1: Adding a New Planet Type

### Creating a "Gravity Anomaly" Planet

```cpp
// In InitializePlanets() - src/main.cpp

// Create a planet with extremely high gravity for challenge
planets.push_back(Planet(
    glm::vec3(50.0f, 20.0f, 30.0f),      // Far away, high up
    1.0f,                                 // Very small (dense)
    glm::vec3(0.2f, 0.1f, 0.2f),        // Dark purple (mysterious)
    50.0f                                 // INTENSE gravity (5x Earth)
));
```

**Effect:** Players jump toward it get pulled MUCH harder. Great for obstacle/challenge.

## Example 2: Creating a Lava World

### Hostile planet with danger theme

```cpp
// Multiple planets to form a lava system
void InitializePlanets()
{
    planets.clear();
    
    // Main lava world - bright red, hot
    planets.push_back(Planet(
        glm::vec3(0.0f, 0.0f, 0.0f),
        4.0f,
        glm::vec3(1.0f, 0.3f, 0.0f),      // Bright red/orange
        12.0f                              // High gravity
    ));
    
    // Cooled rock satellites
    for (int i = 0; i < 3; i++) {
        float angle = (i / 3.0f) * 6.28f;
        float x = cos(angle) * 20.0f;
        float z = sin(angle) * 20.0f;
        
        planets.push_back(Planet(
            glm::vec3(x, 5.0f, z),
            1.5f,
            glm::vec3(0.3f, 0.3f, 0.3f),  // Gray rock
            2.0f                           // Weak gravity
        ));
    }
}
```

## Example 3: Modifying Player Physics for Different Gravity

### Make player bounce like moon

```cpp
// Modify InitializePlanets to use very low gravity
planets.push_back(Planet(
    glm::vec3(0.0f, 0.0f, 0.0f),
    3.0f,
    glm::vec3(0.8f, 0.8f, 0.8f),
    1.62f                                 // Moon gravity
));

// In Player class (player.h), increase jump power for fun
float jumpPower = 50.0f;                 // Bouncy!
float moveSpeed = 8.0f;                  // Slower on low-g
float groundDrag = 0.90f;                // Slippery!
```

## Example 4: Implementing a Tutorial Level

### Progressive difficulty through planets

```cpp
void InitializePlanets()
{
    planets.clear();
    
    // LEVEL 1: Easy - large planet, comfortable gravity
    planets.push_back(Planet(
        glm::vec3(0.0f, 0.0f, 0.0f),
        5.0f,                             // LARGE
        glm::vec3(0.2f, 0.8f, 0.3f),     // Green - go!
        9.81f                             // Earth-like
    ));
    
    // LEVEL 2: Medium - moderate planet
    planets.push_back(Planet(
        glm::vec3(25.0f, 5.0f, 0.0f),
        3.0f,
        glm::vec3(0.8f, 0.8f, 0.2f),     // Yellow
        8.0f                              // Slightly less
    ));
    
    // LEVEL 3: Hard - small, high gravity
    planets.push_back(Planet(
        glm::vec3(50.0f, 0.0f, 0.0f),
        2.0f,                             // SMALL
        glm::vec3(1.0f, 0.0f, 0.0f),     // Red - danger!
        15.0f                             // INTENSE
    ));
    
    // LEVEL 4: Expert - moon-like precision required
    planets.push_back(Planet(
        glm::vec3(70.0f, 30.0f, 0.0f),
        1.5f,
        glm::vec3(0.5f, 0.5f, 0.5f),     // Gray
        1.62f                             // Moon - tricky!
    ));
}
```

## Example 5: Debugging Player State

### Print diagnostics to console

```cpp
// Add to main loop after physics update
void DebugPlayerState()
{
    static float lastPrintTime = 0.0f;
    float currentTime = glfwGetTime();
    
    if (currentTime - lastPrintTime > 0.5f) {  // Print every 0.5s
        lastPrintTime = currentTime;
        
        std::cout << "=== PLAYER STATE ===\n";
        std::cout << "Position: ("
                  << gamePlayer.position.x << ", "
                  << gamePlayer.position.y << ", "
                  << gamePlayer.position.z << ")\n";
        
        std::cout << "Velocity: "
                  << glm::length(gamePlayer.velocity) << " m/s\n";
        
        std::cout << "On Ground: "
                  << (gamePlayer.onGround ? "YES" : "NO") << "\n";
        
        float dist = glm::distance(gamePlayer.position, planets[0].position);
        std::cout << "Distance to Main Planet: " << dist << "\n";
        std::cout << "Surface Distance: " 
                  << (dist - planets[0].radius) << "\n";
        std::cout << "==================\n\n";
    }
}

// Call in main loop:
DebugPlayerState();
```

## Example 6: Creating Gravity Wells

### Invisible high-gravity zones for gameplay mechanics

```cpp
// "Black hole" effect - player falls fast
planets.push_back(Planet(
    glm::vec3(0.0f, 0.0f, 0.0f),
    0.5f,                                 // Almost invisible small sphere
    glm::vec3(0.0f, 0.0f, 0.0f),         // Black
    100.0f                                // EXTREME gravity
));

// Could make this planet:
// - Hard to reach (far away)
// - Collision = death
// - Optional challenge to overcome
```

## Example 7: Dynamic Planet Spawning

### Procedurally generate planets at runtime

```cpp
// Add this function to dynamically spawn planets
void SpawnRandomPlanet(int count)
{
    for (int i = 0; i < count; i++) {
        // Random position in space
        float x = (rand() % 200) - 100.0f;
        float y = (rand() % 100);
        float z = (rand() % 200) - 100.0f;
        
        // Random size (0.5 to 5.0)
        float radius = 0.5f + (rand() % 45) * 0.1f;
        
        // Random color
        float r = (rand() % 100) / 100.0f;
        float g = (rand() % 100) / 100.0f;
        float b = (rand() % 100) / 100.0f;
        
        // Random gravity (0.5 to 20.0)
        float gravity = 0.5f + (rand() % 195) * 0.1f;
        
        planets.push_back(Planet(
            glm::vec3(x, y, z),
            radius,
            glm::vec3(r, g, b),
            gravity
        ));
    }
}

// In main:
InitializePlanets();        // Base planets
SpawnRandomPlanet(10);      // Add 10 random planets
UpdatePlanetBuffer();       // Send to GPU
```

## Example 8: Speed Run Challenge

### Minimal planets, maximum difficulty

```cpp
void InitializePlanets()
{
    planets.clear();
    
    // Tight obstacle course
    planets.push_back(Planet(glm::vec3(0, 0, 0), 2.0f, 
        glm::vec3(0.2, 0.8, 0.3), 9.81f));
    planets.push_back(Planet(glm::vec3(15, 10, 0), 1.0f, 
        glm::vec3(1.0, 0.5, 0.2), 15.0f));
    planets.push_back(Planet(glm::vec3(25, 5, 15), 1.5f, 
        glm::vec3(0.5, 0.2, 1.0), 5.0f));
    planets.push_back(Planet(glm::vec3(40, 20, 5), 0.8f, 
        glm::vec3(1.0, 1.0, 0.0), 20.0f));
}

// Start timing on first planet
// End timing when reaching final planet
```

## Example 9: Physics Tweaking - Adjusting for Feel

### Make game feel more "arcadey" vs "realistic"

```cpp
// ARCADE FEEL - Bouncy, responsive, forgiving
class Player {
    float mass = 70.0f;
    float moveSpeed = 25.0f;           // Fast movement
    float jumpPower = 40.0f;           // High jumps
    float groundDrag = 0.85f;          // Slippery
    float airDrag = 0.99f;             // Float longer
};

// REALISTIC FEEL - Heavy, careful, challenging
class Player {
    float mass = 70.0f;
    float moveSpeed = 10.0f;           // Slow, careful
    float jumpPower = 15.0f;           // Realistic jump
    float groundDrag = 0.98f;          // Lots of friction
    float airDrag = 0.95f;             // Quick fall
};
```

## Example 10: Multiplayer Scenario

### Setup for split-screen / networked players

```cpp
// Instead of single global player:
std::vector<Player> players;

void InitializeGame()
{
    // Create 2 players
    players.resize(2);
    players[0].position = glm::vec3(-5, 5, 0);
    players[1].position = glm::vec3(5, 5, 0);
}

// In main loop:
for (int i = 0; i < players.size(); i++) {
    // Get input for each player (different keys)
    GLFWwindow* window = glfwGetCurrentContext();
    
    if (i == 0) {
        // Player 1: WASD + Space
        HandleInput(window, players[i], GLFW_KEY_W, GLFW_KEY_A, ...);
    } else {
        // Player 2: Arrow keys + Enter
        HandleInput(window, players[i], GLFW_KEY_UP, GLFW_KEY_LEFT, ...);
    }
    
    players[i].UpdatePhysics(deltaTime, planets);
}

// Network sync (pseudo-code):
for (auto& player : players) {
    SendPlayerPosition(player.position);
    SendPlayerVelocity(player.velocity);
}
```

## Example 11: Collectible System Integration

### Place collectibles on planets

```cpp
struct Collectible {
    glm::vec3 position;
    float radius;
    bool collected;
};

std::vector<Collectible> collectibles;

void InitializeCollectibles()
{
    // Place on each planet surface
    for (const auto& planet : planets) {
        glm::vec3 surfacePoint = planet.position + 
            glm::vec3(cos(0), 0, sin(0)) * (planet.radius + 0.5f);
        
        collectibles.push_back({surfacePoint, 0.3f, false});
    }
}

void CheckCollectibles()
{
    for (auto& item : collectibles) {
        if (!item.collected) {
            float dist = glm::distance(gamePlayer.position, item.position);
            if (dist < 1.0f) {  // Pickup distance
                item.collected = true;
                score += 10;
                PlayPickupSound();
            }
        }
    }
}
```

## Example 12: Shader Integration - Visualize Planets

### Modify compute shader to show planet locations

```glsl
// In computeShader.comp

vec3 ComputeColor(vec3 rayDir)
{
    vec3 color = u_bgColor;
    float minDist = 1e6;
    
    // Check distance to each planet
    for (int i = 0; i < u_numPlanets; i++) {
        vec3 planetPos = planets[i].positionRadius.xyz;
        float radius = planets[i].positionRadius.w;
        vec3 planetColor = planets[i].colorGravity.xyz;
        
        // Distance from ray origin to planet center
        float dist = distance(u_camPos, planetPos);
        
        if (dist < minDist) {
            minDist = dist;
            // Fade planet color based on distance
            float fade = 1.0 / (1.0 + dist * 0.01);
            color = mix(u_bgColor, planetColor, fade);
        }
    }
    
    return color;
}
```

## Performance Optimization Examples

### Example 13: Spatial Partitioning

```cpp
// For very large numbers of planets (>20), add spatial hashing
struct SpatialGrid {
    static const int GRID_SIZE = 100;
    std::vector<Planet>* grid[GRID_SIZE][GRID_SIZE];
    
    int GetGridIndex(float x) {
        return (int)(x / 10.0f) & (GRID_SIZE - 1);
    }
    
    Planet* FindClosestPlanet(glm::vec3 pos) {
        int gx = GetGridIndex(pos.x);
        int gz = GetGridIndex(pos.z);
        
        // Only check nearby cells instead of all planets
        float minDist = FLT_MAX;
        Planet* closest = nullptr;
        
        for (int dx = -1; dx <= 1; dx++) {
            for (int dz = -1; dz <= 1; dz++) {
                // Check grid[gx+dx][gz+dz] instead of all planets
            }
        }
        
        return closest;
    }
};
```

### Example 14: LOD - Distance Based Updates

```cpp
void UpdatePhysicsLOD(float deltaTime)
{
    // Full physics updates
    gamePlayer.UpdatePhysics(deltaTime, planets);
    
    // Update planet buffer every frame (GPU doesn't care about frequency)
    UpdatePlanetBuffer();
    
    // But could skip certain calculations if very far away
    for (auto& planet : planets) {
        float dist = glm::distance(gamePlayer.position, planet.position);
        
        if (dist > 1000.0f) {
            // Skip detailed collision for distant planets
            continue;
        }
    }
}
```

## Testing & Validation

### Example 15: Unit Test Structure

```cpp
// Simple test function for physics
bool TestJumpHeight()
{
    Player p;
    p.position = glm::vec3(0, 3, 0);      // On planet of radius 3
    p.velocity = glm::vec3(0, 0, 0);
    p.onGround = true;
    p.jumpPower = 20.0f;
    
    p.Jump();
    
    // After jump, velocity should be 20 m/s upward
    bool passed = abs(glm::length(p.velocity) - 20.0f) < 0.01f;
    
    return passed;
}

bool TestGravity()
{
    Player p;
    Planet planet(glm::vec3(0, 0, 0), 3.0f, glm::vec3(1,1,1), 9.81f);
    
    p.position = glm::vec3(0, 10, 0);     // 7 units above surface
    p.velocity = glm::vec3(0, 0, 0);      // No initial velocity
    p.onGround = false;
    
    // Apply gravity for 1 second
    p.ApplyGravity(1.0f, planet);
    
    // Should have downward velocity
    bool passed = p.velocity.y < 0.0f;
    
    return passed;
}
```
