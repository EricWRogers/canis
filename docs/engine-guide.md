# Canis Engine Guide

This document is a practical overview of how the engine in this repository is structured today. It is meant to help with day-to-day development, onboarding, and future refactors.

## What This Repository Builds

The project is split into three main outputs:

- `CanisEngine`: the engine shared library in `canis/`
- `GameCode`: the gameplay shared library in `game/`
- `c-engine`: the executable that boots the engine and loads `GameCode`

At runtime the executable starts the engine, loads assets and project settings, then loads the game shared library and calls its exported init/update/shutdown functions.

## Repository Layout

```text
.
├── canis/              # Engine code
│   ├── include/Canis/  # Public engine headers
│   └── src/            # Engine implementation
├── game/               # Game-specific scripts and gameplay code
│   ├── include/
│   └── src/
├── project/            # Runtime output folder, assets, and project settings
│   ├── assets/
│   └── project_settings/
├── external/           # Third-party dependencies
└── docs/               # Project documentation
```

## Build And Run

From the repository root:

```bash
cmake -S . -B build
cmake --build build -j4
./project/c-engine
```

Notes:

- The executable is written to `project/c-engine`
- Shared libraries are written next to it in `project/`
- The engine expects to find `assets/` at runtime and already contains logic to normalize the working directory so running from the repo root works

## Runtime Flow

The startup path currently looks like this:

1. `Canis::App::Run()` starts the engine
2. The working directory is normalized so `assets/` can be found
3. Asset meta files are discovered under `project/assets/`
4. Core engine systems and built-in components are registered
5. `GameCode` is loaded as a shared library
6. The main scene is loaded from `assets/scenes/roll_a_ball.scene`
7. The frame loop updates input, scene systems, scripts, and rendering

Important engine entry points:

- `canis/include/Canis/App.hpp`
- `canis/src/App.cpp`
- `canis/include/Canis/Scene.hpp`
- `canis/src/Scene.cpp`

## Core Concepts

### App

`Canis::App` owns the main `Scene`, the script registry, timing information, and editor integration.

Responsibilities:

- bootstraps the engine
- registers built-in components and inspector behavior
- manages the script/component registry
- loads the gameplay shared library
- runs the main loop

### Scene

`Canis::Scene` is the runtime world container.

Responsibilities:

- owns the `entt::registry`
- owns the engine `Entity` objects
- creates and updates systems
- loads and saves YAML scene files
- routes physics raycasts and `RaycastAll` queries
- handles deferred entity ready/destroy behavior

The current built-in systems are created in `Scene::LoadSceneNode()`:

- `MeshRenderer3DSystem`
- `SpriteRenderer2DSystem`
- `ModelAnimation3DSystem`
- `SpriteAnimationSystem`
- `JoltPhysics3DSystem`

### Entity

`Canis::Entity` is a lightweight wrapper around an `entt::entity` handle plus engine-facing metadata:

- runtime handle via `GetHandle()`
- `id`
- `name`
- `tag`
- `uuid`
- `active`
- `scene`

It exposes the main ECS helper API:

- `AddComponent<T>()`
- `AddOrReplaceComponent<T>()`
- `HasComponent<T>()`
- `HasComponents<T...>()`
- `GetComponent<T>()`
- `RemoveComponent<T>()`

Current behavior to be aware of:

- `scene` is a reference, not a nullable pointer
- `GetHandle()` returns the underlying `entt::entity`
- non-const `GetComponent<T>()` returns a reference
- non-const `GetComponent<T>()` auto-adds the component if it is missing
- const `GetComponent<T>()` throws if the component is missing

Identity in the current engine is split three ways:

- `entt::entity`: runtime ECS handle used to address data in the `entt` registry
- `Entity::id`: current scene slot index used by some legacy scene management code
- `UUID`: persistent identity used for serialization and saved references

As a rule of thumb:

- use `GetHandle()` when you need the ECS handle
- use `UUID` for saved or cross-scene references
- avoid introducing new gameplay logic that depends on `Entity::id` unless you are working directly with the existing scene container behavior

### Components

Components are plain structs stored in the `entt` registry. Most engine components also expose:

- `static constexpr const char *ScriptName`
- `Create()`
- `Entity *entity`

Examples of built-in components:

- 2D: `RectTransform`, `Sprite2D`, `Text`, `Camera2D`, `SpriteAnimation`
- 3D: `Transform`, `Camera`, `Model`, `ModelAnimation`
- Physics: `Rigidbody`, `BoxCollider`, `SphereCollider`, `CapsuleCollider`
- Lighting: `DirectionalLight`, `PointLight`
- Rendering/materials: `Material`

### Scripts

Gameplay code derives from `Canis::ScriptableEntity`.

Lifecycle hooks:

- `Create()`
- `Ready()`
- `Update(float dt)`
- `Destroy()`

Scripts live on `Entity` objects rather than as `entt` components. They are attached, stored, and updated through the engine's script registry.

## Registering Scripts And Inspector Properties

The engine uses `ScriptConf` plus a small macro layer to register scripts and component inspectors.

Useful helpers in `canis/include/Canis/ConfigHelper.hpp`:

- `REGISTER_PROPERTY`
- `DEFAULT_CONFIG_AND_REQUIRED`
- `DEFAULT_DRAW_INSPECTOR`
- `DEFAULT_UNREGISTER_SCRIPT`

A typical gameplay script registration looks like this:

```cpp
namespace MyGame
{
    ScriptConf conf = {};

    void RegisterMyScript(Canis::App& app)
    {
        REGISTER_PROPERTY(conf, MyGame::MyScript, speed);
        REGISTER_PROPERTY(conf, MyGame::MyScript, jumpForce);

        DEFAULT_CONFIG_AND_REQUIRED(conf, MyGame::MyScript, Canis::Transform, Canis::Rigidbody);

        conf.DEFAULT_DRAW_INSPECTOR(MyGame::MyScript,
            ImGui::Text("Example debug info");
        );

        app.RegisterScript(conf);
    }
}
```

What this gives you:

- scene serialization for the registered properties
- editor inspector drawing for those properties
- automatic required-script/component setup
- runtime construction through the shared registry

## Scene Serialization

Scenes are YAML files stored under `project/assets/scenes/`.

The engine saves and loads:

- environment settings
- entities
- components
- registered script properties

Built-in component save/load logic is registered in `App::RegisterDefaults()`. Custom gameplay scripts rely on the property registry and script config system.

Asset references increasingly prefer UUID-based serialization, with some legacy path fallback still supported in the loader for older scene data.

## Assets And Meta Files

Assets are discovered by scanning `assets/` for metadata. The `AssetManager` keeps mappings between:

- runtime asset IDs
- source paths
- UUIDs

Common asset types include:

- textures
- text/font assets
- sprite animations
- models
- materials
- skyboxes

The important practical detail is that scene and component data should prefer UUIDs when possible so assets can move without breaking references.

## Physics

Physics is backed by Jolt through `JoltPhysics3DSystem`.

### Rigidbody And Colliders

Physics-related components:

- `Rigidbody`
- `BoxCollider`
- `SphereCollider`
- `CapsuleCollider`

`Rigidbody` exposes a Godot-style filtering model:

- `layer`
- `mask`

Both are `Canis::Mask`, which is a lightweight wrapper over `u32`.

In the editor, `Mask` is drawn as a 32-bit toggle grid so collision filtering can be edited visually instead of as raw integers.

### Combined Static Meshes And Vehicles

Add `Canis::CombineMeshCollider` to a parent with `Transform`. It gathers rigid
descendant `Model` geometry into one static Jolt mesh, without requiring a
`Rigidbody` on the parent or child tile colliders. Nested combined colliders,
animated/deforming models and non-static rigidbody subtrees are excluded. Static
child rigidbodies are suppressed while their ancestor combined collider is active.
Native raycasts and contact events identify the combined parent.

The cooked shape is cached. Child transforms, active state, hierarchy, model/node
selection, geometry revisions and parent scale invalidate it; moving the parent
updates the body's pose. `weldDistance` quantizes vertices to align touching tile
edges (default 1 mm), and mirrored transforms preserve triangle winding. The
Inspector exposes friction, welding, an explicit rebuild button and build/model/
triangle counters. Empty collections keep no body and do not rebuild each frame.
Edit mode cooks this same cached static geometry after authoring pose evaluation,
so the counters, native ground queries and rebuild button work before Play.
This preview never steps simulation or initializes dynamic bodies or vehicle
constraints. Removing or disabling the combined collider removes its preview body.
Group connected static ground in bounded regions for large worlds; one enormous
body reduces broadphase granularity even though Jolt still uses the mesh's triangle tree.

