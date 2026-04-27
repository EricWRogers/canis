const PAGES = [
    {
        slug: "welcome",
        section: "Start Here",
        title: "Welcome",
        summary: "A contributor-friendly entry point for learning how Canis is organized and how to make your first safe, productive change.",
        kicker: "Start Here",
        chips: ["Onboarding", "Workflow", "Reference", "Static docs"],
        keywords: ["welcome", "overview", "manual", "engine", "editor", "docs", "onboarding", "new contributor", "getting started"],
        content: `
            <section>
                <h2 id="what-this-site-is">What this site is</h2>
                <p>This site is meant to shorten the gap between “I cloned the repo” and “I can confidently change things in Canis.” It is part handbook, part workflow guide, and part reference.</p>
                <div class="doc-grid">
                    <article class="doc-card">
                        <h3>Build it</h3>
                        <p>Learn the few commands and folders that matter so you can get to a clean first run without reading the whole codebase.</p>
                    </article>
                    <article class="doc-card">
                        <h3>Understand it</h3>
                        <p>Get a simple mental model for <code>App</code>, <code>Scene</code>, <code>Entity</code>, components, scripts, assets, and the editor.</p>
                    </article>
                    <article class="doc-card">
                        <h3>Change it</h3>
                        <p>Follow practical contributor flows for adding a script, wiring a property into the inspector, editing scenes, and shipping a web build.</p>
                    </article>
                </div>
            </section>

            <section>
                <h2 id="suggested-learning-path">Suggested learning path</h2>
                <ol>
                    <li>Start with <a href="#first-30-minutes">First 30 Minutes</a> to get a successful build and know what “working” looks like.</li>
                    <li>Read <a href="#canis-mental-model">Canis Mental Model</a> to understand the core runtime concepts before you chase implementation details.</li>
                    <li>Use <a href="#first-feature">Build Your First Feature</a> when you are ready to edit a script, register a property, and see the change in-engine.</li>
                    <li>Keep <a href="#common-workflows">Common Workflows</a> nearby as a task map for scenes, prefabs, UI, animation, and export.</li>
                    <li>Dive into <a href="#engine-guide">Engine Guide</a>, <a href="#component-reference">Component Reference</a>, and <a href="#component-api">Component API</a> as needed.</li>
                </ol>
            </section>

            <section>
                <h2 id="quick-start">Quick start</h2>
                <pre><code>cmake -S . -B build
cmake --build build -j4
./project/c-engine</code></pre>
                <p>The runtime expects to find <code>project/assets/</code> and <code>project/project_settings/</code>, so running from the repo root is the normal flow.</p>
                <div class="doc-callout">
                    <h3>Best way to use this site</h3>
                    <p>Do not try to read everything front to back. Use the onboarding pages to build confidence first, then jump into the deeper reference pages when you hit a real task.</p>
                </div>
            </section>
        `
    },
    {
        slug: "first-30-minutes",
        section: "Start Here",
        title: "First 30 Minutes",
        summary: "Get the repo building, understand the important folders, and leave with a clear picture of what your first successful Canis workflow looks like.",
        kicker: "Start Here",
        chips: ["Build", "Repo Tour", "First Run"],
        keywords: ["first 30 minutes", "first run", "build", "repo layout", "new contributor", "quick start"],
        content: `
            <section>
                <h2 id="your-goal">Your goal</h2>
                <p>In your first half hour, you do not need to understand every system in Canis. You only need a stable foundation: build succeeds, the runtime launches, and the folder layout starts to make sense.</p>
            </section>

            <section>
                <h2 id="build-and-run">Build and run</h2>
                <pre><code>git submodule update --init --recursive
cmake -S . -B build
cmake --build build -j4
./project/c-engine</code></pre>
                <p>If this works, you already have the two most important facts confirmed: the toolchain is healthy and the project can find its runtime content.</p>
            </section>

            <section>
                <h2 id="know-where-things-live">Know where things live</h2>
                <div class="doc-grid">
                    <article class="doc-card">
                        <h3><code>canis/</code></h3>
                        <p>Engine runtime code: ECS, rendering, physics, serialization, editor integration, and platform behavior.</p>
                    </article>
                    <article class="doc-card">
                        <h3><code>game/</code></h3>
                        <p>Project-side gameplay code and scripts. This is the usual place to start when adding new behavior.</p>
                    </article>
                    <article class="doc-card">
                        <h3><code>project/assets/</code></h3>
                        <p>Scenes, prefabs, materials, animations, textures, models, and the content the runtime loads.</p>
                    </article>
                    <article class="doc-card">
                        <h3><code>scripts/</code></h3>
                        <p>Build helpers and repo utility scripts, including web export bootstrapping and content tooling.</p>
                    </article>
                </div>
            </section>

            <section>
                <h2 id="leave-with-these-checkpoints">Leave with these checkpoints</h2>
                <ul>
                    <li>You can build and launch the project from the repo root.</li>
                    <li>You know that engine code usually lives in <code>canis/</code> and project behavior usually lives in <code>game/</code>.</li>
                    <li>You know that scenes and prefabs are content under <code>project/assets/</code>, not hardcoded in the executable.</li>
                    <li>You know which pages to read next: <a href="#canis-mental-model">Canis Mental Model</a>, <a href="#first-feature">Build Your First Feature</a>, and <a href="#editor-workflow">Editor Workflow</a>.</li>
                </ul>
                <div class="doc-callout">
                    <h3>A good first milestone</h3>
                    <p>If you can rebuild after a tiny code edit and still launch successfully, you are ready to start learning by doing instead of only reading.</p>
                </div>
            </section>
        `
    },
    {
        slug: "canis-mental-model",
        section: "Start Here",
        title: "Canis Mental Model",
        summary: "A lightweight model for how the runtime, content, and editor fit together so new contributors can reason about Canis without memorizing every file.",
        kicker: "Start Here",
        chips: ["App", "Scene", "Entity", "Assets"],
        keywords: ["mental model", "app", "scene", "entity", "component", "script", "asset", "editor"],
        content: `
            <section>
                <h2 id="one-picture">One picture</h2>
                <div class="doc-grid">
                    <article class="doc-card">
                        <h3>1. App</h3>
                        <p><code>Canis::App</code> boots the engine, owns the main scene, manages registration, and runs the frame loop.</p>
                    </article>
                    <article class="doc-card">
                        <h3>2. Scene</h3>
                        <p>A <code>Scene</code> is the loaded world. It owns entities, systems, serialization, and much of the runtime orchestration.</p>
                    </article>
                    <article class="doc-card">
                        <h3>3. Entity</h3>
                        <p>An <code>Entity</code> is the thing you place in a scene. It is the handle you use to attach components and scripts.</p>
                    </article>
                    <article class="doc-card">
                        <h3>4. Components and scripts</h3>
                        <p>Components hold data. Scripts hold project behavior. Together they make an entity do something useful.</p>
                    </article>
                    <article class="doc-card">
                        <h3>5. Assets</h3>
                        <p>Scenes, prefabs, textures, materials, animations, and models are assets the runtime loads from <code>project/assets/</code>.</p>
                    </article>
                    <article class="doc-card">
                        <h3>6. Editor and inspector</h3>
                        <p>The editor helps you place entities, edit properties, wire references, and save that content back to scene and prefab assets.</p>
                    </article>
                </div>
            </section>

            <section>
                <h2 id="how-a-change-usually-flows">How a change usually flows</h2>
                <ol>
                    <li>You add or edit a script in <code>game/</code>.</li>
                    <li>You register the properties that should be editable and serializable.</li>
                    <li>You attach that script to an entity in a scene or prefab.</li>
                    <li>The engine loads the scene, constructs the entity, and runs the script inside the frame loop.</li>
                    <li>The scene saves those values back into content assets so the behavior is repeatable.</li>
                </ol>
            </section>

            <section>
                <h2 id="rules-of-thumb">Rules of thumb</h2>
                <ul>
                    <li>If you are changing reusable engine behavior, start in <code>canis/</code>. If you are changing project behavior, start in <code>game/</code>.</li>
                    <li>If something should survive saving and loading, think about registration and serialization early.</li>
                    <li>If a change is mostly content, prefer scenes, prefabs, and inspector wiring over hardcoding values.</li>
                    <li>If you are lost, trace the path from <code>App</code> to <code>Scene</code> to <code>Entity</code> before diving into low-level details.</li>
                </ul>
            </section>
        `
    },
    {
        slug: "first-feature",
        section: "Start Here",
        title: "Build Your First Feature",
        summary: "A safe, repeatable workflow for editing a script, exposing a property, attaching it to content, and verifying the change in Canis.",
        kicker: "Start Here",
        chips: ["Scripts", "Inspector", "Scene", "Iteration"],
        keywords: ["first feature", "script", "register property", "inspector", "scene", "iteration"],
        content: `
            <section>
                <h2 id="choose-a-small-change">Choose a small change</h2>
                <p>Your first feature should be boring on purpose: one new property, one behavior tweak, or one extra bit of scene wiring. The goal is to learn the loop, not to win a refactor contest.</p>
            </section>

            <section>
                <h2 id="feature-loop">Feature loop</h2>
                <ol>
                    <li>Create or edit a script under <code>game/include/</code> and <code>game/src/</code>.</li>
                    <li>Register any field that should appear in the inspector or be saved into a scene.</li>
                    <li>Attach the script to an entity in a scene or prefab.</li>
                    <li>Rebuild, launch, and verify the behavior.</li>
                    <li>Adjust the value in the inspector until the result feels right, then save the content.</li>
                </ol>
            </section>

            <section>
                <h2 id="minimum-script-registration">Minimum script registration</h2>
                <pre><code>REGISTER_PROPERTY(conf, MyGame::MyScript, moveSpeed);
REGISTER_PROPERTY(conf, MyGame::MyScript, turnRate);

DEFAULT_CONFIG_AND_REQUIRED(conf, MyGame::MyScript, Canis::Transform);
conf.DEFAULT_DRAW_INSPECTOR(MyGame::MyScript);
</code></pre>
                <p>This is the key bridge between code and tooling. A registered property can be edited in the inspector, serialized into scene data, and used by animation tooling when appropriate.</p>
            </section>

            <section>
                <h2 id="what-success-looks-like">What success looks like</h2>
                <ul>
                    <li>You can point to the code file that owns the behavior.</li>
                    <li>You can point to the scene or prefab that uses the behavior.</li>
                    <li>You can change a value in the inspector and see the result in-engine.</li>
                    <li>You can save the scene and get the same behavior after the next run.</li>
                </ul>
                <div class="doc-callout">
                    <h3>If the property never shows up</h3>
                    <p>That usually means the field was not registered, the script was not registered with the app, or you rebuilt a different target than the one the runtime is loading.</p>
                </div>
            </section>
        `
    },
    {
        slug: "common-workflows",
        section: "Start Here",
        title: "Common Workflows",
        summary: "Use the docs by task, not by chapter: here is where to go when you need to add content, behavior, UI, animation, or a web build.",
        kicker: "Start Here",
        chips: ["Task Map", "UI", "Scenes", "Export"],
        keywords: ["common workflows", "task map", "scene", "prefab", "ui", "animation", "web export"],
        content: `
            <section>
                <h2 id="use-the-manual-by-task">Use the manual by task</h2>
                <div class="doc-grid">
                    <article class="doc-card">
                        <h3>Add or modify behavior</h3>
                        <p>Start with <a href="#first-feature">Build Your First Feature</a>, then use <a href="#engine-guide">Engine Guide</a> and <a href="#component-api">Component API</a> for specifics.</p>
                    </article>
                    <article class="doc-card">
                        <h3>Pick the right components</h3>
                        <p>Use <a href="#component-reference">Component Reference</a> to understand typical bundles and when to reach for each built-in component.</p>
                    </article>
                    <article class="doc-card">
                        <h3>Edit scenes and prefabs</h3>
                        <p>Jump to <a href="#scenes-and-prefabs">Scenes &amp; Prefabs</a> for serialization, prefab instances, overrides, and editor-side content flow.</p>
                    </article>
                    <article class="doc-card">
                        <h3>Work in the editor</h3>
                        <p>Read <a href="#editor-workflow">Editor Workflow</a> for hierarchy, inspector, assets, play mode, and the general layout of the tools.</p>
                    </article>
                    <article class="doc-card">
                        <h3>Animate properties</h3>
                        <p>Go to <a href="#animation">Animation</a> for clips, curves, events, and registered-property animation.</p>
                    </article>
                    <article class="doc-card">
                        <h3>Ship to the browser</h3>
                        <p>Use <a href="#web-export">Web Export</a> once the native workflow is stable and you are ready to build an HTML5 bundle.</p>
                    </article>
                </div>
            </section>

            <section>
                <h2 id="three-typical-loops">Three typical contributor loops</h2>
                <h3 id="behavior-loop">Behavior loop</h3>
                <p>Edit script code, register properties, attach to content, rebuild, verify, and save.</p>

                <h3 id="content-loop">Content loop</h3>
                <p>Open or create a scene or prefab, wire components and references, test in play mode, and save the asset.</p>

                <h3 id="shipping-loop">Shipping loop</h3>
                <p>Get the native flow working first, then switch to the web export path only after the behavior is stable.</p>
            </section>
        `
    },
    {
        slug: "troubleshooting",
        section: "Start Here",
        title: "Troubleshooting",
        summary: "The first problems most newcomers hit in Canis, plus the fastest checks to make before you assume something deep is broken.",
        kicker: "Start Here",
        chips: ["Build", "Runtime", "Inspector", "Web"],
        keywords: ["troubleshooting", "build issue", "runtime issue", "inspector", "web", "debug"],
        content: `
            <section>
                <h2 id="build-starts-failing">Build starts failing</h2>
                <article class="component-entry">
                    <h3 id="build-checks">Fast checks</h3>
                    <ul class="component-meta">
                        <li><strong>Submodules:</strong> make sure dependencies are initialized with <code>git submodule update --init --recursive</code>.</li>
                        <li><strong>Target:</strong> confirm you rebuilt the actual project target and are running the updated executable under <code>project/</code>.</li>
                        <li><strong>Header/source mismatch:</strong> if a new script compiles oddly, double-check the declaration in <code>game/include/</code> matches the implementation in <code>game/src/</code>.</li>
                    </ul>
                </article>
            </section>

            <section>
                <h2 id="runtime-launches-but-looks-wrong">Runtime launches but looks wrong</h2>
                <article class="component-entry">
                    <h3 id="runtime-checks">Fast checks</h3>
                    <ul class="component-meta">
                        <li><strong>Working directory:</strong> launch from the repo root so the runtime can find <code>project/assets/</code> and <code>project/project_settings/</code>.</li>
                        <li><strong>Content vs code:</strong> ask whether the bug lives in scene data, prefab wiring, or script logic before changing engine internals.</li>
                        <li><strong>Saved content:</strong> if a fix disappears after restart, the value may never have been serialized back into the scene or prefab.</li>
                    </ul>
                </article>
            </section>

            <section>
                <h2 id="property-is-missing-in-the-inspector">Property is missing in the inspector</h2>
                <article class="component-entry">
                    <h3 id="inspector-checks">Fast checks</h3>
                    <ul class="component-meta">
                        <li><strong>Registration:</strong> confirm the field is included in <code>REGISTER_PROPERTY(...)</code>.</li>
                        <li><strong>Script config:</strong> confirm the script registration function still runs during app startup.</li>
                        <li><strong>Rebuild:</strong> after changing registration or script code, rebuild before expecting the editor/runtime to reflect it.</li>
                    </ul>
                </article>
            </section>

            <section>
                <h2 id="web-build-behaves-differently">Web build behaves differently</h2>
                <article class="component-entry">
                    <h3 id="web-checks">Fast checks</h3>
                    <ul class="component-meta">
                        <li><strong>Serve it:</strong> do not open the export with <code>file://</code>; serve the folder through a local web server.</li>
                        <li><strong>Native first:</strong> if a feature is already unstable natively, fix that before chasing browser-specific behavior.</li>
                        <li><strong>Runtime model:</strong> remember the web target disables the editor runtime and links gameplay code statically.</li>
                    </ul>
                    <pre><code>./scripts/build-web.sh web-release
python3 -m http.server 8000 --directory build-web-release/web</code></pre>
                </article>
            </section>
        `
    },
    {
        slug: "engine-guide",
        section: "Core Manual",
        title: "Engine Guide",
        summary: "Repository structure, runtime boot flow, ECS concepts, registration macros, serialization, and the main engine boundaries.",
        kicker: "Core Manual",
        chips: ["App", "Scene", "Entity", "Registration"],
        keywords: ["engine", "scene", "entity", "app", "serialization", "ecs", "assetmanager"],
        sourceHref: "../engine-guide.md",
        sourceLabel: "Read source markdown",
        content: `
            <section>
                <h2 id="what-the-repo-builds">What the repository builds</h2>
                <ul>
                    <li><code>CanisEngine</code> from <code>canis/</code></li>
                    <li><code>GameCode</code> from <code>game/</code></li>
                    <li><code>c-engine</code> as the executable under <code>project/</code></li>
                </ul>
                <p>At runtime the executable boots the engine, scans assets and meta files, loads gameplay code, and starts the configured scene.</p>
            </section>

            <section>
                <h2 id="repository-layout">Repository layout</h2>
                <pre><code>.
├── canis/              # Engine code
│   ├── include/Canis/  # Public headers
│   └── src/            # Engine implementation
├── game/               # Gameplay code and scripts
├── project/            # Runtime output, assets, scenes, prefabs
├── external/           # Third-party libraries
└── scripts/            # Build and utility scripts</code></pre>
            </section>

            <section>
                <h2 id="runtime-flow">Runtime flow</h2>
                <ol>
                    <li><code>Canis::App::Run()</code> starts the engine.</li>
                    <li>The working directory is normalized so <code>assets/</code> resolves cleanly.</li>
                    <li>Asset metadata is discovered from <code>project/assets/</code>.</li>
                    <li>Built-in components, systems, and inspector behavior are registered.</li>
                    <li><code>GameCode</code> is loaded and its exported init hooks are called.</li>
                    <li>The active scene is loaded and the frame loop begins.</li>
                </ol>
            </section>

            <section>
                <h2 id="core-concepts">Core concepts</h2>
                <h3 id="app">App</h3>
                <p><code>Canis::App</code> owns the scene, script registry, timing, and editor integration. It is the engine bootstrapper and the main registration point for built-in components.</p>

                <h3 id="scene">Scene</h3>
                <p><code>Canis::Scene</code> owns the runtime world. It stores entities, systems, YAML load/save behavior, and physics queries.</p>

                <h3 id="entity">Entity</h3>
                <p><code>Canis::Entity</code> wraps an <code>entt::entity</code> plus engine metadata such as <code>name</code>, <code>tag</code>, <code>uuid</code>, and scene ownership.</p>

                <h3 id="components-and-scripts">Components and scripts</h3>
                <p>Components are plain data in the ECS registry. Gameplay scripts derive from <code>Canis::ScriptableEntity</code> and are attached through the shared script registry.</p>
            </section>

            <section>
                <h2 id="registering-properties">Registering properties</h2>
                <p><code>ScriptConf</code> and <code>REGISTER_PROPERTY(...)</code> are the main editor/serialization bridge. A registered property gets inspector UI, scene serialization, and now animation metadata too.</p>
                <pre><code>REGISTER_PROPERTY(conf, MyGame::MyScript, speed);
REGISTER_PROPERTY(conf, MyGame::MyScript, jumpForce);

DEFAULT_CONFIG_AND_REQUIRED(conf, MyGame::MyScript, Canis::Transform, Canis::Rigidbody);
conf.DEFAULT_DRAW_INSPECTOR(MyGame::MyScript);</code></pre>
            </section>

            <section>
                <h2 id="asset-and-scene-notes">Asset and scene notes</h2>
                <p>Scenes are YAML files under <code>project/assets/scenes/</code>. Prefabs are also scene assets, usually under <code>project/assets/prefabs/</code>. UUID-based references are preferred so assets can move without constantly breaking links.</p>
            </section>
        `
    },
    {
        slug: "component-reference",
        section: "Reference",
        title: "Component Reference",
        summary: "Examples for the built-in components: what each one is for, what it usually pairs with, and a small setup recipe you can copy.",
        kicker: "Reference",
        chips: ["Built-in components", "Examples", "Usage patterns", "Scene recipes"],
        keywords: [
            "component", "components", "reference", "example", "usage",
            "Canvas", "RectTransform", "Transform", "PrefabInstance",
            "Rigidbody", "BoxCollider", "SphereCollider", "CapsuleCollider", "MeshCollider",
            "Camera", "DirectionalLight", "PointLight", "Model", "Material",
            "ModelAnimation", "AnimationPlayer", "Animator", "Sprite2D", "Text",
            "UIButton", "UIInputField", "UIDragSource", "UIDropTarget",
            "Camera2D", "SpriteAnimation", "NetworkIdentity"
        ],
        content: `
            <section>
                <h2 id="how-to-read-this-page">How to read this page</h2>
                <p>This page is intentionally practical instead of exhaustive. Each component entry answers four questions: what it does, when to use it, what it usually pairs with, and what a small real setup looks like in practice.</p>
                <div class="doc-callout">
                    <h3>Scene recipe vs exact YAML</h3>
                    <p>The snippets below are short “scene recipes”, not literal full scene files. They use the real component names and fields, but they leave out unrelated entity boilerplate so the intent is easier to scan.</p>
                </div>
                <p>If you want the literal public API for each component, including field-by-field notes and gameplay-side C++ snippets, jump to <a href="#component-api">Component API</a>.</p>
            </section>

            <section>
                <h2 id="common-bundles">Common bundles</h2>
                <div class="doc-grid">
                    <article class="doc-card">
                        <h3>Simple 3D prop</h3>
                        <p><code>Transform</code> + <code>Model</code> + optional <code>Material</code>.</p>
                    </article>
                    <article class="doc-card">
                        <h3>Physical prop</h3>
                        <p><code>Transform</code> + one collider + optional <code>Rigidbody</code>.</p>
                    </article>
                    <article class="doc-card">
                        <h3>UI button</h3>
                        <p><code>Canvas</code> + <code>RectTransform</code> + <code>Sprite2D</code>/<code>Text</code> + <code>UIButton</code>.</p>
                    </article>
                    <article class="doc-card">
                        <h3>Animated gameplay object</h3>
                        <p><code>Transform</code> + script + <code>AnimationPlayer</code> or <code>ModelAnimation</code>.</p>
                    </article>
                </div>
            </section>

            <section>
                <h2 id="ui-and-layout">UI &amp; layout</h2>

                <article class="component-entry">
                    <h3 id="canvas">Canis::Canvas</h3>
                    <p>Canvas is the root surface for UI. Use it when you want screen-space menus, HUD elements, or other <code>RectTransform</code>-based content.</p>
                    <ul class="component-meta">
                        <li><strong>Use it for:</strong> start menus, score HUDs, lobby UI, buttons, text overlays.</li>
                        <li><strong>Usually paired with:</strong> <code>RectTransform</code>, <code>Sprite2D</code>, <code>Text</code>, <code>UIButton</code>.</li>
                        <li><strong>Key fields:</strong> <code>renderMode</code>, <code>scaleMode</code>, <code>screenSize</code>.</li>
                    </ul>
                    <pre><code>HUDRoot
  Canis::Canvas
    renderMode: Screen Space Overlay
    scaleMode: Scale With Screen Width
    screenSize: [1280, 800]
  Canis::RectTransform</code></pre>
                </article>

                <article class="component-entry">
                    <h3 id="recttransform">Canis::RectTransform</h3>
                    <p>RectTransform is the UI transform. It positions, sizes, anchors, and rotates 2D UI entities relative to a parent rect or a canvas.</p>
                    <ul class="component-meta">
                        <li><strong>Use it for:</strong> anything that should anchor to the screen instead of existing in 3D world space.</li>
                        <li><strong>Usually paired with:</strong> <code>Canvas</code>, <code>Sprite2D</code>, <code>Text</code>, <code>UIButton</code>, <code>UIInputField</code>.</li>
                        <li><strong>Key fields:</strong> <code>position</code>, <code>size</code>, <code>anchorMin</code>, <code>anchorMax</code>, <code>pivot</code>, <code>depth</code>.</li>
                    </ul>
                    <pre><code>ScoreLabel
  Canis::RectTransform
    anchorMin: [1.0, 1.0]
    anchorMax: [1.0, 1.0]
    pivot: [1.0, 1.0]
    position: [-32, -24]
    size: [220, 48]
  Canis::Text</code></pre>
                </article>

                <article class="component-entry">
                    <h3 id="sprite2d">Canis::Sprite2D</h3>
                    <p>Sprite2D draws a textured quad in the UI/2D renderer. It is the usual image component for buttons, panels, icons, and 2D props.</p>
                    <ul class="component-meta">
                        <li><strong>Use it for:</strong> icons, menu backgrounds, duck life icons, HUD art, 2D pickups.</li>
                        <li><strong>Usually paired with:</strong> <code>RectTransform</code> or a 2D setup, optional <code>SpriteAnimation</code>.</li>
                        <li><strong>Key fields:</strong> <code>textureHandle</code>, <code>color</code>, <code>uv</code>, <code>flipX</code>, <code>flipY</code>.</li>
                    </ul>
                    <pre><code>HostButtonBackground
  Canis::RectTransform
    size: [256, 80]
  Canis::Sprite2D
    texture: assets/textures/bath_battle/host_button.png
    color: [1, 1, 1, 1]</code></pre>
                </article>

                <article class="component-entry">
                    <h3 id="text">Canis::Text</h3>
                    <p>Text renders a font asset with alignment and wrapping controls. Use it for labels, timers, menu copy, and debug readouts.</p>
                    <ul class="component-meta">
                        <li><strong>Use it for:</strong> countdowns, player names, prompts, button labels, status text.</li>
                        <li><strong>Usually paired with:</strong> <code>RectTransform</code>.</li>
                        <li><strong>Key fields:</strong> <code>text</code>, <code>color</code>, <code>alignment</code>, <code>horizontalBoundary</code>.</li>
                    </ul>
                    <pre><code>RoundTimer
  Canis::RectTransform
    anchorMin: [0.5, 1.0]
    anchorMax: [0.5, 1.0]
    position: [0, -28]
    size: [280, 64]
  Canis::Text
    text: "60"
    alignment: Center</code></pre>
                </article>

                <article class="component-entry">
                    <h3 id="uibutton">Canis::UIButton</h3>
                    <p>UIButton wires a UI entity to a script action. It also handles hover and pressed visual feedback by tinting and scaling the same entity.</p>
                    <ul class="component-meta">
                        <li><strong>Use it for:</strong> start, join, host, quit, ready, leave, and other click-driven UI.</li>
                        <li><strong>Usually paired with:</strong> <code>RectTransform</code>, <code>Sprite2D</code> and/or <code>Text</code>.</li>
                        <li><strong>Key fields:</strong> <code>targetEntity</code>, <code>targetScript</code>, <code>actionName</code>, <code>hoverColor</code>, <code>pressedScale</code>.</li>
                    </ul>
                    <pre><code>StartMatchButton
  Canis::RectTransform
  Canis::Sprite2D
  Canis::UIButton
    targetEntity: LobbyController
    targetScript: BathBattle::BathBattleLobbyController
    actionName: StartMatch
    hoverScale: 1.03
    pressedScale: 0.98</code></pre>
                </article>

                <article class="component-entry">
                    <h3 id="uiinputfield">Canis::UIInputField</h3>
                    <p>UIInputField captures keyboard text, displays it through another entity, and can bind it back into a script property.</p>
                    <ul class="component-meta">
                        <li><strong>Use it for:</strong> player name entry, IP address entry, chat boxes, seed fields.</li>
                        <li><strong>Usually paired with:</strong> <code>RectTransform</code>, a background <code>Sprite2D</code>, and a separate <code>Text</code> display entity.</li>
                        <li><strong>Key fields:</strong> <code>displayEntity</code>, <code>targetEntity</code>, <code>targetScript</code>, <code>targetProperty</code>, <code>placeholder</code>.</li>
                    </ul>
                    <pre><code>NameField
  Canis::RectTransform
  Canis::Sprite2D
  Canis::UIInputField
    displayEntity: NameFieldText
    targetEntity: StartMenuController
    targetScript: BathBattle::BathBattleStartMenu
    targetProperty: playerName
    placeholder: "Enter your duck name"</code></pre>
                </article>

                <article class="component-entry">
                    <h3 id="uidragsource">Canis::UIDragSource</h3>
                    <p>UIDragSource turns a UI entity into something you can drag and gives it a typed payload.</p>
                    <ul class="component-meta">
                        <li><strong>Use it for:</strong> inventory icons, editor-style drag items, card or tile movement.</li>
                        <li><strong>Usually paired with:</strong> <code>RectTransform</code>, <code>Sprite2D</code>.</li>
                        <li><strong>Key fields:</strong> <code>payloadType</code>, <code>payloadValue</code>.</li>
                    </ul>
                    <pre><code>DuckCardBlue
  Canis::RectTransform
  Canis::Sprite2D
  Canis::UIDragSource
    payloadType: "duck-card"
    payloadValue: "blue"</code></pre>
                </article>

                <article class="component-entry">
                    <h3 id="uidroptarget">Canis::UIDropTarget</h3>
                    <p>UIDropTarget accepts a drag payload and routes the drop into a script action.</p>
                    <ul class="component-meta">
                        <li><strong>Use it for:</strong> equipment slots, team slots, editor-style asset drops, simple UI crafting layouts.</li>
                        <li><strong>Usually paired with:</strong> <code>RectTransform</code>, optional <code>Sprite2D</code>.</li>
                        <li><strong>Key fields:</strong> <code>acceptedPayloadType</code>, <code>targetEntity</code>, <code>targetScript</code>, <code>actionName</code>.</li>
                    </ul>
                    <pre><code>TeamSlotA
  Canis::RectTransform
  Canis::Sprite2D
  Canis::UIDropTarget
    acceptedPayloadType: "duck-card"
    targetEntity: LobbyController
    targetScript: BathBattle::BathBattleLobbyController
    actionName: HandleDuckDrop</code></pre>
                </article>
            </section>

            <section>
                <h2 id="world-and-rendering">World &amp; rendering</h2>

                <article class="component-entry">
                    <h3 id="transform">Canis::Transform</h3>
                    <p>Transform is the 3D hierarchy component. It stores local position, rotation, and scale, and it handles parent/child world transforms.</p>
                    <ul class="component-meta">
                        <li><strong>Use it for:</strong> anything in the 3D world, including props, cameras, spawn points, and light anchors.</li>
                        <li><strong>Usually paired with:</strong> <code>Model</code>, <code>Camera</code>, <code>PointLight</code>, <code>Rigidbody</code>.</li>
                        <li><strong>Key fields:</strong> <code>position</code>, <code>rotation</code>, <code>scale</code>, <code>parent</code>.</li>
                    </ul>
                    <pre><code>BoostPad
  Canis::Transform
    position: [3.5, 0.12, -6.0]
    rotation: [0.0, 0.0, 0.0]
    scale: [1.0, 1.0, 1.0]</code></pre>
                </article>

                <article class="component-entry">
                    <h3 id="camera">Canis::Camera</h3>
                    <p>Camera is the main 3D view component. One camera is usually marked primary and drives the Game view.</p>
                    <ul class="component-meta">
                        <li><strong>Use it for:</strong> gameplay camera, menu diorama camera, cinematic flythrough camera.</li>
                        <li><strong>Usually paired with:</strong> <code>Transform</code>.</li>
                        <li><strong>Key fields:</strong> <code>primary</code>, <code>fovDegrees</code>, <code>nearClip</code>, <code>farClip</code>.</li>
                    </ul>
                    <pre><code>FollowCamera
  Canis::Transform
    position: [0, 10, 14]
    rotation: [-0.55, 0, 0]
  Canis::Camera
    primary: true
    fovDegrees: 60</code></pre>
                </article>

                <article class="component-entry">
                    <h3 id="directionallight">Canis::DirectionalLight</h3>
                    <p>DirectionalLight is the sun-style light for the whole scene. It is the easiest way to establish the base lighting direction.</p>
                    <ul class="component-meta">
                        <li><strong>Use it for:</strong> outdoor lighting, main fill for stylized scenes, simple showcase scenes.</li>
                        <li><strong>Usually paired with:</strong> nothing required; it uses its own <code>direction</code> vector.</li>
                        <li><strong>Key fields:</strong> <code>enabled</code>, <code>color</code>, <code>intensity</code>, <code>direction</code>.</li>
                    </ul>
                    <pre><code>Sun
  Canis::DirectionalLight
    direction: [-0.4, -1.0, -0.25]
    intensity: 1.1
    color: [1.0, 0.97, 0.92, 1.0]</code></pre>
                </article>

                <article class="component-entry">
                    <h3 id="pointlight">Canis::PointLight</h3>
                    <p>PointLight is a local light source with a transform position and a falloff range.</p>
                    <ul class="component-meta">
                        <li><strong>Use it for:</strong> accent lights, pickups, lamps, interior fills, glowing props.</li>
                        <li><strong>Usually paired with:</strong> <code>Transform</code>.</li>
                        <li><strong>Key fields:</strong> <code>color</code>, <code>intensity</code>, <code>range</code>.</li>
                    </ul>
                    <pre><code>PickupGlow
  Canis::Transform
    position: [0, 1.2, 0]
  Canis::PointLight
    color: [1.0, 0.8, 0.95, 1.0]
    intensity: 1.4
    range: 6.0</code></pre>
                </article>

                <article class="component-entry">
                    <h3 id="model">Canis::Model</h3>
                    <p>Model renders a 3D asset. With the newer model drop workflow, it can also point at a specific imported node inside a larger model hierarchy.</p>
                    <ul class="component-meta">
                        <li><strong>Use it for:</strong> props, players, stage pieces, imported glTF/GLB hierarchy nodes.</li>
                        <li><strong>Usually paired with:</strong> <code>Transform</code>, optional <code>Material</code>, optional <code>ModelAnimation</code>.</li>
                        <li><strong>Key fields:</strong> <code>modelId</code>, <code>nodeIndex</code>, <code>applyNodeTransform</code>, <code>color</code>.</li>
                    </ul>
                    <pre><code>BathDuckVisual
  Canis::Transform
    scale: [24, 24, 24]
  Canis::Model
    model: assets/models/bath_battle/duck_blue.glb
    color: [1, 1, 1, 1]</code></pre>
                </article>

                <article class="component-entry">
                    <h3 id="material">Canis::Material</h3>
                    <p>Material overrides what shader/material a renderer uses and lets you tune tint and custom uniform values.</p>
                    <ul class="component-meta">
                        <li><strong>Use it for:</strong> per-instance recolors, shader graph materials, water tuning, bubble tint, glow variants.</li>
                        <li><strong>Usually paired with:</strong> <code>Model</code> or other renderable components.</li>
                        <li><strong>Key fields:</strong> <code>materialId</code>, <code>materialIds</code>, <code>color</code>, <code>materialFields</code>.</li>
                    </ul>
                    <pre><code>BathWater
  Canis::Transform
  Canis::Model
    model: cube
  Canis::Material
    material: assets/materials/bath_battle/water_graph.material
    color: [1, 1, 1, 0.92]
    uniforms:
      Deep Color: [0.29, 0.72, 0.98, 1.0]</code></pre>
                </article>

                <article class="component-entry">
                    <h3 id="prefabinstance">Canis::PrefabInstance</h3>
                    <p>PrefabInstance marks that an entity came from a prefab scene asset. It is mostly editor-managed metadata rather than something you add manually during gameplay scripting.</p>
                    <ul class="component-meta">
                        <li><strong>Use it for:</strong> rebuild from prefab, apply overrides, scene/prefab linkage.</li>
                        <li><strong>Usually paired with:</strong> any prefabbed entity hierarchy.</li>
                        <li><strong>Key fields:</strong> <code>prefab</code>, <code>firstEntity</code>.</li>
                    </ul>
                    <pre><code>BathBattleLevel/SpinBatonA
  Canis::PrefabInstance
    prefab: assets/prefabs/spin_baton.scene
    firstEntity: SpinBatonA</code></pre>
                </article>
            </section>

            <section>
                <h2 id="physics-and-collision">Physics &amp; collision</h2>

                <article class="component-entry">
                    <h3 id="rigidbody">Canis::Rigidbody</h3>
                    <p>Rigidbody gives a 3D entity physics simulation, collision response, velocity, and force application.</p>
                    <ul class="component-meta">
                        <li><strong>Use it for:</strong> players, physics props, moving hazards, sensors, pickups with impulses.</li>
                        <li><strong>Usually paired with:</strong> <code>Transform</code> and exactly one collider shape.</li>
                        <li><strong>Key fields:</strong> <code>motionType</code>, <code>mass</code>, <code>friction</code>, <code>restitution</code>, <code>useGravity</code>, <code>lockRotationX/Y/Z</code>.</li>
                    </ul>
                    <pre><code>PlayerRoot
  Canis::Transform
  Canis::SphereCollider
    radius: 0.5
  Canis::Rigidbody
    motionType: Dynamic
    mass: 1.0
    friction: 1.2
    useGravity: true
    lockRotationX: false
    lockRotationY: false
    lockRotationZ: false</code></pre>
                </article>

                <article class="component-entry">
                    <h3 id="boxcollider">Canis::BoxCollider</h3>
                    <p>BoxCollider is the default all-purpose 3D collider for rectangular gameplay pieces.</p>
                    <ul class="component-meta">
                        <li><strong>Use it for:</strong> platforms, walls, ramps, crates, trigger pads, simple blockers.</li>
                        <li><strong>Usually paired with:</strong> <code>Transform</code>, optional <code>Rigidbody</code>.</li>
                        <li><strong>Key fields:</strong> <code>size</code>.</li>
                    </ul>
                    <pre><code>StageFloor
  Canis::Transform
    scale: [20, 1, 20]
  Canis::BoxCollider
    size: [20, 1, 20]</code></pre>
                </article>

                <article class="component-entry">
                    <h3 id="spherecollider">Canis::SphereCollider</h3>
                    <p>SphereCollider is a good fit for rolling objects and simple round overlap volumes.</p>
                    <ul class="component-meta">
                        <li><strong>Use it for:</strong> duck bodies, marbles, round pickups, simple radial sensors.</li>
                        <li><strong>Usually paired with:</strong> <code>Transform</code>, often <code>Rigidbody</code>.</li>
                        <li><strong>Key fields:</strong> <code>radius</code>.</li>
                    </ul>
                    <pre><code>DuckBody
  Canis::Transform
  Canis::SphereCollider
    radius: 0.5
  Canis::Rigidbody</code></pre>
                </article>

                <article class="component-entry">
                    <h3 id="capsulecollider">Canis::CapsuleCollider</h3>
                    <p>CapsuleCollider is the better choice for upright characters or tall round props where a sphere feels too loose.</p>
                    <ul class="component-meta">
                        <li><strong>Use it for:</strong> humanoid characters, standing NPCs, vertical bumpers, posts.</li>
                        <li><strong>Usually paired with:</strong> <code>Transform</code>, often <code>Rigidbody</code>.</li>
                        <li><strong>Key fields:</strong> <code>halfHeight</code>, <code>radius</code>.</li>
                    </ul>
                    <pre><code>StandingNPC
  Canis::Transform
  Canis::CapsuleCollider
    halfHeight: 0.9
    radius: 0.3
  Canis::Rigidbody
    motionType: Kinematic</code></pre>
                </article>

                <article class="component-entry">
                    <h3 id="meshcollider">Canis::MeshCollider</h3>
                    <p>MeshCollider uses geometry from a model asset. It is useful for static environment pieces or very specific collision silhouettes.</p>
                    <ul class="component-meta">
                        <li><strong>Use it for:</strong> large imported stage meshes, complex static props, showcase collision.</li>
                        <li><strong>Usually paired with:</strong> <code>Transform</code>, <code>Model</code>; usually static or kinematic.</li>
                        <li><strong>Key fields:</strong> <code>useAttachedModel</code>, <code>modelPath</code>.</li>
                    </ul>
                    <pre><code>TubShell
  Canis::Transform
  Canis::Model
    model: assets/models/bath_battle/tub.glb
  Canis::MeshCollider
    useAttachedModel: true</code></pre>
                </article>

                <div class="doc-callout">
                    <h3>Collider switching</h3>
                    <p>The built-in add flow treats the collider types as alternatives. Adding one collider type through the editor removes the others so each entity keeps one main collision shape at a time.</p>
                </div>
            </section>

            <section>
                <h2 id="animation-components">Animation components</h2>

                <article class="component-entry">
                    <h3 id="modelanimation">Canis::ModelAnimation</h3>
                    <p>ModelAnimation plays imported animation data that ships inside a model asset.</p>
                    <ul class="component-meta">
                        <li><strong>Use it for:</strong> glTF character clips, prop loops, baked model animations.</li>
                        <li><strong>Usually paired with:</strong> <code>Model</code>, <code>Transform</code>.</li>
                        <li><strong>Key fields:</strong> <code>playAnimation</code>, <code>loop</code>, <code>animationSpeed</code>, <code>animationIndex</code>.</li>
                    </ul>
                    <pre><code>AnimatedProp
  Canis::Transform
  Canis::Model
    model: assets/models/showcase/fan.glb
  Canis::ModelAnimation
    playAnimation: true
    loop: true
    animationIndex: 0</code></pre>
                </article>

                <article class="component-entry">
                    <h3 id="animationplayer">Canis::AnimationPlayer</h3>
                    <p>AnimationPlayer is the new generic property animation component. It samples an <code>.animclip</code> and writes keyed values back into components and registered script variables.</p>
                    <ul class="component-meta">
                        <li><strong>Use it for:</strong> animating transforms, UI values, script floats, scripted prop motion, and other Unity-style property clips.</li>
                        <li><strong>Usually paired with:</strong> an animation root entity plus an <code>.animclip</code> asset.</li>
                        <li><strong>Key fields:</strong> <code>clip</code>, <code>playing</code>, <code>loop</code>, <code>speed</code>, <code>time</code>.</li>
                    </ul>
                    <pre><code>SpinBatonExample
  Canis::PrefabInstance
    prefab: assets/prefabs/spin_baton.scene
  Canis::AnimationPlayer
    clip: assets/animations/spin_baton_example.animclip
    playing: true
    loop: true</code></pre>
                </article>

                <article class="component-entry">
                    <h3 id="animator">Canis::Animator</h3>
                    <p>Animator is the higher-level state-machine layer that chooses which clip should play. Use it when you want named states, parameters, triggers, and transitions instead of manually swapping one clip handle yourself.</p>
                    <ul class="component-meta">
                        <li><strong>Use it for:</strong> character state machines, UI state-driven loops, hazard states, idle/run/impact clip switching.</li>
                        <li><strong>Usually paired with:</strong> an animation root hierarchy plus a <code>.animator</code> controller asset and one or more <code>.animclip</code> assets.</li>
                        <li><strong>Key fields:</strong> <code>controller</code>, <code>playing</code>, runtime <code>currentState</code>, runtime parameter values.</li>
                    </ul>
                    <pre><code>SpinBatonExample
  Canis::PrefabInstance
    prefab: assets/prefabs/spin_baton.scene
  BathBattle::BathBattleSpinner
  Canis::Animator
    controller: assets/animations/spin_baton_showcase.animator
    playing: true</code></pre>
                </article>

                <article class="component-entry">
                    <h3 id="spriteanimation">Canis::SpriteAnimation</h3>
                    <p>SpriteAnimation advances frames from a sprite animation asset and updates the Sprite2D UVs for you.</p>
                    <ul class="component-meta">
                        <li><strong>Use it for:</strong> HUD flipbooks, 2D VFX, retro character frames, animated icons.</li>
                        <li><strong>Usually paired with:</strong> <code>Sprite2D</code>.</li>
                        <li><strong>Key fields:</strong> <code>id</code>, <code>speed</code>, plus runtime <code>Play(path)</code> / <code>Pause()</code>.</li>
                    </ul>
                    <pre><code>CoinSparkle
  Canis::RectTransform
  Canis::Sprite2D
  Canis::SpriteAnimation
    animation: assets/animations/ui/coin_spin.spriteanim
    speed: 1.0</code></pre>
                </article>
            </section>

            <section>
                <h2 id="camera-and-2d-tools">Camera &amp; 2D tools</h2>

                <article class="component-entry">
                    <h3 id="camera2d">Canis::Camera2D</h3>
                    <p>Camera2D is the dedicated 2D camera helper. It manages a 2D camera matrix and is most useful in pure 2D scenes or tools.</p>
                    <ul class="component-meta">
                        <li><strong>Use it for:</strong> top-down 2D games, UI-heavy 2D slices, simple map editors.</li>
                        <li><strong>Usually paired with:</strong> scripts that call <code>SetPosition(...)</code> and <code>SetScale(...)</code>.</li>
                        <li><strong>Key methods:</strong> <code>SetPosition</code>, <code>SetScale</code>, <code>GetCameraMatrix</code>.</li>
                    </ul>
                    <pre><code>World2DCamera
  Canis::Camera2D

// script usage
camera2D.SetPosition({ playerX, playerY });
camera2D.SetScale(2.0f);</code></pre>
                </article>
            </section>

            <section>
                <h2 id="networking">Networking</h2>

                <article class="component-entry">
                    <h3 id="networkidentity">Canis::NetworkIdentity</h3>
                    <p>NetworkIdentity marks an entity as a networked object and stores ownership plus prefab information for replication/spawn logic.</p>
                    <ul class="component-meta">
                        <li><strong>Use it for:</strong> player pawns, replicated props, spawned match objects, networked pickups.</li>
                        <li><strong>Usually paired with:</strong> <code>Transform</code>, often <code>Rigidbody</code>, and a prefab asset when the object is spawnable.</li>
                        <li><strong>Key fields:</strong> <code>netId</code>, <code>ownerClientId</code>, <code>serverOwned</code>, <code>localOwned</code>, <code>replicateTransform</code>, <code>prefab</code>.</li>
                    </ul>
                    <pre><code>PlayerPawn
  Canis::Transform
  Canis::SphereCollider
  Canis::Rigidbody
  Canis::NetworkIdentity
    ownerClientId: 2
    localOwned: true
    replicateTransform: true
    prefab: assets/prefabs/bath_battle/player.scene</code></pre>
                </article>
            </section>

            <section>
                <h2 id="example-stacks">Example stacks</h2>

                <article class="component-entry">
                    <h3 id="example-basic-3d-prop">Basic 3D prop</h3>
                    <pre><code>WoodCrate
  Canis::Transform
    position: [0, 0.5, 0]
  Canis::Model
    model: assets/models/bath_battle/wood_crate.glb
  Canis::Material
    material: assets/materials/bath_battle/crate.material</code></pre>
                </article>

                <article class="component-entry">
                    <h3 id="example-physics-bumper">Physics bumper</h3>
                    <pre><code>Bumper
  Canis::Transform
  Canis::Model
    model: assets/models/bath_battle/bumper.glb
  Canis::CapsuleCollider
    halfHeight: 0.5
    radius: 0.35
  Canis::Rigidbody
    motionType: Static
    restitution: 0.2</code></pre>
                </article>

                <article class="component-entry">
                    <h3 id="example-ui-button">Menu button</h3>
                    <pre><code>JoinButton
  Canis::RectTransform
    size: [256, 80]
  Canis::Sprite2D
    texture: assets/textures/bath_battle/join_button.png
  Canis::UIButton
    targetEntity: BathBattleStartMenu
    targetScript: BathBattle::BathBattleStartMenu
    actionName: Join</code></pre>
                </article>

                <article class="component-entry">
                    <h3 id="example-animated-scripted-prop">Animated scripted prop</h3>
                    <pre><code>SpinBatonExample
  Canis::PrefabInstance
    prefab: assets/prefabs/spin_baton.scene
  BathBattle::BathBattleSpinner
    rotationSpeedDegrees: 170
  Canis::AnimationPlayer
    clip: assets/animations/spin_baton_example.animclip</code></pre>
                    <p>The matching example scene already lives in <code>project/assets/scenes/spin_baton_animation_example.scene</code>.</p>
                </article>
            </section>
        `
    },
    {
        slug: "component-api",
        section: "Reference",
        title: "Component API",
        summary: "The fuller built-in component API: public variables, public methods, and short gameplay-side C++ examples.",
        kicker: "Reference",
        chips: ["Public variables", "Public methods", "Gameplay code", "C++ examples"],
        keywords: [
            "component api", "api", "public variables", "public methods", "game side code",
            "Canvas", "RectTransform", "Transform", "PrefabInstance", "Rigidbody",
            "BoxCollider", "SphereCollider", "CapsuleCollider", "MeshCollider",
            "Camera", "DirectionalLight", "PointLight", "Model", "Material",
            "ModelAnimation", "AnimationPlayer", "Animator", "Sprite2D", "Text", "UIButton",
            "UIInputField", "UIDragSource", "UIDropTarget", "Camera2D",
            "SpriteAnimation", "NetworkIdentity"
        ],
        content: `
            <section>
                <h2 id="how-this-page-is-organized">How this page is organized</h2>
                <p>This is the literal follow-up to the shorter <a href="#component-reference">Component Reference</a>. The goal here is not just “what is this component for?”, but “what does each public field mean, what public methods exist, and what does game-side usage look like in C++?”</p>
                <div class="doc-callout">
                    <h3>Shared members explained once</h3>
                    <p>Almost every built-in component exposes <code>ScriptName</code>, an <code>entity</code> back-pointer, a default constructor, and a <code>Create()</code> hook. To keep this page readable, those shared boilerplate members are explained here instead of being repeated in every single entry.</p>
                    <ul>
                        <li><code>ScriptName</code>: the registry/serialization name used by the editor, scene YAML, and config helpers.</li>
                        <li><code>entity</code>: the owning <code>Canis::Entity</code>; the engine fills this in when the component is attached.</li>
                        <li><code>Create()</code>: a lifecycle hook called after creation/load. For many built-ins it is intentionally empty.</li>
                        <li>Constructors/destructors are omitted below unless they do something meaningful for gameplay code.</li>
                    </ul>
                </div>
            </section>

            <section>
                <h2 id="ui-api">UI API</h2>

                <article class="component-entry">
                    <h3 id="api-canvas">Canis::Canvas</h3>
                    <p>Canvas is the root UI surface.</p>
                    <h4>Public variables</h4>
                    <ul class="component-meta">
                        <li><code>active</code>: enables or disables the canvas.</li>
                        <li><code>renderMode</code>: chooses overlay, camera-space, or world-space UI.</li>
                        <li><code>scaleMode</code>: controls how overlay UI scales against the reference resolution.</li>
                        <li><code>screenSize</code>: reference size used by scaling logic, usually something like <code>1280x800</code>.</li>
                    </ul>
                    <h4>Public methods</h4>
                    <ul class="component-meta">
                        <li><code>Create()</code>: lifecycle hook; currently empty.</li>
                    </ul>
                    <h4>Game-side example</h4>
                    <pre><code>void HudBootstrap::Create()
{
    auto &amp;canvas = entity.GetComponent&lt;Canis::Canvas&gt;();
    canvas.active = true;
    canvas.renderMode = Canis::CanvasRenderMode::SCREEN_SPACE_OVERLAY;
    canvas.scaleMode = Canis::CanvasScaleMode::SCALE_WITH_SCREEN_WIDTH;
    canvas.screenSize = Canis::Vector2(1280.0f, 800.0f);
}</code></pre>
                </article>

                <article class="component-entry">
                    <h3 id="api-recttransform">Canis::RectTransform</h3>
                    <p>RectTransform is the 2D/UI transform with anchoring and hierarchy support.</p>
                    <h4>Public variables</h4>
                    <ul class="component-meta">
                        <li><code>active</code>: enables or disables the rect in UI layout.</li>
                        <li><code>position</code>: local anchored position in UI space.</li>
                        <li><code>size</code>: local size before inherited scale.</li>
                        <li><code>scale</code>: local 2D scale.</li>
                        <li><code>anchorMin</code> / <code>anchorMax</code>: normalized anchor range inside the parent or canvas.</li>
                        <li><code>pivot</code>: normalized pivot used for placement and rotation.</li>
                        <li><code>originOffset</code>: extra offset applied before final placement.</li>
                        <li><code>depth</code>: local UI depth sorting offset.</li>
                        <li><code>rotation</code>: local UI rotation in radians.</li>
                        <li><code>rotationOriginOffset</code>: extra pivot offset for rotation.</li>
                        <li><code>parent</code>: parent UI entity, if any.</li>
                        <li><code>children</code>: child UI entities.</li>
                    </ul>
                    <h4>Public methods</h4>
                    <ul class="component-meta">
                        <li><code>GetPosition()</code> / <code>SetPosition(...)</code>: read or write global UI position.</li>
                        <li><code>GetResolvedSize()</code>: final size after anchoring.</li>
                        <li><code>GetRectMin()</code>: minimum corner of the rect.</li>
                        <li><code>GetCanvasRenderMode()</code>: returns the current canvas mode.</li>
                        <li><code>IsActiveInHierarchy()</code>: true if this rect and its parents are active.</li>
                        <li><code>SetAnchorPreset(...)</code> / <code>GetAnchorPreset()</code>: use anchor presets like <code>TOPRIGHT</code>.</li>
                        <li><code>Move(...)</code>: adds a delta to <code>position</code>.</li>
                        <li><code>GetRotation()</code> / <code>GetDepth()</code>: inherited rotation/depth in hierarchy.</li>
                        <li><code>SetDepth(...)</code>: writes a global depth-like value back into local space.</li>
                        <li><code>GetScale()</code> / <code>SetScale(...)</code>: inherited/global UI scale helpers.</li>
                        <li><code>GetRight()</code> / <code>GetUp()</code>: rotated UI basis vectors.</li>
                        <li><code>HasParent()</code>, <code>SetParentAtIndex(...)</code>, <code>SetParent(...)</code>, <code>Unparent()</code>, <code>IsChildOf(...)</code>: hierarchy management.</li>
                        <li><code>HasChildren()</code>, <code>AddChild(...)</code>, <code>RemoveChild(...)</code>, <code>RemoveAllChildren()</code>: child management.</li>
                        <li><code>GetAnchor(...)</code>: static helper that resolves a preset anchor against window size.</li>
                    </ul>
                    <h4>Game-side example</h4>
                    <pre><code>void ScoreHud::Create()
{
    auto &amp;rect = entity.GetComponent&lt;Canis::RectTransform&gt;();
    rect.SetAnchorPreset(Canis::TOPRIGHT);
    rect.position = Canis::Vector2(-32.0f, -24.0f);
    rect.size = Canis::Vector2(220.0f, 48.0f);
}</code></pre>
                </article>

                <article class="component-entry">
                    <h3 id="api-sprite2d">Canis::Sprite2D</h3>
                    <p>Sprite2D draws a textured quad in the 2D/UI renderer.</p>
                    <h4>Public variables</h4>
                    <ul class="component-meta">
                        <li><code>textureHandle</code>: the texture to draw.</li>
                        <li><code>color</code>: per-instance tint and alpha.</li>
                        <li><code>uv</code>: UV origin and size inside the texture.</li>
                        <li><code>flipX</code> / <code>flipY</code>: whether the sprite should mirror horizontally or vertically.</li>
                    </ul>
                    <h4>Public methods</h4>
                    <ul class="component-meta">
                        <li><code>Create()</code>: lifecycle hook; currently empty.</li>
                        <li><code>GetSpriteFromTextureAtlas(...)</code>: computes UVs for a frame inside a texture atlas.</li>
                    </ul>
                    <h4>Game-side example</h4>
                    <pre><code>void LivesHud::SetDimmed(Canis::Entity &amp;icon, bool dimmed)
{
    auto &amp;sprite = icon.GetComponent&lt;Canis::Sprite2D&gt;();
    sprite.color.a = dimmed ? 0.25f : 1.0f;
}</code></pre>
                </article>

                <article class="component-entry">
                    <h3 id="api-text">Canis::Text</h3>
                    <p>Text renders a font asset and tracks when the displayed string needs rebuilding.</p>
                    <h4>Public variables</h4>
                    <ul class="component-meta">
                        <li><code>assetId</code>: loaded font asset id.</li>
                        <li><code>text</code>: current displayed string.</li>
                        <li><code>color</code>: text tint.</li>
                        <li><code>alignment</code>: left, right, or center alignment.</li>
                        <li><code>horizontalBoundary</code>: overflow or wrap behavior.</li>
                        <li><code>_status</code>: runtime dirty flags; usually engine-managed.</li>
                    </ul>
                    <h4>Public methods</h4>
                    <ul class="component-meta">
                        <li><code>Create()</code>: lifecycle hook; currently empty.</li>
                        <li><code>SetText(...)</code>: updates <code>text</code> and marks the text as dirty.</li>
                    </ul>
                    <h4>Game-side example</h4>
                    <pre><code>void MatchHud::SetTimer(Canis::Entity &amp;timerLabel, const std::string &amp;value)
{
    timerLabel.GetComponent&lt;Canis::Text&gt;().SetText(value);
}</code></pre>
                </article>

                <article class="component-entry">
                    <h3 id="api-uibutton">Canis::UIButton</h3>
                    <p>UIButton routes clicks into a registered UI action and manages hover/pressed visual state.</p>
                    <h4>Public variables</h4>
                    <ul class="component-meta">
                        <li><code>active</code>: enables or disables the button interaction.</li>
                        <li><code>targetEntity</code>: entity that should receive the action.</li>
                        <li><code>targetScript</code>: optional script name to target explicitly.</li>
                        <li><code>actionName</code>: UI action name registered on the script.</li>
                        <li><code>baseColor</code>, <code>hoverColor</code>, <code>pressedColor</code>: visual tint states.</li>
                        <li><code>baseScale</code>, <code>hoverScale</code>, <code>pressedScale</code>: visual scale states.</li>
                        <li><code>hovered</code>, <code>pressed</code>, <code>baseValuesSaved</code>: runtime UI state, usually engine-managed.</li>
                    </ul>
                    <h4>Public methods</h4>
                    <ul class="component-meta">
                        <li><code>Create()</code>: lifecycle hook; currently empty.</li>
                    </ul>
                    <h4>Game-side example</h4>
                    <pre><code>// registration side
Canis::RegisterUIAction&lt;BathBattle::BathBattleStartMenu&gt;(conf, "Host", &amp;BathBattle::BathBattleStartMenu::Host);

// script side
void BathBattle::BathBattleStartMenu::Host(const Canis::UIActionContext &amp;)
{
    entity.scene.app-&gt;GetNetwork().Host();
}</code></pre>
                </article>

                <article class="component-entry">
                    <h3 id="api-uiinputfield">Canis::UIInputField</h3>
                    <p>UIInputField owns editable text, optional binding to a script property, and runtime focus/caret state.</p>
                    <h4>Public variables</h4>
                    <ul class="component-meta">
                        <li><code>active</code>: enables or disables interaction.</li>
                        <li><code>displayEntity</code>: entity that visually shows the text, usually a <code>Text</code> entity.</li>
                        <li><code>targetEntity</code>: entity whose script property should be updated.</li>
                        <li><code>targetScript</code>: script name containing the target property.</li>
                        <li><code>targetProperty</code>: registered property name to push text into.</li>
                        <li><code>text</code>: current input text.</li>
                        <li><code>placeholder</code>: text to show when empty.</li>
                        <li><code>allowedCharacters</code>: whitelist string; empty means unrestricted.</li>
                        <li><code>maxLength</code>: maximum characters allowed.</li>
                        <li><code>baseColor</code>, <code>hoverColor</code>, <code>focusedColor</code>: background tint states.</li>
                        <li><code>baseTextColor</code>, <code>textColor</code>, <code>placeholderColor</code>: text tint states.</li>
                        <li><code>hovered</code>, <code>focused</code>, <code>baseValuesSaved</code>, <code>caretBlinkTimer</code>, <code>caretVisible</code>: runtime interaction state.</li>
                    </ul>
                    <h4>Public methods</h4>
                    <ul class="component-meta">
                        <li><code>Create()</code>: lifecycle hook; currently empty.</li>
                    </ul>
                    <h4>Game-side example</h4>
                    <pre><code>std::string playerName = "Player";
REGISTER_PROPERTY(conf, BathBattle::BathBattleStartMenu, playerName);

// In the scene, UIInputField.targetProperty points at "playerName".
// The UI system pushes typed text into that registered property.</code></pre>
                </article>

                <article class="component-entry">
                    <h3 id="api-uidragsource">Canis::UIDragSource</h3>
                    <p>UIDragSource turns a UI entity into something draggable with a typed payload.</p>
                    <h4>Public variables</h4>
                    <ul class="component-meta">
                        <li><code>active</code>: enables dragging.</li>
                        <li><code>payloadType</code>: high-level type name like <code>"item"</code> or <code>"duck-card"</code>.</li>
                        <li><code>payloadValue</code>: the concrete id/value sent with the drag.</li>
                        <li><code>dragging</code>, <code>dragOffset</code>, <code>originalPosition</code>, <code>originalDepth</code>: runtime drag state, usually engine-managed.</li>
                    </ul>
                    <h4>Public methods</h4>
                    <ul class="component-meta">
                        <li><code>Create()</code>: lifecycle hook; currently empty.</li>
                    </ul>
                    <h4>Game-side example</h4>
                    <pre><code>void CardSetup::Create()
{
    auto &amp;drag = entity.GetComponent&lt;Canis::UIDragSource&gt;();
    drag.payloadType = "duck-card";
    drag.payloadValue = "blue";
}</code></pre>
                </article>

                <article class="component-entry">
                    <h3 id="api-uidroptarget">Canis::UIDropTarget</h3>
                    <p>UIDropTarget accepts drops and dispatches a UI action with payload data inside the <code>UIActionContext</code>.</p>
                    <h4>Public variables</h4>
                    <ul class="component-meta">
                        <li><code>active</code>: enables drop handling.</li>
                        <li><code>targetEntity</code>: entity that receives the action.</li>
                        <li><code>targetScript</code>: optional specific script to call.</li>
                        <li><code>actionName</code>: registered UI action name.</li>
                        <li><code>acceptedPayloadType</code>: allowed payload type; empty accepts anything.</li>
                        <li><code>baseColor</code>, <code>hoverColor</code>: idle and hovered tint states.</li>
                        <li><code>hovered</code>: runtime hover state.</li>
                    </ul>
                    <h4>Public methods</h4>
                    <ul class="component-meta">
                        <li><code>Create()</code>: lifecycle hook; currently empty.</li>
                    </ul>
                    <h4>Game-side example</h4>
                    <pre><code>Canis::RegisterUIAction&lt;LobbyController&gt;(conf, "HandleDuckDrop", &amp;LobbyController::HandleDuckDrop);

void LobbyController::HandleDuckDrop(const Canis::UIActionContext &amp;context)
{
    if (context.payloadType == "duck-card")
        AssignDuckCard(context.payloadValue);
}</code></pre>
                </article>
            </section>

            <section>
                <h2 id="world-api">World &amp; rendering API</h2>

                <article class="component-entry">
                    <h3 id="api-transform">Canis::Transform</h3>
                    <p>Transform is the main 3D hierarchy component. Rotations are stored in radians.</p>
                    <h4>Public variables</h4>
                    <ul class="component-meta">
                        <li><code>active</code>: whether the transform should be considered active.</li>
                        <li><code>position</code>: local 3D position.</li>
                        <li><code>rotation</code>: local Euler rotation in radians.</li>
                        <li><code>scale</code>: local non-uniform scale.</li>
                        <li><code>parent</code>: parent transform entity.</li>
                        <li><code>children</code>: child transform entities.</li>
                    </ul>
                    <h4>Public methods</h4>
                    <ul class="component-meta">
                        <li><code>Create()</code>: lifecycle hook; currently empty.</li>
                        <li><code>Destroy()</code>: unparents and clears child links.</li>
                        <li><code>GetLocalMatrix()</code> / <code>GetModelMatrix()</code>: local and world transform matrices.</li>
                        <li><code>GetGlobalPosition()</code>, <code>GetGlobalRotation()</code>, <code>GetGlobalScale()</code>: inherited world-space values.</li>
                        <li><code>GetForward()</code>, <code>GetUp()</code>, <code>GetRight()</code>: world-space basis vectors.</li>
                        <li><code>HasParent()</code>, <code>SetParentAtIndex(...)</code>, <code>SetParent(...)</code>, <code>Unparent()</code>, <code>IsChildOf(...)</code>: hierarchy helpers.</li>
                        <li><code>HasChildren()</code>, <code>AddChild(...)</code>, <code>RemoveChild(...)</code>, <code>RemoveAllChildren()</code>: child management.</li>
                    </ul>
                    <h4>Game-side example</h4>
                    <pre><code>void BathBattle::BathBattleSpinner::Update(float dt)
{
    entity.GetComponent&lt;Canis::Transform&gt;().rotation +=
        glm::normalize(axis) * (rotationSpeedDegrees * Canis::DEG2RAD * dt);
}</code></pre>
                </article>

                <article class="component-entry">
                    <h3 id="api-prefabinstance">Canis::PrefabInstance</h3>
                    <p>PrefabInstance links a live scene entity back to the prefab scene asset it came from.</p>
                    <h4>Public variables</h4>
                    <ul class="component-meta">
                        <li><code>prefab</code>: source prefab scene asset handle.</li>
                        <li><code>firstEntity</code>: first spawned/root entity for this instance.</li>
                    </ul>
                    <h4>Public methods</h4>
                    <ul class="component-meta">
                        <li><code>Create()</code>: lifecycle hook; currently empty.</li>
                    </ul>
                    <h4>Game-side example</h4>
                    <pre><code>if (entity.HasComponent&lt;Canis::PrefabInstance&gt;())
{
    auto &amp;instance = entity.GetComponent&lt;Canis::PrefabInstance&gt;();
    if (instance.firstEntity != nullptr)
        instance.firstEntity-&gt;tag = "RuntimeSpawned";
}</code></pre>
                </article>

                <article class="component-entry">
                    <h3 id="api-camera">Canis::Camera</h3>
                    <p>Camera defines a 3D view frustum.</p>
                    <h4>Public variables</h4>
                    <ul class="component-meta">
                        <li><code>primary</code>: whether this is the main active camera.</li>
                        <li><code>fovDegrees</code>: vertical field of view in degrees.</li>
                        <li><code>nearClip</code> / <code>farClip</code>: visible depth range.</li>
                    </ul>
                    <h4>Public methods</h4>
                    <ul class="component-meta">
                        <li><code>Create()</code>: lifecycle hook; currently empty.</li>
                    </ul>
                    <h4>Game-side example</h4>
                    <pre><code>void ChaseCamera::Create()
{
    auto &amp;camera = entity.GetComponent&lt;Canis::Camera&gt;();
    camera.primary = true;
    camera.fovDegrees = 60.0f;
    camera.nearClip = 0.1f;
    camera.farClip = 500.0f;
}</code></pre>
                </article>

                <article class="component-entry">
                    <h3 id="api-directionallight">Canis::DirectionalLight</h3>
                    <p>DirectionalLight is the scene-wide sun/fill light.</p>
                    <h4>Public variables</h4>
                    <ul class="component-meta">
                        <li><code>enabled</code>: whether the light contributes.</li>
                        <li><code>color</code>: light tint.</li>
                        <li><code>intensity</code>: brightness multiplier.</li>
                        <li><code>direction</code>: normalized world direction the light points toward.</li>
                    </ul>
                    <h4>Public methods</h4>
                    <ul class="component-meta">
                        <li><code>Create()</code>: lifecycle hook; currently empty.</li>
                    </ul>
                    <h4>Game-side example</h4>
                    <pre><code>void DayNightRig::Update(float dt)
{
    auto &amp;sun = entity.GetComponent&lt;Canis::DirectionalLight&gt;();
    sun.intensity = glm::mix(0.3f, 1.1f, daylight);
    sun.direction = Canis::Vector3(-0.4f, -1.0f, -0.25f);
}</code></pre>
                </article>

                <article class="component-entry">
                    <h3 id="api-pointlight">Canis::PointLight</h3>
                    <p>PointLight is a local light centered on the entity transform.</p>
                    <h4>Public variables</h4>
                    <ul class="component-meta">
                        <li><code>enabled</code>: whether the light contributes.</li>
                        <li><code>color</code>: light tint.</li>
                        <li><code>intensity</code>: brightness multiplier.</li>
                        <li><code>range</code>: radius of influence.</li>
                    </ul>
                    <h4>Public methods</h4>
                    <ul class="component-meta">
                        <li><code>Create()</code>: lifecycle hook; currently empty.</li>
                    </ul>
                    <h4>Game-side example</h4>
                    <pre><code>void PickupGlow::Create()
{
    auto &amp;light = entity.GetComponent&lt;Canis::PointLight&gt;();
    light.color = Canis::Color(1.0f, 0.85f, 0.95f, 1.0f);
    light.intensity = 1.4f;
    light.range = 6.0f;
}</code></pre>
                </article>

                <article class="component-entry">
                    <h3 id="api-model">Canis::Model</h3>
                    <p>Model renders a 3D asset, optionally down to a single imported node.</p>
                    <h4>Public variables</h4>
                    <ul class="component-meta">
                        <li><code>modelId</code>: loaded model asset id.</li>
                        <li><code>nodeIndex</code>: optional imported sub-node index; <code>-1</code> means the whole model.</li>
                        <li><code>applyNodeTransform</code>: whether the imported node transform should be applied.</li>
                        <li><code>color</code>: per-instance tint.</li>
                    </ul>
                    <h4>Public methods</h4>
                    <ul class="component-meta">
                        <li><code>Create()</code>: lifecycle hook; currently empty.</li>
                    </ul>
                    <h4>Game-side example</h4>
                    <pre><code>void MatchSetup::TintPlayerVisual(Canis::Entity &amp;visual, const Canis::Color &amp;slotColor)
{
    visual.GetComponent&lt;Canis::Model&gt;().color = slotColor;
}</code></pre>
                </article>

                <article class="component-entry">
                    <h3 id="api-material">Canis::Material</h3>
                    <p>Material overrides the material/shader used by a renderer and carries custom uniform overrides.</p>
                    <h4>Public variables</h4>
                    <ul class="component-meta">
                        <li><code>materialId</code>: primary material asset id.</li>
                        <li><code>materialIds</code>: optional per-slot/submesh overrides.</li>
                        <li><code>color</code>: instance tint and alpha.</li>
                        <li><code>materialFields</code>: custom shader uniforms. This is where helpers like <code>SetFloat</code>, <code>SetColor</code>, and <code>SetTexture</code> live.</li>
                    </ul>
                    <h4>Public methods</h4>
                    <ul class="component-meta">
                        <li><code>Create()</code>: lifecycle hook; currently empty.</li>
                    </ul>
                    <h4>Game-side example</h4>
                    <pre><code>void FireCubeAnimator::Update(float dt)
{
    auto &amp;material = entity.GetComponent&lt;Canis::Material&gt;();
    material.color.a = 0.9f;
    material.materialFields.SetFloat("firePulse", pulse);
}</code></pre>
                </article>
            </section>

            <section>
                <h2 id="physics-api">Physics &amp; collision API</h2>

                <article class="component-entry">
                    <h3 id="api-rigidbody">Canis::Rigidbody</h3>
                    <p>Rigidbody provides 3D physics simulation and force accumulation.</p>
                    <h4>Public variables</h4>
                    <ul class="component-meta">
                        <li><code>active</code>: whether the rigidbody is simulated.</li>
                        <li><code>motionType</code>: <code>STATIC</code>, <code>KINEMATIC</code>, or <code>DYNAMIC</code>.</li>
                        <li><code>mass</code>, <code>friction</code>, <code>restitution</code>: core physical response settings.</li>
                        <li><code>linearDamping</code>, <code>angularDamping</code>: drag-like damping.</li>
                        <li><code>useGravity</code>, <code>gravityFactor</code>: gravity control.</li>
                        <li><code>isSensor</code>: sensor bodies detect overlaps without normal solid response.</li>
                        <li><code>layer</code>, <code>mask</code>: collision filtering.</li>
                        <li><code>allowSleeping</code>: whether physics can sleep the body.</li>
                        <li><code>lockRotationX</code>, <code>lockRotationY</code>, <code>lockRotationZ</code>: axis locks.</li>
                        <li><code>linearVelocity</code>, <code>angularVelocity</code>: current velocities.</li>
                        <li><code>pendingForce</code>, <code>pendingAcceleration</code>, <code>pendingImpulse</code>, <code>pendingVelocityChange</code>: runtime force queues used by <code>AddForce(...)</code>.</li>
                    </ul>
                    <h4>Public methods</h4>
                    <ul class="component-meta">
                        <li><code>Create()</code>: lifecycle hook; currently empty.</li>
                        <li><code>AddForce(...)</code>: queues a force/impulse/velocity change for dynamic bodies.</li>
                    </ul>
                    <h4>Game-side example</h4>
                    <pre><code>void DashPad::Launch(Canis::Entity &amp;player)
{
    auto &amp;body = player.GetComponent&lt;Canis::Rigidbody&gt;();
    body.AddForce(Canis::Vector3(0.0f, 0.0f, 18.0f), Canis::Rigidbody3DForceMode::IMPULSE);
}</code></pre>
                </article>

                <article class="component-entry">
                    <h3 id="api-boxcollider">Canis::BoxCollider</h3>
                    <p>BoxCollider is the general-purpose rectangular collision shape.</p>
                    <h4>Public variables</h4>
                    <ul class="component-meta">
                        <li><code>active</code>: whether the collider participates.</li>
                        <li><code>size</code>: box size in local space.</li>
                        <li><code>entered</code>, <code>exited</code>, <code>stayed</code>: runtime overlap lists populated by the physics system.</li>
                    </ul>
                    <h4>Public methods</h4>
                    <ul class="component-meta">
                        <li><code>Create()</code>: lifecycle hook; currently empty.</li>
                    </ul>
                    <h4>Game-side example</h4>
                    <pre><code>void TriggerVolume::Update(float dt)
{
    auto &amp;box = entity.GetComponent&lt;Canis::BoxCollider&gt;();
    if (!box.entered.empty())
        OpenDoor();
}</code></pre>
                </article>

                <article class="component-entry">
                    <h3 id="api-spherecollider">Canis::SphereCollider</h3>
                    <p>SphereCollider is a good fit for rolling bodies and simple round triggers.</p>
                    <h4>Public variables</h4>
                    <ul class="component-meta">
                        <li><code>active</code>: whether the collider participates.</li>
                        <li><code>radius</code>: sphere radius.</li>
                        <li><code>entered</code>, <code>exited</code>, <code>stayed</code>: runtime overlap lists populated by the physics system.</li>
                    </ul>
                    <h4>Public methods</h4>
                    <ul class="component-meta">
                        <li><code>Create()</code>: lifecycle hook; currently empty.</li>
                    </ul>
                    <h4>Game-side example</h4>
                    <pre><code>void DuckSetup::Create()
{
    auto &amp;sphere = entity.GetComponent&lt;Canis::SphereCollider&gt;();
    sphere.radius = 0.5f;
}</code></pre>
                </article>

                <article class="component-entry">
                    <h3 id="api-capsulecollider">Canis::CapsuleCollider</h3>
                    <p>CapsuleCollider is useful for upright round-topped collision.</p>
                    <h4>Public variables</h4>
                    <ul class="component-meta">
                        <li><code>active</code>: whether the collider participates.</li>
                        <li><code>halfHeight</code>: half the cylinder height between the caps.</li>
                        <li><code>radius</code>: capsule radius.</li>
                        <li><code>entered</code>, <code>exited</code>, <code>stayed</code>: runtime overlap lists populated by the physics system.</li>
                    </ul>
                    <h4>Public methods</h4>
                    <ul class="component-meta">
                        <li><code>Create()</code>: lifecycle hook; currently empty.</li>
                    </ul>
                    <h4>Game-side example</h4>
                    <pre><code>void BumperSetup::Create()
{
    auto &amp;capsule = entity.GetComponent&lt;Canis::CapsuleCollider&gt;();
    capsule.halfHeight = 0.5f;
    capsule.radius = 0.35f;
}</code></pre>
                </article>

                <article class="component-entry">
                    <h3 id="api-meshcollider">Canis::MeshCollider</h3>
                    <p>MeshCollider uses model geometry instead of a primitive collision shape.</p>
                    <h4>Public variables</h4>
                    <ul class="component-meta">
                        <li><code>active</code>: whether the collider participates.</li>
                        <li><code>useAttachedModel</code>: use the same model as the attached <code>Model</code> component.</li>
                        <li><code>modelId</code>: explicit model id used internally when not using the attached model.</li>
                        <li><code>modelPath</code>: explicit model path to build collision from.</li>
                        <li><code>entered</code>, <code>exited</code>, <code>stayed</code>: runtime overlap lists.</li>
                    </ul>
                    <h4>Public methods</h4>
                    <ul class="component-meta">
                        <li><code>Create()</code>: lifecycle hook; currently empty.</li>
                    </ul>
                    <h4>Game-side example</h4>
                    <pre><code>void TubShellSetup::Create()
{
    auto &amp;collider = entity.GetComponent&lt;Canis::MeshCollider&gt;();
    collider.useAttachedModel = true;
}</code></pre>
                </article>
            </section>

            <section>
                <h2 id="animation-api">Animation &amp; 2D runtime API</h2>

                <article class="component-entry">
                    <h3 id="api-modelanimation">Canis::ModelAnimation</h3>
                    <p>ModelAnimation plays animation data embedded in a 3D model asset.</p>
                    <h4>Public variables</h4>
                    <ul class="component-meta">
                        <li><code>playAnimation</code>: whether playback is active.</li>
                        <li><code>loop</code>: whether playback loops.</li>
                        <li><code>animationSpeed</code>: time scale multiplier.</li>
                        <li><code>animationTime</code>: current playback time.</li>
                        <li><code>animationIndex</code>: current animation clip index inside the model asset.</li>
                        <li><code>poseModelId</code>, <code>pose</code>, <code>poseInitialized</code>, <code>lastEvaluatedAnimationIndex</code>, <code>lastEvaluatedAnimationTime</code>: runtime caching fields maintained by the animation system.</li>
                    </ul>
                    <h4>Public methods</h4>
                    <ul class="component-meta">
                        <li><code>Create()</code>: lifecycle hook; currently empty.</li>
                    </ul>
                    <h4>Game-side example</h4>
                    <pre><code>void FanController::Create()
{
    auto &amp;anim = entity.GetComponent&lt;Canis::ModelAnimation&gt;();
    anim.playAnimation = true;
    anim.loop = true;
    anim.animationSpeed = 1.25f;
    anim.animationIndex = 0;
}</code></pre>
                </article>

                <article class="component-entry">
                    <h3 id="api-animationplayer">Canis::AnimationPlayer</h3>
                    <p>AnimationPlayer is the generic property animation component for <code>.animclip</code> assets.</p>
                    <h4>Public variables</h4>
                    <ul class="component-meta">
                        <li><code>clip</code>: animation clip asset handle.</li>
                        <li><code>playing</code>: whether the clip is currently playing.</li>
                        <li><code>loop</code>: whether the clip loops.</li>
                        <li><code>speed</code>: playback speed multiplier.</li>
                        <li><code>time</code>: current clip time.</li>
                        <li><code>lastEventSampleTime</code>, <code>lastEventSampleValid</code>, <code>lastEventClipPath</code>: runtime event bookkeeping used by the engine so clip events only fire once when time crosses them.</li>
                    </ul>
                    <h4>Public methods</h4>
                    <ul class="component-meta">
                        <li><code>Create()</code>: lifecycle hook; currently empty.</li>
                    </ul>
                    <h4>Game-side example</h4>
                    <pre><code>void ShowcaseController::Create()
{
    auto &amp;player = entity.GetComponent&lt;Canis::AnimationPlayer&gt;();
    player.playing = true;
    player.loop = true;
    player.speed = 1.0f;
}</code></pre>
                </article>

                <article class="component-entry">
                    <h3 id="api-animator">Canis::Animator</h3>
                    <p>Animator is the first-pass controller/state-machine component layered above <code>AnimationPlayer</code>-style clips.</p>
                    <h4>Public variables</h4>
                    <ul class="component-meta">
                        <li><code>controller</code>: the <code>.animator</code> asset handle.</li>
                        <li><code>playing</code>: whether the current state should advance in time.</li>
                        <li><code>currentState</code>: runtime active state name selected by the controller.</li>
                        <li><code>time</code>: runtime local time inside the active state's clip.</li>
                        <li><code>parameters</code>: runtime parameter values and trigger state. Usually edited through helper methods or the inspector rather than by mutating the array directly.</li>
                        <li><code>parametersInitialized</code>, <code>lastEventSampleTime</code>, <code>lastEventSampleValid</code>, <code>lastEventClipPath</code>: runtime bookkeeping owned by the animator system.</li>
                    </ul>
                    <h4>Public methods</h4>
                    <ul class="component-meta">
                        <li><code>Create()</code>: lifecycle hook; currently empty.</li>
                        <li><code>GetParameter(name)</code>: returns a runtime parameter entry by name if present.</li>
                        <li><code>SetFloat(name, value)</code>, <code>SetInt(name, value)</code>, <code>SetBool(name, value)</code>: change controller parameters from gameplay code.</li>
                        <li><code>SetTrigger(name)</code>: arm a trigger parameter so a transition condition can consume it.</li>
                        <li><code>GetFloat(...)</code>, <code>GetInt(...)</code>, <code>GetBool(...)</code>: read back current runtime parameter values.</li>
                    </ul>
                    <h4>Game-side example</h4>
                    <pre><code>void DuckStateDriver::Update(float dt)
{
    auto &amp;animator = entity.GetComponent&lt;Canis::Animator&gt;();
    animator.SetFloat("speed", currentSpeed);
    animator.SetBool("grounded", isGrounded);

    if (justBumped)
        animator.SetTrigger("bump");
}</code></pre>
                </article>

                <article class="component-entry">
                    <h3 id="api-camera2d">Canis::Camera2D</h3>
                    <p>Camera2D manages a 2D camera matrix and zoom/position state.</p>
                    <h4>Public variables</h4>
                    <ul class="component-meta">
                        <li>This component does not expose authored gameplay fields beyond the shared <code>entity</code> back-pointer.</li>
                    </ul>
                    <h4>Public methods</h4>
                    <ul class="component-meta">
                        <li><code>Create()</code>, <code>Destroy()</code>, <code>Update(...)</code>: lifecycle methods for the 2D camera.</li>
                        <li><code>SetPosition(...)</code> / <code>GetPosition()</code>: move or read the 2D camera center.</li>
                        <li><code>SetScale(...)</code> / <code>GetScale()</code>: control or read zoom.</li>
                        <li><code>GetCameraMatrix()</code>, <code>GetViewMatrix()</code>, <code>GetProjectionMatrix()</code>: matrices used by the renderer.</li>
                        <li><code>UpdateMatrix()</code>: rebuilds internal matrices after state changes.</li>
                    </ul>
                    <h4>Game-side example</h4>
                    <pre><code>void TopDownCameraRig::Update(float dt)
{
    auto &amp;camera = entity.GetComponent&lt;Canis::Camera2D&gt;();
    camera.SetPosition(playerPosition2D);
    camera.SetScale(2.0f);
}</code></pre>
                </article>

                <article class="component-entry">
                    <h3 id="api-spriteanimation">Canis::SpriteAnimation</h3>
                    <p>SpriteAnimation flips through frames in a sprite animation asset and updates Sprite2D UVs.</p>
                    <h4>Public variables</h4>
                    <ul class="component-meta">
                        <li><code>id</code>: sprite animation asset id.</li>
                        <li><code>speed</code>: playback speed multiplier.</li>
                        <li><code>countDown</code>, <code>index</code>, <code>redraw</code>: runtime frame state, usually engine-managed.</li>
                    </ul>
                    <h4>Public methods</h4>
                    <ul class="component-meta">
                        <li><code>Create()</code>, <code>Destroy()</code>, <code>Update(...)</code>: lifecycle hooks.</li>
                        <li><code>Play(path)</code>: load and start a sprite animation asset.</li>
                        <li><code>Pause()</code>: pause by setting speed to zero.</li>
                    </ul>
                    <h4>Game-side example</h4>
                    <pre><code>void CoinVfx::Create()
{
    auto &amp;anim = entity.GetComponent&lt;Canis::SpriteAnimation&gt;();
    anim.Play("assets/animations/ui/coin_spin.spriteanim");
    anim.speed = 1.0f;
}</code></pre>
                </article>
            </section>

            <section>
                <h2 id="network-api">Networking API</h2>

                <article class="component-entry">
                    <h3 id="api-networkidentity">Canis::NetworkIdentity</h3>
                    <p>NetworkIdentity marks an entity as a replicated/spawnable network object.</p>
                    <h4>Public variables</h4>
                    <ul class="component-meta">
                        <li><code>netId</code>: unique network object id.</li>
                        <li><code>ownerClientId</code>: owning client id.</li>
                        <li><code>serverOwned</code>: true if the server/host owns this object.</li>
                        <li><code>localOwned</code>: true if the local process owns this object.</li>
                        <li><code>replicateTransform</code>: whether transform replication is active.</li>
                        <li><code>prefab</code>: prefab used when the network object is spawnable from an asset.</li>
                    </ul>
                    <h4>Public methods</h4>
                    <ul class="component-meta">
                        <li><code>Create()</code>: lifecycle hook; currently empty.</li>
                    </ul>
                    <h4>Game-side example</h4>
                    <pre><code>void BoatController::Create()
{
    if (entity.HasComponent&lt;Canis::NetworkIdentity&gt;())
    {
        ownerClientId = entity.GetComponent&lt;Canis::NetworkIdentity&gt;().ownerClientId;
    }
}</code></pre>
                </article>
            </section>
        `
    },
    {
        slug: "editor-workflow",
        section: "Editor",
        title: "Editor Workflow",
        summary: "How the current editor is organized: Scene, Game, Hierarchy, Inspector, Assets, Scripts, ShaderGraph, and Animation.",
        kicker: "Editor",
        chips: ["Hierarchy", "Inspector", "ShaderGraph", "Animation"],
        keywords: ["editor", "hierarchy", "inspector", "shadergraph", "animation", "play mode", "layout"],
        content: `
            <section>
                <h2 id="main-panels">Main panels</h2>
                <ul>
                    <li><strong>Scene</strong>: editor camera, gizmos, selection, and transform editing.</li>
                    <li><strong>Game</strong>: runtime camera output and input viewport.</li>
                    <li><strong>Hierarchy</strong>: scene tree, prefab drops, model hierarchy drops, and parenting.</li>
                    <li><strong>Inspector</strong>: selected entity components, script fields, prefab metadata, and asset handles.</li>
                    <li><strong>Assets</strong>: files, scenes, materials, shader graphs, animation clips, and prefab creation.</li>
                    <li><strong>ShaderGraph</strong>: node-based material authoring.</li>
                    <li><strong>Animation</strong>: clip selection, Add Property, record/preview, sequence timeline, curve editing, and events.</li>
                </ul>
            </section>

            <section>
                <h2 id="common-loop">Common loop</h2>
                <ol>
                    <li>Edit the scene in <strong>Scene</strong> and <strong>Hierarchy</strong>.</li>
                    <li>Tune values in <strong>Inspector</strong>.</li>
                    <li>Hit <strong>Save</strong>.</li>
                    <li>Hit <strong>Play</strong> for in-editor runtime.</li>
                    <li>Hit <strong>Stop</strong> to restore the saved scene state.</li>
                </ol>
            </section>

            <section>
                <h2 id="quality-of-life-features">Quality-of-life features</h2>
                <div class="doc-grid">
                    <article class="doc-card">
                        <h3>Double-click focus</h3>
                        <p>Double-clicking a hierarchy entry with a transform frames it in the scene view.</p>
                    </article>
                    <article class="doc-card">
                        <h3>Scene selection sync</h3>
                        <p>Selecting a 3D object in the scene view reveals and focuses it in the hierarchy.</p>
                    </article>
                    <article class="doc-card">
                        <h3>Prefab workflows</h3>
                        <p>You can drag entities into asset folders to create prefabs, rebuild instances, and apply overrides back to the prefab asset.</p>
                    </article>
                    <article class="doc-card">
                        <h3>Animation authoring</h3>
                        <p>The Animation window now supports explicit property picking, curve editing, and clip events, and the new Animator window handles controller graphs and transitions.</p>
                    </article>
                </div>
            </section>

            <section>
                <h2 id="animation-window">Animation window</h2>
                <ol>
                    <li>Choose a clip or create a new <code>.animclip</code>.</li>
                    <li>Choose the animation root entity.</li>
                    <li>Use <strong>Add Property</strong> to create tracks explicitly, or turn on <strong>Record</strong> and edit values in the Inspector.</li>
                    <li>Use the sequencer to scrub time and select a track.</li>
                    <li>Use the curve editor for finer shaping and the Events panel for timed callbacks.</li>
                    <li>Assign the clip to <code>AnimationPlayer</code>, or reference it from an <code>.animator</code> controller state.</li>
                </ol>
            </section>

            <section>
                <h2 id="animator-window">Animator window</h2>
                <ol>
                    <li>Select a <code>.animator</code> asset, or select an entity that already has <code>Canis::Animator</code>.</li>
                    <li>Use the graph to move states around, drag links between them, and right click to add, duplicate, or delete states.</li>
                    <li>Drag an <code>.animclip</code> asset onto the graph to create a new state from that clip.</li>
                    <li>Use the right sidebar for parameters, selected-state settings, and transition conditions.</li>
                </ol>
            </section>

            <section>
                <h2 id="handy-scenes">Handy scenes in this repo</h2>
                <ul>
                    <li><code>project/assets/scenes/bath_battle_start.scene</code></li>
                    <li><code>project/assets/scenes/bath_battle_lobby.scene</code></li>
                    <li><code>project/assets/scenes/bath_battle_arena.scene</code></li>
                    <li><code>project/assets/scenes/spin_baton_animation_example.scene</code></li>
                    <li><code>project/assets/scenes/shadergraph_showcase.scene</code></li>
                </ul>
            </section>
        `
    },
    {
        slug: "scenes-and-prefabs",
        section: "Editor",
        title: "Scenes & Prefabs",
        summary: "How scene YAML, prefab scene assets, prefab instances, overrides, and drag-and-drop model hierarchies fit together today.",
        kicker: "Editor",
        chips: ["Scene YAML", "PrefabInstance", "Drag & Drop"],
        keywords: ["scene", "prefab", "prefabinstance", "hierarchy", "drag drop", "model nodes", "apply overrides"],
        content: `
            <section>
                <h2 id="scene-files">Scene files</h2>
                <p>Scenes are YAML files stored under <code>project/assets/scenes/</code>. They contain environment settings and a serialized list of entities with their components and scripts.</p>
            </section>

            <section>
                <h2 id="prefabs-are-scenes">Prefabs are scene assets</h2>
                <p>In the current editor, a prefab is just a scene asset used as an instancing source. That keeps the serialization model simple and makes prefab contents inspectable without inventing a new file format.</p>
                <ul>
                    <li>Prefab assets usually live under <code>project/assets/prefabs/</code>.</li>
                    <li>A scene instance carries a <code>Canis::PrefabInstance</code> component that remembers the source prefab and root entity mapping.</li>
                    <li>The inspector can rebuild an instance from its prefab or apply overrides back into the prefab asset.</li>
                </ul>
            </section>

            <section>
                <h2 id="creating-prefabs">Creating prefabs</h2>
                <ol>
                    <li>Build or arrange the entity hierarchy in the scene.</li>
                    <li>Drag the hierarchy item into a folder in the Assets panel.</li>
                    <li>The editor writes a new <code>.scene</code> asset and turns the live entity into a prefab instance.</li>
                </ol>
            </section>

            <section>
                <h2 id="model-hierarchies">Model hierarchies</h2>
                <p>Dropping a model asset into the Hierarchy now builds a real transform hierarchy from the imported model nodes instead of flattening it into one renderer. That means submodels show up as separate game objects and can be edited or turned into prefabs.</p>
            </section>

            <section>
                <h2 id="current-notes">Current notes</h2>
                <div class="doc-callout">
                    <h3>Important</h3>
                    <p>Because prefab instances are scene-based, root transforms and scene-level edits are very explicit. That makes the system easy to reason about, but it also means “apply overrides” should be used intentionally so you do not accidentally bake a one-off scene transform into the shared prefab.</p>
                </div>
            </section>
        `
    },
    {
        slug: "animation",
        section: "Editor",
        title: "Animation",
        summary: "The current property-animation stack: clips, Add Property, curves, events, and the first-pass animator/controller layer.",
        kicker: "Editor",
        chips: ["AnimationPlayer", "Animator", "Events", "Curves"],
        keywords: ["animation", "animationplayer", "animator", "animclip", "animator controller", "record", "preview", "add property", "curve", "event", "public variables", "sequence", "spin baton"],
        content: `
            <section>
                <h2 id="current-animation-model">Current animation model</h2>
                <p>The engine now has two layers:</p>
                <ul>
                    <li><code>Canis::AnimationPlayer</code> for direct clip playback.</li>
                    <li><code>Canis::Animator</code> for state-driven playback through a controller asset.</li>
                </ul>
                <p>An <code>.animclip</code> stores tracks as:</p>
                <pre><code>relative path + component/script name + property name + keyed values over time</code></pre>
                <p>At runtime the clip sampler writes values back through the same registered-property path used by the Inspector, which is what makes Unity-style public-variable animation possible.</p>
            </section>

            <section>
                <h2 id="what-you-can-animate">What you can animate today</h2>
                <ul>
                    <li><code>Transform</code> and <code>RectTransform</code> fields</li>
                    <li>Registered script variables using <code>REGISTER_PROPERTY(...)</code></li>
                    <li>Registered component fields that already go through the property registry</li>
                </ul>
                <p>Supported value types are currently float, int, bool, vec2, vec3, and vec4-style values.</p>
            </section>

            <section>
                <h2 id="registering-animatable-variables">Registering animatable variables</h2>
                <p>Add the variable to your script or component, then register it:</p>
                <pre><code>float wobbleAmount = 0.0f;

REGISTER_PROPERTY(scriptConf, BathBattle::BathBattleSpinner, wobbleAmount);</code></pre>
                <p>That registration gives you three things at once:</p>
                <ul>
                    <li>Inspector UI</li>
                    <li>scene serialization</li>
                    <li>animation binding metadata</li>
                </ul>
            </section>

            <section>
                <h2 id="authoring-a-clip">Authoring a clip</h2>
                <ol>
                    <li>Select the animation root.</li>
                    <li>Choose or create an <code>.animclip</code>.</li>
                    <li>Use <strong>Add Property</strong> to explicitly add a track, or turn on <strong>Record</strong> and edit the value in the Inspector.</li>
                    <li>Scrub the timeline and capture keys.</li>
                    <li>Select a track to edit interpolation, raw keys, and curves.</li>
                    <li>Assign the clip to <code>AnimationPlayer</code> or use it in an animator state.</li>
                </ol>
                <div class="doc-callout">
                    <h3>Add Property vs Record</h3>
                    <p><strong>Add Property</strong> is the cleaner “set up the track first” workflow. <strong>Record</strong> is still useful when you want the editor to auto-key inspector edits while you scrub.</p>
                </div>
            </section>

            <section>
                <h2 id="add-property-picker">Add Property picker</h2>
                <p>The Animation window can now enumerate animatable properties from the selected root hierarchy. The picker walks the root and its children, looks at registered properties, and builds explicit tracks without requiring you to change a value first.</p>
                <p>This is the workflow to use when you want to:</p>
                <ul>
                    <li>set up a clean clip before animating</li>
                    <li>add a child-object property by path</li>
                    <li>avoid accidental keys from inspector edits</li>
                </ul>
            </section>

            <section>
                <h2 id="curve-editing">Curve editing</h2>
                <p>After selecting a track in the sequencer, the lower curve panel shows the keyed values on editable curves.</p>
                <ul>
                    <li>Scalar tracks expose one curve.</li>
                    <li>Vector tracks expose one curve per component.</li>
                    <li><code>bool</code> and <code>int</code> style tracks use discrete/step behavior.</li>
                    <li><code>float</code> and vector tracks use linear interpolation in this first pass.</li>
                </ul>
                <p>The curve view is the better place for shaping motion over time. The raw key list is still useful for exact time/value editing.</p>
            </section>

            <section>
                <h2 id="animation-events">Animation events</h2>
                <p>Clips now support timed events. Each event stores:</p>
                <ul>
                    <li>time</li>
                    <li>target path relative to the animation root</li>
                    <li>optional script name</li>
                    <li>event name</li>
                    <li>string, float, and int payload fields</li>
                </ul>
                <p>Register an event handler on a script with <code>RegisterAnimationEvent(...)</code>:</p>
                <pre><code>Canis::RegisterAnimationEvent&lt;BathBattle::BathBattleSpinner&gt;(
    conf,
    "Pulse",
    &amp;BathBattle::BathBattleSpinner::OnPulse);</code></pre>
                <pre><code>void BathBattleSpinner::OnPulse(const Canis::AnimationEventContext &amp;context)
{
    glowAmount = context.floatPayload;
}</code></pre>
                <p>If <code>script</code> is left blank on the event, the engine tries every registered script on the target entity until one handles the matching event name.</p>
            </section>

            <section>
                <h2 id="animator-controllers">Animator controllers</h2>
                <p>The new <code>.animator</code> asset is the first-pass higher-level state machine. It contains:</p>
                <ul>
                    <li>named parameters: float, int, bool, trigger</li>
                    <li>states, each with a clip, loop flag, speed, and saved graph position</li>
                    <li>transitions with exit time and parameter conditions</li>
                </ul>
                <p>You can create a controller either from the Assets panel or from an animation clip inspector. Then edit it in the dedicated Animator window and assign it to a <code>Canis::Animator</code> component.</p>
                <pre><code>DuckShowcase
  Canis::Animator
    controller: assets/animations/duck_showcase.animator</code></pre>
            </section>

            <section>
                <h2 id="driving-animator-parameters">Driving animator parameters</h2>
                <p>Gameplay code is expected to set controller parameters directly.</p>
                <pre><code>auto &amp;animator = entity.GetComponent&lt;Canis::Animator&gt;();
animator.SetFloat("speed", movementSpeed);
animator.SetBool("grounded", isGrounded);

if (justHitSomething)
    animator.SetTrigger("bump");</code></pre>
                <p>In this first pass, transitions are instant. There is no blend tree or crossfade system yet.</p>
            </section>

            <section>
                <h2 id="how-to-add-a-new-variable">Record-by-edit workflow</h2>
                <p>The older flow still works and is still useful for quick authoring:</p>
                <ol>
                    <li>Select the animation root.</li>
                    <li>Choose or create an <code>.animclip</code>.</li>
                    <li>Turn on <strong>Record</strong>.</li>
                    <li>Move the timeline.</li>
                    <li>Edit the value in the Inspector.</li>
                </ol>
                <p>If the track does not exist yet, the editor creates it automatically when the property changes during recording.</p>
            </section>

            <section>
                <h2 id="spin-baton-example">Spin baton example</h2>
                <p>The scene <code>project/assets/scenes/spin_baton_animation_example.scene</code> is a concrete sample. It uses:</p>
                <ul>
                    <li><code>project/assets/prefabs/spin_baton.scene</code> as the prefab source</li>
                    <li><code>project/assets/animations/spin_baton_showcase.animator</code> as the controller</li>
                    <li>three clips: <code>spin_baton_idle.animclip</code>, <code>spin_baton_example.animclip</code>, and <code>spin_baton_turbo.animclip</code></li>
                    <li>the Animator window graph as the main authoring surface for those states and transitions</li>
                    <li><code>BathBattle::BathBattleSpinner.rotationSpeedDegrees</code> as an animated public variable</li>
                    <li>child transform animation on <code>Poll/Arm</code> and <code>TopCap</code></li>
                    <li>an <code>int</code> controller parameter named <code>mode</code> to switch between Idle, Showcase, and Turbo</li>
                </ul>
            </section>

            <section>
                <h2 id="current-limits">Current limits</h2>
                <p>This is a strong first pass, not the final stack. A few current limits are worth knowing:</p>
                <ul>
                    <li>controller transitions are instant and do not crossfade yet</li>
                    <li>curve editing is one selected track at a time, not a full multi-track curve canvas</li>
                    <li>events are typed by name today instead of chosen from a method browser</li>
                    <li>the state-machine layer is intentionally simple: no blend trees yet</li>
                </ul>
            </section>
        `
    },
    {
        slug: "web-export",
        section: "Build & Ship",
        title: "Web Export",
        summary: "Build the runtime for the browser with Emscripten and serve the generated HTML5 output.",
        kicker: "Build & Ship",
        chips: ["Emscripten", "HTML5", "Itch.io"],
        keywords: ["web", "emscripten", "html5", "itch", "browser", "wasm"],
        sourceHref: "../web-build.md",
        sourceLabel: "Read source markdown",
        content: `
            <section>
                <h2 id="quick-start">Quick start</h2>
                <pre><code>./scripts/build-web.sh

# or
./scripts/build-web.sh web-debug</code></pre>
                <p>The script bootstraps <code>emsdk</code>, configures the web preset, and writes the browser bundle into the matching build folder.</p>
            </section>

            <section>
                <h2 id="output-folders">Output folders</h2>
                <ul>
                    <li><code>build-web-release/web/</code> for <code>web-release</code></li>
                    <li><code>build-web-debug/web/</code> for <code>web-debug</code></li>
                </ul>
                <p>You should see <code>index.html</code>, <code>index.js</code>, <code>index.wasm</code>, and <code>index.data</code>.</p>
            </section>

            <section>
                <h2 id="serve-locally">Serve locally</h2>
                <pre><code>python3 -m http.server 8000 --directory build-web-release/web</code></pre>
            </section>

            <section>
                <h2 id="deployment-notes">Deployment notes</h2>
                <ul>
                    <li>The editor runtime is disabled for web builds.</li>
                    <li>Gameplay code is linked statically instead of hot-loaded.</li>
                    <li>The web path targets WebGL 2 / GLES 3 shader compilation.</li>
                </ul>
            </section>
        `
    }
];

