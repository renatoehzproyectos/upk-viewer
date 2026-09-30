// Real decode API (StaticMesh geometry only).

#include <emscripten/bind.h>
#include <emscripten/val.h>
#include <string>
#include <vector>
#include <csetjmp>
#include <cstring>

#include "Core.h"
#include "UnCore.h"
#include "UnObject.h"
#include "TypeInfo.h"
#include "UnrealPackage/UnPackage.h"
#include "Mesh/StaticMesh.h"
#include "UnrealMesh/UnMesh3.h"
#include "UnrealMaterial/UnMaterial3.h"

using namespace emscripten;

static const char* GAME_ROOT = "/game";
static bool GScanned = false;
static bool GClassesRegistered = false;
static UnPackage* GCurrentPackage = nullptr;

// Soft-error recovery: appError/assert longjmps here instead of aborting the module.
static jmp_buf GSoftJmp;
static bool GSoftJmpReady = false;
static char GSoftErrorMsg[1024] = "";

// Called from patched Core.cpp appError under __EMSCRIPTEN__
extern "C" void wasm_soft_error(const char* msg)
{
    if (msg && msg[0])
    {
        strncpy(GSoftErrorMsg, msg, sizeof(GSoftErrorMsg) - 1);
        GSoftErrorMsg[sizeof(GSoftErrorMsg) - 1] = 0;
    }
    if (GSoftJmpReady)
        longjmp(GSoftJmp, 1);
}

static val make_error(const char* stage, const std::string& detail)
{
    val out = val::object();
    std::string e = stage;
    if (!detail.empty()) e += ": " + detail;
    if (GSoftErrorMsg[0])
    {
        e += " | ";
        e += GSoftErrorMsg;
        GSoftErrorMsg[0] = 0;
    }
    out.set("error", e);
    return out;
}

static void EnsureClassesRegistered()
{
    if (GClassesRegistered) return;
    GClassesRegistered = true;

    RegisterCoreClasses();

    BEGIN_CLASS_TABLE
        REGISTER_MATERIAL_CLASSES
#if UNREAL3
        REGISTER_MATERIAL_CLASSES_U3
        REGISTER_MESH_CLASSES_U3
#endif
    END_CLASS_TABLE

#if UNREAL3
    REGISTER_MATERIAL_ENUMS
    REGISTER_MATERIAL_ENUMS_U3
    REGISTER_MESH_ENUMS_U3
#endif

    SuppressUnknownClass("UBodySetup");
    SuppressUnknownClass("UMaterialExpression*");
    SuppressUnknownClass("UPhysicalMaterial");
    SuppressUnknownClass("USkeletalMeshSocket");
}

int scan_and_open(const std::string& mainFilename)
{
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
}

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

val get_static_mesh(int exportIndex)
{
    if (!GCurrentPackage)
        return make_error("no_package", "");

    EnsureClassesRegistered();

    GSoftErrorMsg[0] = 0;
    GSoftJmpReady = true;
    if (setjmp(GSoftJmp) != 0)
    {
        GSoftJmpReady = false;
        return make_error("soft_assert", "");
    }

    FObjectExport& Exp = GCurrentPackage->GetExport(exportIndex);
    const char* className = GCurrentPackage->GetClassNameFor(Exp);
    const char* objName = *Exp.ObjectName;

    // If a previous failed attempt left a half-baked object, clear it so we retry
    if (Exp.Object && !Exp.Object->IsA("StaticMesh3") && !Exp.Object->IsA("StaticMesh"))
        Exp.Object = nullptr;

    UObject* obj = GCurrentPackage->CreateExport(exportIndex);
    GSoftJmpReady = false;

    if (!obj)
        return make_error("CreateExport_null",
            std::string(className ? className : "?") + " '" + (objName ? objName : "?") + "'");

    if (!obj->IsA("StaticMesh3") && !obj->IsA("StaticMesh") && !obj->IsA("FracturedStaticMesh"))
        return make_error("not_staticmesh",
            std::string("cpp=") + obj->GetClassName() + " pkg=" + (className ? className : "?"));

    UStaticMesh3* SM = static_cast<UStaticMesh3*>(obj);
    CStaticMesh* mesh = SM->ConvertedMesh;
    if (!mesh)
        return make_error("no_ConvertedMesh", objName ? objName : "");

    if (mesh->Lods.Num() == 0)
        return make_error("no_lods",
            std::string(objName ? objName : "") + " rawLods=" + std::to_string(SM->Lods.Num()));

    CStaticMeshLod& lod = mesh->Lods[0];
    int vertCount = lod.NumVerts;
    int idxCount = lod.Indices.Num();

    if (vertCount <= 0)
        return make_error("no_verts", objName ? objName : "");
    if (idxCount <= 0)
        return make_error("no_indices", objName ? objName : "");
    if (!lod.Verts)
        return make_error("null_verts_ptr", objName ? objName : "");

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
}

std::string decoder_version()
{
    return "udk-decoder-wasm - StaticMesh geometry extraction (LOD0, registered classes)";
}

EMSCRIPTEN_BINDINGS(udk_decoder)
{
    function("decoder_version", &decoder_version);
    function("scan_and_open", &scan_and_open);
    function("list_exports", &list_exports);
    function("get_static_mesh", &get_static_mesh);
}
