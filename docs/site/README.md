# Canis Docs Site

Starter static docs site for the engine and editor.

## Open

You can open [index.html](./index.html) directly in a browser.

## Serve

From the repo root:

```bash
python3 -m http.server 8000 --directory canis/docs/site
```

Then visit:

```text
http://localhost:8000
```

## Notes

- No build step
- No package manager
- Content lives in `app.js`
- Existing markdown guides in `canis/docs/` are still useful source material
