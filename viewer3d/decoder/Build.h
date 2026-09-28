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

// WASM: strip game/platform features that pull missing headers or UE2/UE4-only code
#undef BIOSHOCK
#define BIOSHOCK 0
#undef TRIBES3
#define TRIBES3 0
#undef UC1
#define UC1 0
#undef UC2
#define UC2 0
#undef SPLINTER_CELL
#define SPLINTER_CELL 0
#undef LINEAGE2
#define LINEAGE2 0
#undef SWRC
#define SWRC 0
#undef LOCO
#define LOCO 0
#undef BATTLE_TERR
#define BATTLE_TERR 0
#undef XIII
#define XIII 0
#undef UT2
#define UT2 0
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

