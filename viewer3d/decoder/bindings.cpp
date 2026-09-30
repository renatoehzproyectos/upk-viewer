// StaticMesh geometry + Texture2D diffuse for workshop map preview.

#include <emscripten/bind.h>
#include <emscripten/val.h>
#include <string>
#include <vector>
#include <csetjmp>
#include <cstring>
#include <cstdio>
#include <cstdarg>

#include "Core.h"
#include "UnCore.h"
#include "UnObject.h"
#include "TypeInfo.h"
#include "UnrealPackage/UnPackage.h"
#include "Mesh/StaticMesh.h"
#include "UnrealMesh/UnMesh3.h"
#include "UnrealMaterial/UnMaterial.h"
#include "UnrealMaterial/UnMaterial3.h"

using namespace emscripten;


// ---- Elite debug ring buffer (JS: get_debug_log) ----
static const int DBG_CAP = 64;
static const int DBG_LINE = 240;
static char GDbg[DBG_CAP][DBG_LINE];
static int GDbgWrite = 0;
static int GDbgCount = 0;

static void Dbg(const char* stage, const char* detail = "")
{
    char line[DBG_LINE];
    if (detail && detail[0])
        snprintf(line, sizeof(line), "[%s] %s", stage, detail);
    else
        snprintf(line, sizeof(line), "[%s]", stage);
    strncpy(GDbg[GDbgWrite], line, DBG_LINE - 1);
    GDbg[GDbgWrite][DBG_LINE - 1] = 0;
    GDbgWrite = (GDbgWrite + 1) % DBG_CAP;
    if (GDbgCount < DBG_CAP) GDbgCount++;
    appPrintf("DBG %s\n", line);
}

static void DbgF(const char* stage, const char* fmt, ...)
{
    char detail[200];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(detail, sizeof(detail), fmt, ap);
    va_end(ap);
    Dbg(stage, detail);
}

static const char* GAME_ROOT = "/game";
static bool GScanned = false;
static bool GClassesRegistered = false;
static UnPackage* GCurrentPackage = nullptr;

static jmp_buf GSoftJmp;
static bool GSoftJmpReady = false;
static char GSoftErrorMsg[1024] = "";

extern "C" void wasm_soft_error(const char* msg)
{
    if (msg && msg[0])
    {
        strncpy(GSoftErrorMsg, msg, sizeof(GSoftErrorMsg) - 1);
        GSoftErrorMsg[sizeof(GSoftErrorMsg) - 1] = 0;
        Dbg("soft_error", msg);
    }
    if (GSoftJmpReady)
        longjmp(GSoftJmp, 1);
}

static val make_error(const char* stage, const std::string& detail)
{
    DbgF("ERR", "%s %s", stage, detail.c_str());
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
    out.set("stage", std::string(stage));
    return out;
}

static void EnsureClassesRegistered()
{
    if (GClassesRegistered) return;
    GClassesRegistered = true;

    RegisterCoreClasses();

    BEGIN_CLASS_TABLE
#if UNREAL3
        REGISTER_MESH_CLASSES_U3
        REGISTER_MATERIAL_CLASSES_U3
#endif
    END_CLASS_TABLE

#if UNREAL3
    REGISTER_MESH_ENUMS_U3
    REGISTER_MATERIAL_ENUMS_U3
#endif

    SuppressUnknownClass("UBodySetup");
    SuppressUnknownClass("UMaterialExpression*");
    SuppressUnknownClass("UPhysicalMaterial");
    SuppressUnknownClass("USkeletalMeshSocket");
}

