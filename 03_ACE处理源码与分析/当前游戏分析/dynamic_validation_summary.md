# 动态验证摘要

## 正常结果

- NoACE Launcher 能以正确工作目录启动 Unity Player。
- 可选更新提示修改后进入大厅。
- 地图内桥接可读取怪物、物资和投射物。
- 外部绘制窗口菜单关闭时点击穿透，菜单打开时可交互。
- 最小化时移除置顶，恢复后重新置顶。

## 卡死记录

一次大厅会话出现主线程无心跳，日志持续报告：

- 最后阶段：`Driver.FixedUpdate` 完成后等待。
- 网络回调：`module=6, cmd=6`。
- 方法：`HOPPMNFEIPG+OIHHOEGHPGG.KDACBHFDKNN`。
- 绘制端状态：`available=0x0`、`applied=0x0`，未应用功能写入。

结束该进程后，通过 `Start_NoACE.cmd` 重新启动，纯游戏基线运行正常并进入地图；随后启动绘制端，进程仍保持响应。

