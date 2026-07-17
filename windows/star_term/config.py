import json
import os
from pathlib import Path

APP_DIR_NAME = "star_term"
SESSIONS_FILE = "sessions.json"
SETTINGS_FILE = "star-term-settings.json"
KNOWN_HOSTS_FILE = "known_hosts"

# Legacy per-purpose files, merged into SETTINGS_FILE on first load.
_LEGACY_FILES = {
    "settings": "settings.json",
    "folders": "folders.json",
    "macros": "macros.json",
}


def get_app_data_dir() -> Path:
    appdata = os.environ.get("APPDATA")
    base = Path(appdata) if appdata else Path.home() / "AppData" / "Roaming"
    app_dir = base / APP_DIR_NAME
    app_dir.mkdir(parents=True, exist_ok=True)
    return app_dir


def _load_combined() -> dict:
    path = get_app_data_dir() / SETTINGS_FILE
    if path.exists():
        with open(path, "r", encoding="utf-8") as f:
            return json.load(f)

    combined = {"settings": {}, "folders": [], "macros": []}
    migrated = False
    for key, filename in _LEGACY_FILES.items():
        legacy_path = get_app_data_dir() / filename
        if legacy_path.exists():
            with open(legacy_path, "r", encoding="utf-8") as f:
                combined[key] = json.load(f)
            migrated = True

    if migrated:
        _save_combined(combined)
        for filename in _LEGACY_FILES.values():
            legacy_path = get_app_data_dir() / filename
            if legacy_path.exists():
                legacy_path.unlink()

    return combined


def _save_combined(data: dict):
    path = get_app_data_dir() / SETTINGS_FILE
    with open(path, "w", encoding="utf-8") as f:
        json.dump(data, f, indent=2)


def load_sessions() -> list:
    path = get_app_data_dir() / SESSIONS_FILE
    if not path.exists():
        return []
    with open(path, "r", encoding="utf-8") as f:
        return json.load(f)


def save_sessions(sessions: list):
    path = get_app_data_dir() / SESSIONS_FILE
    with open(path, "w", encoding="utf-8") as f:
        json.dump(sessions, f, indent=2)


def load_settings() -> dict:
    return _load_combined().get("settings", {})


def save_settings(settings: dict):
    combined = _load_combined()
    combined["settings"] = settings
    _save_combined(combined)


def get_known_hosts_path() -> Path:
    return get_app_data_dir() / KNOWN_HOSTS_FILE


def load_macros() -> list:
    return _load_combined().get("macros", [])


def save_macros(macros: list):
    combined = _load_combined()
    combined["macros"] = macros
    _save_combined(combined)


def load_folders() -> list:
    return _load_combined().get("folders", [])


def save_folders(folders: list):
    combined = _load_combined()
    combined["folders"] = folders
    _save_combined(combined)
