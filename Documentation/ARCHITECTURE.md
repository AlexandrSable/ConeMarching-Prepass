# Planetary System Architecture Diagram

## System Overview

```
┌─────────────────────────────────────────────────────────────────┐
│                        GAME LOOP (main())                       │
└─────────────────────────────────────────────────────────────────┘
                                │
                ┌───────────────┼───────────────┐
                │               │               │
        ┌───────▼─────┐  ┌──────▼──────┐  ┌───▼────────┐
        │ Input Frame │  │Calculate ΔT │  │ Compute FPS│
        └─────────────┘  └──────────────┘  └────────────┘
                                │
                        ┌───────▼────────┐
                        │ Update Physics │
                        └───────┬────────┘
                                │
        ┌───────────────────────┼───────────────────────┐
        │                       │                       │
   ┌────▼──────────┐  ┌─────────▼─────────┐  ┌─────────▼─────┐
   │ Apply Gravity │  │Apply Movement Inp │  │ Collision Chk │
   └────────────────  └──────┬──────────────  └────────────────┘
        │                    │
        │  F = G*m1*m2/r²    │  WASD + Space
        │  v += a * dt       │  Local coords
        │  p += v * dt       │
        │                    │
        └────────────────────┘
                │
        ┌───────▼────────────┐
        │ Update Planet SSBO  │
        │ (GPU Buffer)        │
        └────────────────────┘
                │
        ┌───────▼────────────┐
        │ Render (Shaders)   │
        │ - Cascade passes   │
        │ - Main march       │
        │ - SSBO lookup      │
        └────────────────────┘
```

## Data Structure Relationships

```
Player (CPU)                 Planet (CPU)              GPU SSBO
┌─────────────────┐         ┌──────────────┐          ┌─────────────────┐
│ position        │  ◄──    │ position     │  ◄───┐   │ PlanetData[0]   │
│ velocity        │  gravity│ radius       │      │   │ {pos+r,col+g}   │
│ orientation     │  points │ color        │  pack│   ├─────────────────┤
│ onGround        │  toward │ gravity      │      │   │ PlanetData[1]   │
│ mass            │         │              │  ┌───┴──►│ {pos+r,col+g}   │
└─────────────────┘         └──────────────┘  │       └─────────────────┘
        ▲                           ▲          │
        │                           │          │
        │  std::vector<Planet>      │          └──UpdatePlanetBuffer()
        └───────────────────────────┘              called each frame


Local Coordinate Frame (on planet surface)
                    ▲ local_up
                    │
                    ● position (player)
                   /│\
                  / │ \
                 /  │  \  ◄─ local_right
                    │
              local_forward ➙
              (into page)
```

## Physics Pipeline

```
Physics Update Sequence
═══════════════════════════════════════════════════════════════

PHASE 1: GRAVITY
    Find Closest Planet:
        min_dist = ∞
        for each planet:
            dist = distance(player_pos, planet_center)
            if dist < min_dist:
                closest_planet = planet
                min_dist = dist
    
    Apply Gravity:
        dir = normalize(closest_planet_pos - player_pos)
        magnitude = (gravity * 10.0) / (distance * distance)
        
        velocity += magnitude * dir * delta_time
        
        if length(velocity) > 100.0:  // terminal velocity
            velocity = normalize(velocity) * 100.0

    Apply Air Resistance:
        velocity *= 0.98  // airDrag


PHASE 2: MOVEMENT
    if player_on_ground:
        Get local coordinate frame relative to planet:
            local_up = planet_surface_normal
            local_forward = align(player_orientation, local_up)
            local_right = cross(forward, up)
        
        Get movement input:
            move_dir = (0, 0, 0)
            if WASD_pressed: move_dir += local_dirs
            move_dir = normalize(move_dir)
        
        Apply movement:
            velocity += move_dir * moveSpeed * delta_time
        
        Apply friction:
            flat_vel = velocity - up * dot(velocity, up)
            flat_vel *= groundDrag
            velocity = flat_vel + up * dot(velocity, up)
        
        Handle jump:
            if SPACE_pressed and !jumped_this_frame:
                velocity += local_up * jumpPower
                jumped_this_frame = true


PHASE 3: INTEGRATION
    position += velocity * delta_time


PHASE 4: COLLISION
    For each planet:
        dist_to_center = distance(position, planet_center)
        dist_to_surface = dist_to_center - planet_radius
        
        if 0.0 ≤ dist_to_surface ≤ 0.5:  // on ground
            snap_height = planet_radius + 0.25
            surface_normal = normalize(position - planet_center)
            position = planet_center + surface_normal * snap_height
            
            Cancel inward velocity:
                inward_vel = dot(velocity, -surface_normal)
                if inward_vel > 0:
                    velocity -= surface_normal * inward_vel
            
            onGround = true
            surface_normal_for_next_frame = surface_normal
            break  // land on first planet
    
    if no_collision_detected:
        onGround = false
```

