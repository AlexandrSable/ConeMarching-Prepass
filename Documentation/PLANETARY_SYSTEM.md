# Planetary Game System Documentation

## Overview

This document describes the planetary jumping game system built on the cone marching ray rendering engine. The system enables spherical gravity, player movement on planet surfaces, and smooth inter-planetary navigation.

## Architecture

### Core Components

#### 1. **Planet Class** (`src/player.h`)
```cpp
struct Planet {
    glm::vec3 position;      // World position
    float radius;            // Planet radius
    glm::vec3 color;         // Surface color (for visualization)
    float gravity;           // Gravity strength (9.81 ≈ Earth-like)
};
```

#### 2. **Player Class** (`src/player.h` & `src/player.cpp`)

Core physics-enabled player with:

**Position & Movement:**
- `position` - Current world position
- `velocity` - Current velocity vector
- `orientation` - Looking direction
- `moveSpeed` (15 m/s) - Ground movement speed
- `jumpPower` (20 m/s) - Initial jump velocity

**State:**
- `onGround` - Whether player is on a planet surface
- `currentPlanetSurfaceNormal` - Local "up" direction for the current planet
- `distanceToPlanet` - Distance to nearest planet center

**Physics Parameters:**
- `mass` (70 kg) - Player mass (mostly for reference)
- `groundDrag` (0.95) - Friction coefficient when grounded
- `airDrag` (0.98) - Air resistance when airborne

#### 3. **GPU Planet Buffer (SSBO)**

Each frame, planet data is packed into a GPU shader storage buffer object (SSBO):

```cpp
struct PlanetData {
    glm::vec4 positionRadius;  // xyz = position, w = radius
    glm::vec4 colorGravity;    // xyz = color, w = gravity strength
};
```

Bound to binding point 4 in compute shaders, accessible to all render passes.

## Physics System

### Spherical Gravity

Players experience gravity towards the nearest planet:

```cpp
void Player::ApplyGravity(float deltaTime, const Planet& planet)
{
    // Direction from player to planet center
    glm::vec3 dirToPlanet = planet.position - position;
    float distance = glm::length(dirToPlanet);
    
    // F = G * (m1 * m2) / r^2
    float gravityMagnitude = (planet.gravity * 10.0f) / (distance * distance);
    glm::vec3 gravityDir = glm::normalize(dirToPlanet);
    
    // Apply acceleration
    velocity += gravityDir * gravityMagnitude * deltaTime;
    
    // Terminal velocity: 100 m/s
}
```

**Key Features:**
- Inverse square law falloff
- Terminal velocity clamping (100 m/s)
- Continuous gravity from closest planet
- Smooth transition when jumping between planets

### Ground Collision Detection

```cpp
void Player::CheckGroundCollision(const std::vector<Planet>& planets)
{
    // Test distance from player to each planet surface
    float surfaceDistance = distance - planet.radius;
    
    // On ground if within 0.5 units of surface
    if (surfaceDistance >= 0.0f && surfaceDistance <= 0.5f) {
        // Snap to surface (0.25 units above for collision margin)
        float targetHeight = planet.radius + 0.25f;
        position = planet.center + direction * targetHeight;
        
        // Cancel velocity component pointing into planet
        velocity = tangential_component_only;
    }
}
```

**Behavior:**
- Automatic snapping to closest planet surface
- Maintains tangential velocity when landing
- Collision margin prevents tunneling
- Supports curved planet surfaces with correct "down" direction

### Movement & Jumping

**Grounded Movement:**
- Local coordinate system aligned with planet surface normal
- WASD controls move relative to "up" direction
- Friction applied to lateral velocity only
- Jump applies velocity in the "up" direction

```cpp
void Player::ApplyMovementInput(GLFWwindow* window, float deltaTime)
{
    // Build local coordinate frame
    glm::vec3 up = currentPlanetSurfaceNormal;
    glm::vec3 forward = calculateLocalForward();
    glm::vec3 right = glm::cross(forward, up);
    
    // WASD applies forces in local frame
    // Jump applies impulse in local up direction
}
```

**Airborne Movement:**
- Gravity pulls toward nearest planet
- Air resistance decelerates velocity
- Only gravity affects trajectory
- No directional input while airborne

## Game Systems

### Planet Initialization

```cpp
void InitializePlanets()
{
    planets.push_back(Planet(
        glm::vec3(0.0f, 0.0f, 0.0f),    // Center
        3.0f,                            // Radius
        glm::vec3(0.3f, 0.6f, 1.0f),    // Color (blue)
        9.81f                            // Earth-like gravity
    ));
    
    // Additional planets around the primary one
    planets.push_back(Planet(glm::vec3(15.0f, 5.0f, 0.0f), 2.5f, ..., 5.0f));
    planets.push_back(Planet(glm::vec3(-12.0f, 3.0f, 10.0f), 2.0f, ..., 7.0f));
}
```

