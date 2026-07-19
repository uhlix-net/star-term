# star-term / windows-cpp — Build & Debug Session Notes
> Project memory file generated from a Claude.ai troubleshooting session (2026-07-16/17).
> Purpose: give Claude Code (and future humans) full context on the Windows build environment,
> its quirks, and every fix applied while getting the app to build, run, and package.

## Project overview

- **App**: `star_term_cpp` — a Qt6 Widgets SSH terminal client ("Star Term C++ Edition")
- **Repo path**: `U:\home\huhl\git-repos\star-term\windows-cpp` (U: is a mapped network drive, volume serial 0000-0000)
- **Sources**: `src\` (main.cpp, config.cpp, debug.cpp, theme.cpp, icons.cpp, licensing.cpp,
  vt100.cpp, terminalwidget.cpp, sshsession.cpp, sessionpane.cpp, sidebar.cpp/.h,
  remotebrowser.cpp, connectiondialog.cpp, preferencesdialog.cpp, licensedialog.cpp,
  macrospanel.cpp, mainwindow.cpp, statusbar.cpp)
- **Installer**: `installer\` (setup.nsi for NSIS, app.rc, app.ico, run_star_term_cpp.bat)
- **Dependencies**: Qt6 (Widgets/Gui/Core/Network), libssh2 + OpenSSL + zlib via **vcpkg manifest mode** (vcpkg.json in project root)

## Toolchain (working configuration)

- **Visual Studio 2026 Community** (v18, MSBuild 18.8.x), default toolset **v145**
  - v143 toolset is NOT installed (attempting `-T v143` fails with MSB8020 unless added via VS Installer)
- **CMake 4.4.0-rc2** — VS 2026 generator name is `"Visual Studio 18 2026"` (added in CMake 4.2; plain "Visual Studio 2026" is invalid)
- **vcpkg** at `C:\vcpkg`, toolchain file `C:/vcpkg/scripts/buildsystems/vcpkg.cmake`
- **Qt 6.9.3 MSVC 2022 64-bit** — see path quirk below
- **NSIS** at `c:\Program Files (x86)\NSIS\makensis.exe`

### CRITICAL Qt path quirk

The Qt online installer was originally pointed at `C:\Qt\6.5.3\msvc2019_64` as its INSTALL ROOT,
so all Qt versions are nested one level deeper than the folder name suggests:

- Qt root (MaintenanceTool.exe lives here): `C:\Qt\6.5.3\msvc2019_64\`
- **Actual Qt 6.9.3 kit**: `C:\Qt\6.5.3\msvc2019_64\6.9.3\msvc2022_64\`
- Qt6_DIR: `C:/Qt/6.5.3/msvc2019_64/6.9.3/msvc2022_64/lib/cmake/Qt6`
- windeployqt: `C:\Qt\6.5.3\msvc2019_64\6.9.3\msvc2022_64\bin\windeployqt.exe`
- A `6.11.1` install also exists but has ONLY mingw/android kits (no MSVC) — do not use for this project.
- The old 6.5.3 kit was removed. Qt 6.5.3 CANNOT compile under VS 2026's v145 toolset anyway
  (its qvarlengtharray.h uses `stdext::checked_array_iterator`, removed from the modern MSVC STL).
  Qt 6.5.4+ / 6.9.x compile fine with v145.

## Working commands

Configure:
```bat
cmake -B build -G "Visual Studio 18 2026" -A x64 -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake -DQt6_DIR=C:/Qt/6.5.3/msvc2019_64/6.9.3/msvc2022_64/lib/cmake/Qt6
```

Build:
```bat
cmake --build build --config Release
```
- Multi-config generator: `--config Release` is required or you get a Debug build in build\Debug.
- Exe output: `build\Release\star_term_cpp.exe`
- Source or CMakeLists edits → just rebuild. Delete `build\` only when changing
  generator/toolset/Qt paths or when the cache is corrupted (`rmdir /s /q build`).
- Harmless noise: many MSB8064/MSB8065 warnings about lowercase dependency paths
  ("does not exist ... incremental build") — a casing artifact of building from a U: network
  drive; safe to ignore.

Deploy Qt runtime next to exe (required after Qt version changes, not after source edits):
```bat
C:\Qt\6.5.3\msvc2019_64\6.9.3\msvc2022_64\bin\windeployqt.exe --release build\Release\star_term_cpp.exe
```

Copy vcpkg runtime DLLs next to exe (release ones, NOT from debug\bin):
```bat
copy build\vcpkg_installed\x64-windows\bin\*.dll build\Release\
```
- NOTE: this vcpkg zlib builds as **z.dll** (not zlib1.dll). libssh2.dll, libssl-3-x64.dll,
  libcrypto-3-x64.dll also come from here.

Build installer:
```bat
cd installer
"c:\Program Files (x86)\NSIS\makensis.exe" setup.nsi
```
- Output: `installer\Output\star_term_cpp_setup.exe` (the Output dir must exist; NSIS won't create it).
- Treat any "warning 7010: no files found" as a packaging bug, not a warning.

## Source fixes applied during this session (all in repo now)

1. **src\sidebar.h** — added `#include <QTreeWidget>` (was only forward-declared while being
   used as a base class; caused a huge cascade of errors across sidebar.cpp, mainwindow.cpp, and moc files).