// Decode UTexture2D (or subclass) to RGBA8 for the browser.
static val TextureToVal(UTexture2D* Tex, const char* why = "?")
{
    if (!Tex)
    {
        DbgF("tex_null", "why=%s", why ? why : "?");
        return val::null();
    }

    DbgF("tex_begin", "%s class=%s name=%s fmt=%d",
         why ? why : "?", Tex->GetClassName(), Tex->Name, (int)Tex->Format);

    CTextureData TexData;
    if (!Tex->GetTextureData(TexData))
    {
        DbgF("tex_GetTextureData_fail", "%s", Tex->Name);
        return val::null();
    }
    if (TexData.Mips.Num() == 0)
    {
        DbgF("tex_no_mips", "%s pixelFmt=%d", Tex->Name, (int)TexData.Format);
        return val::null();
    }

    DbgF("tex_mips", "%s mips=%d pixelFmt=%d %dx%d",
         Tex->Name, TexData.Mips.Num(), (int)TexData.Format,
         TexData.Mips[0].USize, TexData.Mips[0].VSize);

    byte* rgba = TexData.Decompress(0);
    if (!rgba)
    {
        DbgF("tex_decompress_fail", "%s pixelFmt=%d", Tex->Name, (int)TexData.Format);
        return val::null();
    }

    const CMipMap& Mip = TexData.Mips[0];
    int w = Mip.USize;
    int h = Mip.VSize;
    if (w <= 0 || h <= 0)
    {
        appFree(rgba);
        return val::null();
    }

    size_t nbytes = (size_t)w * (size_t)h * 4;
    std::vector<uint8_t> pixels(nbytes);
    memcpy(pixels.data(), rgba, nbytes);
    appFree(rgba);

    DbgF("tex_ok", "%s %dx%d", Tex->Name, w, h);
    val out = val::object();
    out.set("name", std::string(Tex->Name));
    out.set("width", w);
    out.set("height", h);
    out.set("format", (int)TexData.Format);
    out.set("rgba", val(typed_memory_view(pixels.size(), pixels.data())).call<val>("slice"));
    return out;
}

// Prefer diffuse-like texture parameters from a material instance.
static UTexture2D* PickDiffuseFromMaterial(UUnrealMaterial* Mat)
{
    if (!Mat) return nullptr;

    if (Mat->IsA("Texture2D") || Mat->IsA("LightMapTexture2D"))
        return static_cast<UTexture2D*>(Mat);

    // MaterialInstance / MIC: TextureParameterValues
    if (Mat->IsA("MaterialInstanceConstant") || Mat->IsA("MaterialInstance"))
    {
        UMaterialInstance* MI = static_cast<UMaterialInstance*>(Mat);
        UTexture2D* fallback = nullptr;
        for (int i = 0; i < MI->TextureParameterValues.Num(); i++)
        {
            UTexture3* T = MI->TextureParameterValues[i].ParameterValue;
            if (!T || !(T->IsA("Texture2D") || T->IsA("LightMapTexture2D")))
                continue;
            UTexture2D* T2 = static_cast<UTexture2D*>(T);
            const char* pname = MI->TextureParameterValues[i].GetName();
            if (pname)
            {
                // Prefer common diffuse param names
                if (appStristr((char*)pname, (char*)"Diffuse") || appStristr(pname, "BaseColor") ||
                    appStristr(pname, "Albedo") || appStristr(pname, "Color") ||
                    appStristr(pname, "DiffuseMap") || appStristr(pname, "Tex"))
                    return T2;
            }
            if (!fallback) fallback = T2;
        }
        if (fallback) return fallback;
        // Parent material
        if (MI->Parent && MI->Parent != Mat)
            return PickDiffuseFromMaterial(MI->Parent);
    }

    // UMaterial3: ReferencedTextures (older packages)
    if (Mat->IsA("Material3") || Mat->IsA("Material"))
    {
        UMaterial3* M = static_cast<UMaterial3*>(Mat);
        for (int i = 0; i < M->ReferencedTextures.Num(); i++)
        {
            UTexture3* T3 = M->ReferencedTextures[i];
            if (T3 && (T3->IsA("Texture2D") || T3->IsA("LightMapTexture2D")))
                return static_cast<UTexture2D*>(T3);
        }
    }

    return nullptr;
}

