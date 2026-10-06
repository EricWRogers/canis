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

Instanced draws now restore attribute 7 to the imported vertex-color stream.
Without this restoration, an instanced shadow pass replaced baked irradiance with
instance colors in the subsequent ordinary draw. Adjacent primitives also reuse
unchanged material bindings and model uniforms within one draw invocation.

### Offline visibility

`tools/bake-static-visibility.py INPUT.gltf OUTPUT.gltf` partitions rigid opaque
meshes and writes `OUTPUT.gltf.visibility`. It requires Python 3 and NumPy. This
initial converter targets the engine's POSITION/NORMAL/TEXCOORD_0/COLOR_0 layout
and matrix-authored nodes; skinning, animations, sparse/normalized accessors and
TRS nodes are rejected rather than converted incorrectly. Source assets remain
unchanged. Original primitive order and complete glazing primitives are retained
to preserve coplanar layer ordering. Embedded texture references are preserved
and rebased because a material slot can override tint while retaining embedded albedo.

The bake considers only opaque triangles in the same immutable asset. A target
chunk is hidden only if its complete bounding box lies behind a triangle from
**every** corner of the camera cell. It intersects the corresponding convex shadow
volumes; it does not guess visibility from a handful of sampled rays. Door holes
and uncertain cases stay visible. Moving doors and other changing occluders must
remain separate from the immutable input. Transparent/cutout/displaced geometry
must not be supplied as occluders. Largest-face selection intentionally sacrifices
some culling opportunities to bound bake time. `--chunk-size`, `--cell-size`,
`--minimum`, `--dimensions`, and `--occluders` control the bake.

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
across instanced passes. `python3 tests/test_bake_static_visibility.py` independently
checks the offline proof, door gaps and 36,480 sampled rays from proven-hidden boxes.