`Canis::WheeledVehicle` attaches a native four-wheel constraint to a dynamic
`Rigidbody` with a chassis collider. Local **−Z** is forward. Assign wheel entities
in front-left, front-right, rear-left, rear-right order; they should be children of
the chassis with unit scale. Wheel radius/width, track width, wheelbase, suspension,
steering, rear steering ratio and engine torque are editable. The constraint drives
both axles and casts cylinders for wheel contact. Runtime `throttle`, `steering`,
`brake` and `handbrake` inputs control it; `groundedWheels` and `speed` report results.
The system updates wheel transforms and removes constraints/listeners before a
body is rebuilt, the component is removed or the scene is unloaded.

The vehicle Inspector groups power/brakes, steering/grip, suspension and wheel
geometry, with units in hover help. `engineMaxRPM`, `brakeTorque`,
`handbrakeTorque`, `longitudinalGrip`, `lateralGrip` and `antiRollStiffness` are
serialized alongside the original dimensions and spring settings. Handling edits
update owned Jolt wheel settings between simulation steps, preserving engine RPM
and wheel angular velocity; wheel dimensions/placement/travel rebuild the constraint.
`throttleResponse` and `steeringResponse` are exponential response rates per second
(0 = instantaneous). `highSpeedSteeringRatio` fades steering authority from full
at rest to that fraction at `steeringFadeSpeedKph`. Braking cancels smoothed throttle
immediately. Runtime applied-input telemetry and constraint build count help tune
and verify these changes.

`WheeledVehicle::centerOfMass` is an optional descendant `Entity` with a
`Transform`. Its origin specifies the absolute mass centre in chassis space,
including nested transforms and the body's scale. Jolt's `OffsetCenterOfMassShape`
shifts mass/inertia while leaving collider geometry and the model origin fixed.
An empty, deleted, non-Transform or non-descendant reference uses the automatic
shape centre. Reference/marker edits invalidate the body settings and rebuild the
body/vehicle constraint; ordinary vehicle translation and rotation reuse the body.
`effectiveCenterOfMass` reports Jolt's actual local centre during simulation.

`CanisRenderOptimizationTests` verifies seam queries, cache reuse, inactive/empty
ground, hot-replaced geometry, transformed/mirrored parents, suspension, forward
driving, steering and cleanup.

### Animated Ragdolls

`Canis::Ragdoll` requires Transform, Model and ModelAnimation. Its `definitionPath`
points to JSON/YAML with a `parts` sequence of bone names, preceding parent indices,
lengths, radii and masses. Set `active` and initial linear/angular velocity to
instantiate Jolt capsule bodies with swing/twist joints at the current animated
pose. The component suppresses the ordinary owner body while active. Parent/child
and initially overlapping limbs do not collide; ECS layer/mask filtering remains
effective independently of Jolt's limb collision groups. Queries retain the owner
entity, and body counters include all ragdoll parts.

After stepping, native joint poses drive ModelAnimation's external local matrices,
so the original skinned model follows the physical limbs. Disabling/removing the
component, deleting its owner or unloading the scene removes bodies/constraints
and pose overrides. Set `freezePoseOnDisable` before setting `active = false` to
retain the final bone/skin matrices and stop clip playback while removing all
ragdoll collisions and constraints. The Inspector reports body count and
simulation status.
`NativeRagdollLifecycle` checks inherited velocity, ground contact, query ownership
and frozen-pose cleanup/reactivation without depending on private test assets. Imported glTF image URIs
are decoded before resolving external base-colour/normal/emissive files.

### Trigger-Style Behavior

Jolt sensors are exposed through `Rigidbody::isSensor`.

Runtime collider overlap state is currently available through public vectors on collider components:

- `entered`
- `stayed`
- `exited`

That gives scripts a simple polling-based way to react to overlap events.

### Raycasts

The scene currently exposes two physics query entry points:

- `Scene::Raycast(...)`
- `Scene::RaycastAll(...)`

Current hit data includes:

- hit entity
- hit point
- hit normal
- hit distance
- hit fraction

Both queries accept a mask so gameplay code can limit what a query sees.

Behavior:

- `Raycast(...)` returns the nearest valid hit
- `RaycastAll(...)` returns a `std::vector<RaycastHit>`
- `RaycastAll(...)` results are sorted nearest to farthest

Typical usage:

