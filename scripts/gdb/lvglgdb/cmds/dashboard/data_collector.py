import base64
import traceback
from datetime import datetime, timezone

import gdb


def collect_all() -> dict:
    """Collect all LVGL runtime data into a JSON-compatible dict."""
    from lvglgdb.lvgl import curr_inst

    lvgl = curr_inst()
    version = _get_lvgl_version()

    data = {
        "meta": {
            "timestamp": datetime.now(timezone.utc).isoformat(),
            "lvgl_version": version,
        },
        "displays": _collect_displays(lvgl),
        "object_trees": _collect_object_trees(lvgl),
        "animations": _collect_anims(lvgl),
        "timers": _collect_timers(lvgl),
        "image_cache": _collect_image_cache(lvgl),
        "image_header_cache": _collect_image_header_cache(lvgl),
        "indevs": _collect_indevs(lvgl),
        "groups": _collect_groups(lvgl),
        "draw_units": _collect_draw_units(lvgl),
        "draw_tasks": _collect_draw_tasks(lvgl),
        "subjects": _collect_subjects(lvgl),
        "image_decoders": _collect_image_decoders(lvgl),
        "fs_drivers": _collect_fs_drivers(lvgl),
    }
    return data


def _get_lvgl_version() -> str | None:
    """Try to read LVGL version from macros."""
    try:
        major = int(gdb.parse_and_eval("LVGL_VERSION_MAJOR"))
        minor = int(gdb.parse_and_eval("LVGL_VERSION_MINOR"))
        patch = int(gdb.parse_and_eval("LVGL_VERSION_PATCH"))
        return f"{major}.{minor}.{patch}"
    except gdb.error:
        return None


def _buf_to_dict(draw_buf) -> dict | None:
    """Convert an LVDrawBuf to a dict with base64 PNG image."""
    if draw_buf is None:
        return None
    try:
        cf_info = draw_buf.color_format_info()
        header = draw_buf.super_value("header")
        stride = int(header["stride"])
        height = int(header["h"])
        bpp = cf_info["bpp"]
        width = (stride * 8) // bpp if bpp else 0

        png_bytes = draw_buf.to_png_bytes()
        image_b64 = base64.b64encode(png_bytes).decode("ascii") if png_bytes else None

        return {
            "addr": hex(int(draw_buf)),
            "width": width,
            "height": height,
            "color_format": cf_info["name"],
            "data_size": int(draw_buf.super_value("data_size")),
            "image_base64": image_b64,
        }
    except Exception:
        return None


def _collect_displays(lvgl) -> list:
    """Collect display info with framebuffer data."""
    result = []
    try:
        for disp in lvgl.displays():
            d = disp.snapshot().as_dict()
            d["buf_1"] = _buf_to_dict(disp.buf_1)
            d["buf_2"] = _buf_to_dict(disp.buf_2)
            result.append(d)
    except Exception as e:
        gdb.write(f"Warning: failed to collect displays: {e}\n")
    return result


def _collect_object_trees(lvgl) -> list:
    """Collect object trees for all displays."""
    result = []
    try:
        for disp in lvgl.displays():
            tree = {
                "display_addr": hex(int(disp)),
                "screens": [],
            }
            for screen in disp.screens:
                snap = screen.snapshot(
                    include_children=True, include_styles=True
                ).as_dict()
                tree["screens"].append(snap)
            result.append(tree)
    except Exception as e:
        gdb.write(f"Warning: failed to collect object trees: {e}\n")
    return result


def _collect_anims(lvgl) -> list:
    try:
        return [a.snapshot().as_dict() for a in lvgl.anims()]
    except Exception as e:
        gdb.write(f"Warning: failed to collect animations: {e}\n")
        return []


def _collect_timers(lvgl) -> list:
    try:
        return [t.snapshot().as_dict() for t in lvgl.timers()]
    except Exception as e:
        gdb.write(f"Warning: failed to collect timers: {e}\n")
        return []