function extractSearchText(html) {
    return String(html)
        .replace(/<[^>]+>/g, " ")
        .replace(/&nbsp;/g, " ")
        .replace(/&amp;/g, "&")
        .replace(/&lt;/g, "<")
        .replace(/&gt;/g, ">")
        .replace(/&#039;/g, "'")
        .replace(/\s+/g, " ")
        .trim();
}

for (const page of PAGES) {
    page.searchText = extractSearchText(page.content);
}

const state = {
    search: "",
    slug: window.location.hash ? window.location.hash.slice(1) : "welcome"
};

const navRoot = document.getElementById("docs-nav");
const pageRoot = document.getElementById("doc-page");
const tocRoot = document.getElementById("toc");
const kickerRoot = document.getElementById("page-kicker");
const sourceLink = document.getElementById("page-source-link");
const searchField = document.getElementById("docs-search");
const menuToggle = document.getElementById("menu-toggle");
const sidebar = document.getElementById("sidebar");

function groupPages(pages) {
    return pages.reduce((groups, page) => {
        if (!groups[page.section]) groups[page.section] = [];
        groups[page.section].push(page);
        return groups;
    }, {});
}

function getPage(slug) {
    return PAGES.find((page) => page.slug === slug) || PAGES[0];
}

function getFilteredPages() {
    const query = state.search.trim().toLowerCase();
    if (!query) return PAGES;

    return PAGES.filter((page) => {
        const haystack = [
            page.title,
            page.section,
            page.summary,
            page.kicker,
            ...(page.keywords || []),
            page.searchText || ""
        ].join(" ").toLowerCase();
        return haystack.includes(query);
    });
}

function renderNav() {
    const filteredPages = getFilteredPages();
    const grouped = groupPages(filteredPages);

    if (!filteredPages.length) {
        navRoot.innerHTML = `<p class="empty-state">No pages match <strong>${escapeHtml(state.search)}</strong>.</p>`;
        return;
    }

    navRoot.innerHTML = Object.entries(grouped)
        .map(([section, pages]) => `
            <section class="nav-group">
                <h2 class="nav-group__title">${escapeHtml(section)}</h2>
                ${pages.map((page) => `
                    <a class="nav-link ${page.slug === state.slug ? "is-active" : ""}" href="#${page.slug}">
                        <span class="nav-link__title">${escapeHtml(page.title)}</span>
                        <span class="nav-link__summary">${escapeHtml(page.summary)}</span>
                    </a>
                `).join("")}
            </section>
        `)
        .join("");
}

function renderPage() {
    const page = getPage(state.slug);
    const pageIndex = PAGES.findIndex((item) => item.slug === page.slug);
    const previousPage = pageIndex > 0 ? PAGES[pageIndex - 1] : null;
    const nextPage = pageIndex < PAGES.length - 1 ? PAGES[pageIndex + 1] : null;

    kickerRoot.textContent = `${page.title} · ${page.section}`;

    if (page.sourceHref) {
        sourceLink.hidden = false;
        sourceLink.href = page.sourceHref;
        sourceLink.textContent = page.sourceLabel || "Read source file";
    } else {
        sourceLink.hidden = true;
        sourceLink.removeAttribute("href");
        sourceLink.textContent = "";
    }

    pageRoot.innerHTML = `
        <header class="doc-hero">
            <p class="eyebrow">${escapeHtml(page.kicker)}</p>
            <h1>${escapeHtml(page.title)}</h1>
            <p class="doc-page__summary">${escapeHtml(page.summary)}</p>
            <div class="hero-chips">
                ${(page.chips || []).map((chip) => `<span class="hero-chip">${escapeHtml(chip)}</span>`).join("")}
            </div>
        </header>
        ${page.content}
        <nav class="page-pagination">
            ${previousPage ? `<a class="page-link-button" href="#${previousPage.slug}">← ${escapeHtml(previousPage.title)}</a>` : ""}
            ${nextPage ? `<a class="page-link-button" href="#${nextPage.slug}">${escapeHtml(nextPage.title)} →</a>` : ""}
        </nav>
    `;

    renderToc();
}

function renderToc() {
    const headings = [...pageRoot.querySelectorAll("h2[id], h3[id]")];
    if (!headings.length) {
        tocRoot.innerHTML = `<p class="empty-state">No page outline yet.</p>`;
        return;
    }

    tocRoot.innerHTML = headings.map((heading) => {
        const level = heading.tagName === "H3" ? "&nbsp;&nbsp;" : "";
        return `<a class="toc-link" href="#${state.slug}:${heading.id}" data-anchor="${heading.id}">${level}${escapeHtml(heading.textContent)}</a>`;
    }).join("");
}

function escapeHtml(value) {
    return String(value)
        .replaceAll("&", "&amp;")
        .replaceAll("<", "&lt;")
        .replaceAll(">", "&gt;")
        .replaceAll('"', "&quot;")
        .replaceAll("'", "&#039;");
}

function syncHash() {
    const raw = window.location.hash.slice(1);
    if (!raw) {
        state.slug = "welcome";
        renderNav();
        renderPage();
        return;
    }

    const [slug, anchor] = raw.split(":");
    state.slug = getPage(slug).slug;

    renderNav();
    renderPage();

    if (anchor) {
        const target = document.getElementById(anchor);
        if (target) target.scrollIntoView({ behavior: "smooth", block: "start" });
    }
}

function closeSidebarOnSmallScreens() {
    if (window.innerWidth > 920) return;
    sidebar.classList.remove("is-open");
    menuToggle.setAttribute("aria-expanded", "false");
}

searchField.addEventListener("input", (event) => {
    state.search = event.target.value;
    renderNav();
});

menuToggle.addEventListener("click", () => {
    const willOpen = !sidebar.classList.contains("is-open");
    sidebar.classList.toggle("is-open", willOpen);
    menuToggle.setAttribute("aria-expanded", String(willOpen));
});

window.addEventListener("hashchange", () => {
    syncHash();
    closeSidebarOnSmallScreens();
});

renderNav();
renderPage();
syncHash();