```cpp
Canis::RaycastHit hit;
if (entity.scene.Raycast(origin, direction, hit, 10.0f, groundMask))
{
    // use hit.entity, hit.point, hit.normal, etc.
}

std::vector<Canis::RaycastHit> hits =
    entity.scene.RaycastAll(origin, direction, 10.0f, groundMask);
```

## GameCode Shared Library

Gameplay code is compiled into `GameCode`, a shared library loaded at runtime by the engine.

Current exported function pattern:

- `GameInit`
- `GameUpdate`
- `GameShutdown`

The helper wrapper lives in `canis/include/Canis/GameCodeObject.hpp`.

There is a reload helper path in the codebase, but the automatic file-watching call is currently commented out in `App::Run()`. It is best to think of the shared-library split as the current architecture, not as fully polished hot reload.

## Editor Notes

The engine is compiled with editor support enabled in this repository.

The editor integration currently provides:

- inspector drawing for registered scripts and built-in components
- runtime/editor window mode support through project config
- editor camera overrides on the scene
- multiple scene tabs with per-tab working snapshots and undo/redo history
- a scene-local viewport toolbar below the scene tabs
- system timing collection for update and render systems

Scene tabs reuse the application's single live `Scene`. Before switching, the
editor captures the active scene YAML, terrain assets, hierarchy and selection
state, and history stacks in a `SceneTabState`. It restores the destination
snapshot into the same scene object and loads that asset's saved editor-camera
configuration. This avoids updating or rendering several worlds at once while
still preserving unsaved work in every open tab. A saved snapshot provides the
dirty-state baseline, and dirty tabs cannot be closed until saved.

Blockout Edit Mode is implemented in `src/EditorMeshEdit.cpp`. Tab on one
unlocked blockout converts its primitive into `EditableBlockoutMesh`, stored
inside `BlockoutShape` with indexed vertices and ordered face loops. Face slots
and vertex indices remain stable; removed face slots are empty. Edge adjacency
is derived from vertex pairs. `src/BlockoutEditing.cpp` implements quad-ring
splitting, region extrusion, coplanar dissolve, conversion, and topology
validation. Rendering triangulates the polygons; edited shapes use mesh
collision. Scene serialization, history snapshots, and OBJ baking all use the
same committed mesh. During a preview only render geometry changes; confirmation
updates collision and navigation and records a single history entry.

Editor framebuffer captures now include the composed UI when `--editor` is
combined with `--capture-dir` and `--stop-at`, allowing viewport UI checks as
well as runtime scene captures.

Inspector rendering is a mix of:

- script/component `DrawInspector` callbacks stored in `ScriptConf`
- generic registered-property drawing through `DrawInspectorField()`

## Conventions In This Codebase

These are the patterns the current engine code is leaning toward:

- prefer references over raw pointers when an object must exist
- use `HasComponent<T>()` or `HasComponents<T...>()` before fetching optional components
- use `GetComponent<T>()` as a reference-returning access point
- keep serialization and inspector behavior together in registration code
- prefer UUID-based asset references over path-based references
- keep gameplay-specific behavior inside `game/`, engine behavior inside `canis/`

## Suggested Starting Points

If you are trying to understand or extend the engine, these are good places to start:

- `canis/src/App.cpp`: engine boot, built-in component registration, serialization
- `canis/src/Scene.cpp`: scene lifecycle, system creation, entity load/save
- `canis/include/Canis/Entity.hpp`: entity, script, and component access model
- `canis/src/ECS/Systems/JoltPhysics3DSystem.cpp`: physics integration
- `game/src/RollABall/PlayerController.cpp`: example gameplay script
- `game/src/RollABall/PickupSpinner.cpp`: example sensor/trigger gameplay

## Future Documentation Ideas

This guide is intentionally broad. Good next docs to add later would be:

- a component reference
- a scene file format reference
- a physics guide with common gameplay patterns
- an asset pipeline guide
- an editor guide
- a scripting cookbook with examples

## Experimental Single-Pass Opaque VR

Set `CANIS_VR_SINGLE_PASS=1` before launching with `--vr` or `--vr-sim` to
experiment with submitting opaque world meshes once for both eyes. Leave it unset
(or set it to `0`) for the established two-pass renderer. This does not change the
requested OpenGL version: the desktop VR context requests 4.5, and the experiment
requires 4.1 viewport arrays and geometry-shader invocations. It does not require
`OVR_multiview`, which is absent on the tested RX 7700S Mesa desktop driver even
though that driver supports OpenGL 4.6.

