cmake_minimum_required(VERSION 3.16)
project(udk_decoder CXX C)

if(NOT EMSCRIPTEN)
  message(FATAL_ERROR "This project is meant to be built with emcmake/emscripten. Got: ${CMAKE_CXX_COMPILER}")
endif()

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(VENDOR ${CMAKE_SOURCE_DIR}/vendor)

include_directories(
  ${CMAKE_SOURCE_DIR}
  ${VENDOR}/Core
  ${VENDOR}/Unreal
  ${VENDOR}/Unreal/FileSystem
  ${VENDOR}/Unreal/Mesh
  ${VENDOR}/Unreal/UnrealMaterial
  ${VENDOR}/Unreal/UnrealMesh
  ${VENDOR}/Unreal/UnrealPackage
  ${VENDOR}/Unreal/Wrappers
  ${VENDOR}/Unreal/GameSpecific
  ${VENDOR}/libs/include
  ${VENDOR}/libs/lzo
  ${VENDOR}/libs/lz4
  ${VENDOR}/libs/zlib
  ${VENDOR}/libs/mspack
  ${VENDOR}/libs/rijndael
  ${VENDOR}/libs/detex
)

# -DBUILD_H_OVERRIDE trick: our own Build.h at the project root shadows the
# vendored per-target Build.h files (UEViewer normally has several, one per
# app - UmodelTool/Build.h, etc.) since it comes first on the include path.
add_compile_definitions(HAS_UI=0)

file(GLOB CORE_SRC ${VENDOR}/Core/*.cpp)
file(GLOB UNREAL_SRC ${VENDOR}/Unreal/*.cpp)
file(GLOB UNREAL_FS_SRC ${VENDOR}/Unreal/FileSystem/*.cpp)
file(GLOB UNREAL_MESH_SRC ${VENDOR}/Unreal/Mesh/*.cpp)
file(GLOB UNREAL_MAT_SRC ${VENDOR}/Unreal/UnrealMaterial/*.cpp)
file(GLOB UNREAL_UMESH_SRC ${VENDOR}/Unreal/UnrealMesh/*.cpp)
file(GLOB UNREAL_PKG_SRC ${VENDOR}/Unreal/UnrealPackage/*.cpp)
file(GLOB UNREAL_WRAP_SRC ${VENDOR}/Unreal/Wrappers/*.cpp)
file(GLOB UNREAL_GAME_SRC ${VENDOR}/Unreal/GameSpecific/*.cpp)

file(GLOB LZO_SRC ${VENDOR}/libs/lzo/*.c)
file(GLOB LZ4_SRC ${VENDOR}/libs/lz4/*.c)
file(GLOB ZLIB_SRC ${VENDOR}/libs/zlib/*.c)
file(GLOB MSPACK_SRC ${VENDOR}/libs/mspack/*.c)
file(GLOB RIJNDAEL_SRC ${VENDOR}/libs/rijndael/*.c)
file(GLOB DETEX_SRC ${VENDOR}/libs/detex/*.c)

add_executable(udk_decoder
  bindings.cpp
  ${CORE_SRC}
  ${UNREAL_SRC}
  ${UNREAL_FS_SRC}
  ${UNREAL_MESH_SRC}
  ${UNREAL_MAT_SRC}
  ${UNREAL_UMESH_SRC}
  ${UNREAL_PKG_SRC}
  ${UNREAL_WRAP_SRC}
  ${UNREAL_GAME_SRC}
  ${LZO_SRC}
  ${LZ4_SRC}
  ${ZLIB_SRC}
  ${MSPACK_SRC}
  ${RIJNDAEL_SRC}
  ${DETEX_SRC}
)

target_link_options(udk_decoder PRIVATE
  "SHELL:-s WASM=1"
  "SHELL:-s MODULARIZE=1"
  "SHELL:-s EXPORT_NAME=UdkDecoderModule"
  "SHELL:-s ALLOW_MEMORY_GROWTH=1"
  "SHELL:-s ENVIRONMENT=web"
  "SHELL:-s EXPORTED_RUNTIME_METHODS=['ccall','cwrap','FS']"
  "SHELL:--bind"
)
