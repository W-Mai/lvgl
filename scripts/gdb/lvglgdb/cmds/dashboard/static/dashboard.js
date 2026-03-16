(function() {
"use strict";

/* --- Cross-reference helpers --- */
const XREF_TARGET = {
  parent_addr: "obj", group_addr: "group", display_addr: "disp",
  read_timer_addr: "timer", focused_addr: "obj", var_addr: "obj",
  user_data_addr: "obj", subject_addr: "subject", target_addr: "obj",
  decoded_addr: "imgcache"
};

/* Section registry: key, icon, title, panelClass, color accent */
const SECTIONS = [
  { key: "displays",           icon: "🖥", title: "Displays",           cls: "panel-displays" },
  { key: "object_trees",       icon: "🌳", title: "Object Trees",       cls: "panel-obj-trees" },
  { key: "animations",         icon: "🎬", title: "Animations",         cls: "panel-animations" },
  { key: "timers",             icon: "⏱",  title: "Timers",             cls: "panel-timers" },
  { key: "image_cache",        icon: "🖼", title: "Image Cache",        cls: "panel-img-cache" },
  { key: "image_header_cache", icon: "📋", title: "Header Cache",       cls: "panel-hdr-cache" },
  { key: "indevs",             icon: "🕹", title: "Input Devices",      cls: "panel-indevs" },
  { key: "groups",             icon: "👥", title: "Groups",             cls: "panel-groups" },
  { key: "draw_units",         icon: "🎨", title: "Draw Units",         cls: "panel-draw-units" },
  { key: "draw_tasks",         icon: "📝", title: "Draw Tasks",         cls: "panel-draw-tasks" },
  { key: "subjects",           icon: "📡", title: "Subjects",           cls: "panel-subjects" },
  { key: "image_decoders",     icon: "🔓", title: "Decoders",           cls: "panel-decoders" },
  { key: "fs_drivers",         icon: "💾", title: "FS Drivers",         cls: "panel-fs-drivers" },
];

/* Stat card definitions: label, dataKey, icon, colorClass */
const STAT_DEFS = [
  { label: "Displays",   key: "displays",     icon: "🖥", color: "blue" },
  { label: "Objects",    key: "_objects",      icon: "🌳", color: "green" },
  { label: "Animations", key: "animations",    icon: "🎬", color: "mauve" },
  { label: "Timers",     key: "timers",        icon: "⏱",  color: "peach" },
  { label: "Img Cache",  key: "image_cache",   icon: "🖼", color: "teal" },
  { label: "Input Devs", key: "indevs",        icon: "🕹", color: "pink" },
];

function el(tag, cls, text) {
  const e = document.createElement(tag);
  if (cls) e.className = cls;
  if (text !== undefined) e.textContent = text;
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

function countObjects(trees) {
  let n = 0;
  function walk(obj) { n++; if (obj.children) obj.children.forEach(walk); }
  trees.forEach(t => (t.screens || []).forEach(walk));
  return n;
}

/* --- Generic table builder --- */
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
          const c = document.createElement("th"); c.textContent = h; th.appendChild(c);
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

/* --- Panel factory --- */
function makePanel(cls, icon, title, count) {
  const panel = el("div", "panel " + cls);
  panel.id = "sec-" + cls.replace("panel-", "");
  const hdr = el("div", "panel-header");
  hdr.appendChild(el("span", "panel-icon", icon));
  hdr.appendChild(el("span", "panel-title", title));
  if (count !== undefined) {
    hdr.appendChild(el("span", "panel-badge", String(count)));
  }
  panel.appendChild(hdr);
  const body = el("div", "panel-body");
  panel.appendChild(body);
  return { panel, body };
}

function emptyMsg() { return el("p", "empty", "No entries."); }

/* --- Stat card factory --- */
function makeStatPanel(icon, label, value, colorClass) {
  const panel = el("div", "panel panel-stat");
  const body = el("div", "stat-mini");
  const iconWrap = el("div", "stat-icon-wrap " + colorClass, icon);
  body.appendChild(iconWrap);
  const info = el("div", "stat-info");
  info.appendChild(el("div", "stat-value", String(value)));
  info.appendChild(el("div", "stat-label", label));
  body.appendChild(info);
  panel.appendChild(body);
  return panel;
}

/* --- Section builders --- */
function buildDisplays(data) {
  const items = data.displays || [];
  const { panel, body } = makePanel("panel-displays", "🖥", "Displays", items.length);
  if (items.length === 0) { body.appendChild(emptyMsg()); return panel; }
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
      const b = d[bk]; if (!b) return;
      const bc = el("div", "buf-card");
      bc.appendChild(el("div", "buf-label", bk.replace("_", " ")));
      bc.appendChild(el("div", "buf-info", b.width + "×" + b.height + "  " + b.color_format));
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
  return panel;
}

function buildObjectTrees(data) {
  const trees = data.object_trees || [];
  const screenCount = trees.reduce((n, t) => n + (t.screens ? t.screens.length : 0), 0);
  const { panel, body } = makePanel("panel-obj-trees", "🌳", "Object Trees", screenCount);
  if (trees.length === 0) { body.appendChild(emptyMsg()); return panel; }
  trees.forEach(tree => {
    body.appendChild(el("div", "disp-addr", "Display: " + tree.display_addr));
    tree.screens.forEach(s => body.appendChild(renderObjTree(s)));
  });
  return panel;
}

function buildSimpleTable(data, key, cls, icon, title, headers, anchorPrefix) {
  const items = data[key] || [];
  const { panel, body } = makePanel(cls, icon, title, items.length);
  body.appendChild(makeTable(headers, items, anchorPrefix));
  return panel;
}

function buildImageCache(data) {
  const entries = data.image_cache || [];
  const { panel, body } = makePanel("panel-img-cache", "🖼", "Image Cache", entries.length);
  if (entries.length === 0) { body.appendChild(emptyMsg()); return panel; }
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
  return panel;
}

function buildSubjects(data) {
  const subjects = data.subjects || [];
  const { panel, body } = makePanel("panel-subjects", "📡", "Subjects", subjects.length);
  if (subjects.length === 0) { body.appendChild(emptyMsg()); return panel; }
  subjects.forEach(s => {
    const card = el("div", "subject-card");
    if (s.addr) card.id = "subject-" + s.addr;
    const hdr = el("div", "subject-header");
    hdr.appendChild(el("span", "subject-addr", s.addr || ""));
    hdr.appendChild(el("span", "subject-type", s.type_name));
    hdr.appendChild(document.createTextNode("  " + (s.observers||[]).length + " observers"));
    card.appendChild(hdr);
    if (s.observers && s.observers.length > 0) {
      const t = document.createElement("table");
      const th = t.createTHead().insertRow();
      ["addr","cb","target_addr","for_obj"].forEach(h => {
        const c = document.createElement("th"); c.textContent = h; th.appendChild(c);
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
  return panel;
}

/* --- Top bar nav builder --- */
function buildTopNav(data) {
  const nav = document.getElementById("topbar-nav");
  nav.innerHTML = "";
  SECTIONS.forEach(s => {
    const count = s.key === "object_trees"
      ? (data[s.key]||[]).reduce((n,t) => n + (t.screens?t.screens.length:0), 0)
      : (data[s.key]||[]).length;
    if (count === 0) return;
    const a = el("a", "", s.icon + " " + count);
    a.href = "#sec-" + s.cls.replace("panel-", "");
    a.title = s.title + " (" + count + ")";
    nav.appendChild(a);
  });
}

/* --- Main render --- */
function renderDashboard(data) {
  const meta = document.getElementById("header-meta");
  meta.innerHTML = "";
  meta.appendChild(document.createTextNode(data.meta?.timestamp || ""));
  if (data.meta?.lvgl_version) {
    meta.appendChild(el("span", "version-tag", "LVGL " + data.meta.lvgl_version));
  }

  buildTopNav(data);

  const grid = document.getElementById("bento-grid");
  grid.innerHTML = "";

  /* Stat cards row */
  const objCount = countObjects(data.object_trees || []);
  STAT_DEFS.forEach(s => {
    const val = s.key === "_objects" ? objCount : (data[s.key]||[]).length;
    grid.appendChild(makeStatPanel(s.icon, s.label, val, s.color));
  });

  /* Main panels in bento layout order */
  grid.appendChild(buildDisplays(data));
  grid.appendChild(buildObjectTrees(data));

  grid.appendChild(buildSimpleTable(data, "animations", "panel-animations", "🎬", "Animations",
    ["addr","var","exec_cb","start_value","current_value","end_value",
     "duration","act_time","repeat_cnt","status","var_addr"], "anim"));
  grid.appendChild(buildSimpleTable(data, "timers", "panel-timers", "⏱", "Timers",
    ["addr","timer_cb","period","frequency","last_run","repeat_count",
     "paused","user_data_addr"], "timer"));
  grid.appendChild(buildSimpleTable(data, "indevs", "panel-indevs", "🕹", "Input Devices",
    ["addr","type_name","enabled","read_cb","long_press_time","scroll_limit",
     "display_addr","group_addr","read_timer_addr"], "indev"));

  grid.appendChild(buildImageCache(data));
  grid.appendChild(buildSimpleTable(data, "groups", "panel-groups", "👥", "Groups",
    ["addr","obj_count","frozen","editing","wrap","focused_addr","member_addrs"], "group"));
  grid.appendChild(buildSimpleTable(data, "draw_units", "panel-draw-units", "🎨", "Draw Units",
    ["addr","name","idx"], "drawunit"));

  grid.appendChild(buildSimpleTable(data, "image_header_cache",
    "panel-hdr-cache", "📋", "Header Cache",
    ["entry_addr","src","size","cf","ref_count","src_type","decoder_name"], "imghdr"));
  grid.appendChild(buildSimpleTable(data, "draw_tasks", "panel-draw-tasks", "📝", "Draw Tasks",
    ["addr","type_name","state_name","area","opa","preferred_draw_unit_id"], "drawtask"));
  grid.appendChild(buildSubjects(data));

  grid.appendChild(buildSimpleTable(data, "image_decoders",
    "panel-decoders", "🔓", "Decoders",
    ["addr","name","info_cb","open_cb","close_cb"], "decoder"));
  grid.appendChild(buildSimpleTable(data, "fs_drivers", "panel-fs-drivers", "💾", "FS Drivers",
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
    document.getElementById("bento-grid").textContent =
      "Failed to parse embedded JSON: " + e.message;
  }
} else {
  const dropZone = document.getElementById("drop-zone");
  dropZone.style.display = "block";
  const fileInput = document.getElementById("file-input");

  dropZone.addEventListener("dragover", e => {
    e.preventDefault(); dropZone.classList.add("dragover");
  });
  dropZone.addEventListener("dragleave", () => dropZone.classList.remove("dragover"));
  dropZone.addEventListener("drop", e => {
    e.preventDefault(); dropZone.classList.remove("dragover");
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
      } catch(e) { alert("Invalid JSON file: " + e.message); }
    };
    reader.readAsText(file);
  }
}

/* --- Search / filter --- */
document.getElementById("search").addEventListener("input", function() {
  const q = this.value.toLowerCase();
  document.querySelectorAll(".panel").forEach(p => {
    if (p.classList.contains("panel-stat")) return;
    if (!q) { p.classList.remove("hidden"); return; }
    p.classList.toggle("hidden", !p.textContent.toLowerCase().includes(q));
  });
});

/* --- Active nav tracking via IntersectionObserver --- */
const observer = new IntersectionObserver(entries => {
  entries.forEach(entry => {
    const id = entry.target.id;
    if (!id) return;
    const link = document.querySelector('.topbar-nav a[href="#' + id + '"]');
    if (link) link.classList.toggle("active", entry.isIntersecting);
  });
}, { rootMargin: "-20% 0px -70% 0px" });

new MutationObserver(() => {
  document.querySelectorAll(".panel[id]").forEach(p => observer.observe(p));
}).observe(document.getElementById("bento-grid"), { childList: true });

/* --- Theme toggle (dark/light) --- */
(function initTheme() {
  const btn = document.getElementById("theme-toggle");
  const root = document.documentElement;
  const STORAGE_KEY = "lvgl-dash-theme";

  function applyTheme(theme) {
    root.setAttribute("data-theme", theme);
    btn.textContent = theme === "light" ? "☀️" : "🌙";
    btn.title = theme === "light" ? "Switch to dark theme" : "Switch to light theme";
  }

  /* Restore from localStorage, or follow system preference */
  const saved = localStorage.getItem(STORAGE_KEY);
  if (saved) {
    applyTheme(saved);
  } else if (window.matchMedia("(prefers-color-scheme: light)").matches) {
    applyTheme("light");
  }

  btn.addEventListener("click", () => {
    const next = root.getAttribute("data-theme") === "light" ? "dark" : "light";
    applyTheme(next);
    localStorage.setItem(STORAGE_KEY, next);
  });

  /* Follow system changes when no manual override */
  window.matchMedia("(prefers-color-scheme: light)").addEventListener("change", e => {
    if (!localStorage.getItem(STORAGE_KEY)) {
      applyTheme(e.matches ? "light" : "dark");
    }
  });
})();

})();
