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
├── project/            # Runtime output folder and game assets
│   └── assets/
├── project_settings/   # Project config copied/symlinked next to the executable
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
- system timing collection for update and render systems

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
