# CZR绕过反作弊开源项目交流群:835775657

这是一个面向当前 Unity 游戏版本的客户端启动辅助与资源版本适配项目，包含启动组件、Bundle 分析工具、版本化资源载荷和验证资料。

## 项目结构

```text
03_ACE处理源码与分析/
├─ 源码/
│  ├─ analysis/      UnityFS、IL2CPP 定位与 Bundle 修补脚本
│  ├─ launcher/      启动组件与兼容接口源码
│  └─ project_scripts/辅助脚本
├─ 当前游戏分析/     当前版本的结构与兼容性分析报告
├─ IL2CPP定位资料/    dump.cs、字符串表和必要 Unity 资料
├─ 补丁样本/          原始与修补后的 Bundle
└─ 验证证据/          启动、更新提示和窗口行为截图

04_NoACE一键启动程序/
├─ Start_NoACE.cmd    一键入口
├─ Start_Game_NoACE.ps1 自动定位、校验、备份和部署载荷
├─ bin/               启动器和兼容组件
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

## 免责声明

本项目中的代码、脚本、样本和文档用于软件结构研究、版本适配、兼容性验证以及本地测试记录。首页仅提供概念性说明，具体实现以仓库文件和对应版本资料为准。

使用者应自行确认运行环境、文件来源和使用范围，并保留原始文件与回滚副本。项目维护者不对因版本变化、环境差异、第三方组件或误操作造成的数据损失、程序异常、账号状态变化或其他影响承担责任。

请勿将本项目用于影响他人设备、账户、服务或网络环境的活动，也请遵守适用的产品条款、软件许可和当地法规。使用、修改和分发本项目即表示接受上述说明。