int scan_and_open(const std::string& mainFilename)
{
    EnsureClassesRegistered();

    if (!GScanned)
    {
        appSetRootDirectory(GAME_ROOT, /*recurse=*/true);
        GScanned = true;
    }

    DbgF("scan_and_open", "file=%s", mainFilename.c_str());
    UnPackage* pkg = UnPackage::LoadPackage(mainFilename.c_str(), /*silent=*/true);
    if (!pkg)
    {
        Dbg("LoadPackage_fail", mainFilename.c_str());
        return 1;
    }

    GCurrentPackage = pkg;
    DbgF("LoadPackage_ok", "exports=%d", pkg->Summary.ExportCount);
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

val get_texture(int exportIndex)
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

    UObject* obj = GCurrentPackage->CreateExport(exportIndex);
    GSoftJmpReady = false;
    if (!obj)
        return make_error("CreateExport_null", "");

    if (!obj->IsA("Texture2D") && !obj->IsA("LightMapTexture2D"))
        return make_error("not_texture", obj->GetClassName());

    val tex = TextureToVal(static_cast<UTexture2D*>(obj));
    if (tex.isNull())
        return make_error("decode_failed", obj->Name);
    return tex;
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

        UUnrealMaterial* Mat = lod.Sections[s].Material;
        std::string matName = Mat ? std::string(Mat->Name) : "";
        sec.set("materialName", matName);

        // Soft-extract diffuse texture for this section
        val texVal = val::null();
        std::string texStatus = Mat ? "pending" : "no_material";
        if (Mat)
        {
            GSoftJmpReady = true;
            if (setjmp(GSoftJmp) == 0)
            {
                UTexture2D* Diff = PickDiffuseFromMaterial(Mat);
                if (Diff)
                {
                    texVal = TextureToVal(Diff, Mat->Name);
                    texStatus = texVal.isNull() ? "decode_fail" : "ok";
                }
                else
                    texStatus = "no_diffuse_param";
            }
            else
                texStatus = std::string("soft_assert:") + GSoftErrorMsg;
            GSoftJmpReady = false;
            GSoftErrorMsg[0] = 0;
        }
        sec.set("diffuse", texVal);
        sec.set("texStatus", texStatus);
        sec.set("materialClass", Mat ? std::string(Mat->GetClassName()) : std::string(""));
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


val get_debug_log()
{
    val arr = val::array();
    int start = (GDbgCount < DBG_CAP) ? 0 : GDbgWrite;
    int n = GDbgCount;
    for (int i = 0; i < n; i++)
    {
        int idx = (start + i) % DBG_CAP;
        arr.call<void>("push", std::string(GDbg[idx]));
    }
    return arr;
}

void clear_debug_log()
{
    GDbgWrite = 0;
    GDbgCount = 0;
}

val list_textures()
{
    val result = val::array();
    if (!GCurrentPackage) return result;
    int count = GCurrentPackage->Summary.ExportCount;
    for (int i = 0; i < count; i++)
    {
        const FObjectExport& exp = GCurrentPackage->GetExport(i);
        std::string cn = GCurrentPackage->GetClassNameFor(exp);
        if (cn != "Texture2D" && cn != "LightMapTexture2D") continue;
        val entry = val::object();
        entry.set("index", i);
        entry.set("className", cn);
        entry.set("objectName", std::string(exp.ObjectName.Str));
        result.call<void>("push", entry);
    }
    return result;
}

std::string decoder_version()
{
    return "udk-decoder-wasm v0.6.0-debug - StaticMesh + textures + elite Dbg";
}

EMSCRIPTEN_BINDINGS(udk_decoder)
{
    function("decoder_version", &decoder_version);
    function("scan_and_open", &scan_and_open);
    function("list_exports", &list_exports);
    function("get_static_mesh", &get_static_mesh);
    function("get_texture", &get_texture);
    function("list_textures", &list_textures);
    function("get_debug_log", &get_debug_log);
    function("clear_debug_log", &clear_debug_log);
}
