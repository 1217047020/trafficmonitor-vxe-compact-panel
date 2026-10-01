# 独立鼠标电量采集与安装

当前0.3.0 DLL直接读取VXE R1星闪接收器；保留原单电量项目并新增组合面板。
只运行TrafficMonitor，无需BatteryMonitor、Qt、hidapi.dll、WebSocket或伴随进程。

## 协议

- 精确匹配VID `373B`、PID `10C9`、`MI_01/Col05`、Usage Page `FF02`/Usage2、输入/输出17字节。
- 多个匹配设备拒绝查询，避免读错设备。
- 唯一请求：`08 04 00 00 00 00 00 00 00 00 00 00 00 00 00 00 49`。
- 回复必须17字节、ID08/命令04/状态00，载荷低8位校验和4D，第6字节电量0–100；不推测充电状态。
- 一个后台线程，30秒单次查询。命名互斥锁避免本插件多个实例并行查询。
- 输入队列64报告，清空旧输入并预置异步读取后发送命令；单次写/读各500ms，取消清理最多再等100ms。
- 枚举和设备打开另计；不能把整个查询概括为绝对500ms。
- 取消不完成时保留仍可能被内核使用的缓冲区并停止线程，避免悬空指针和循环泄漏。
- 超过40秒未完成采样显示不可用。绘制及悬停不访问设备。
- 宿主缺少插件卸载回调，DLL固定到进程生命周期；更新或回退需退出宿主。

## 初次安装

1. 备份TrafficMonitor配置与已有同名DLL，正常退出宿主。
2. 把对应架构DLL复制到`<TrafficMonitor>/plugins/VxeBattery.dll`。
3. 启动后任务栏只启用`vxe_compact_panel`，关闭重复五项显示，但不关闭采集。
4. 保持双行、9pt。原桌面备用皮肤可使用`vxe_r1_battery`。

默认不修改自启、连接选择、桌面皮肤、原菜单或历史记录。

## 已有本机0.2安装的迁移

`tools/set_compact_panel.ps1`依赖本机`evidence/traffic-bridge-install.json`安装记录。
记录含精确安装路径和原DLL哈希，不上传到仓库。不存在时请使用初次手动安装流程，不伪造记录。

```powershell
# 此命令是预演，仅适用于仍安装0.2的本机
./tools/set_compact_panel.ps1
# 先正常退出宿主；已确认视觉才应用
./tools/set_compact_panel.ps1 -Apply -VisualConfirmed
```

应用前备份原DLL、完整配置和安装记录到`work/compact-switch/<时间>`，返回确切备份路径，不自动强杀宿主。
只修改五个任务栏显示键；启动时宿主自动添加新项目排序、标签和颜色槽。
发生文件变化、哈希不符、点名键冲突或宿主仍运行则拒绝。写入失败恢复原件。

## 回退

正常退出宿主，使用应用时报告的真实备份目录：

```powershell
./tools/set_compact_panel.ps1 -Mode rollback -BackupDirectory '<你的本机备份绝对路径>' -Apply
```

正常回退恢复原DLL和点名显示键，不整份覆盖配置，保留后来无关设置和历史。
旧桌面模式脚本或彻底卸载须先回退组合面板。

Windows API依据：[读取HID报告](https://learn.microsoft.com/en-us/windows-hardware/drivers/hid/obtaining-hid-reports)、
[取消异步I/O](https://learn.microsoft.com/en-us/windows/win32/fileio/canceling-pending-i-o-operations)。
