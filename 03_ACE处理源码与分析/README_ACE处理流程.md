# 游戏 ACE 处理与分析过程

## 1. 样本与环境盘点

1. 对游戏根目录、`AntiCheatExpert`、主 EXE、UnityPlayer、GameAssembly 和资源 Bundle 建立文件清单。
2. 记录 PE 架构、导入导出、签名、版本、节区和 SHA-256。
3. 查询 ACE 服务、驱动、设备名、管道和安装目录，区分游戏态 DLL、服务和内核驱动。
4. 保存原始文件，只在工作副本和用户资源缓存中部署修改。

本机分析确认的核心链路：

`游戏入口 -> ACE 客户端 DLL -> ACE 服务/驱动 -> UnityPlayer.dll!UnityMain`

## 2. ACE 接口与启动链定位

### 角色

- `ACE-Base64.dll`：游戏进程侧初始化与通信入口。
- `ACE-Service64.exe`：服务、驱动加载和进程通信。
- `ACE-BASE.sys`：设备接口及内核监控。
- 原游戏入口：包含保护启动层，最终进入 Unity Player。

### 证据

- `InitAceClient*` 导出。
- `CreateService/StartService/DeviceIoControl` 等导入。
- `\\.\ACE-BASE`、ACE 服务名和命名管道字符串。
- 零售启动后会重新安装 ACE 服务和驱动。

### 当前实现

1. 按当前 DLL 导出表生成兼容桩，保留名称、序号和调用约定。
2. `NoAceUnityLauncher.exe` 设置正确游戏工作目录和 DLL 搜索目录。
3. 启动器加载接口桩，再加载 `UnityPlayer.dll`，调用 `UnityMain`。
4. 使用 `-dataFolder` 指向当前游戏的 Unity 数据目录。

源码位于 `源码\launcher`，ABI 探测和构建脚本也保留在该目录。

## 3. 数据异常强退定位

日志出现“数据异常，请检查客户端环境后再尝试重登”后，对热更新 Bundle 解包并解析 .NET IL。

- 程序集：`UpdateScript_500.dll`
- 方法：`DPNKPNAACPA::FODJBPAEILJ`
- MethodDef：`0x060233D5`
- 业务角色：处理 `ForceQuitNotice type == 1`
- 原回调最终调用：`UnityEngine.Application::Quit()`
- Bundle 文件偏移：`0xD63B61`
- 内嵌 DLL 偏移：`0xD63045`
- 原字节：`17 33 50`
- 修改字节：`26 2B 4F`

修改仅让 `type == 1` 在设置退出回调前返回；其他类型分支保持原流程。

哈希：

- 原始：`D0E7CCEABB57AD5A05C072ED4D9FF3B82D5D9F4364803E13169089BAB21BF6A0`
- 修改：`F25AB3ADABCEC20CEF0EC25E19A6BEFC086C1950F1677D44A80B68C837555666`

## 4. 可选更新提示定位

- Bundle：`il2cppscripts_0.dll.ab`
- 程序集：`IL2CppScripts_0.dll`
- 类型/方法：`AssetsManagerStart.CheckUpdateTipUI()`
- MethodDef：`0x060000E0`
- RVA：`0x671C`
- 原字节：`2D 08`，`brtrue.s`
- 修改字节：`26 00`，`pop; nop`

结果：`updatetype=1` 延续原回调但不设置更新状态；`updatetype=2` 的强制更新流程保留。

哈希：

- 原始：`D9032C445FED9206D8F11238A6721F0F3BEAD9857610A579AED087B9A47A1B55`
- 修改：`ED9DA6D2B0161A16B3FCCD8578A46032455729969EA73D373F83F5D6CFE2077F`

## 5. 一键部署

`04_NoACE一键启动程序\Start_Game_NoACE.ps1` 会：

1. 校验启动器、原始载荷和修改载荷的 SHA-256。
2. 查找当前用户资源目录中的目标 Bundle。
3. 只接受已知原始哈希或已知修改哈希。
4. 首次部署时创建回滚副本，使用临时文件完成原子替换。
5. 同时处理数据异常 Bundle 和可选更新 Bundle 的普通名、哈希名副本。
6. 使用正确工作目录启动 NoACE Launcher，并写入启动状态日志。

## 6. 动态验证结果

- 可选更新提示修改后成功进入大厅。
- 数据异常强退分支的 IL、跳转目标和栈平衡完成静态复核。
- 外部桥接在地图内读取到怪物、物资和投射物，典型扫描耗时约 `0.3-2 ms`。
- Alt+Tab 的透明、置顶和点击穿透状态完成窗口级验证。
- 一次大厅卡死定位为网络消息回调 `module=6, cmd=6` 阻塞，运行方法为 `HOPPMNFEIPG+OIHHOEGHPGG.KDACBHFDKNN`；绘制端当时未应用功能位。

## 7. 当前版本与通用方法的边界

本文件中的 MethodDef、RVA、Bundle 偏移、原始字节和哈希只属于当前资源版本。处理其他游戏或更新版本时，必须通过字符串、调用关系、导出表和数据流重新定位，随后重新计算偏移并核验原字节。

