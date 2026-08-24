# ACE CZR 项目

这是一个面向当前 Unity 游戏版本的客户端启动与资源修补项目，包含 ACE 接口桩、NoACE 启动器、Bundle 分析工具、版本化修补载荷和验证资料。

## 项目结构

```text
03_ACE处理源码与分析/
├─ 源码/
│  ├─ analysis/      UnityFS、IL2CPP 定位与 Bundle 修补脚本
│  ├─ launcher/      NoACE 启动器和接口桩源码
│  └─ project_scripts/辅助脚本
├─ 当前游戏分析/     ACE 与当前游戏版本的分析报告
├─ IL2CPP定位资料/    dump.cs、字符串表和必要 Unity 资料
├─ 补丁样本/          原始与修补后的 Bundle
└─ 验证证据/          启动、更新提示和窗口行为截图

04_NoACE一键启动程序/
├─ Start_NoACE.cmd    一键入口
├─ Start_Game_NoACE.ps1 自动定位、校验、备份和部署载荷
├─ bin/               启动器和 ACE 接口桩
├─ payload/           当前版本修补载荷
├─ backups/           自动生成的回滚副本
└─ docs/              补丁说明
```

## 快速启动

默认游戏位置：

```text
C:\Program Files (x86)\preternatural\preternatural.exe
```

双击：

```text
04_NoACE一键启动程序\Start_NoACE.cmd
```

也可以使用环境变量指定其他安装位置：

```powershell
$env:PRETERNATURAL_GAME_EXE = '游戏目录\preternatural.exe'
```

脚本会自动完成资源定位、SHA-256 校验、原始文件备份、载荷部署和启动器运行检查。运行日志位于 `04_NoACE一键启动程序\logs` 及 `bin\logs`。

## 当前版本说明

当前载荷针对 Unity `2022.3.62f3` 资源版本生成。脚本只接受已记录的 Bundle 哈希；游戏资源更新后，需要重新定位 IL 锚点并生成对应载荷。

项目保留原始 Bundle、修补 Bundle、哈希清单和分析脚本，便于回滚、复核和后续版本更新。

## 构建启动器

使用 Visual Studio 2022 x64 工具链运行：

```powershell
03_ACE处理源码与分析\源码\launcher\build_portable.ps1
```

构建前请确认游戏目录和 `UnityPlayer.dll` 存在。编译产物会写入 `04_NoACE一键启动程序\bin`。

## 校验资料

- `SHA256SUMS.txt`：交付文件 SHA-256
- `FILE_MANIFEST.csv`：文件清单、大小和分类
- `CLEANUP_REPORT.md`：项目整理和验证记录
- `03_ACE处理源码与分析\README_ACE处理流程.md`：完整分析流程
