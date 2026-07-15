# Silent Storm

*[Русский](README.md) | English*

The Silent Storm source code belongs to Nival; the original repository:
https://github.com/nival/Silent-Storm

This repository contains ongoing work on the source code, with the ultimate goal of
producing a behaviour-equivalent version of Silent Storm v1.2 by analyzing the
.pdb files of version v1.1 (RussianPatch1) and decompiling v1.2.

Current status: the game builds and runs using the Steam version's files, but it
is prone to freezes and crashes, and is generally far from release-ready.

<div align="center">
  <table>
    <tr>
      <td colspan="3" align="center">
        <img width="200" alt="image" src="https://github.com/user-attachments/assets/fff39b1f-1c8c-42d4-9b68-ca73cb45d51f" />
      </td>
    </tr>
    <tr>
      <td colspan="3" align="center">
        <img width="200" alt="image" src="https://github.com/user-attachments/assets/483a8f95-eada-4e40-8d54-f18576291676" />
        <img width="200" alt="image" src="https://github.com/user-attachments/assets/ab4f9895-265b-4b83-af60-c0e8401f29be" />
      </td>
    </tr>
  </table>
</div>

---

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
(`DataImport`, `PkgBuilder`, `FontGen`, `TexConv`, `TexMipStrip`, `ShaderCompiler`).

For debugging, run **`build-debug.bat`** (builds the `RelWithDebInfo` configuration),
open **`build\A5.sln`** in Visual Studio, and start debugging the `Game` project.
CMake will try to set your game folder as the debugger's working directory
automatically; if that fails, you'll need to set it manually: right-click the
`Game` project - Properties - Debugger - Working Directory. Example:
`E:/SteamLibrary/steamapps/common/Silent Storm`

### Running the game
Place Game.exe into the game folder (example: `E:/SteamLibrary/steamapps/common/Silent Storm`).
Run Game.exe.

### Notes
- The imported proprietary libraries fmod / Bink / LifeStudio are **generated at
  build time** from the committed `.def` export tables in the `third_party/`
  directory - the original SDKs are not required.
- `MapEdit`, `Scintilla`, `OpenDynamix`, and `LSConverter` are kept in the
  repository but are **not built** (for various reasons - `MapEdit` in particular
  is quite complicated); their source file lists are preserved in `sources.cmake`.
