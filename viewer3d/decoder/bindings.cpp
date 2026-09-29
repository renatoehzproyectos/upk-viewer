// Real decode API (first functional slice, scope: StaticMesh geometry only -
// no textures/materials yet, and no cross-package import resolution beyond
// whatever files have also been uploaded into the virtual filesystem).
//
// Integration approach: UEViewer's own directory-scanning game file system
// (appSetRootDirectory -> ScanGameDirectory -> opendir/readdir) is fully
// POSIX-based on non-Windows platforms, which means it works unmodified on
// top of Emscripten's in-memory MEMFS. So: the JS side writes uploaded file
// bytes into MEMFS via FS.writeFile(), we scan that directory the normal
// UEViewer way, then use the normal UnPackage::LoadPackage(name) / CreateExport
// API exactly as UEViewer's own tools do - no custom virtual file system
// class needed.

#include <emscripten/bind.h>
#include <emscripten/val.h>
#include <string>
#include <vector>

#include "Core.h"
#include "UnCore.h"
#include "UnObject.h"
#include "TypeInfo.h"
#include "UnrealPackage/UnPackage.h"
#include "Mesh/StaticMesh.h"
#include "UnrealMesh/UnMesh3.h"
#include "UnrealMesh/UnAnimNotify.h"
#include "UnrealMaterial/UnMaterial2.h"
#include "UnrealMaterial/UnMaterial3.h"
#include "UnrealMaterial/UnMaterialExpression.h"

using namespace emscripten;

static const char* GAME_ROOT = "/game";
static bool GScanned = false;
static bool GClassesRegistered = false;
static UnPackage* GCurrentPackage = nullptr;

// UEViewer's normal CLI tool (UmodelTool/Main.cpp) registers every known
// Unreal class before loading any package, via RegisterCoreClasses() plus a
// BEGIN_CLASS_TABLE/END_CLASS_TABLE block. Without this, UnPackage::CreateExport
// has no C++ class to instantiate for e.g. "StaticMesh" and silently returns
// NULL for every single export - which is exactly the symptom we were seeing
// (every mesh failing, not just some). We vendor only the UE3-relevant subset
// of what Main.cpp registers (see RegisterUnrealClasses3() there for the
// full/authoritative list this is trimmed from).
static void EnsureClassesRegistered()
{
    if (GClassesRegistered)
        return;
    GClassesRegistered = true;

    RegisterCoreClasses();
    BEGIN_CLASS_TABLE
        REGISTER_MATERIAL_CLASSES
        REGISTER_ANIM_NOTIFY_CLASSES
        REGISTER_MATERIAL_CLASSES_U3
        REGISTER_MESH_CLASSES_U3
        REGISTER_EXPRESSION_CLASSES
    END_CLASS_TABLE
    REGISTER_MATERIAL_ENUMS
    REGISTER_MATERIAL_ENUMS_U3
    REGISTER_MESH_ENUMS_U3
}

// Called after JS has written one or more uploaded files into MEMFS under
// /game via FS.writeFile(). Scans that directory the normal UEViewer way,
// then opens 'mainFilename' as the current package.
//
// Returns: 0 = ok, 1 = package not found after scan, 2 = reserved for future use.
int scan_and_open(const std::string& mainFilename)
{
    guard(scan_and_open);

    EnsureClassesRegistered();

    if (!GScanned)
    {
        appSetRootDirectory(GAME_ROOT, /*recurse=*/true);
        GScanned = true;
    }

    UnPackage* pkg = UnPackage::LoadPackage(mainFilename.c_str(), /*silent=*/true);
    if (!pkg)
        return 1;

    GCurrentPackage = pkg;
    return 0;

    unguard;
}

// List of exports in the currently open package, as a JS array of
// { index, className, objectName }. JS decides which ones to try loading
// as a mesh (className == "StaticMesh").
val list_exports()
{
    val result = val::array();
    if (!GCurrentPackage)
        return result;

    int count = GCurrentPackage->Summary.ExportCount;
    for (int i = 0; i < count; i++)
    {
        const FObjectExport& exp = GCurrentPackage->GetExport(i);
        val entry = val::object();
        entry.set("index", i);
        entry.set("className", std::string(GCurrentPackage->GetClassNameFor(exp)));
        entry.set("objectName", std::string(exp.ObjectName.Str));
        result.call<void>("push", entry);
    }
    return result;
}