def _collect_image_cache(lvgl) -> list:
    """Collect image cache entries with optional decoded buffer previews."""
    try:
        cache = lvgl.image_cache()
        entries = cache.snapshots()
        result = []
        for snap in entries:
            d = snap.as_dict()
            # Try to get preview of decoded buffer
            d["preview_base64"] = None
            decoded_addr = d.get("decoded_addr")
            if decoded_addr and decoded_addr != "0x0":
                try:
                    from lvglgdb.lvgl.draw.lv_draw_buf import LVDrawBuf
                    buf = LVDrawBuf(gdb.Value(int(decoded_addr, 16)))
                    png_bytes = buf.to_png_bytes()
                    if png_bytes:
                        d["preview_base64"] = base64.b64encode(
                            png_bytes
                        ).decode("ascii")
                except Exception:
                    pass
            result.append(d)
        return result
    except Exception as e:
        gdb.write(f"Warning: failed to collect image cache: {e}\n")
        return []


def _collect_image_header_cache(lvgl) -> list:
    try:
        cache = lvgl.image_header_cache()
        return [s.as_dict() for s in cache.snapshots()]
    except Exception as e:
        gdb.write(f"Warning: failed to collect image header cache: {e}\n")
        return []


def _collect_indevs(lvgl) -> list:
    try:
        return [i.snapshot().as_dict() for i in lvgl.indevs()]
    except Exception as e:
        gdb.write(f"Warning: failed to collect indevs: {e}\n")
        return []


def _collect_groups(lvgl) -> list:
    try:
        return [g.snapshot().as_dict() for g in lvgl.groups()]
    except Exception as e:
        gdb.write(f"Warning: failed to collect groups: {e}\n")
        return []


def _collect_draw_units(lvgl) -> list:
    try:
        return [u.snapshot().as_dict() for u in lvgl.draw_units()]
    except Exception as e:
        gdb.write(f"Warning: failed to collect draw units: {e}\n")
        return []


def _collect_draw_tasks(lvgl) -> list:
    """Collect draw tasks from draw_info task list."""
    try:
        from lvglgdb.lvgl.draw.lv_draw_task import LVDrawTask
        head = lvgl.lv_global.draw_info.task_head
        if not int(head):
            return []
        return [t.snapshot().as_dict() for t in LVDrawTask(head)]
    except Exception as e:
        gdb.write(f"Warning: failed to collect draw tasks: {e}\n")
        return []


def _collect_subjects(lvgl) -> list:
    """Collect subjects from object event lists across all displays."""
    seen = set()
    result = []
    try:
        from lvglgdb.lvgl.core.lv_observer import LVSubject
        for disp in lvgl.displays():
            for screen in disp.screens:
                _collect_subjects_from_obj(screen, seen, result)
    except Exception as e:
        gdb.write(f"Warning: failed to collect subjects: {e}\n")
    return result


def _collect_subjects_from_obj(obj, seen, result):
    """Recursively collect subjects from an object's event list."""
    from lvglgdb.lvgl.core.lv_observer import LVSubject

    event_list = obj.event_list
    if event_list:
        for dsc in event_list:
            try:
                user_data = dsc.user_data
                if not int(user_data):
                    continue
                # Check if this looks like a subject (has subs_ll field)
                subject = LVSubject(user_data)
                addr = int(subject)
                if addr not in seen:
                    seen.add(addr)
                    result.append(subject.snapshot().as_dict())
            except Exception:
                continue

    for child in obj.children:
        _collect_subjects_from_obj(child, seen, result)


def _collect_image_decoders(lvgl) -> list:
    try:
        return [d.snapshot().as_dict() for d in lvgl.image_decoders()]
    except Exception as e:
        gdb.write(f"Warning: failed to collect image decoders: {e}\n")
        return []


def _collect_fs_drivers(lvgl) -> list:
    try:
        return [d.snapshot().as_dict() for d in lvgl.fs_drivers()]
    except Exception as e:
        gdb.write(f"Warning: failed to collect fs drivers: {e}\n")
        return []
