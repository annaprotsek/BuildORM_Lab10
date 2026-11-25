# BuildORM — Лабораторна робота №10 (варіант 19, CI/CD)

Розвиток проєкту ЛР8 (sqlite_orm + Repository): додано патерни **State**, **Strategy**, **Observer** та додатковий патерн **Facade**.

| Патерн | Де | Призначення |
|---|---|---|
| State | `BuildState.h/.cpp` | стани Created → Running → Completed, окремий стан Failed; контроль переходів |
| Strategy | `DurationStrategy.h/.cpp` | абсолютне / відносне / нормоване відхилення тривалості збірки |
| Observer | `BuildEvents.h`, `Subscribers.h` | події BuildStarted, BuildFinished; підписники: журнал проєкту, статистика виконання |
| Facade | `BuildPipelineFacade.h/.cpp` | єдина точка входу; транзакція «статус + історія» |

## Збирання (x64 Developer PowerShell)

```powershell
& "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\Launch-VsDevShell.ps1" -Arch amd64 -HostArch amd64
cd <папка проєкту>
cmake --preset default
cmake --build --preset default
.\build\BuildORM.exe
ctest --test-dir build --output-on-failure
.\build\BuildTests.exe
```

`vcpkg` у маніфесті: `sqlite3`, `sqlite-orm`, `gtest` (перший запуск встановлює залежності).
