// First-pass bindings: intentionally minimal. Goal of this file right now is
// NOT to expose useful functionality yet - it's to prove that the vendored
// UEViewer subset (Core + Unreal parsing, minus the native GL/SDL renderer)
// actually compiles and links cleanly under Emscripten. Once CI is green on
// this, we grow this file into the real decode API (LoadPackage, GetStaticMesh,
// GetTexturePixels, GetLevelActors, etc).

#include <emscripten/bind.h>
#include <string>

// Pull in one representative header from each corner of the vendored tree so
// that a passing build here actually proves those subsystems compile, rather
// than only proving an empty bindings file links.
#include "Core.h"
#include "UnCore.h"
#include "UnrealPackage/UnPackage.h"
#include "Mesh/StaticMesh.h"

std::string decoder_version() {
    return "udk-decoder-wasm skeleton build - Core/Unreal parsing subset only, no renderer";
}

EMSCRIPTEN_BINDINGS(udk_decoder) {
    emscripten::function("decoder_version", &decoder_version);
}
