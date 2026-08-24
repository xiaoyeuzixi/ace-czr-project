# 可选更新提示修复报告

- 本地资源版本：`795`
- 服务端资源版本：`795`
- 服务端更新类型：`updatetype=1`
- 程序集：`IL2CppScripts_0.dll`
- 方法：`AssetsManagerStart.CheckUpdateTipUI()`
- MethodDef：`0x060000E0`
- RVA：`0x671C`
- 原字节：`2D 08`
- 修改字节：`26 00`
- 原 Bundle SHA-256：`D9032C445FED9206D8F11238A6721F0F3BEAD9857610A579AED087B9A47A1B55`
- 修改 Bundle SHA-256：`ED9DA6D2B0161A16B3FCCD8578A46032455729969EA73D373F83F5D6CFE2077F`
- 修改后内嵌 DLL SHA-256：`0E37D9C396012F8CFCE407088B7367FA028C37068C39120D7CC3658FF2AE98E3`

`updatetype=1` 不再设置启动更新状态，`updatetype=2` 的强制更新分支保持原逻辑。部署后成功越过提示并进入大厅。

