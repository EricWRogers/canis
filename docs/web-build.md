# Web Export

The engine now supports an editor-free browser export path using Emscripten.

## Quick start

```bash
./scripts/build-web.sh
```

That command will:

1. Download or update `emsdk` into `${XDG_CACHE_HOME:-$HOME/.cache}/c-engine/emsdk`
2. Activate the latest Emscripten SDK
3. Configure the `web-release` preset
4. Build the web bundle into `build-web-release/web`

For a debug build:

```bash
./scripts/build-web.sh web-debug
```

## Output

The generated export lives in:

- `build-web-release/web/` for `web-release`
- `build-web-debug/web/` for `web-debug`

You should see files similar to:

- `index.html`
- `index.js`
- `index.wasm`
- `index.data`

`index.data` contains the packaged `project/assets` directory plus `project/project.canis`, so the browser build loads the same content as the native runtime.

## Itch.io upload

Upload the contents of the export directory as an HTML5 project. The release bundle is the one intended for jams and deployment:

```bash
./scripts/build-web.sh web-release
```

Then upload everything inside `build-web-release/web/`.

## Notes

- The editor runtime is disabled for web builds.
- Gameplay code is statically linked for the browser build instead of being hot-loaded as a shared library.
- The web target uses WebGL 2 / OpenGL ES 3 shader compilation.
