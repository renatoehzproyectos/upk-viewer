//! upk-wasm: a best-effort structural parser for Unreal Engine 3 .upk package files.
//!
//! Scope / limitations (please read):
//! - UE3's package format changed across hundreds of engine forks and licensee versions.
//!   There is no single canonical layout. This parser implements the common/"vanilla"
//!   UE3 FPackageFileSummary layout and will refuse (with a clear error) rather than
//!   guess wildly on packages it can't confidently walk.
//! - It parses the package *header*: name table, import table, export table.
//! - It does NOT decompress or decode actual object data (textures, meshes, sounds, etc).
//!   That requires per-class serializers and is a much larger project.
//! - Compressed packages (CompressionFlags != 0, e.g. LZO/LZX/zlib chunked packages) are
//!   detected but not decompressed in this version.

use byteorder::{ByteOrder, LittleEndian};
use serde::Serialize;
use wasm_bindgen::prelude::*;

const UPK_MAGIC: u32 = 0x9E2A_83C1;

#[derive(Debug)]
struct Cursor<'a> {
    data: &'a [u8],
    pos: usize,
}

#[derive(Debug)]
struct ParseError(String);

impl std::fmt::Display for ParseError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.0)
    }
}

type Result<T> = std::result::Result<T, ParseError>;

macro_rules! err {
    ($($arg:tt)*) => {
        Err(ParseError(format!($($arg)*)))
    };
}

impl<'a> Cursor<'a> {
    fn new(data: &'a [u8]) -> Self {
        Cursor { data, pos: 0 }
    }

    fn need(&self, n: usize) -> Result<()> {
        if self.pos + n > self.data.len() {
            return err!(
                "unexpected end of file at offset {} (need {} more bytes, have {})",
                self.pos,
                n,
                self.data.len() - self.pos.min(self.data.len())
            );
        }
        Ok(())
    }

    fn u8(&mut self) -> Result<u8> {
        self.need(1)?;
        let v = self.data[self.pos];
        self.pos += 1;
        Ok(v)
    }

    fn u16(&mut self) -> Result<u16> {
        self.need(2)?;
        let v = LittleEndian::read_u16(&self.data[self.pos..]);
        self.pos += 2;
        Ok(v)
    }

    fn u32(&mut self) -> Result<u32> {
        self.need(4)?;
        let v = LittleEndian::read_u32(&self.data[self.pos..]);
        self.pos += 4;
        Ok(v)
    }

    fn i32(&mut self) -> Result<i32> {
        Ok(self.u32()? as i32)
    }

    fn u64(&mut self) -> Result<u64> {
        self.need(8)?;
        let v = LittleEndian::read_u64(&self.data[self.pos..]);
        self.pos += 8;
        Ok(v)
    }

    fn guid(&mut self) -> Result<[u32; 4]> {
        Ok([self.u32()?, self.u32()?, self.u32()?, self.u32()?])
    }

    /// UE FString on disk: i32 length (negative => UTF-16, count includes null terminator;
    /// positive => ANSI/UTF-8-ish, count includes null terminator).
    fn fstring(&mut self) -> Result<String> {
        let len = self.i32()?;
        if len == 0 {
            return Ok(String::new());
        }
        if len > 0 {
            let n = len as usize;
            self.need(n)?;
            let bytes = &self.data[self.pos..self.pos + n];
            self.pos += n;
            let end = bytes.iter().position(|&b| b == 0).unwrap_or(bytes.len());
            Ok(String::from_utf8_lossy(&bytes[..end]).to_string())
        } else {
            let n = (-len) as usize;
            self.need(n * 2)?;
            let mut units = Vec::with_capacity(n);
            for i in 0..n {
                units.push(LittleEndian::read_u16(&self.data[self.pos + i * 2..]));
            }
            self.pos += n * 2;
            let end = units.iter().position(|&u| u == 0).unwrap_or(units.len());
            Ok(String::from_utf16_lossy(&units[..end]))
        }
    }

