// Build configuration for the WASM decoder target.
//
// This intentionally narrows UEViewer's normal "support everything" build down
// to just what a browser-based UDK/UE3 static-mesh + texture + level-layout
// viewer needs. Widen later (UNREAL4, THREADING, specific GameSpecific hacks)
// once the base UE3 path is compiling and working end-to-end.

#define DO_GUARD        1

#define UNREAL1         0
#define UNREAL25        0
#define UNREAL3         1
#define UNREAL4         0

#include "GameDefines.h"

// No native window/OpenGL renderer in this build - we ship parsed mesh/texture
// data to JS and render with WebGL/three.js on the JS side instead.
#define RENDERING       0

// Emscripten's default single-threaded build. Revisit with pthreads + a
// SharedArrayBuffer-enabled hosting setup if parsing large packages turns
// out to be too slow single-threaded.
#define THREADING       0

#define PROFILE         0
#define DECLARE_VIEWER_PROPS 0

// WASM: force-disable extras that need missing deps or TRIBES3
#undef BIOSHOCK
#define BIOSHOCK 0
#undef TRIBES3
#define TRIBES3 0
#undef SUPPORT_XBOX360
#define SUPPORT_XBOX360 0
#undef SUPPORT_IPHONE
#define SUPPORT_IPHONE 0
#undef SUPPORT_ANDROID
#define SUPPORT_ANDROID 0
#undef SUPPORT_PS4
#define SUPPORT_PS4 0
#undef SUPPORT_SWITCH
#define SUPPORT_SWITCH 0

