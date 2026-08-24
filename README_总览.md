# ACE/CZR 最终整理包

整理时间：2026-07-30

## 目录

- `01_外挂项目源码_VS2022`：独立 ImGui 绘制端与桥接 DLL 的 VS2022 x64 源码。
- `02_外挂成品_x64`：可直接启动的外部绘制端、桥接 DLL、中文配置和验证资料。
- `03_ACE处理源码与分析`：ACE 静态分析、启动链源码、Bundle 分析脚本、当前版本补丁与复用流程。
- `04_NoACE一键启动程序`：`Start_NoACE.cmd`、NoACE Launcher、接口桩和两项 Bundle 修复。

## 使用顺序

1. 双击 `04_NoACE一键启动程序\Start_NoACE.cmd` 启动游戏。
2. 游戏进入大厅或地图后，双击 `02_外挂成品_x64\Start_External.cmd`。
3. 外部绘制端运行日志会在其所在目录自动生成。

## 编译

使用 Visual Studio 2022 打开：

`01_外挂项目源码_VS2022\PreternaturalInject.sln`

选择 `Release | x64`，或运行 `Build_Release_x64.cmd`。

## 校验

- `SHA256SUMS.txt`：交付文件的 SHA-256。
- `FILE_MANIFEST.csv`：相对路径、大小和分类。
- `CLEANUP_REPORT.md`：清理前后体积及删除类别。