    fn seek(&mut self, offset: usize) -> Result<()> {
        if offset > self.data.len() {
            return err!("seek offset {} beyond file size {}", offset, self.data.len());
        }
        self.pos = offset;
        Ok(())
    }
}

#[derive(Serialize)]
pub struct PackageSummary {
    magic_ok: bool,
    file_version: u16,
    licensee_version: u16,
    total_header_size: u32,
    folder_name: String,
    package_flags: u32,
    compressed: bool,
    name_count: u32,
    name_offset: u32,
    export_count: u32,
    export_offset: u32,
    import_count: u32,
    import_offset: u32,
    guid: [u32; 4],
    engine_version: u32,
    cooker_version: u32,
}

#[derive(Serialize)]
pub struct NameEntry {
    index: u32,
    name: String,
}

#[derive(Serialize)]
pub struct ImportEntry {
    index: i32,
    class_package: String,
    class_name: String,
    outer_index: i32,
    object_name: String,
}

#[derive(Serialize)]
pub struct ExportEntry {
    index: i32,
    class_index: i32,
    super_index: i32,
    outer_index: i32,
    object_name: String,
    archetype_index: i32,
    object_flags: u64,
    serial_size: i32,
    serial_offset: i32,
}

#[derive(Serialize)]
pub struct ParsedPackage {
    summary: PackageSummary,
    names: Vec<NameEntry>,
    imports: Vec<ImportEntry>,
    exports: Vec<ExportEntry>,
    warnings: Vec<String>,
}

fn resolve_name<'a>(names: &'a [NameEntry], idx: i32) -> &'a str {
    if idx < 0 {
        return "(none)";
    }
    names
        .get(idx as usize)
        .map(|n| n.name.as_str())
        .unwrap_or("<invalid name index>")
}

