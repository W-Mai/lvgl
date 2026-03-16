(function() {
"use strict";

/* --- Cross-reference helpers --- */
const XREF_TARGET = {
  parent_addr: "obj", group_addr: "group", display_addr: "disp",
  read_timer_addr: "timer", focused_addr: "obj", var_addr: "obj",
  user_data_addr: "obj", subject_addr: "subject", target_addr: "obj",
  decoded_addr: "imgcache"
};

function xref(addr, prefix) {
  if (!addr || addr === "None" || addr === "0x0") return addr || "-";
  const a = document.createElement("a");
  a.className = "xref";
  a.href = "#" + prefix + "-" + addr;
  a.textContent = addr;
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
  if (!rows || rows.length === 0) {
    const p = document.createElement("p");
    p.className = "empty";
    p.textContent = "No entries.";
    return p;
  }
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
  return tbl;
}

/* --- Object tree rendering --- */
function renderObjTree(obj) {
  const det = document.createElement("details");
  det.className = "obj-node";
  if (obj.addr) det.id = "obj-" + obj.addr;
  const sum = document.createElement("summary");
  const c = obj.coords || {};
  sum.textContent = (obj.class_name || "obj") + "@" + (obj.addr || "?") +
    " " + (c.x1||0) + "," + (c.y1||0) + "," + (c.x2||0) + "," + (c.y2||0) +
    " children=" + (obj.child_count||0) + " styles=" + (obj.style_count||0);
  det.appendChild(sum);

  if (obj.parent_addr) {
    const p = document.createElement("div");
    p.style.fontSize = "12px";
    p.style.marginLeft = "20px";
    p.appendChild(document.createTextNode("parent: "));
    p.appendChild(xref(obj.parent_addr, "obj"));
    if (obj.group_addr) {
      p.appendChild(document.createTextNode(" group: "));
      p.appendChild(xref(obj.group_addr, "group"));
    }
    det.appendChild(p);
  }

  if (obj.styles && obj.styles.length > 0) {
    const sp = document.createElement("div");
    sp.className = "style-panel";
    obj.styles.forEach(s => {
      const title = document.createElement("div");
      title.textContent = "[" + s.index + "] " + s.selector_str + "  " + s.flags_str;
      title.style.fontWeight = "bold";
      sp.appendChild(title);
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

/* --- Section builders --- */
function buildDisplays(data) {
  const sec = makeSection("sec-displays", "Displays");
  if (!data.displays || data.displays.length === 0) { sec.appendChild(emptyMsg()); return sec; }
  data.displays.forEach(d => {
    const div = document.createElement("div");
    if (d.addr) div.id = "disp-" + d.addr;
    div.innerHTML = "<b>" + (d.addr||"") + "</b> " + d.hor_res + "x" + d.ver_res +
      " screens=" + d.screen_count;
    ["buf_1", "buf_2"].forEach(bk => {
      const b = d[bk];
      if (b && b.image_base64) {
        const img = document.createElement("img");
        img.className = "buf-img";
        img.src = "data:image/png;base64," + b.image_base64;
        img.alt = bk + " " + b.width + "x" + b.height + " " + b.color_format;
        div.appendChild(document.createElement("br"));
        div.appendChild(document.createTextNode(
          bk + ": " + b.width + "x" + b.height + " " + b.color_format));
        div.appendChild(document.createElement("br"));
        div.appendChild(img);
      } else if (b) {
        div.appendChild(document.createElement("br"));
        div.appendChild(document.createTextNode(
          bk + ": " + b.width + "x" + b.height + " " + b.color_format + " (no image)"));
      }
    });
    sec.appendChild(div);
  });
  return sec;
}

function buildObjectTrees(data) {
  const sec = makeSection("sec-object-trees", "Object Trees");
  if (!data.object_trees || data.object_trees.length === 0) {
    sec.appendChild(emptyMsg());
    return sec;
  }
  data.object_trees.forEach(tree => {
    const label = document.createElement("div");
    label.textContent = "Display: " + tree.display_addr;
    label.style.fontWeight = "bold";
    label.style.marginBottom = "4px";
    sec.appendChild(label);
    tree.screens.forEach(s => sec.appendChild(renderObjTree(s)));
  });
  return sec;
}

function buildSimpleTable(data, key, sectionId, title, headers, anchorPrefix) {
  const sec = makeSection(sectionId, title);
  sec.appendChild(makeTable(headers, data[key] || [], anchorPrefix));
  return sec;
}

function buildSubjects(data) {
  const sec = makeSection("sec-subjects", "Subjects");
  const subjects = data.subjects || [];
  if (subjects.length === 0) { sec.appendChild(emptyMsg()); return sec; }
  subjects.forEach(s => {
    const div = document.createElement("div");
    if (s.addr) div.id = "subject-" + s.addr;
    div.innerHTML = "<b>Subject " + (s.addr||"") + "</b> type=" + s.type_name +
      " observers=" + (s.observers||[]).length;
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
      div.appendChild(t);
    }
    sec.appendChild(div);
  });
  return sec;
}

function buildImageCache(data) {
  const sec = makeSection("sec-image-cache", "Image Cache");
  const entries = data.image_cache || [];
  if (entries.length === 0) { sec.appendChild(emptyMsg()); return sec; }
  entries.forEach(e => {
    const div = document.createElement("div");
    if (e.entry_addr) div.id = "imgcache-" + e.entry_addr;
    div.innerHTML = "<b>" + (e.entry_addr||"") + "</b> " + e.src + " " + e.size +
      " cf=" + e.cf + " rc=" + e.ref_count + " decoder=" + e.decoder_name;
    if (e.preview_base64) {
      const img = document.createElement("img");
      img.className = "buf-img";
      img.src = "data:image/png;base64," + e.preview_base64;
      img.alt = "decoded preview";
      div.appendChild(document.createElement("br"));
      div.appendChild(img);
    }
    sec.appendChild(div);
  });
  return sec;
}

/* --- Helpers --- */
function makeSection(id, title) {
  const sec = document.createElement("section");
  sec.id = id;
  const h = document.createElement("h3");
  h.textContent = title;
  sec.appendChild(h);
  return sec;
}

function emptyMsg() {
  const p = document.createElement("p");
  p.className = "empty";
  p.textContent = "No entries.";
  return p;
}

/* --- Main render --- */
function renderDashboard(data) {
  const hdr = document.getElementById("header-bar");
  hdr.textContent = "Generated: " + (data.meta?.timestamp || "unknown") +
    (data.meta?.lvgl_version ? "  |  LVGL " + data.meta.lvgl_version : "");

  const dash = document.getElementById("dashboard");
  dash.innerHTML = "";

  dash.appendChild(buildDisplays(data));
  dash.appendChild(buildObjectTrees(data));
  dash.appendChild(buildSimpleTable(data, "animations", "sec-animations", "Animations",
    ["addr","var","exec_cb","start_value","current_value","end_value",
     "duration","act_time","repeat_cnt","status","var_addr"], "anim"));
  dash.appendChild(buildSimpleTable(data, "timers", "sec-timers", "Timers",
    ["addr","timer_cb","period","frequency","last_run","repeat_count",
     "paused","user_data_addr"], "timer"));
  dash.appendChild(buildImageCache(data));
  dash.appendChild(buildSimpleTable(data, "image_header_cache",
    "sec-image-header-cache", "Image Header Cache",
    ["entry_addr","src","size","cf","ref_count","src_type","decoder_name"], "imghdr"));
  dash.appendChild(buildSimpleTable(data, "indevs", "sec-indevs", "Input Devices",
    ["addr","type_name","enabled","read_cb","long_press_time","scroll_limit",
     "display_addr","group_addr","read_timer_addr"], "indev"));
  dash.appendChild(buildSimpleTable(data, "groups", "sec-groups", "Groups",
    ["addr","obj_count","frozen","editing","wrap","focused_addr","member_addrs"], "group"));
  dash.appendChild(buildSimpleTable(data, "draw_units", "sec-draw-units", "Draw Units",
    ["addr","name","idx"], "drawunit"));
  dash.appendChild(buildSimpleTable(data, "draw_tasks", "sec-draw-tasks", "Draw Tasks",
    ["addr","type_name","state_name","area","opa","preferred_draw_unit_id"], "drawtask"));
  dash.appendChild(buildSubjects(data));
  dash.appendChild(buildSimpleTable(data, "image_decoders", "sec-image-decoders",
    "Image Decoders", ["addr","name","info_cb","open_cb","close_cb"], "decoder"));
  dash.appendChild(buildSimpleTable(data, "fs_drivers", "sec-fs-drivers", "FS Drivers",
    ["addr","letter","driver_name","cache_size","open_cb","read_cb",
     "write_cb","close_cb"], "fsdrv"));
}

/* --- Boot logic --- */
const jsonEl = document.getElementById("lvgl-data");
const raw = jsonEl ? jsonEl.textContent.trim() : "";

if (raw) {
  try {
    const data = JSON.parse(raw);
    renderDashboard(data);
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
    const file = e.dataTransfer.files[0];
    if (file) loadFile(file);
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

})();
