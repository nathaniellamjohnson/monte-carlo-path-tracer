# Monte Carlo Path Tracer && Caustics Only Photon Mapping


- Render Command 
`scons && ./ray_tracer -i ./tests/cornellbox.txt`

- Browser WASM Monte Carlo Path Tracer -> see /wasm/README.md

## Deploy to Vercel (WASM)

This project serves the browser app from [wasm/index.html](wasm/index.html) and uses prebuilt wasm artifacts.

### 1. Build wasm locally

Run from project root:

```bash
bash ./wasm/build_wasm.sh
```

This generates:

- [wasm/ray_tracer.js](wasm/ray_tracer.js)
- [wasm/ray_tracer.wasm](wasm/ray_tracer.wasm)

### 2. Commit required files

Make sure these are committed before deploy:

- [vercel.json](vercel.json)
- [wasm/index.html](wasm/index.html)
- [wasm/app.js](wasm/app.js)
- [wasm/style.css](wasm/style.css)
- [wasm/ray_tracer.js](wasm/ray_tracer.js)
- [wasm/ray_tracer.wasm](wasm/ray_tracer.wasm)
- [tests/wasm_default_cornell_scene.txt](tests/wasm_default_cornell_scene.txt)

### 3. Deploy

Push to your Git provider and import the repository in Vercel.

No custom build step is required if wasm artifacts are already committed.

### 4. Verify after deploy

- Open `/` and confirm it rewrites to [wasm/index.html](wasm/index.html).
- Confirm initial scene text loads from [tests/wasm_default_cornell_scene.txt](tests/wasm_default_cornell_scene.txt).
- Click Render and verify image output appears.

If updates do not appear immediately, do a hard refresh to clear browser cache.
