# NoACE 一键启动包

双击 `Start_NoACE.cmd`。

入口会自动完成：

1. 查找 `C:\Program Files (x86)\preternatural` 下的 Unity 游戏目录。
2. 校验并安装 `payload\updatescript_500_forcequit_fix.ab`。
3. 在 `backups` 中保留原始 bundle。
4. 预加载包内 ABI v3 桩，然后从 `UnityPlayer.dll` 进入游戏。

默认游戏路径不存在时，设置环境变量 `PRETERNATURAL_GAME_ROOT` 后再双击入口。

运行日志位于 `logs\launcher_status.log`、`bin\logs\launcher_boot.log` 和 `bin\logs\ace_stub.log`。

`source\build_portable.ps1` 可用 Visual Studio 2022 x64 工具链重新编译启动器和桩。

该包只处理客户端启动链和已确认的客户端数据异常强退分支，不修改原始游戏 EXE、`GameAssembly.dll` 或服务端数据。
