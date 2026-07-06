# Silent Storm

Исходники Silent Storm принадлежат Nival, оригинальный репозиторий: 
https://github.com/nival/Silent-Storm

В этом репозитории ведется доработка исходников с конечной целью -
получение behaviour-equivalent версии Silent Storm v1.2, путём анализа
.pdb файлов версии v1.1 (RussianPatch1) и декомпиляции v1.2.

Текущий статус: игра собирается и запускается на файлах Steam версии,
но склонна к зависаниям, вылетам и в целом далека от релизной.

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

## Сборка

### Требования
- **Windows** с **Visual Studio 2022** (нужен компонент "Разработка классических
  приложений на C++", MSVC v143, Windows SDK 10) или новее. Проверено на VS 2026.
- **CMake 3.21+**
- Сборка **только Win32 / x86**.
- *Необязательно:* **DirectX SDK June 2010** (https://www.microsoft.com/en-us/download/details.aspx?id=6812) - нужен
  только для сборки инструмента `ShaderCompiler`; если его нет, инструмент
  пропускается автоматически.

### Сборка
Запустите **`build.bat`** или выполните в терминале из папки репозитория:

```
cmake -S . -B build -A Win32
cmake --build build --config Release
```

Результаты появятся в **`build\Release\`** - `Game.exe` и инструменты (`DataImport`,
`PkgBuilder`, `FontGen`, `TexConv`, `TexMipStrip`, `ShaderCompiler`).

Для отладки запустите **`build-debug.bat`** (собирает конфигурацию `RelWithDebInfo`),
откройте **`build\A5.sln`** в Visual Studio и запустите отладку проекта `Game`.
CMake попытается подсунуть вашу папку с игрой в рабочий каталог отладчика, если
у него это не получится, то нужно указать её вручную. Правой кнопкой по проекту
`Game` - Свойства - Отладчик - Рабочий каталог. Пример 
`E:/SteamLibrary/steamapps/common/Silent Storm`

### Запуск игры
Поместите Game.exe в папку с игрой (пример: `E:/SteamLibrary/steamapps/common/Silent Storm`). 
Замените (с бэкапом) `LifeStudioHeadAPI.dll` и `GDPFile.dll` на их более новые версии из 
Silent Storm Sentinels (2.6.0.0). Запустите Game.exe.

### Примечания
- импортируемые проприетарные библиотеки fmod / Bink / LifeStudio **генерируются во время сборки** из
  зафиксированных таблиц экспорта `.def` в каталоге `third_party/` - оригинальные SDK не требуются.
- `MapEdit`, `Scintilla`, `OpenDynamix` и `LSConverter` оставлены в репозитории, но **не
  собираются** (по тем или иным причинам, с MapEdit там вообще всё сложно); их списки 
  исходников сохранены в `sources.cmake`.
