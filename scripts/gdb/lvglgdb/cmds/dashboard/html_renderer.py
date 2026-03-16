import json
from pathlib import Path

_STATIC_DIR = Path(__file__).parent / "static"


def render(data: dict, output_path: str) -> None:
    """Generate self-contained HTML with JSON data embedded."""
    json_str = _safe_json_encode(data)
    html = _build_html(json_str)
    with open(output_path, "w", encoding="utf-8") as f:
        f.write(html)


def render_viewer(output_path: str) -> None:
    """Generate empty shell HTML viewer (no embedded data)."""
    html = _build_html("")
    with open(output_path, "w", encoding="utf-8") as f:
        f.write(html)


def _safe_json_encode(data: dict) -> str:
    """Serialize dict to JSON with HTML-safe escaping."""
    raw = json.dumps(data, ensure_ascii=False, indent=None)
    # Escape HTML-special chars to prevent injection in <script> block
    return raw.replace("&", "\\u0026").replace("<", "\\u003c").replace(">", "\\u003e")


# Anchor ID prefix mapping for cross-reference link generation
ANCHOR_PREFIXES = {
    "displays": "disp",
    "animations": "anim",
    "timers": "timer",
    "image_cache": "imgcache",
    "image_header_cache": "imghdr",
    "indevs": "indev",
    "groups": "group",
    "draw_units": "drawunit",
    "draw_tasks": "drawtask",
    "subjects": "subject",
    "image_decoders": "decoder",
    "fs_drivers": "fsdrv",
}

# Cross-reference field -> target anchor prefix mapping
XREF_TARGET = {
    "parent_addr": "obj",
    "group_addr": "group",
    "display_addr": "disp",
    "read_timer_addr": "timer",
    "focused_addr": "obj",
    "var_addr": "obj",
    "user_data_addr": "obj",
    "subject_addr": "subject",
    "target_addr": "obj",
    "decoded_addr": "imgcache",
}


def _read_static(filename: str) -> str:
    """Read a static asset file from the static/ directory."""
    return (_STATIC_DIR / filename).read_text(encoding="utf-8")


def _build_html(json_content: str) -> str:
    """Build the complete HTML page with embedded or empty JSON."""
    template = _read_static("template.html")
    css = _read_static("style.css")
    js = _read_static("dashboard.js")
    return (
        template
        .replace("{{CSS}}", css)
        .replace("{{JS}}", js)
        .replace("{{JSON_DATA}}", json_content)
    )
