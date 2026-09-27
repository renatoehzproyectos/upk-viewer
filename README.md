# UPK Viewer

A browser-based, client-side viewer for Unreal Engine 3 / UDK package files
(`.upk`, `.udk`, `.u`, `.umap`), powered by a Rust→WebAssembly parser. UDK
(Unreal Development Kit) is Epic's own UE3 build, so it uses the same
container format; the viewer flags a package as "likely UDK" using a
heuristic (unmodified, high FileVersion + LicenseeVersion 0) since UDK
doesn't stamp itself explicitly in the header. Nothing is uploaded anywhere —
files are parsed entirely in the browser.

## What it does

Reads the package header and shows:
- Package summary (version, flags, GUID, header size, compression flag)
- Name table
- Import table (class/package/object name, outer)
- Export table (class/super/outer indices, object flags, serial size/offset)

You can import a package two ways:
- **Import UPK / UDK file** — pick a single `.upk`/`.udk`/`.u`/`.umap` file, or drag one onto the page.
- **Import ZIP** — pick a `.zip` (or drag one on), and it's unzipped entirely in the browser
  (via [JSZip](https://stuk.github.io/jszip/), loaded from cdnjs). If the zip contains exactly
  one package file it's opened automatically; if it contains several, you get a list to choose from.

## What it doesn't do (yet)

- Decompress compressed packages (some UE3 titles ship LZO/zlib-chunked `.upk`s)
- Decode actual object data — textures, meshes, sounds, etc. Each UE class has
  its own binary serialization format; this would be a substantial follow-up.
- Guarantee correctness across every UE3 engine fork. The on-disk header layout
  varies by licensee; this targets the common "vanilla" UE3 layout and reports
  parse warnings/errors rather than silently producing garbage.

## ⚠️ One manual step after unzipping

This zip stores the workflow under `github_hidden_rename_me/` instead of `.github/`,
because folders starting with a dot are hidden by default in Finder/Explorer/most
file managers and were causing confusion after unzipping. After you unzip:

```bash
mv github_hidden_rename_me .github
```

(or just rename the folder to `.github` in your file manager — enable "show hidden
files" first so you can see it afterward). If you're pushing straight to git instead
of unzipping by hand, do the rename before `git add`/`git commit`.

## Local development

Requires Rust + [wasm-pack](https://rustwasm.github.io/wasm-pack/installer/).

```bash
cd upk-wasm
wasm-pack build --release --target web --out-dir ../www/pkg
cd ../www
python3 -m http.server 8080
# open http://localhost:8080
```

## Deployment

`.github/workflows/deploy.yml` builds the wasm module on every push to `main`
and publishes `www/` to GitHub Pages. Enable Pages for this repo with source
"GitHub Actions" (Settings → Pages → Build and deployment → Source).

## Project layout

```
upk-wasm/      Rust crate compiled to WebAssembly (the parser)
www/           Static site: index.html, main.js, and the built wasm (www/pkg, gitignored)
.github/workflows/deploy.yml   CI: build wasm, deploy to GitHub Pages
```