The pass renders opaque geometry to two indexed viewports in a shared HDR atlas.
Cached shader variants run the existing vertex shader once per submitted vertex
and emit triangles for each eye from its world position. Existing object
instancing and model transforms are retained. Per-eye camera position is supplied
to fragment shaders for specular lighting. The shared directional shadow map is
still rendered once. Color and depth are then copied to each eye's intermediate
buffer before its skybox, transparent objects, trails, debug drawing, world UI and
post-processing. Bloom never crosses the stereo seam. OpenXR swapchain acquisition,
release, projection submission and comfort fade use the established per-eye path.

This is a geometry-shader-based opaque optimization, not hardware multiview or a
single pass for the entire frame. Shader variants support the engine's ordinary
`P`/`V` world-mesh contract with `vec3 worldPosition` or `fragmentWorldPos` outputs.
View-dependent vertex deformation, unknown interfaces and failed variants cause
fallback to the two-pass renderer for the remainder of the application session.
Shader recompilation invalidates cached variants. Unsupported GL versions,
unequal eye dimensions, active hardware foveation, or an oversized/incomplete
stereo framebuffer also retain/fall back to two-pass rendering. The pass submits
a conservative opaque superset instead of applying left-eye occlusion results to
both eyes. A scene with heavy hidden geometry or costly geometry shaders may lose
performance; measure representative scenes and actual headset resolution.

Validation in MatrixMechanic includes asymmetric projection / per-eye shading
pixel checks, existing instanced models with mirrored scales, renderer viewport
checks, and a 13-view stereo scene comparison. Run
`python3 scripts/halo/compare-stereo.py` from the game repository to reproduce the
on/off capture and timing comparison. GPU timer-query results and CPU submission
times exclude headset compositor cost; simulated VR does not establish headset
frame pacing or comfort. The feature remains opt-in pending physical headset
validation.


## Particle Emitters

Particle emitters expose **Spawn shape** with Box, Point and Plane (local XZ).
Box is the default to preserve existing effects. `spawnExtents` contains local half
extents; Plane uses its X/Z values and ignores Y. Point uses only the emitter origin.
Emitter rotation and parent scale affect the spawn region in both local and world
space. Native render tests verify all shapes under a rotated, scaled parent.

`useAgeUniforms` defaults to true for existing particle shaders. Disable it when a
shader does not consume particle age/lifetime uniforms; this leaves material field
overrides empty so opaque ShaderGraph particles can use dynamic instance batches.
Particle colour and scale still animate, and expiry still removes registry handles.

The first improvement pass follows five steps: profile ordinary weather, bound
particle growth/catch-up, make timing and randomness reproducible, add collision
effects, and expose controls with regressions. All five are implemented in the
native system. Simulation remains CPU-based and feeds the existing mesh renderer.

`maxParticles` defaults to 2048 and `maxSpawnPerFrame` to 512. Live limits include
impact particles, and reducing the live cap trims existing particles immediately.
Missed emissions above the frame budget are dropped rather than queued into future
frames. Continuous emitters age catch-up particles from their actual birth times
and stop spawning at their finite emission duration. Gravity and drag use analytic
integration; equivalent seeded runs at 30 and 120 updates/second match within the
native regression tolerance. Scratch buffers retain their capacity between frames.

`randomSeed=0` chooses a fresh seed when played. Positive seeds reproduce each
emitter independently of other effects. Inactive parents stop emission; deleting
an emitter or removing its component cleans its particles on the next update.
Changing a model/material path invalidates the emitter's asset cache.

**Collision** offers None, a cheap horizontal World plane at
`collisionPlaneHeight`, or swept native colliders through the scene physics API.
Connect `impactEmitter` to an authored emitter configured as a burst with
`playOnStart=false` and `destroyEntityWhenDone=false`. Impacts spawn at the hit
point, orient to its normal, and remain in world space. Multiple hits share the
target's live/frame budgets. A collision-disabled target avoids effect feedback.
The registry is mutated only after the particle update iteration completes.

The inspector has Restart, Stop and Clear controls and live/spawned/dropped/hit
counts. Stop drains an effect without destroying the authored emitter. In Edit
mode, stopped/cleared previews stay stopped until restarted or selected again.
Tests cover high-rate bursts, long frames, finite durations, invalid deltas, seeded
repeatability, parent activation, owner removal, and fast impacts on planes and
native walls. Mesh/sprite transparency batching, billboard rendering, richer
curves/shapes and GPU simulation remain separate scaling work for larger effects.