## Memory Layout

### CPU Side

```
std::vector<Planet> planets  (dynamic array)
├─[0] Planet {
│     ├─ position: vec3
│     ├─ radius: float
│     ├─ color: vec3
│     └─ gravity: float
├─[1] Planet { ... }
├─[2] Planet { ... }
└─[3] Planet { ... }

Player gamePlayer
├─ position: vec3
├─ velocity: vec3
├─ orientation: vec3
├─ onGround: bool
├─ mass: float
├─ moveSpeed: float
├─ jumpPower: float
├─ groundDrag: float
├─ airDrag: float
└─ ... more fields

GLuint planetSSBO  (GPU buffer handle)
```

### GPU Side (VRAM)

```
Binding Point 4: Planet Storage Buffer
┌──────────────────────────────────┐
│ layout(std430)                   │
│ buffer PlanetBuffer {            │
│   PlanetData planets[];          │
│ };                               │
└──────────────────────────────────┘
   │
   ├─ planets[0]
   │  ├─ vec4 positionRadius  (x, y, z, radius)
   │  └─ vec4 colorGravity    (r, g, b, gravity_strength)
   │
   ├─ planets[1]
   │  ├─ vec4 positionRadius
   │  └─ vec4 colorGravity
   │
   └─ planets[N-1]
      ├─ vec4 positionRadius
      └─ vec4 colorGravity
```

## Shader Access Pattern

```
┌─ Compute Shader (all passes) ─────────────────────────┐
│                                                        │
│  uniform int u_numPlanets;                             │
│  layout(std430, binding = 4) readonly buffer           │
│  PlanetBuffer { PlanetData planets[]; };              │
│                                                        │
│  // In main shader code:                              │
│  for (int i = 0; i < u_numPlanets; i++) {             │
│      vec3 pos = planets[i].positionRadius.xyz;        │
│      float rad = planets[i].positionRadius.w;         │
│      vec3 col = planets[i].colorGravity.xyz;          │
│      float grav = planets[i].colorGravity.w;          │
│                                                        │
│      // Use planet data for:                          │
│      // - Distance field generation                   │
│      // - Lighting calculations                       │
│      // - Color/material assignment                   │
│      // - Gravity visualization                       │
│  }                                                    │
│                                                        │
└────────────────────────────────────────────────────────┘
```

## Call Graph

```
main()
├─ glfwInit()
├─ glfwCreateWindow()
├─ gladLoadGLLoader()
├─ ImGui_ImplGlfw_InitForOpenGL()
├─ ImGui_ImplOpenGL3_Init()
├─ InitializePlanets()
│  └─ planets.push_back(Planet(...))  [4 times]
├─ UpdatePlanetBuffer()
│  ├─ Create PlanetData array
│  ├─ glGenBuffers(1, &planetSSBO)
│  ├─ glBindBuffer(GL_SHADER_STORAGE_BUFFER, planetSSBO)
│  ├─ glBufferData(...)
│  └─ glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, planetSSBO)
│
└─ while(!glfwWindowShouldClose)  [Main Loop]
   ├─ Calculate deltaTime
   ├─ gamePlayer.UpdatePhysics(deltaTime, planets)
   │  ├─ Find closest planet
   │  ├─ ApplyGravity(deltaTime, closest)
   │  │  └─ velocity += gravity * dt
   │  ├─ velocity *= airDrag
   │  ├─ position += velocity * deltaTime
   │  ├─ CheckGroundCollision(planets)
   │  │  └─ Snap to surface or mark airborne
   │  └─ ApplyMovementInput(window, deltaTime)
   │     └─ WASD + Space handling
   ├─ UpdatePlanetBuffer()
   │  └─ glBufferData(GL_SHADER_STORAGE_BUFFER, ...)
   ├─ Dispatch compute shader cascades (existing code)
   │  └─ Shader accesses planets[] via SSBO
   ├─ Render to screen (existing code)
   ├─ ImGui rendering (existing code)
   └─ glfwSwapBuffers()
```

