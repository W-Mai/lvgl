(function() {
"use strict";

/* --- Cross-reference helpers --- */
const XREF_TARGET = {
  parent_addr: "obj", group_addr: "group", display_addr: "disp",
  read_timer_addr: "timer", focused_addr: "obj", var_addr: "obj",
  user_data_addr: "obj", subject_addr: "subject", target_addr: "obj",
  decoded_addr: "imgcache"
};

/* Section metadata: key, icon, title */
const SECTIONS = [
  { key: "displays",           icon: "🖥", title: "Displays" },
  { key: "object_trees",       icon: "🌳", title: "Object Trees" },
  { key: "animations",         icon: "🎬", title: "Animations" },
  { key: "timers",             icon: "⏱",  title: "Timers" },
  { key: "image_cache",        icon: "🖼", title: "Image Cache" },
  { key: "image_header_cache", icon: "📋", title: "Image Header Cache" },
  { key: "indevs",             icon: "🕹", title: "Input Devices" },
  { key: "groups",             icon: "👥", title: "Groups" },
  { key: "draw_units",         icon: "🎨", title: "Draw Units" },
  { key: "draw_tasks",         icon: "📝", title: "Draw Tasks" },
  { key: "subjects",           icon: "📡", title: "Subjects" },
  { key: "image_decoders",     icon: "🔓", title: "Image Decoders" },
  { key: "fs_drivers",         icon: "💾", title: "FS Drivers" },
];

function el(tag, cls, text) {
  const e = document.createElement(tag);
  if (cls) e.className = cls;
  if (text) e.textContent = text;
  return e;
}

function xref(addr, prefix) {
  if (!addr || addr === "None" || addr === "0x0") return document.createTextNode(addr || "-");
  const a = el("a", "xref", addr);
  a.href = "#" + prefix + "-" + addr;
  return a;
}

function xrefCell(td, key, val) {
  const prefix = XREF_TARGET[key];
  if (prefix && val && val !== "None" && val !== "0x0") {
    td.appendChild(xref(val, prefix));
  } else {
    td.textContent = val != null ? String(val) : "-";
  }
}

function makeTable(headers, rows, anchorPrefix) {
  if (!rows || rows.length === 0) return el("p", "empty", "No entries.");
  const wrap = el("div", "table-wrap");
  const tbl = document.createElement("table");
  const thead = tbl.createTHead();
  const hr = thead.insertRow();
  headers.forEach(h => {
    const th = document.createElement("th");
    th.textContent = h;
    hr.appendChild(th);
  });
  const tbody = tbl.createTBody();
  rows.forEach(row => {
    const tr = tbody.insertRow();
    if (anchorPrefix && row.addr) tr.id = anchorPrefix + "-" + row.addr;
    headers.forEach(h => {
      const key = h.toLowerCase().replace(/ /g, "_");
      const td = tr.insertCell();
      const val = row[key];
      if (key === "area" && typeof val === "object") {
        td.textContent = "(" + val.x1 + "," + val.y1 + "," + val.x2 + "," + val.y2 + ")";
      } else if (key === "member_addrs" && Array.isArray(val)) {
        val.forEach((a, i) => {
          if (i > 0) td.appendChild(document.createTextNode(", "));
          td.appendChild(xref(a, "obj"));
        });
      } else if (key === "observer_addrs" && Array.isArray(val)) {
        val.forEach((a, i) => {
          if (i > 0) td.appendChild(document.createTextNode(", "));
          td.appendChild(xref(a, "observer"));
        });
      } else {
        xrefCell(td, key, val != null ? String(val) : "-");
      }
    });
  });
  wrap.appendChild(tbl);
  return wrap;
}

/* --- Object tree rendering --- */
function renderObjTree(obj) {
  const det = document.createElement("details");
  det.className = "obj-node";
  if (obj.addr) det.id = "obj-" + obj.addr;
  const sum = document.createElement("summary");
  const c = obj.coords || {};
  sum.textContent = (obj.class_name || "obj") + "@" + (obj.addr || "?") +
    "  [" + (c.x1||0) + "," + (c.y1||0) + "," + (c.x2||0) + "," + (c.y2||0) + "]" +
    "  children=" + (obj.child_count||0) + "  styles=" + (obj.style_count||0);
  det.appendChild(sum);

  if (obj.parent_addr) {
    const meta = el("div", "obj-meta");
    meta.appendChild(document.createTextNode("parent: "));
    meta.appendChild(xref(obj.parent_addr, "obj"));
    if (obj.group_addr) {
      meta.appendChild(document.createTextNode("  group: "));
      meta.appendChild(xref(obj.group_addr, "group"));
    }
    det.appendChild(meta);
  }

  if (obj.styles && obj.styles.length > 0) {
    const sp = el("div", "style-panel");
    obj.styles.forEach(s => {
      sp.appendChild(el("div", "style-header",
        "[" + s.index + "] " + s.selector_str + "  " + s.flags_str));
      if (s.properties && s.properties.length > 0) {
        const t = document.createElement("table");
        const th = t.createTHead().insertRow();
        ["prop", "value"].forEach(h => {
          const c = document.createElement("th");
          c.textContent = h;
          th.appendChild(c);
        });
        const tb = t.createTBody();
        s.properties.forEach(p => {
          const r = tb.insertRow();
          r.insertCell().textContent = p.prop_name;
          r.insertCell().textContent = p.value_str;
        });
        sp.appendChild(t);
      }
    });
    det.appendChild(sp);
  }

  if (obj.children) obj.children.forEach(ch => det.appendChild(renderObjTree(ch)));
  return det;
}

/* --- Section wrapper --- */
function makeSection(id, icon, title, count) {
  const sec = document.createElement("section");
  sec.id = id;
  const h = document.createElement("h3");
  h.innerHTML = '<span class="sec-icon">' + icon + '</span> ' + title;
  if (count !== undefined) {
    const badge = el("span", "sec-count", String(count));
    h.appendChild(badge);
  }
  sec.appendChild(h);
  const body = el("div", "section-body");
  sec.appendChild(body);
  return { sec, body };
}

function emptyMsg() { return el("p", "empty", "No entries."); }

/* --- Section builders --- */
function buildDisplays(data) {
  const items = data.displays || [];
  const { sec, body } = makeSection("sec-displays", "🖥", "Displays", items.length);
  if (items.length === 0) { body.appendChild(emptyMsg()); return sec; }
  items.forEach(d => {
    const card = el("div", "display-card");
    if (d.addr) card.id = "disp-" + d.addr;
    const hdr = el("div", "disp-header");
    hdr.appendChild(el("span", "disp-addr", d.addr || ""));
    hdr.appendChild(el("span", "disp-res", d.hor_res + " × " + d.ver_res));
    hdr.appendChild(el("span", "disp-screens", d.screen_count + " screens"));
    card.appendChild(hdr);

    const bufRow = el("div", "buf-row");
    ["buf_1", "buf_2"].forEach(bk => {
      const b = d[bk];
      if (!b) return;
      const bc = el("div", "buf-card");
      bc.appendChild(el("div", "buf-label", bk.replace("_", " ")));
      bc.appendChild(el("div", "buf-info",
        b.width + "×" + b.height + "  " + b.color_format));
      if (b.image_base64) {
        const img = document.createElement("img");
        img.className = "buf-img";
        img.src = "data:image/png;base64," + b.image_base64;
        img.alt = bk + " " + b.width + "x" + b.height;
        bc.appendChild(img);
      }
      bufRow.appendChild(bc);
    });
    card.appendChild(bufRow);
    body.appendChild(card);
  });
  return sec;
}

function buildObjectTrees(data) {
  const trees = data.object_trees || [];
  const screenCount = trees.reduce((n, t) => n + (t.screens ? t.screens.length : 0), 0);
  const { sec, body } = makeSection("sec-object-trees", "🌳", "Object Trees", screenCount);
  if (trees.length === 0) { body.appendChild(emptyMsg()); return sec; }
  trees.forEach(tree => {
    body.appendChild(el("div", "disp-addr",
      "Display: " + tree.display_addr));
    tree.screens.forEach(s => body.appendChild(renderObjTree(s)));
  });
  return sec;
}

function buildSimpleTable(data, key, sectionId, icon, title, headers, anchorPrefix) {
  const items = data[key] || [];
  const { sec, body } = makeSection(sectionId, icon, title, items.length);
  body.appendChild(makeTable(headers, items, anchorPrefix));
  return sec;
}

function buildSubjects(data) {
  const subjects = data.subjects || [];
  const { sec, body } = makeSection("sec-subjects", "📡", "Subjects", subjects.length);
  if (subjects.length === 0) { body.appendChild(emptyMsg()); return sec; }
  subjects.forEach(s => {
    const card = el("div", "subject-card");
    if (s.addr) card.id = "subject-" + s.addr;
    const hdr = el("div", "subject-header");
    hdr.appendChild(el("span", "subject-addr", s.addr || ""));
    hdr.appendChild(el("span", "subject-type", s.type_name));
    hdr.appendChild(document.createTextNode(
      "  " + (s.observers||[]).length + " observers"));
    card.appendChild(hdr);
    if (s.observers && s.observers.length > 0) {
      const t = document.createElement("table");
      const th = t.createTHead().insertRow();
      ["addr","cb","target_addr","for_obj"].forEach(h => {
        const c = document.createElement("th");
        c.textContent = h;
        th.appendChild(c);
      });
      const tb = t.createTBody();
      s.observers.forEach(o => {
        const r = tb.insertRow();
        if (o.addr) r.id = "observer-" + o.addr;
        r.insertCell().textContent = o.addr || "-";
        r.insertCell().textContent = o.cb || "-";
        const tc = r.insertCell();
        xrefCell(tc, "target_addr", o.target_addr);
        r.insertCell().textContent = String(o.for_obj);
      });
      card.appendChild(t);
    }
    body.appendChild(card);
  });
  return sec;
}

function buildImageCache(data) {
  const entries = data.image_cache || [];
  const { sec, body } = makeSection("sec-image-cache", "🖼", "Image Cache", entries.length);
  if (entries.length === 0) { body.appendChild(emptyMsg()); return sec; }
  entries.forEach(e => {
    const card = el("div", "cache-entry");
    if (e.entry_addr) card.id = "imgcache-" + e.entry_addr;
    const info = el("div", "cache-info");
    info.appendChild(el("div", "cache-addr", e.entry_addr || ""));
    info.appendChild(el("div", "cache-detail",
      e.src + "  " + e.size + "  cf=" + e.cf +
      "  rc=" + e.ref_count + "  decoder=" + e.decoder_name));
    card.appendChild(info);
    if (e.preview_base64) {
      const preview = el("div", "cache-preview");
      const img = document.createElement("img");
      img.src = "data:image/png;base64," + e.preview_base64;
      img.alt = "decoded preview";
      preview.appendChild(img);
      card.appendChild(preview);
    }
    body.appendChild(card);
  });
  return sec;
}

/* --- Stats cards --- */
function buildStats(data) {
  const container = document.getElementById("stats-container");
  const grid = el("div", "stats-grid");
  const stats = [
    { label: "Displays",   value: (data.displays||[]).length,           icon: "🖥" },
    { label: "Objects",     value: countObjects(data.object_trees||[]),  icon: "🌳" },
    { label: "Animations",  value: (data.animations||[]).length,        icon: "🎬" },
    { label: "Timers",      value: (data.timers||[]).length,            icon: "⏱" },
    { label: "Img Cache",   value: (data.image_cache||[]).length,       icon: "🖼" },
    { label: "Input Devs",  value: (data.indevs||[]).length,            icon: "🕹" },
  ];
  stats.forEach(s => {
    const card = el("div", "stat-card");
    card.appendChild(el("span", "stat-icon", s.icon));
    card.appendChild(el("div", "stat-label", s.label));
    card.appendChild(el("div", "stat-value", String(s.value)));
    grid.appendChild(card);
  });
  container.appendChild(grid);
}

function countObjects(trees) {
  let n = 0;
  function walk(obj) { n++; if (obj.children) obj.children.forEach(walk); }
  trees.forEach(t => (t.screens||[]).forEach(walk));
  return n;
}

/* --- Update sidebar badges --- */
function updateBadges(data) {
  const counts = {
    displays: (data.displays||[]).length,
    object_trees: (data.object_trees||[]).reduce(
      (n, t) => n + (t.screens ? t.screens.length : 0), 0),
    animations: (data.animations||[]).length,
    timers: (data.timers||[]).length,
    image_cache: (data.image_cache||[]).length,
    image_header_cache: (data.image_header_cache||[]).length,
    indevs: (data.indevs||[]).length,
    groups: (data.groups||[]).length,
    draw_units: (data.draw_units||[]).length,
    draw_tasks: (data.draw_tasks||[]).length,
    subjects: (data.subjects||[]).length,
    image_decoders: (data.image_decoders||[]).length,
    fs_drivers: (data.fs_drivers||[]).length,
  };
  Object.entries(counts).forEach(([key, val]) => {
    const badge = document.getElementById("badge-" + key);
    if (badge) badge.textContent = String(val);
  });
}

/* --- Main render --- */
function renderDashboard(data) {
  const hdr = document.getElementById("header-bar");
  hdr.innerHTML = "";
  hdr.appendChild(document.createTextNode(
    "Generated: " + (data.meta?.timestamp || "unknown")));
  if (data.meta?.lvgl_version) {
    const tag = el("span", "version-tag", "LVGL " + data.meta.lvgl_version);
    hdr.appendChild(tag);
  }

  updateBadges(data);
  buildStats(data);

  const dash = document.getElementById("dashboard");
  dash.innerHTML = "";

  dash.appendChild(buildDisplays(data));
  dash.appendChild(buildObjectTrees(data));
  dash.appendChild(buildSimpleTable(data, "animations", "sec-animations", "🎬", "Animations",
    ["addr","var","exec_cb","start_value","current_value","end_value",
     "duration","act_time","repeat_cnt","status","var_addr"], "anim"));
  dash.appendChild(buildSimpleTable(data, "timers", "sec-timers", "⏱", "Timers",
    ["addr","timer_cb","period","frequency","last_run","repeat_count",
     "paused","user_data_addr"], "timer"));
  dash.appendChild(buildImageCache(data));
  dash.appendChild(buildSimpleTable(data, "image_header_cache",
    "sec-image-header-cache", "📋", "Image Header Cache",
    ["entry_addr","src","size","cf","ref_count","src_type","decoder_name"], "imghdr"));
  dash.appendChild(buildSimpleTable(data, "indevs", "sec-indevs", "🕹", "Input Devices",
    ["addr","type_name","enabled","read_cb","long_press_time","scroll_limit",
     "display_addr","group_addr","read_timer_addr"], "indev"));
  dash.appendChild(buildSimpleTable(data, "groups", "sec-groups", "👥", "Groups",
    ["addr","obj_count","frozen","editing","wrap","focused_addr","member_addrs"], "group"));
  dash.appendChild(buildSimpleTable(data, "draw_units", "sec-draw-units", "🎨", "Draw Units",
    ["addr","name","idx"], "drawunit"));
  dash.appendChild(buildSimpleTable(data, "draw_tasks", "sec-draw-tasks", "📝", "Draw Tasks",
    ["addr","type_name","state_name","area","opa","preferred_draw_unit_id"], "drawtask"));
  dash.appendChild(buildSubjects(data));
  dash.appendChild(buildSimpleTable(data, "image_decoders", "sec-image-decoders",
    "🔓", "Image Decoders", ["addr","name","info_cb","open_cb","close_cb"], "decoder"));
  dash.appendChild(buildSimpleTable(data, "fs_drivers", "sec-fs-drivers", "💾", "FS Drivers",
    ["addr","letter","driver_name","cache_size","open_cb","read_cb",
     "write_cb","close_cb"], "fsdrv"));
}

/* --- Boot logic --- */
const jsonEl = document.getElementById("lvgl-data");
const raw = jsonEl ? jsonEl.textContent.trim() : "";

if (raw) {
  try {
    renderDashboard(JSON.parse(raw));
  } catch(e) {
    document.getElementById("dashboard").textContent =
      "Failed to parse embedded JSON: " + e.message;
  }
} else {
  const dropZone = document.getElementById("drop-zone");
  dropZone.style.display = "block";
  const fileInput = document.getElementById("file-input");

  dropZone.addEventListener("dragover", e => {
    e.preventDefault();
    dropZone.classList.add("dragover");
  });
  dropZone.addEventListener("dragleave", () => dropZone.classList.remove("dragover"));
  dropZone.addEventListener("drop", e => {
    e.preventDefault();
    dropZone.classList.remove("dragover");
    if (e.dataTransfer.files[0]) loadFile(e.dataTransfer.files[0]);
  });
  dropZone.addEventListener("click", () => fileInput.click());
  fileInput.addEventListener("change", () => {
    if (fileInput.files[0]) loadFile(fileInput.files[0]);
  });

  function loadFile(file) {
    const reader = new FileReader();
    reader.onload = () => {
      try {
        const data = JSON.parse(reader.result);
        dropZone.style.display = "none";
        renderDashboard(data);
      } catch(e) {
        alert("Invalid JSON file: " + e.message);
      }
    };
    reader.readAsText(file);
  }
}

/* --- Search --- */
document.getElementById("search").addEventListener("input", function() {
  const q = this.value.toLowerCase();
  document.querySelectorAll("section").forEach(sec => {
    if (!q) { sec.classList.remove("hidden"); return; }
    sec.classList.toggle("hidden", !sec.textContent.toLowerCase().includes(q));
  });
});

/* --- Active nav tracking --- */
const observer = new IntersectionObserver(entries => {
  entries.forEach(entry => {
    const link = document.querySelector('#nav-list a[href="#' + entry.target.id + '"]');
    if (link) link.classList.toggle("active", entry.isIntersecting);
  });
}, { rootMargin: "-20% 0px -70% 0px" });

document.querySelectorAll("section").forEach(sec => observer.observe(sec));

})();
