# Silent Storm

*[Русский](README.md) | English*

The Silent Storm source code belongs to Nival; the original repository:
https://github.com/nival/Silent-Storm

This repository contains ongoing work on the source code, with the ultimate goal of
producing a behaviour-equivalent version of Silent Storm v1.2 by analyzing the
.pdb files of version v1.1 (RussianPatch1) and decompiling v1.2.

Current status: the game builds and runs using the Steam version's files.
Is mostly playable, with some subtle and not-so-subtle bugs and divergences from the
retail version.

Current status (18.10.2026): the game builds and runs using the Steam version's files,
allies campaign was successfully completed from start to finish[^1], some bugs and divergences
from the retail version may be present.

[^1]: This repo does not fix bugs with game's LUA scripts and other assets, so, for example,
you still have to install the unoffical patch to beat the last mission on Steam version.

<div align="center">
  <table>
    <tr>
      <td colspan="3" align="center">
        <img width="300" alt="image" src="https://github.com/user-attachments/assets/38ed17a9-1eb2-49c8-a3e5-37e54a1365c7" />
      </td>
    </tr>
    <tr>
      <td colspan="3" align="center">
        <img width="300" alt="image" src="https://github.com/user-attachments/assets/33eb9149-4716-4c9a-931a-e8c2102f9761" />
        <img width="300" alt="image" src="https://github.com/user-attachments/assets/5ac5fc48-9e68-4551-a1ff-022f59d13c3b" />
      </td>
    </tr>
  </table>
</div>

---

## Changes from retail game
Most notable differences of this binary compared to retail:

- All resolutions are supported, like in Sentinels (without clipped symbols)
- Ctrl+V supported in console
- Bug with high AA settings on high resolutions is fixed

## Build

### Requirements
- **Windows** with **Visual Studio 2022** (requires the "Desktop development with
  C++" workload, MSVC v143, Windows SDK 10) or newer. Tested with VS 2026.
- **CMake 3.21+**
- Build is **Win32 / x86 only**.
- *Optional:* **DirectX SDK June 2010** (https://www.microsoft.com/en-us/download/details.aspx?id=6812) - only
  needed to build the `ShaderCompiler` tool; if it's not installed, the tool is
  skipped automatically.

### Building
Run **`build.bat`**, or execute the following in a terminal from the repository folder:

```
cmake -S . -B build -A Win32
cmake --build build --config Release
```

The results will appear in **`build\Release\`** - `Game.exe` and the tools
(`DataImport`, `PkgBuilder`, `FontGen`, `TexConv`, `TexMipStrip`, `ShaderCompiler`, `LSConverter`).

For debugging, run **`build-debug.bat`** (builds the `RelWithDebInfo` configuration),
open **`build\A5.sln`** in Visual Studio, and start debugging the `Game` project.
CMake will try to set your game folder as the debugger's working directory
automatically; if that fails, you'll need to set it manually: right-click the
`Game` project - Properties - Debugger - Working Directory. Example:
`E:/SteamLibrary/steamapps/common/Silent Storm`

### Running the game
Place Game.exe (can freely rename as to not replace the original) into the game folder
(example: `E:/SteamLibrary/steamapps/common/Silent Storm`).
Run Game.exe.

### Notes
- The imported proprietary libraries fmod / Bink / LifeStudio are **generated at
  build time** from the committed `.def` export tables in the `third_party/`
  directory - the original SDKs are not required.
- `MapEdit`, `Scintilla`, `OpenDynamix` are kept in the
  repository but are **not built** (for various reasons - `MapEdit` in particular
  is quite complicated); their source file lists are preserved in `sources.cmake`.