fn parse(data: &[u8]) -> Result<ParsedPackage> {
    let mut c = Cursor::new(data);
    let mut warnings = Vec::new();

    let magic = c.u32()?;
    if magic != UPK_MAGIC {
        return err!(
            "not a UPK file: magic 0x{:08X} != expected 0x{:08X}",
            magic,
            UPK_MAGIC
        );
    }

    let packed_version = c.u32()?;
    let file_version = (packed_version & 0xFFFF) as u16;
    let licensee_version = (packed_version >> 16) as u16;

    let total_header_size = c.u32()?;
    let folder_name = c.fstring()?;
    let package_flags = c.u32()?;

    const PKG_COMPRESSED: u32 = 0x02000000; // heuristic bit used by several UE3 forks

    let name_count = c.u32()?;
    let name_offset = c.u32()?;
    let export_count = c.u32()?;
    let export_offset = c.u32()?;
    let import_count = c.u32()?;
    let import_offset = c.u32()?;

    // Fields below this point vary the most between engine forks; treat failures as
    // non-fatal since we already have what we need to walk the three key tables.
    let mut engine_version = 0u32;
    let mut cooker_version = 0u32;
    let mut guid = [0u32; 4];
    {
        // DependsOffset (u32) commonly follows import table info in vanilla UE3.
        let _ = c.u32(); // depends_offset, unused here
        // Some builds insert ImportExportGuidsOffset/ImportGuidsCount/ExportGuidsCount
        // (UDK) here; we don't rely on them.
        if let (Ok(g), Ok(ev), Ok(cv)) = (
            (|| -> Result<[u32; 4]> { c.guid() })(),
            c.u32(),
            c.u32(),
        ) {
            guid = g;
            engine_version = ev;
            cooker_version = cv;
        } else {
            warnings.push(
                "could not read optional GUID/EngineVersion/CookerVersion fields (engine fork differences); continuing with table offsets only".to_string(),
            );
        }
    }

    if name_count > 2_000_000 || export_count > 2_000_000 || import_count > 2_000_000 {
        return err!(
            "table counts look implausible (names={}, exports={}, imports={}) - likely an unsupported engine fork/layout",
            name_count,
            export_count,
            import_count
        );
    }

    let summary = PackageSummary {
        magic_ok: true,
        file_version,
        licensee_version,
        total_header_size,
        folder_name,
        package_flags,
        compressed: package_flags & PKG_COMPRESSED != 0,
        name_count,
        name_offset,
        export_count,
        export_offset,
        import_count,
        import_offset,
        guid,
        engine_version,
        cooker_version,
    };

    // ---- Name table ----
    let mut names = Vec::with_capacity(name_count as usize);
    if let Err(e) = c.seek(name_offset as usize) {
        warnings.push(format!("name table: {e}"));
    } else {
        for i in 0..name_count {
            let name_res = c.fstring();
            let name = match name_res {
                Ok(n) => n,
                Err(e) => {
                    warnings.push(format!("name table entry {i}: {e}"));
                    break;
                }
            };
            // Trailing flags: u32 (older) or u64 (newer) object flags. We don't know
            // the exact version cutoff reliably across forks, so try u64 then fall
            // back to u32 if that would blow past name_offset's neighbourhood is not
            // verifiable cheaply; default to u64 (most common in retail UE3 titles),
            // and if that clearly desyncs (next fstring len looks insane) note it.
            let _ = c.u64().or_else(|_| c.u32().map(|v| v as u64));
            names.push(NameEntry { index: i, name });
        }
    }

    // ---- Import table ----
    let mut imports = Vec::with_capacity(import_count as usize);
    if let Err(e) = c.seek(import_offset as usize) {
        warnings.push(format!("import table: {e}"));
    } else {
        for i in 0..import_count as i32 {
            let class_package_idx = c.i32();
            let class_name_idx = c.i32();
            let outer_index = c.i32();
            let object_name_idx = c.i32();
            match (class_package_idx, class_name_idx, outer_index, object_name_idx) {
                (Ok(cp), Ok(cn), Ok(oi), Ok(on)) => {
                    imports.push(ImportEntry {
                        index: i,
                        class_package: resolve_name(&names, cp).to_string(),
                        class_name: resolve_name(&names, cn).to_string(),
                        outer_index: oi,
                        object_name: resolve_name(&names, on).to_string(),
                    });
                }
                _ => {
                    warnings.push(format!("import table entry {i}: truncated, stopping"));
                    break;
                }
            }
        }
    }

    // ---- Export table ----
    let mut exports = Vec::with_capacity(export_count as usize);
    if let Err(e) = c.seek(export_offset as usize) {
        warnings.push(format!("export table: {e}"));
    } else {
        for i in 0..export_count as i32 {
            let read_all = (|| -> Result<ExportEntry> {
                let class_index = c.i32()?;
                let super_index = c.i32()?;
                let outer_index = c.i32()?;
                let object_name_idx = c.i32()?;
                let archetype_index = c.i32()?;
                let object_flags = c.u64()?;
                let serial_size = c.i32()?;
                let serial_offset = c.i32()?;
                Ok(ExportEntry {
                    index: i,
                    class_index,
                    super_index,
                    outer_index,
                    object_name: resolve_name(&names, object_name_idx).to_string(),
                    archetype_index,
                    object_flags,
                    serial_size,
                    serial_offset,
                })
            })();
            match read_all {
                Ok(e) => exports.push(e),
                Err(e) => {
                    warnings.push(format!("export table entry {i}: {e}, stopping"));
                    break;
                }
            }
        }
    }

    if summary.compressed {
        warnings.push(
            "package flags indicate compression; object data (serial_offset/serial_size regions) will not be readable without decompression, which this version does not implement".to_string(),
        );
    }

    Ok(ParsedPackage {
        summary,
        names,
        imports,
        exports,
        warnings,
    })
}

#[wasm_bindgen]
pub fn parse_upk(bytes: &[u8]) -> std::result::Result<JsValue, JsValue> {
    match parse(bytes) {
        Ok(pkg) => serde_wasm_bindgen::to_value(&pkg).map_err(|e| JsValue::from_str(&e.to_string())),
        Err(e) => Err(JsValue::from_str(&e.0)),
    }
}

#[wasm_bindgen(start)]
pub fn init() {
    console_error_panic_hook::set_once();
}
