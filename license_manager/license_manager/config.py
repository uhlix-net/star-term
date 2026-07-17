import json
import os
from pathlib import Path

APP_DIR_NAME = "star_term_license_manager"
SETTINGS_FILE = "settings.json"


def get_app_data_dir() -> Path:
    appdata = os.environ.get("APPDATA")
    base = Path(appdata) if appdata else Path.home() / "AppData" / "Roaming"
    app_dir = base / APP_DIR_NAME
    app_dir.mkdir(parents=True, exist_ok=True)
    return app_dir


def load_settings() -> dict:
    path = get_app_data_dir() / SETTINGS_FILE
    if not path.exists():
        return {}
    with open(path, "r", encoding="utf-8") as f:
        return json.load(f)


def save_settings(settings: dict):
    path = get_app_data_dir() / SETTINGS_FILE
    with open(path, "w", encoding="utf-8") as f:
        json.dump(settings, f, indent=2)
