import init, { parse_upk } from "./pkg/upk_wasm.js";

const drop = document.getElementById("drop");
const fileInput = document.getElementById("file");
const zipInput = document.getElementById("zipFile");
const btnFile = document.getElementById("btnFile");
const btnZip = document.getElementById("btnZip");
const zipPicker = document.getElementById("zipPicker");
const zipName = document.getElementById("zipName");
const zipEntryList = document.getElementById("zipEntryList");
const zipCancel = document.getElementById("zipCancel");
const layout = document.getElementById("layout");
const listEl = document.getElementById("list");
const detailEl = document.getElementById("detail");
const tabButtons = document.querySelectorAll(".tab button");

const PACKAGE_EXT = /\.(upk|udk|u|umap)$/i;

let pkg = null;
let activeTab = "names";
let currentLabel = "";

await init();

btnFile.addEventListener("click", () => fileInput.click());
btnZip.addEventListener("click", () => zipInput.click());
zipCancel.addEventListener("click", () => {
  zipPicker.style.display = "none";
  drop.style.display = "block";
});

drop.addEventListener("dragover", (e) => { e.preventDefault(); drop.classList.add("drag"); });
drop.addEventListener("dragleave", () => drop.classList.remove("drag"));
drop.addEventListener("drop", (e) => {
  e.preventDefault();
  drop.classList.remove("drag");
  if (!e.dataTransfer.files.length) return;
  const f = e.dataTransfer.files[0];
  if (f.name.toLowerCase().endsWith(".zip")) {
    handleZip(f);
  } else {
    loadFile(f);
  }
});

fileInput.addEventListener("change", () => {
  if (fileInput.files.length) loadFile(fileInput.files[0]);
  fileInput.value = "";
});

zipInput.addEventListener("change", () => {
  if (zipInput.files.length) handleZip(zipInput.files[0]);
  zipInput.value = "";
});

tabButtons.forEach((btn) => {
  btn.addEventListener("click", () => {
    tabButtons.forEach((b) => b.classList.remove("active"));
    btn.classList.add("active");
    activeTab = btn.dataset.tab;
    renderList();
    detailEl.innerHTML = "<p style='color:var(--muted)'>Select an entry from the list.</p>";
  });
});

async function handleZip(file) {
  let zip;
  try {
    zip = await JSZip.loadAsync(file);
  } catch (e) {
    showError(file.name, "Could not open ZIP: " + e);
    return;
  }
  const entries = Object.values(zip.files).filter(
    (f) => !f.dir && PACKAGE_EXT.test(f.name)
  );
  if (entries.length === 0) {
    showError(file.name, "No .upk / .udk / .u / .umap files found inside this ZIP.");
    return;
  }
  if (entries.length === 1) {
    const buf = new Uint8Array(await entries[0].async("arraybuffer"));
    loadBytes(buf, `${file.name} → ${entries[0].name}`);
    return;
  }
  drop.style.display = "none";
  zipPicker.style.display = "block";
  zipName.textContent = file.name;
  zipEntryList.innerHTML = "";
  entries.forEach((entry) => {
    const div = document.createElement("div");
    div.className = "zip-entry";
    const sizeKb = entry._data ? Math.round(entry._data.uncompressedSize / 1024) : null;
    div.innerHTML = `<span>${escapeHtml(entry.name)}</span><span class="size">${sizeKb !== null ? sizeKb + " KB" : ""}</span>`;
    div.addEventListener("click", async () => {
      const buf = new Uint8Array(await entry.async("arraybuffer"));
      zipPicker.style.display = "none";
      loadBytes(buf, `${file.name} → ${entry.name}`);
    });
    zipEntryList.appendChild(div);
  });
}

async function loadFile(file) {
  const buf = new Uint8Array(await file.arrayBuffer());
  loadBytes(buf, file.name);
}

function loadBytes(buf, label) {
  try {
    pkg = parse_upk(buf);
  } catch (e) {
    showError(label, e);
    return;
  }
  currentLabel = label;
  drop.style.display = "none";
  zipPicker.style.display = "none";
  layout.classList.add("active");
  renderList();
  renderSummary(label);
}

function showError(label, e) {
  layout.classList.remove("active");
  zipPicker.style.display = "none";
  drop.style.display = "block";
  drop.querySelector("p").outerHTML = `<div class="err">Failed to parse "${escapeHtml(label)}": ${escapeHtml(String(e))}</div><p>Try another file.</p>`;
}

function renderSummary(label) {
  const s = pkg.summary;
  let html = `<h2>Package Summary</h2><div class="kv">`;
  const rows = [
    ["Source", label],
    ["File version", s.file_version],
    ["Licensee version", s.licensee_version],
    ["Likely UDK", s.is_udk ? "yes (heuristic)" : "no / unknown"],
    ["Total header size", s.total_header_size],
    ["Folder name", s.folder_name || "(none)"],
    ["Package flags", "0x" + s.package_flags.toString(16)],
    ["Compressed", s.compressed ? "yes" : "no"],
    ["Compression flags", "0x" + s.compression_flags.toString(16)],
    ["Compressed chunk count", s.compressed_chunk_count],
    ["Name count", s.name_count],
    ["Export count", s.export_count],
    ["Import count", s.import_count],
    ["Depends offset", s.depends_offset],
    ["Engine version", s.engine_version],
    ["Cooker version", s.cooker_version],
  ];
  for (const [k, v] of rows) {
    html += `<div class="k">${k}</div><div>${escapeHtml(String(v))}</div>`;
  }
  html += `</div>`;
  if (pkg.warnings && pkg.warnings.length) {
    html += pkg.warnings.map((w) => `<div class="warn">${escapeHtml(w)}</div>`).join("");
  }
  detailEl.innerHTML = html;
}

function renderList() {
  if (!pkg) return;
  const items = pkg[activeTab] || [];
  listEl.innerHTML = "";
  items.forEach((item, i) => {
    const div = document.createElement("div");
    div.className = "list-item";
    const name = item.name ?? item.object_name ?? "";
    div.innerHTML = `<span>${escapeHtml(name)}</span><span class="idx">#${item.index ?? i}</span>`;
    div.addEventListener("click", () => renderDetail(item));
    listEl.appendChild(div);
  });
}

function renderDetail(item) {
  let html = `<h2>${escapeHtml(item.name ?? item.object_name ?? "")}</h2><div class="kv">`;
  for (const [k, v] of Object.entries(item)) {
    html += `<div class="k">${escapeHtml(k)}</div><div>${escapeHtml(String(v))}</div>`;
  }
  html += `</div><p><a href="#" id="backToSummary">&larr; back to package summary</a></p>`;
  detailEl.innerHTML = html;
  document.getElementById("backToSummary").addEventListener("click", (e) => {
    e.preventDefault();
    renderSummary(currentLabel);
  });
}

function escapeHtml(str) {
  return str.replace(/[&<>"']/g, (c) => ({
    "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;",
  }[c]));
}
