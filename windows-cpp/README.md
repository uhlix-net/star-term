# Star Term C++ Edition

A complete C++17/Qt6 port of the Star Term SSH terminal application.

## Dependencies

- **Qt 6.5+** — QtCore, QtGui, QtWidgets, QtNetwork
- **libssh2** — SSH connectivity (via vcpkg)
- **OpenSSL 3.x** — Ed25519 license verification + libssh2 backend (via vcpkg)
- **CMake 3.20+**
- **vcpkg** — dependency manager

## Build on Windows

### 1. Install prerequisites

- [Visual Studio 2022](https://visualstudio.microsoft.com/) with "Desktop development with C++" workload
- [Qt 6.5+](https://www.qt.io/download) — install to e.g. `C:\Qt\6.5.3\msvc2019_64`
- [CMake 3.20+](https://cmake.org/download/) — included in VS or install separately
- [vcpkg](https://github.com/microsoft/vcpkg):

```cmd
git clone https://github.com/microsoft/vcpkg C:\vcpkg
C:\vcpkg\bootstrap-vcpkg.bat
C:\vcpkg\vcpkg integrate install
```

### 2. Install vcpkg dependencies

```cmd
C:\vcpkg\vcpkg install libssh2:x64-windows openssl:x64-windows
```

### 3. Configure and build

Open a Visual Studio Developer Command Prompt and run:

```cmd
cd C:\path\to\star-term\windows-cpp

cmake -B build -G "Visual Studio 17 2022" -A x64 ^
  -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake ^
  -DCMAKE_PREFIX_PATH=C:/Qt/6.5.3/msvc2019_64

cmake --build build --config Release
```

The executable will be at `build\Release\star_term_cpp.exe`.

### 4. Deploy Qt DLLs

```cmd
C:\Qt\6.5.3\msvc2019_64\bin\windeployqt.exe --release build\Release\star_term_cpp.exe
```

This copies the required Qt DLLs alongside the executable.

## Build the NSIS Installer

1. Install [NSIS 3.x](https://nsis.sourceforge.io/Download)
2. Copy the built executable and all DLLs into `installer\`:

```cmd
copy build\Release\star_term_cpp.exe installer\
copy build\Release\*.dll installer\
xcopy /E build\Release\platforms installer\platforms\
xcopy /E build\Release\styles installer\styles\
```

3. Copy the app icon from the Python installer (required for exe icon and installer):

```cmd
copy ..\windows\installer\app.ico installer\
```

4. Compile the installer:

```cmd
cd installer
"C:\Program Files (x86)\NSIS\makensis.exe" setup.nsi
```

Output: `installer\Output\star_term_cpp_setup.exe`

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
