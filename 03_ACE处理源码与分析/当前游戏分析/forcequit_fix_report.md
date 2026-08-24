# 数据异常强退修复报告

## 结论

已定位并修复 `数据异常，请检查客户端环境后再尝试重登` 对应的客户端强退分支。

- 热更新程序集：`UpdateScript_500.dll`
- 类型/方法：`DPNKPNAACPA::FODJBPAEILJ`
- MethodDef：`0x060233D5`
- 分支类型：`ForceQuitNotice type == 1`
- 本地化文本 ID：`72000001`
- 原确认回调：`<>c::DBJDBDPCMNE -> UnityEngine.Application::Quit()`

健康系统日志 `MsgHealthState:OK` 与该弹窗无关，本次没有修改健康系统、防沉迷、网络错误或资源错误流程。

## 补丁

CAB 节点：`CAB-9129cc3845e159f1b33e5a3abde50bda`

- CAB 文件偏移：`0xD63B61`
- 内嵌 DLL 文件偏移：`0xD63045`
- 原 IL 字节：`17 33 50`，即 `ldc.i4.1; bne.un.s IL_00AC`
- 新 IL 字节：`26 2B 4F`，即 `pop; br.s IL_00AB`

补丁只让 `type == 1` 在显示数据异常提示和设置退出回调之前直接返回。`type == 2` 的重登处理以及其他强退类型保持原逻辑。

## 文件校验

- 原 bundle SHA256：`D0E7CCEABB57AD5A05C072ED4D9FF3B82D5D9F4364803E13169089BAB21BF6A0`
- 修复 bundle SHA256：`F25AB3ADABCEC20CEF0EC25E19A6BEFC086C1950F1677D44A80B68C837555666`
- 文件大小：`12,387,748` 字节，与原文件一致
- 解压后 CAB 差异：仅目标 3 字节
- 内嵌 PE：x86 PE、3 个 section、CLR descriptor RVA `0x2008`，解析正常

已部署到：

`C:\Users\Administrator\AppData\LocalLow\pi\超自然行动组\bundles\data\code\updatescript_500.dll.ab_u_4548ac6984db86d9f5a2ad35fbb456b9`

原文件备份：

`D:\vs\ACE boli\backups\updatescript_500.original_D0E7CCEABB57AD5A05C072ED4D9FF3B82D5D9F4364803E13169089BAB21BF6A0.bak`

## 动态验证

- 启动器：`D:\vs\ACE boli\noace_launcher\run_noace.ps1`
- 启动时间：`2026-07-29 19:01:21`
- 连续监控：15.4 分钟
- 进程：全程存活并响应
- `ForceQuitNotice`：0
- `数据异常` / `72000001`：0
- 断线标记：0
- 部署文件启动后 SHA256：仍为修复版哈希

本轮没有收到服务端 `ForceQuitNotice`，因此动态验证覆盖了启动、登录、资源加载和原故障时间窗口，但没有在运行时实际进入已修改分支。该分支的静态 IL、跳转目标、栈平衡和退出回调均已单独核验。

日志中另有图片缓存 URL 被拼成 Windows 目录后产生的 `System.IO.IOException`，以及一次线程结束时的 `ThreadAbortException`。二者与数据异常强退分支无关。
