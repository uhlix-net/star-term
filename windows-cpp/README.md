# Star Term C++ Edition

A C++17/Qt6 SSH terminal application for Windows.

C++ is the standard for Star Term going forward; the Python/PySide6 edition in
`../windows/` is legacy and maintained as-is.

## Dependencies

- **Qt 6.9.x** (MSVC 2022 64-bit) — QtCore, QtGui, QtWidgets, QtNetwork, QtSvg
  - Qt **6.5.3 will not compile** under the v145 toolset: its `qvarlengtharray.h` uses
    `stdext::checked_array_iterator`, removed from the modern MSVC STL. Use 6.5.4+ / 6.9.x.
- **libssh2** — SSH connectivity (via vcpkg)
- **OpenSSL 3.x** — Ed25519 license verification + libssh2 backend (via vcpkg)
- **zlib** — via vcpkg (builds as `z.dll` here, *not* `zlib1.dll`)
- **CMake 4.2+** — the `Visual Studio 18 2026` generator name was added in 4.2
- **vcpkg** — dependency manager, used in **manifest mode** (`vcpkg.json`)

## Build on Windows

Builds run Windows-native. The commands below work from any Windows shell — a VS Code
integrated terminal, a Developer Command Prompt, or plain `cmd`. Opening the generated
solution in Visual Studio is optional (see [Debugging](#debugging)).

### 1. Install prerequisites

- [Visual Studio 2026 Community](https://visualstudio.microsoft.com/) (v18) with the
  "Desktop development with C++" workload — default toolset **v145**
  - The v143 toolset is not installed by default; `-T v143` fails with MSB8020 unless
    you add it via the VS Installer.
- [Qt 6.9.x](https://www.qt.io/download), MSVC 2022 64-bit kit
- [CMake 4.2+](https://cmake.org/download/)
- [NSIS 3.x](https://nsis.sourceforge.io/Download) — for the installer
- [vcpkg](https://github.com/microsoft/vcpkg):

```cmd
git clone https://github.com/microsoft/vcpkg C:\vcpkg
C:\vcpkg\bootstrap-vcpkg.bat
```

### ⚠️ Qt install-path quirk

The Qt online installer was originally pointed at `C:\Qt\6.5.3\msvc2019_64` as its
**install root**, so every Qt version sits one level deeper than the folder name
suggests. The outer `6.5.3\msvc2019_64` is the root, not a kit:

| | Path |
|---|---|
| Qt root (`MaintenanceTool.exe`) | `C:\Qt\6.5.3\msvc2019_64\` |
| Actual Qt 6.9.3 kit | `C:\Qt\6.5.3\msvc2019_64\6.9.3\msvc2022_64\` |
| `Qt6_DIR` | `C:/Qt/6.5.3/msvc2019_64/6.9.3/msvc2022_64/lib/cmake/Qt6` |
| `windeployqt` | `C:\Qt\6.5.3\msvc2019_64\6.9.3\msvc2022_64\bin\windeployqt.exe` |

A `6.11.1` install also exists but ships **only** mingw/android kits — no MSVC. Do not
use it for this project.

### 2. Configure

vcpkg runs in manifest mode, so `vcpkg.json` installs `libssh2`, `openssl`, and `zlib`
automatically during configure. There is no separate `vcpkg install` step.

```cmd
cd U:\home\huhl\git-repos\star-term\windows-cpp

cmake -B build -G "Visual Studio 18 2026" -A x64 ^
  -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake ^
  -DQt6_DIR=C:/Qt/6.5.3/msvc2019_64/6.9.3/msvc2022_64/lib/cmake/Qt6
```

Use `-DQt6_DIR`, not `-DCMAKE_PREFIX_PATH` — the nested layout above defeats prefix
discovery.

### 3. Build

```cmd
cmake --build build --config Release
```

- This is a **multi-config generator**: `--config Release` is required, or you get a
  Debug build in `build\Debug`.
- Executable: `build\Release\star_term.exe`
- Source or `CMakeLists.txt` edits → just rebuild. Delete `build\` only when changing
  generator, toolset, or Qt paths, or when the cache is corrupted (`rmdir /s /q build`).
- **Harmless noise:** MSB8064/MSB8065 warnings about lowercase dependency paths
  ("does not exist … incremental build") are a casing artifact of building from the
  `U:` mapped drive. Safe to ignore.

### 4. Deploy the runtime DLLs

Required after a Qt or vcpkg version change — **not** after ordinary source edits.

```cmd
C:\Qt\6.5.3\msvc2019_64\6.9.3\msvc2022_64\bin\windeployqt.exe --release build\Release\star_term.exe

copy build\vcpkg_installed\x64-windows\bin\*.dll build\Release\
```

Copy the **release** vcpkg DLLs, not the ones under `debug\bin`. This is where
`libssh2.dll`, `libssl-3-x64.dll`, `libcrypto-3-x64.dll`, and `z.dll` come from.

`build\Release\` is then self-contained and runnable.

## Build the NSIS Installer

`setup.nsi` sweeps the tested build output recursively — it does **not** read loose
DLLs from `installer\`, so there is nothing to stage by hand:

```nsis
File /r /x *.pdb /x *.lib /x *.exp "..\build\Release\*.*"
File "app.ico"
File "run_star_term.bat"
```

Compile it:

```cmd
cd installer
"C:\Program Files (x86)\NSIS\makensis.exe" setup.nsi
```

Output: `installer\Output\star_term_setup.exe`

- The `Output\` directory must already exist — NSIS will not create it.
- Treat any **warning 7010 "no files found" as a packaging bug**, not a warning.
- A correct build produces **zero** 7010 warnings and a setup much larger than 14 MB
  (that's the Qt payload). An earlier revision listed DLLs one-by-one with `/nonfatal`
  and silently shipped installers missing `z.dll`, most Qt DLLs, and all plugins.
- Paths are relative: `installer\` is a sibling of `build\`, so use `..\build\…`.

## Debugging

- **Crash triage:** Event Viewer → Windows Logs → Application. The error entry names the
  faulting module and exception code.
- `dumpbin /dependents build\Release\star_term.exe` (from a VS Developer Prompt) lists
  the true DLL dependencies.
- Open `build\star_term.slnx` in Visual Studio and press F5 for real stacks.
- The app logs through `debugLog()` — run the exe from a terminal to see the output.

## Settings Compatibility

The C++ edition reads and writes the same settings files as the Python edition:

- `%APPDATA%\star_term\star-term-settings.json` — settings, macros, folders
- `%APPDATA%\star_term\sessions.json` — saved sessions
- `%APPDATA%\star_term\known_hosts` — SSH known hosts

Sessions and settings saved by the Python edition carry over automatically.

## License Key

Uses the same Ed25519 public key and key format as the Python edition.
Public key: `4d055cd85dfd9c1849759c3c596b65ad99b1aee31103c6c43ea5bc98537697e6`

Trial tracking uses the Windows registry (`HKCU\Software\StarTerm`) on Windows,
falling back to a JSON file on other platforms.
