"""Optional debug logging, toggled via Preferences > General > "Enable debug
logging" (off by default). Since the packaged app runs via pythonw.exe (no
console), this is the only way to capture unhandled exceptions and
connection errors on Windows."""

import datetime

from .config import get_app_data_dir, load_settings

LOG_FILE = "debug.log"


def is_enabled() -> bool:
    return bool(load_settings().get("debug", False))


def get_log_path():
    return get_app_data_dir() / LOG_FILE


def log(msg: str):
    if not is_enabled():
        return
    try:
        with open(get_log_path(), "a", encoding="utf-8") as f:
            f.write(f"{datetime.datetime.now().isoformat()} {msg}\n")
    except Exception:
        pass
