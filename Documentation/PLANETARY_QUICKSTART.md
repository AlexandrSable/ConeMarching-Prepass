# Planetary System - Quick Start Guide

## Adding Planets

Edit `src/main.cpp` in the `InitializePlanets()` function:

```cpp
void InitializePlanets()
{
    planets.clear();
    
    // Example: Small moon with weak gravity
    planets.push_back(Planet(
        glm::vec3(20.0f, 10.0f, 0.0f),   // Position
        1.5f,                            // Radius
        glm::vec3(0.8f, 0.8f, 0.8f),    // Light gray color
        1.6f                             // Moon-like gravity
    ));
}
```

### Color Palette Reference
- **Blue** (0.3, 0.6, 1.0) - Water world
- **Red** (1.0, 0.3, 0.3) - Rocky/lava
- **Green** (0.3, 1.0, 0.5) - Forest
- **Orange** (1.0, 0.6, 0.2) - Sand
- **Gray** (0.8, 0.8, 0.8) - Stone

### Gravity Strength Reference
- **1.6** - Moon (1/6 Earth)
- **3.7** - Mercury (1/3 Earth)
- **8.87** - Venus (9/10 Earth)
- **9.81** - Earth (1.0)
- **24.79** - Jupiter (2.5 Earth)

## Physics Parameters

### Controlling Movement Speed

```cpp
// In Player class (player.h)
float moveSpeed = 15.0f;  // m/s - increase for faster movement
float jumpPower = 20.0f;  // m/s - increase for higher jumps
```

### Adjusting Friction

```cpp
// Ground friction (0.0 = ice, 1.0 = perfect grip)
float groundDrag = 0.95f;  // Decrease for more slippery

// Air resistance (higher = slower fall)
float airDrag = 0.98f;
```

### Terminal Velocity

Set maximum fall speed in `ApplyGravity()`:
```cpp
// Clamp terminal velocity
float speed = glm::length(velocity);
if (speed > 100.0f) {  // Change this value
    velocity = glm::normalize(velocity) * 100.0f;
}
```

## Collision Tuning

### Surface Detection Range

In `CheckGroundCollision()`:
```cpp
// Increase for larger collision margin
if (IsNearPlanetSurface(planet, 0.5f))  // Change 0.5f
```

### Surface Snap Height

```cpp
// Adjust how far above surface player stands
float targetHeight = planet.radius + 0.25f;  // Change 0.25f
```

## GPU Integration

### Access Planets in Compute Shader

```glsl
// In computeShader.comp
for (int i = 0; i < u_numPlanets; i++) {
    vec3 planetPos = planets[i].positionRadius.xyz;
    float radius = planets[i].positionRadius.w;
    vec3 color = planets[i].colorGravity.xyz;
    float gravity = planets[i].colorGravity.w;
    
    // Use planet data for distance field, lighting, etc.
}
```

### Update Buffer Binding

Planet buffer is automatically updated each frame via:
```cpp
UpdatePlanetBuffer();  // Called in main loop
```

## Common Scenarios

### Small Planet Cluster

```cpp
void InitializePlanets()
{
    planets.clear();
    
    // Central planet
    planets.push_back(Planet(glm::vec3(0, 0, 0), 3.0f, 
        glm::vec3(0.3, 0.6, 1.0), 9.81f));
    
    // Orbiting satellites (static positions for now)
    for (int i = 0; i < 4; i++) {
        float angle = (i / 4.0f) * 6.28f;
        float x = cos(angle) * 15.0f;
        float z = sin(angle) * 15.0f;
        planets.push_back(Planet(glm::vec3(x, 0, z), 1.5f,
            glm::vec3(0.8, 0.4, 0.2), 3.0f));
    }
}
```

### Moon System

```cpp
void InitializePlanets()
{
    planets.clear();
    
    // Primary planet
    planets.push_back(Planet(glm::vec3(0, 0, 0), 5.0f,
        glm::vec3(0.2, 0.4, 0.8), 9.81f));
    
    // Small moon
    planets.push_back(Planet(glm::vec3(30, 10, 0), 1.0f,
        glm::vec3(0.6, 0.6, 0.6), 1.6f));
}
```

### Binary System

```cpp
void InitializePlanets()
{
    planets.clear();
    
    // Two similar planets far apart
    planets.push_back(Planet(glm::vec3(-15, 0, 0), 2.5f,
        glm::vec3(1.0, 0.3, 0.3), 8.0f));
    
    planets.push_back(Planet(glm::vec3(15, 0, 0), 2.5f,
        glm::vec3(0.3, 1.0, 0.3), 8.0f));
}
```

## Debugging

### Print Player State

Add to main loop:
```cpp
std::cout << "Player pos: (" << gamePlayer.position.x << ", "
          << gamePlayer.position.y << ", " 
          << gamePlayer.position.z << ")\n";
std::cout << "On ground: " << (gamePlayer.onGround ? "YES" : "NO") << "\n";
std::cout << "Velocity: " << glm::length(gamePlayer.velocity) << " m/s\n";
```

### Visualize Gravity

Modify compute shader to color planets by gravity strength:
```glsl
vec3 debugColor = vec3(gravity / 10.0f);  // Normalize to 0-1
```

### Track Performance

GPU timing is displayed in window title:
- **C1/C2/C3**: Cascade pass times
- **Main**: Main march time
- Total frame time shown as FPS

## Camera Integration

Player position doesn't automatically control camera. To follow player:

```cpp
// Add to main loop after physics update
camera.Position = gamePlayer.position + gamePlayer.currentPlanetSurfaceNormal * 2.0f;
```

For first-person view:
```cpp
camera.Position = gamePlayer.position;
// Orient camera toward player orientation
```

## Next Steps

1. **Customize planets** - Edit InitializePlanets()
2. **Test gravity** - Jump between planets
3. **Tune physics** - Adjust moveSpeed, jumpPower, drag
4. **Integrate visuals** - Add planet rendering in shader
5. **Add gameplay** - Collectibles, objectives, obstacles

## Performance Tips

- Limit planets to ~8-16 for smooth 60+ FPS
- Use coarser cascade scales if framerate drops
- Disable MSAA (set aaSamples = 1) for performance
- Monitor GPU timing in title bar
