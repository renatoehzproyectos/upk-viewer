# udk3d — browser 3D level viewer for UDK/UE3, powered by UEViewer's parser

## Where this came from

This is a fresh direction after the earlier UPK/UDK *header* browser: instead
of just listing names/imports/exports, the goal is an actual 3D view of a
`.umap` level — real static mesh geometry, real textures, actors placed at
their real transforms, with lighting/materials as close to in-engine as is
realistically achievable in a browser.

Rather than write a UE3 binary format parser from scratch (a years-long
reverse-engineering effort), this vendors the relevant subset of
[UEViewer/UModel](https://github.com/gildor2/UEModel) — the mature open-source
C++ tool that already parses UE1 through UE4 packages — and compiles *only its
format-parsing code* to WebAssembly. UEViewer's own native renderer (Win32,
SDL2, legacy immediate-mode OpenGL) is **not** ported; a browser-native
WebGL/three.js renderer is built instead, fed by the parsed mesh/texture data
that comes out of the WASM module.

## Current status: Step 0 — prove the vendored subset compiles

`decoder/` contains:
- `vendor/Core` — UEViewer's base utility layer (memory, math, containers,
  compression front-ends), with the OpenGL/SDL/Win32 files (`CoreGL.cpp`,
  `GlWindow.cpp`, `GLText.cpp`, `GLBind.cpp`, `CoreWin32.cpp`) excluded.
- `vendor/Unreal` — the actual UE1-4 package/mesh/material/texture parsers,
  with only `UnRenderer.cpp` (the one file here that calls OpenGL directly)
  excluded.
- `vendor/libs/{lzo,lz4,zlib,mspack,rijndael,detex}` — the compression and
  texture-decompression libraries the parser needs. Proprietary Oodle (used by
  some newer UE4 titles) is deliberately left out; not needed for UE3/UDK.
- `Build.h` — a narrowed build config: UE3 only (`UNREAL1/25/4 = 0`),
  `RENDERING = 0`, `THREADING = 0` for this first pass.
- `bindings.cpp` — currently a **placeholder**. It doesn't do anything useful
  yet; it just includes one header from each corner of the vendored tree and
  exposes a single dummy function, so that a green CI build actually proves
  "this ~30,000-line subset compiles under Emscripten" before we invest in
  writing the real decode API on top of it.

I don't have Emscripten available to compile-test this locally, so
`.github/workflows/build-decoder.yml` is the real compiler here: it installs
Emscripten and builds on every push. **Push this, check the Actions tab, and
paste me the first error** — with a codebase this size, the realistic path is
several rounds of "CI fails on file X" → fix → push, the same loop we used for
the Rust project.

## Planned next steps (after the skeleton compiles)

1. Replace the placeholder `bindings.cpp` with a real API:
   `load_package(bytes) -> handle`, `get_static_mesh(handle, export_index) ->
   {vertices, indices, uvs, normals}`, `get_texture(handle, export_index) ->
   {width, height, rgba_bytes}`, `get_level_actors(handle) -> [{mesh_ref,
   position, rotation, scale}]`.
2. `web/` — a three.js front end: loads the `.umap` + referenced `.upk`s
   (you'll need to supply the whole `CookedPC`/content folder the map
   references, not just the `.umap` alone — imports point outside the file),
   places each actor's mesh at its real transform.
3. Real textures (DXT1/3/5 decompression via `detex`, already vendored).
4. Approximate lighting in three.js (baked lightmap textures where present +
   basic dynamic lights) — explicitly an approximation, not a pixel-accurate
   reproduction of UE3's material/shader graph.

## Local build (once you have Emscripten installed yourself)

```bash
cd decoder
emcmake cmake -B build
emmake cmake --build build
```
