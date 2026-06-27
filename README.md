# XKICK Server (Rebuild)

A portable, cross-platform C++ reconstruction of the XKICK game server. Two
executables are produced from a single source tree:

| Target          | Role                                                        |
|-----------------|-------------------------------------------------------------|
| `XKICK_Certify` | Auth / account server (login, character, server-select)     |
| `XKICK_Game`    | Lobby / room / match / persistence server                   |

Both build and run on **Windows** and **Linux** (32-bit / `x86`). All OS- and
networking-specific code is confined to the platform layer (`connect.cpp` for
Asio); the rest of the tree is portable C++17.

---
[Client Download](https://mega.nz/file/P41HmSrK#hsfRHwWYsG58O9hgzedx7IjyYowzDRa53kVMI4zE9-k)

[No-HackShield EXE](https://mega.nz/file/e4N2HTiQ#aD2OwO0_rhWpRMlMm8yB2rer_Yst2eMCtvh7rTLuOsU)

---

## 1. Prerequisites

### Common (all platforms)

| Tool      | Minimum | Notes                                                            |
|-----------|---------|------------------------------------------------------------------|
| CMake     | 3.21    | Presets v3 are used (`CMakePresets.json`).                       |
| vcpkg     | recent  | Provides `asio` (standalone) + `libmariadb`. Set `VCPKG_ROOT`.   |
| MariaDB / MySQL server | 10.x / 5.7+ | Runtime dependency for both servers. |
| Git       | any     | To clone vcpkg and (optionally) this repo.                       |

The two C/C++ library dependencies are declared in `vcpkg.json` and installed
automatically by the vcpkg manifest during the CMake configure step — you do
**not** install them by hand:

- **asio** (standalone, no Boost) — networking. Built with `ASIO_STANDALONE`.
- **libmariadb** (MariaDB Connector/C) — pure-C MySQL-wire-compatible client,
  exposes the standard `<mysql.h>` API.

### Windows

- **Visual Studio 2026** with the **C++ Clang tools for Windows (clang-cl)**
  workload component. The preset uses generator `Visual Studio 18 2026`,
  architecture `Win32`, toolset `ClangCL`.
  - If you use a different VS version, change `"generator"` in
    `CMakePresets.json` (e.g. `"Visual Studio 17 2022"`).
- vcpkg triplet: **`x86-windows-static`** (32-bit, static deps + static CRT).
  libmariadb, zlib and the C/C++ runtime are linked **into** each executable, so
  the resulting `.exe` is self-contained — no DLLs to ship and no VC++ redist
  required. It imports only core Windows system DLLs (ws2_32, crypt32, bcrypt,
  secur32, …). TLS uses the native Windows backend (Schannel), so no OpenSSL.
  - To build against shared deps instead (ships `libmariadb.dll` + `z.dll`),
    set the triplet back to `x86-windows` in `CMakePresets.json`.

### Linux

- **clang / clang++** with 32-bit support. On Debian/Ubuntu:
  ```bash
  sudo apt install clang cmake ninja-build git \
                   gcc-multilib g++-multilib   # 32-bit (-m32) runtime/headers
  ```
- The preset passes `-m32` for a 32-bit build (`x86-linux` triplet), matching
  the original binaries.

---

## 2. One-time setup: vcpkg

Clone and bootstrap vcpkg anywhere, then point `VCPKG_ROOT` at it. The CMake
toolchain file is referenced via `$env{VCPKG_ROOT}` in the presets.

**Windows (PowerShell):**
```powershell
git clone https://github.com/microsoft/vcpkg C:\vcpkg
C:\vcpkg\bootstrap-vcpkg.bat
# Make it permanent for new shells:
setx VCPKG_ROOT C:\vcpkg
# ...then open a NEW terminal so $env:VCPKG_ROOT is set.
```

**Linux (bash):**
```bash
git clone https://github.com/microsoft/vcpkg ~/vcpkg
~/vcpkg/bootstrap-vcpkg.sh
export VCPKG_ROOT=~/vcpkg          # add to ~/.bashrc to persist
```

> The first CMake configure will install `asio` and `libmariadb` for the
> selected triplet. This can take several minutes the first time; subsequent
> builds reuse the vcpkg binary cache.

---

## 3. Build

From the repository root:

### Windows
```powershell
cmake --preset x86-windows
cmake --build build/x86-windows --config Release
```

### Linux
```bash
cmake --preset x86-linux
cmake --build build/x86-linux
```

**Output binaries:**

| File            | Windows                                                | Linux                                  |
|-----------------|--------------------------------------------------------|----------------------------------------|
| `XKICK_Certify` | `build/x86-windows/src/certify/Release/XKICK_Certify.exe` | `build/x86-linux/src/certify/XKICK_Certify` |
| `XKICK_Game`    | `build/x86-windows/src/game/Release/XKICK_Game.exe`       | `build/x86-linux/src/game/XKICK_Game`       |

> **Build the `Release` config on Windows.** Debug changes the layout of
> `std::vector`/`std::string` (debug iterators / container proxy), which breaks
> the byte-exact `static_assert(offsetof/sizeof, …)` wire-struct checks the
> reconstruction relies on.

The build is split into three CMake subprojects:
`src/common` (static lib `kicks_common`) → `src/certify` and `src/game`
(executables linking the common lib).

On Windows the `x86-windows-static` triplet links all third-party deps
(libmariadb, zlib) and the C/C++ runtime statically, producing self-contained
executables with no DLLs to ship. To verify, dump the imports — only Windows
system DLLs should appear:

```powershell
dumpbin /dependents build\x86-windows\src\certify\Release\XKICK_Certify.exe
```

---

## 4. Database setup

Both servers need a running MariaDB/MySQL instance. There is one dump file per
database — `kicks2.sql` (main: accounts, rooms, plus the seed rows the servers
need to boot) and `kicks2_log.sql` (log shards). Neither file creates or selects
its database, so create both first, then import each into its own DB:

```bash
mysql -u root -e "CREATE DATABASE IF NOT EXISTS kicks2; CREATE DATABASE IF NOT EXISTS kicks2_log;"
mysql -u root kicks2     < kicks2.sql
mysql -u root kicks2_log < kicks2_log.sql
```

If you later change the schema or seed data, re-export each database so the
files stay authoritative:

```bash
mysqldump -u root --skip-dump-date kicks2     > kicks2.sql
mysqldump -u root --skip-dump-date kicks2_log > kicks2_log.sql
```

Default databases (configurable, see below):

| Flag         | DB name      | Contents                                  |
|--------------|--------------|-------------------------------------------|
| `DB_Main`    | `kicks2`     | `tb_mst_*`, `tb_game_*` (accounts, rooms) |
| `DB_Log`     | `kicks2_log` | `tb_log_*` shards                         |
| `DB_Sample`  | `kicks2`     | `CREATE TABLE` templates for log shards   |

The certify server reads the registered game/auth server list (and their
listen ports) from `tb_game_server`. **If MySQL is down or that table is empty,
the servers fall back to ephemeral ports and clients can't connect** — make
sure the DB is up and seeded before launching.

---

## 5. Runtime configuration

Each server runs from its own directory under `run/` and reads
**`../config.ini`** (i.e. `run/config.ini`, shared by both):

```
run/
├── config.ini            # shared DB + deployment settings
├── Table/                # CSV game tables (Table_*.csv, maps, locales)
├── Log/                  # server log output
├── certify/              # working dir for XKICK_Certify
│   ├── XKICK_Certify.exe  # self-contained (deps statically linked, no DLLs)
│   └── ServerCode = 1     # this deployment's server code
└── game/                 # working dir for XKICK_Game
    ├── XKICK_Game.exe
    └── ServerCode = 101
```

`run/config.ini`:
```ini
DB_IP   = 127.0.0.1
DB_User = root
DB_Pass =
DB_Main = kicks2
DB_Log  = kicks2_log
DB_Sample = kicks2
Charset = cp1256
Company = 200
Nation  = 101
HackCode = 0
```

After a fresh build, copy the freshly built executables over the staged copies
in `run/certify/` and `run/game/`. The MariaDB client, zlib and the C/C++
runtime are statically linked, so there are **no DLLs to ship** — each working
directory needs only the `.exe` (plus the shared `../config.ini` and `Table/`).

---

## 6. Run

Start MariaDB first, then each server **from its own working directory** (so
`../config.ini` and `Table/` resolve correctly):

**Windows (PowerShell):**
```powershell
cd run\certify ;  .\XKICK_Certify.exe
# in a second terminal:
cd run\game ;     .\XKICK_Game.exe
```

**Linux (bash):**
```bash
( cd run/certify && ./XKICK_Certify )
( cd run/game    && ./XKICK_Game )
```

On startup each server prints its resolved `ServerCode` and listen ports. The
certify server handles login → character → server-select; selecting a server
hands the client off to the game server.

---

## 7. Troubleshooting

| Symptom | Cause / fix |
|---------|-------------|
| CMake: *"MariaDB/MySQL client not found"* | vcpkg didn't install `libmariadb`. Confirm `VCPKG_ROOT` is set and re-run `cmake --preset …`. |
| `static_assert` offset/size failures | You built **Debug** on Windows. Use `--config Release`. |
| Server binds random high ports (54xxx) | MySQL is down or `tb_game_server` is empty. Start MySQL and load the schema. |
| Linker `LNK2038` *RuntimeLibrary mismatch* | Your objects and the vcpkg libs disagree on the CRT. The `x86-windows-static` triplet needs the static CRT — keep `CMAKE_MSVC_RUNTIME_LIBRARY` = `MultiThreaded…` (set in the root `CMakeLists.txt`) and reconfigure a clean build dir. |
| Wrong Visual Studio version | Edit the `generator` field in `CMakePresets.json`. |

---

## 8. Quick reference

```powershell
# Windows, from repo root, end to end:
setx VCPKG_ROOT C:\vcpkg            # one time (new shell afterwards)
cmake --preset x86-windows
cmake --build build/x86-windows --config Release
mysql -u root -e "CREATE DATABASE IF NOT EXISTS kicks2; CREATE DATABASE IF NOT EXISTS kicks2_log;"
mysql -u root kicks2     < kicks2.sql          # main DB + seed rows
mysql -u root kicks2_log < kicks2_log.sql      # log shards
cd run\certify ; .\XKICK_Certify.exe
```
