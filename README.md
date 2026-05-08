# Monte Carlo Path Tracer & Photon Mapping for Global Illumination & Caustics

## Quick Start

- Render Command 
`scons && ./ray_tracer -i ./tests/cornellbox.txt`

## Implementation Overview

For the project report, here are the main files for each implementation:

- **Monte Carlo Path Tracing**: `render_world.cpp` / `render_world.h`
- **Photon Mapping**: `photon.h` / `caustic_map.cpp` / `caustic_map.h`
- **Glass Shader**: `glass_shader.cpp` / `glass_shader.h`
- **BRDF Implementation**: `BRDF_shader.cpp` / `BRDF_shader.h` / `brdf.h` library

- Browser WASM Monte Carlo Path Tracer -> see /wasm/README.md
## Deploy to Vercel (WASM)

This project serves the browser app from `wasm/index.html` and uses prebuilt WASM artifacts.

1. Build wasm locally

Run from project root:

```bash
bash ./wasm/build_wasm.sh
```

This generates:

- `wasm/ray_tracer.js`
- `wasm/ray_tracer.wasm`

2. Commit required files

Ensure the following are committed:

- `vercel.json`
- `wasm/index.html`
- `wasm/app.js`
- `wasm/style.css`
- `wasm/ray_tracer.js`
- `wasm/ray_tracer.wasm`
- `tests/wasm_default_cornell_scene.txt`

3. Deploy

Push to your Git provider and import the repository in Vercel. No custom build step is required if the artifacts are committed.

4. Verify after deploy

- Open `/` and confirm it rewrites to `wasm/index.html`.
- Confirm initial scene text loads from `tests/wasm_default_cornell_scene.txt`.
- Click Render and verify image output appears.

If updates don't appear, do a hard refresh to clear browser cache.