### Per-Frame Updates

```cpp
// In main loop:
gamePlayer.UpdatePhysics(deltaTime, planets);
UpdatePlanetBuffer();  // Update GPU SSBO
```

**UpdatePhysics sequence:**
1. Find closest planet
2. Apply gravity from closest planet
3. Apply air resistance
4. Update position
5. Check ground collision with all planets

### GPU Integration

Planet buffer is updated each frame and available in compute shaders:

```glsl
// In computeShader.comp
struct PlanetData {
    vec4 positionRadius;
    vec4 colorGravity;
};

layout(std430, binding = 4) readonly buffer PlanetBuffer {
    PlanetData planets[];
};

uniform int u_numPlanets;
```

This enables:
- Planet visualization
- SDF distance field generation
- Gravity calculations on GPU if needed
- Per-planet rendering passes

## Configuration

Edit `InitializePlanets()` to customize the world:

```cpp
planets.push_back(Planet(
    glm::vec3(x, y, z),           // Position
    radius,                        // Radius (visible scale)
    glm::vec3(r, g, b),          // Color (0-1 range)
    gravityStrength               // Gravity (9.81 ≈ Earth)
));
```

### Physics Tuning

**Player Physics:**
- `Player::moveSpeed = 15.0f` - Ground movement speed
- `Player::jumpPower = 20.0f` - Jump velocity
- `Player::groundDrag = 0.95f` - Friction (lower = slipperier)
- `Player::airDrag = 0.98f` - Air resistance
- `Player::mass = 70.0f` - Reference mass

**Collision:**
- Surface detection threshold: `0.5f` units
- Surface snap height: `0.25f` units above radius
- Terminal velocity: `100.0f` m/s

**Gravity:**
- Scaling factor: `10.0f` (multiplier on planet.gravity)
- Always pulls toward nearest planet center
- No multiple planet gravity (for performance)

## Future Enhancements

### Potential Features

1. **Multiple Gravity Sources** - Apply gravity from all planets with falloff
2. **Atmosphere Effects** - Variable air density based on altitude
3. **Planet Rotation** - Rotating planets with surface movement
4. **Camera Follow** - Third-person camera aligned with gravity
5. **Gameplay Elements** - Collectibles, portals, obstacles
6. **Physics Solver** - Runge-Kutta integration for better stability
7. **Player Animation** - Mesh deformation based on gravity direction
8. **Particle Effects** - Dust clouds when landing
9. **Sound Design** - Footsteps, jump sounds, gravitational effects
10. **Multiplayer** - Network synchronization for planetary positions

### Performance Optimization

- Planet queries could use spatial hashing for large numbers
- GPU-accelerated player physics (compute shader)
- LOD system for distant planets
- Instanced rendering of multiple small planets

## Technical Details

### Coordinate System

- World space: XYZ with Y-up by default
- Local planet space: Surface normal is "up", radius outward
- Gravity always points toward planet center
- No assumption of fixed up direction

### Delta Time Handling

Physics uses frame-independent delta time:
- Velocity updated as `velocity += acceleration * deltaTime`
- Position updated as `position += velocity * deltaTime`
- Allows variable frame rates without instability

### Collision Margins

- Surface detection: 0.5 units (prevents jitter)
- Snap height: 0.25 units (prevents sticking through surface)
- Total effective collision radius: `planet.radius + 0.25`

## Debugging & Monitoring

**Console Output:**
- GPU timing stats show per-pass milliseconds
- Player position/velocity can be logged

**Visual Debugging:**
- Cascade depth visualizations available in compute shader
- Planet positions visible via color assignment
- Surface normals affect lighting in shader

## Build & Compilation

The system requires:
- **C++17** for structured bindings and variant types
- **GLM** for math (already in project)
- **GLFW** for windowing
- **GLAD** for OpenGL loading
- **ImGui** for UI

Build command includes all required source files:
```bash
g++.exe -std=c++17 -I./include -L./lib \
  ./src/main.cpp ./src/camera.cpp ./src/player.cpp ./src/glad.c \
  ./include/imgui/*.cpp \
  -lglfw3dll -o cutable.exe
```

## Testing the System

1. **Launch application** - Fullscreen at native resolution
2. **WASD movement** - Move on main planet surface
3. **SPACE** - Jump toward adjacent planets
4. **Mouse** - Look around while airborne
5. **Gravity** - Falls toward nearest planet when airborne
6. **Landing** - Snaps to surface with correct orientation