## State Machine: Player Ground State

```
                    ┌─────────────────────┐
                    │    IN_AIR           │
                    │ (onGround = false)  │
                    └──────────┬──────────┘
                               ▲
                      no collision         jump
                      check fails          triggered
                               │           or
                               │    leave surface
                               │
                    ┌──────────┴──────────┐
                    │  ON_GROUND          │
                    │ (onGround = true)   │
                    └──────────┬──────────┘
                               │
                   collision with planet
                    surface detected
                               │
                               ▼
                    ┌─────────────────────┐
                    │  SNAP_AND_ORIENT    │
                    │  (one frame)        │
                    └─────────────────────┘

Distance checking (IsNearPlanetSurface):
    surface_dist = distance_to_center - radius
    
    if 0.0 ≤ surface_dist ≤ 0.5:
        COLLIDING
    else:
        NOT_COLLIDING
```

## Integration with Existing Ray Marching

```
Existing System          New System
═══════════════════════════════════════════════════

Compute Shader
│
├─ Cascade1: Coarse march
│  └─ Reads cascade1Depth texture
│
├─ Cascade2: Medium march
│  └─ Reads cascade2Depth texture
│
├─ Cascade3: Fine march
│  └─ Reads cascade3Depth texture
│
├─ Main: High-quality march
│  └─ Reads cascade3Depth
│
└─ ✨ NEW: Access to planets[] SSBO
   ├─ Can use planet positions for distance fields
   ├─ Can apply planet colors to materials
   ├─ Can visualize planet positions
   └─ Can write planet-specific SDFs

Uniform Updates
├─ u_camPos, u_camRot (existing)
├─ u_time (existing)
├─ u_maxStepsCone, u_maxStepsMain (existing)
└─ u_numPlanets ✨ NEW
   └─ Tells shader how many planets to process
```

## Performance Analysis

```
Frame Budget (16.67ms @ 60 FPS)

Existing Rendering:      ~2-3ms
├─ Cascade1:   ~0.1-0.2ms
├─ Cascade2:   ~0.2-0.3ms
├─ Cascade3:   ~0.3-0.4ms
├─ Main:       ~1-1.5ms
└─ Screen:     ~0.3-0.5ms

New Physics:             ~0.1-0.2ms
├─ Find closest:  ~0.01ms
├─ Gravity:       ~0.03ms
├─ Integration:   ~0.01ms
├─ Collision:     ~0.04ms
└─ Movement:      ~0.01ms

GPU Buffer Update:       ~0.1ms
├─ Pack data:     ~0.02ms
├─ glBufferData:  ~0.05ms
└─ glMemBarrier:  ~0.03ms

Total:                   ~2.3-3.5ms (leaves ~13ms margin)
```

## Testing Checklist Flowchart

```
┌─ Compile ─────────────────┐
│ All files?                │
│ No linker errors?         │
│ Correct warnings only?    │
└─────┬─────────────────────┘
      │ YES
      ▼
┌─ Launch Application ──────┐
│ Window opens?             │
│ No GL errors?             │
│ ImGui renders?            │
└─────┬─────────────────────┘
      │ YES
      ▼
┌─ Player Initialization ───┐
│ 4 planets visible?        │
│ Player starts on main?    │
│ Gravity pulls toward?     │
└─────┬─────────────────────┘
      │ YES
      ▼
┌─ Movement Testing ────────┐
│ WASD moves player?        │
│ Movement is local frame?  │
│ No floating through?      │
└─────┬─────────────────────┘
      │ YES
      ▼
┌─ Gravity & Jumping ───────┐
│ Jump works?               │
│ Falls toward planet?      │
│ Land snaps correctly?     │
└─────┬─────────────────────┘
      │ YES
      ▼
┌─ Multi-Planet Testing ────┐
│ Can reach other planets?  │
│ Gravity changes direction?│
│ Orientation correct?      │
└─────┬─────────────────────┘
      │ YES
      ▼
┌─ Performance Check ───────┐
│ 60+ FPS maintained?       │
│ GPU timing < 5ms?         │
│ No stutter/hitches?       │
└─────┬─────────────────────┘
      │ YES
      ▼
✅ SYSTEM READY
```