## Soft Intersections for Transparent Meshes

A transparent material whose main shader declares an active `softSceneDepth`
sampler opts into an opaque-depth snapshot before transparent geometry. The
renderer supplies `softDepthAvailable`, `softDepthViewport` (x, y, width, height),
`softInverseProjection`, and `softTime` (simulation seconds, advanced once per
stereo frame). Texture unit 15 is reserved while drawing these materials; custom
material textures must not occupy that unit. Its prior texture and sampler are
restored afterward. The snapshot is a separate depth texture, never the depth
attachment currently being rendered into. Materials without this sampler incur
no depth copy. The current implementation detects the main material shader,
not independent material-slot shaders.

Use `(gl_FragCoord.xy - softDepthViewport.xy) / softDepthViewport.zw` to sample
the snapshot, reconstruct its view position with `softInverseProjection`, and
compare linear eye depths for an intersection fade. The viewport origin matters
for direct stereo mirrors. Single-pass opaque rendering is compatible: its
per-eye depth transfer happens before this transparent pass. A depth-bearing,
single-sampled draw target is required; for multisampled targets the availability
uniform is false and the shader must omit the depth fade. The built-in Cairo
light shafts demonstrate source UV fade, camera fade, inverted Fresnel, and
optional animated cloud modulation on top of this depth support.

## Static lighting and baked visibility (first performance pass)