2. **src\mainwindow.cpp** — added includes: `<QMenuBar>`, `<QMenu>`, `<QCheckBox>`,
   `<QPushButton>`, `<QApplication>`.
3. **src\mainwindow.cpp line ~564** — `QApplication::instance()->setStyleSheet(...)` →
   `qApp->setStyleSheet(...)`. (instance() returns QCoreApplication*, which lacks setStyleSheet.
   Also: it's `qApp`, lowercase q — `QApp` does not exist.)
4. **CMakeLists.txt** — added `add_compile_definitions(NOMINMAX)` (after project(), before
   add_executable) to stop windows.h min/max macros breaking `std::min`/`std::max`
   (C2589 "illegal token on right side of '::'").
5. **src\icons.cpp** — deleted the unused `makePainter(QPixmap&)` helper that returned QPainter
   by value (QPainter is non-copyable/non-movable; the function was dead code and uncompilable).
   All icon functions correctly construct `QPainter p(&pm);` locally.
6. **src\sshsession.cpp** — libssh2 known-host fixes:
   - Constant is `LIBSSH2_KNOWNHOST_KEYENC_RAW` (RAW and BASE64 are mutually exclusive values,
     not OR-able bits; `LIBSSH2_KNOWNHOST_KEY_BASE64` doesn't exist).
   - `libssh2_knownhost_addc` takes **9 args**: (hosts, host, salt, key, keylen, comment,
     commentlen, typemask, &store). Was being called with 8 (missing commentlen; pass 0 when comment is nullptr).
   - **Crash fix (0xc0000005 in libssh2.dll on connect)**: in `checkKnownHost`, the
     `LIBSSH2_KNOWNHOST_CHECK_MISMATCH` branch called `libssh2_knownhost_free(kh)` and then
     fell through, so kh was used after free (addc/writefile/second free). Removed that one
     free; the frees in the MATCH branch, the user-rejected branch, and end-of-function are
     correct and were kept.
7. **installer\app.rc** — references `app.ico`; the file was missing from the repo and was
   located and placed in `installer\`. (RC error RC2135 otherwise.)

### Known remaining code smells in sshsession.cpp (not yet fixed, flagged for later)

- `sha256FingerprintHex()` is fed the output of `libssh2_hostkey_hash(..., SHA256)` — i.e. it
  hashes a hash. Should compute the fingerprint from the raw key (`key`/`keyLen`) instead.
- `QByteArray(fingerprint, 20).toHex()` overreads when fingerprint is the substituted `""`
  literal, and 20 is the SHA1 length while SHA256 is 32 bytes.
- MISMATCH (key changed = possible MITM) is currently presented the same as "unknown host";
  should eventually get distinct, scarier UI.
- WSAStartup is called per connectSocket() with no WSACleanup pairing; `libssh2_init(0)` is
  never called explicitly (works because openssl backend + blocking mode, but fragile).

## Installer (setup.nsi) state

- Original script listed DLLs one-by-one with `/nonfatal` and expected them **in the installer
  directory** — this silently shipped installers missing z.dll, most Qt DLLs, and plugins.
- Fixed (lines ~44–67 replaced) to sweep the tested build output recursively:
  ```nsis
  SetOutPath "$INSTDIR"
  File /r /x *.pdb /x *.lib /x *.exp "..\build\Release\*.*"
  File "app.ico"
  File "run_star_term_cpp.bat"
  ```
- Stale names from the old list, for reference: `styles\qwindowsvistastyle.dll` no longer exists
  in Qt 6.9 (it's `qmodernwindowsstyle.dll`); zlib here is `z.dll`.
- Rule of thumb: a correct installer build produces **zero** 7010 warnings and a setup.exe
  much larger than 14 MB (Qt payload).

## Standard iteration loops

- **Code change** → `cmake --build build --config Release` → run `build\Release\star_term_cpp.exe` (folder is self-contained) → repeat. Only the exe changes; DLLs stay valid.
- **Qt/vcpkg version change** → delete build, reconfigure, rebuild, re-run windeployqt, recopy vcpkg DLLs.
- **Ship** → makensis on setup.nsi (packages build\Release wholesale).

## Debugging tips proven useful here

- Crash triage: Event Viewer → Windows Logs → Application → Error entry shows faulting module
  + exception code (e.g., this session: libssh2.dll + 0xc0000005 → use-after-free in our code).
- `dumpbin /dependents build\Release\star_term_cpp.exe` (VS dev prompt) lists true DLL deps.
- Open `build\star_term_cpp.slnx` in VS and F5 for a debugger with real stacks.
- App logs via debugLog() — run the exe from a terminal to see output.
- Beware relative paths: `installer\` is a sibling of `build\`, so from installer\ use `..\build\...`.
