# NoACE Unity 启动器验证记录

目标游戏目录：`C:\Program Files (x86)\preternatural`

## 分析的 3 个游戏侧关键文件

1. `超自然行动组.exe`
   - PE x64，导入表只导入 `超自然行动组Base.dll` 的 ordinal 1。
   - 入口在 Windows Loader 阶段即绑定 TPShell/Base DLL，因此直接启动零售 EXE 会先进入保护启动层。

2. `超自然行动组Base.dll`
   - 导出名：`TPShell64.dll`。
   - 唯一导出：ordinal 1，NONAME，RVA `0x231CE0`。
   - 签名主体：`ACEVILLE PTE LTD`；PDB/结构指向 TenProtect/TPShell。
   - 含大型 `.tvm0` 虚拟化/保护段，是 ACE/TenProtect 启动链边界。

3. `GameAssembly.dll`
   - Unity IL2CPP 逻辑模块。
   - 可由 `UnityPlayer.dll` 在未加载 Base/TPShell 的情况下装入。

补充边界：`UnityPlayer.dll` 导出 `UnityMain`，ordinal 1。自制启动器通过 `LoadLibraryW(UnityPlayer.dll)` + `GetProcAddress("UnityMain")` 直接进入 Unity Player，并显式传入 `-dataFolder "C:\Program Files (x86)\preternatural\超自然行动组_Data"`。

## 生成物

- `D:\vs\ACE boli\noace_launcher\NoAceUnityLauncher.cpp`
- `D:\vs\ACE boli\noace_launcher\NoAceUnityLauncher.exe`

启动器不修改游戏原文件，不导入 `超自然行动组Base.dll`，也不导入任何 ACE DLL/SYS。静态导入仅有 `KERNEL32.dll`、`USER32.dll`。

## 动态验证

执行方式：

```powershell
Start-Process -FilePath 'D:\vs\ACE boli\noace_launcher\NoAceUnityLauncher.exe' \
  -ArgumentList @('-batchmode','-nographics','-logFile','D:\vs\ACE boli\noace_launcher\noace_unity_run2.log') \
  -WorkingDirectory 'C:\Program Files (x86)\preternatural'
```

验证结果：

- 进程 8 秒后仍存活。
- 已加载模块包含：
  - `C:\Program Files (x86)\preternatural\UnityPlayer.dll`
  - `C:\Program Files (x86)\preternatural\GameAssembly.dll`
  - `C:\Program Files (x86)\preternatural\baselib.dll`
- 未加载：
  - `超自然行动组Base.dll`
  - `AntiCheatExpert\ACE-Base64.dll`
  - `AntiCheatExpert\ACE-CSI64.dll`
  - `ACE-Service64.exe`
- 服务/驱动状态验证：
  - `AntiCheatExpert Protection`：不存在 / 1060
  - `ACE-BASE`：不存在 / 1060
  - `ACE-GAME`：不存在 / 1060
  - `ACE-ADVT`：不存在 / 1060
  - `AntiCheatExpert Service`：不存在 / 1060
  - `ACE-SSC-DRV64`：不存在 / 1060
  - `ace-game-0`：仍为 STOPPED 的孤立项，未启动

结论：该启动器能够绕过零售 EXE 的 Base/TPShell 导入链，直接装入 UnityPlayer 与 GameAssembly；本轮动态验证未触发 ACE 服务/驱动安装，也未加载 ACE/TPShell 用户态模块。