// Extracts real geometry for a StaticMesh export: flat Float32Arrays for
// position/normal/uv (kept as separate arrays, which map directly onto
// three.js BufferAttributes), and a Uint32Array of triangle indices.
// LOD 0 only for this first pass.
//
// Returns null (JS side) on failure; otherwise an object:
//   { vertexCount, indexCount, positions, normals, uvs, indices, sections: [{firstIndex, numFaces}] }
val get_static_mesh(int exportIndex)
{
    if (!GCurrentPackage)
    {
        val out = val::object();
        out.set("error", std::string("No hay ningun paquete abierto."));
        return out;
    }

    guard(get_static_mesh);

    UObject* obj = GCurrentPackage->CreateExport(exportIndex);
    if (!obj)
    {
        val out = val::object();
        out.set("error", std::string("CreateExport devolvio null (clase no registrada, o export marcado como Default__)."));
        return out;
    }

    // DECLARE_CLASS(UStaticMesh3) -> type name "StaticMesh3" (alias "StaticMesh" is CreateClass-only)
    if (!obj->IsA("StaticMesh3") && !obj->IsA("StaticMesh") && !obj->IsA("FracturedStaticMesh"))
    {
        val out = val::object();
        out.set("error", std::string("El objeto cargo pero no es un StaticMesh."));
        return out;
    }

    UStaticMesh3* SM = static_cast<UStaticMesh3*>(obj);
    CStaticMesh* mesh = SM->ConvertedMesh;
    if (!mesh)
    {
        val out = val::object();
        out.set("error", std::string("StaticMesh cargo pero ConvertedMesh es null (ConvertMesh() no se ejecuto)."));
        return out;
    }
    if (mesh->Lods.Num() == 0)
    {
        val out = val::object();
        out.set("error", std::string("ConvertedMesh no tiene ningun LOD."));
        return out;
    }

    CStaticMeshLod& lod = mesh->Lods[0];
    int vertCount = lod.NumVerts;
    int idxCount = lod.Indices.Num();

    std::vector<float> positions(vertCount * 3);
    std::vector<float> normals(vertCount * 3);
    std::vector<float> uvs(vertCount * 2);
    std::vector<uint32_t> indices(idxCount);

    for (int v = 0; v < vertCount; v++)
    {
        const CStaticMeshVertex& mv = lod.Verts[v];
        positions[v * 3 + 0] = mv.Position.v[0];
        positions[v * 3 + 1] = mv.Position.v[1];
        positions[v * 3 + 2] = mv.Position.v[2];

        CVec3 n;
        Unpack(n, mv.Normal);
        normals[v * 3 + 0] = n.X;
        normals[v * 3 + 1] = n.Y;
        normals[v * 3 + 2] = n.Z;

        uvs[v * 2 + 0] = mv.UV.U;
        uvs[v * 2 + 1] = mv.UV.V;
    }

    CIndexBuffer::IndexAccessor_t GetIndex = lod.Indices.GetAccessor();
    for (int i = 0; i < idxCount; i++)
        indices[i] = (uint32_t)GetIndex(i);

    val sections = val::array();
    for (int s = 0; s < lod.Sections.Num(); s++)
    {
        val sec = val::object();
        sec.set("firstIndex", lod.Sections[s].FirstIndex);
        sec.set("numFaces", lod.Sections[s].NumFaces);
        sections.call<void>("push", sec);
    }

    val out = val::object();
    out.set("vertexCount", vertCount);
    out.set("indexCount", idxCount);
    out.set("positions", val(typed_memory_view(positions.size(), positions.data())).call<val>("slice"));
    out.set("normals", val(typed_memory_view(normals.size(), normals.data())).call<val>("slice"));
    out.set("uvs", val(typed_memory_view(uvs.size(), uvs.data())).call<val>("slice"));
    out.set("indices", val(typed_memory_view(indices.size(), indices.data())).call<val>("slice"));
    out.set("sections", sections);
    return out;

    unguard;
}

std::string decoder_version()
{
    return "udk-decoder-wasm - StaticMesh geometry extraction (LOD0, no materials/textures yet)";
}

EMSCRIPTEN_BINDINGS(udk_decoder)
{
    function("decoder_version", &decoder_version);
    function("scan_and_open", &scan_and_open);
    function("list_exports", &list_exports);
    function("get_static_mesh", &get_static_mesh);
}