This pass preserves imported vertex/lightmap irradiance, caches static directional
shadow depth, and adds offline visibility data. It does **not** generate new GI,
unwrap lightmap UVs, or replace dynamic point-light shading with a general lightmapper.
Research: [lightmap baking](https://docs.unity.com/en-us/engine/6000.6/manual/lighting-overview/direct-and-indirect-lighting/lightmapping/baking-before-runtime/lightmappers),
[baked visibility cells](https://docs.unity.com/en-us/engine/6000.5/manual/cameras/occlusion-culling/occlusion-culling),
and [visibility for lighting](https://developer.nvidia.com/gpugems/gpugems/part-ii-lighting-and-shadows/chapter-15-managing-visibility-pixel-lighting).

Implementation sequence: establish a source-mesh baseline; reuse static shadow
work; partition immutable meshes and bake conservative visibility; compare both
VR eyes and independent feature switches; validate the mission and loading cost.
A general authoring lightmapper remains a separate project.

### Static shadow depth

`Model.static` casters without deformation or animation populate a cached depth
map. Moving casters render into a fresh copy of that map each frame, so moving
objects continue casting and receiving shadows. The cache key includes static
membership, hierarchy transforms, model geometry revision, node selection, light
matrix and shadow shader program. Moving/removing a static object or changing the
light direction rebuilds the map. The camera/light-space volume advances in
approximately 8m blocks aligned to complete shadow texels, allowing ordinary head
motion within a block to reuse the bake. The first frame and a volume transition
still require a full static shadow render; this is a runtime cache, not a saved
irradiance atlas. The cache adds one 2048² D24 texture and framebuffer to the existing combined
shadow target.

Set `CANIS_DISABLE_STATIC_SHADOW_CACHE=1` to compare full shadow rendering with
identical light-space projection. Dynamic models must not be opted into visibility
bakes; movement of an ordinary `static` model still invalidates its shadow cache.

Directional shadow depth uses texture unit 6, separate from imported normal maps
on unit 5, embedded emission on unit 7 and reflection probes on units 8–11. Shader
linking caches active uniform types; the renderer uses nearest raw-depth sampling
for `sampler2D` and linear comparison sampling for `sampler2DShadow`. The shadow
unit's prior sampler binding is restored after rendering. These modes must agree
with the GLSL sampler type ([OpenGL sampling rules](https://wikis.khronos.org/opengl/Sampler_Object)).
The GPU regression imports normal-mapped geometry and alternates both shader
types, checking that normal maps cannot replace shadow depth.
Material samplers cover units 0–5, including normal maps. Linear magnification,
trilinear mipmaps and anisotropic filtering keep close wet highlights smooth while
leaving shared texture defaults and UI rendering unchanged. Probe fingerprints
include a capture revision so older nearest-normal captures are rebuilt once.

Instanced draws now restore attribute 7 to the imported vertex-color stream.
Without this restoration, an instanced shadow pass replaced baked irradiance with
instance colors in the subsequent ordinary draw. Adjacent primitives also reuse
unchanged material bindings and model uniforms within one draw invocation.

### Offline visibility

`canis-asset-tool bake-visibility INPUT.gltf OUTPUT.gltf [chunkSize cellSize minX minY minZ dimX dimY dimZ]`
partitions rigid meshes and writes `OUTPUT.gltf.visibility` entirely in C++.
Animated/skinned inputs are rejected. Transparent primitives are retained and never
used as occluders. Source assets remain unchanged; external image references are rebased.

The bake considers only opaque triangles in the same immutable asset. A target
chunk is hidden only if its complete bounding box lies behind a triangle from
**every** corner of the camera cell. It intersects the corresponding convex shadow
volumes; it does not guess visibility from a handful of sampled rays. Door holes
and uncertain cases stay visible. Moving doors and other changing occluders must
remain separate from the immutable input. Transparent/cutout/displaced geometry
must not be supplied as occluders. Largest-face selection intentionally sacrifices
some culling opportunities to bound bake time. The native CLI accepts chunk size, cell size, minimum and dimensions as positional arguments; the C++ API also exposes the occluder budget.

Opt in on the rendered component:

```yaml
Canis::Model:
  ModelAsset: {path: assets/Models/compiled-station.gltf}
  static: true
  bakedVisibility: true
```

`bakedVisibility` is saved and exposed in the Model inspector. It is an explicit
contract that the render shader leaves the mesh rigid and opaque. Animated and
transparent entities bypass the bake. For custom shaders, clear the flag before
introducing vertex displacement or alpha clipping. Baked mesh bounds provide
primitive-level frustum rejection even outside the baked camera volume; PVS
occlusion outside that volume falls back to visible. A CRC of the complete glTF
and referenced buffer bytes invalidates stale tables when the output geometry or
node hierarchy changes. Native builds use zlib CRC-32 when available (SDL fallback
otherwise), avoiding several seconds of scalar checksum work on large levels.
Malformed/stale tables render all geometry; replacing
runtime geometry clears the bake. Changing an input source requires rebaking its
derived output; the Cairo scene builder checks the recorded source CRC for this.

Single-pass stereo queries both eye frusta and cells and retains the union of
visible chunks. Shadow passes never use camera occlusion data. Runtime transforms
are converted into model-local coordinates, allowing a whole rigid assembly to
move without making its own visibility proof stale.

`CANIS_DISABLE_BAKED_VISIBILITY=1` disables primitive bounds and PVS together.
`CANIS_DISABLE_BAKED_OCCLUSION=1` disables only PVS while retaining frustum rejection.
The frame metrics include rejected primitives in `culled` and avoided static
shadow work is visible in `shadow_draws`.

Validation: `CanisRenderOptimizationTests` covers both-eye visibility, outside-cell
fallback, invalid matrices, stale CRCs, static shadow reuse and invalidation,
dynamic shadow composition, geometry edits, and preservation of vertex colors
across instanced passes. `CanisNativeBakeTests` checks the offline proof, partial coverage, camera cells crossing occluders, and native lighting/import behavior.

### Environment panel controls

The Environment panel now groups Lightmap Baking and Baked Occlusion controls.
Its settings serialize under `Environment.Baking` and participate in normal scene
save/history. Lightmaps defaults on and sets the `disableBakedLighting` uniform;
imported-lighting shaders must honor that uniform (the Cairo shader does). New
Native C++ baking generates shadowed direct irradiance, ambient occlusion and one diffuse
bounce in subdivided vertex colors. This is vertex lighting, not a UV lightmap atlas.
Static Shadows enables cached directional depth; Rebuild Static Shadows invalidates it.
Occlusion switches PVS independently of primitive frustum culling.

The native bake runs asynchronously on a detached scene snapshot. Runs start in Edit
mode; completed jobs wait for Edit mode before applying. There is no external baker
Program/Arguments setting or Python process. Save the scene after applying the bake.
`canis-asset-tool bake-lighting INPUT.scene OUTPUT_MODEL_DIRECTORY [vertexSpacing indirectSamples]`
also creates an adjacent `INPUT-baked.scene` using the same implementation.
Imported material slots remain editable and collision keeps the original unsubdivided mesh.

### Local reflection probes and indirect illumination

In **Environment → Reflection probes / Global illumination**, add up to six probes.
Place each capture point in open space inside its room, set its world-aligned box
half extents to that room, and use the blend distance to fade outside the projection box
boundary. Surface reflections remain visible on the box boundaries, and reflected
rays leaving a neighboring room are rejected. Overlapping probes blend per fragment; outside all boxes they contribute
nothing. Settings are saved under `Environment.LightingProbes` and are also available
through `Scene::GetLightingProbes()`.

The renderer captures six 64×64 HDR faces from static models and the sky on first
render, then reuses those textures for both VR eyes. Changing probe settings causes
a recapture; after editing geometry, materials or lights, click **Recapture lighting
probes** (or call `InvalidateLightingProbes`). Captures can be persisted through each probe's `CachePath`; valid caches reload without recapturing. Fresh captures require six
scene views per probe (36 at the six-probe limit). These are cubemaps, not lightmap
atlases.

Diffuse indirect lighting is a solid-angle-weighted projection of captured radiance
onto nine spherical-harmonic coefficients, convolved with the cosine kernel. This
provides approximate one-bounce color bleeding from directly lit or emissive static
surfaces. Existing lightmapped geometry can contribute its baked radiance. It does
not solve additional dynamic bounces or bake new lightmaps. Local box projection
corrects reflection parallax; ordinary mipmaps provide approximate roughness blur,
not a GGX prefilter or a split-sum BRDF integration. Boxes have no visibility field,
so keep them inside rooms to limit light leakage through walls.

Shader integration is explicit: add `[CANIS PROBE LIGHTING]` after the version
marker, then add `CanisProbeLighting(worldPosition, normal, viewDirection,
linearAlbedo, roughness, fresnelF0, diffuseWeight)` before display conversion. During
`canisProbeCapture`, output linear radiance without tone mapping or gamma encoding.
Set diffuseWeight to zero on surfaces already receiving baked diffuse GI to avoid
counting it twice. The default model, Cairo gameplay, worn hangar, and Anniversary
shaders implement this contract. Other custom shaders and shader graphs must opt in;
their existing output is otherwise captured as-is. Texture units 8–11 are reserved
for probe maps in participating shaders. Material previews start with probes off.

Research: [Ramamoorthi and Hanrahan's irradiance representation](https://lightfield.stanford.edu/papers/envmap/)
establishes the compact nine-coefficient diffuse approximation;
[Unity's box projection documentation](https://docs.unity.com/en-us/engine/6000.5/manual/lighting-overview/reflections/advanced-ref-probe)
describes the local reflection correction used here. The implementation deliberately
uses bounded static captures to suit the existing forward OpenGL and VR renderer.

### Room partitions for static visibility

The native converter uses a conservative spatial grid. Room grouping is not inferred
from mesh names; uncertain visibility stays visible.

### Desktop focus, controller availability and frame pacing

Managed gameplay can read `Input.Focused` and `Input.GamepadConnected` alongside
immutable action snapshots. Focus reflects the game window/editor routing state;
controller availability includes the runtime's synthetic gamepad feed. Respond to
`ActionSnapshot.Canceled` with `FocusLost`, `Disconnected` or `BackendChanged` by
suspending held actions. Availability alone is insufficient when another controller
remains connected. A fresh release/press gate prevents firing or jumping through
pause, focus recovery or an input backend change.

`Application.FrameLimit` reads/writes the runtime rendering target. Zero removes
pacing; it does not alter the simulation delta. Values must be finite and in
0–100000. Scene-specific presentation should save the prior limit and restore it
on teardown; XR pacing remains controlled by the XR session.

Runtime input timelines now accept top-level `focus: false/true` and
`gamepad: {connected: false/true}` for reproducible input-loss/recovery checks.
These exercise game input cancellation and synthetic availability, rather than
changing desktop OS focus or unplugging physical hardware. Disconnect clears the
synthetic source; a reconnect starts neutral. Existing key/button/axis events
continue to work. Physical SDL hotplug, focus and controller-origin glyph validation
remain separate acceptance checks.

Desktop input focus tracks actual focus loss, hiding and minimizing. An SDL
`WINDOW_OCCLUDED` notification does not imply focus loss and no longer cancels game
input: compositors can deliver it after a focused window is restored. Native mouse
positions outside an editor Game viewport are converted from SDL window coordinates
to drawable pixels before UI projection, matching fractional/high-DPI menu layouts.
`CanisInputFocusTests` checks Enter/click after focused occlusion, focus/minimize
recovery and 2×/1.25× coordinate scaling through the same input implementation.
