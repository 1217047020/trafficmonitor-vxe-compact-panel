# MOONDROP Pudding 电量插件：开发预览

## 主线轮播接入（0.5.0-pudding）

主线生产入口现在是原有 **`VxeBattery.dll`**：复用每5秒轮播和已预留的E3耳机仓图标，真实GAIA结果写入现有`earcase`数据槽。不新增显示列，不改M3/G5/E3掩码、9pt、固定宽度、CPU/内存/网速绘制、菜单或双击处理。

- 同一宿主进程中的独立蓝牙线程每60秒查询一次，仓样本90秒过期；鼠标/手柄线程仍为30秒查询、40秒过期。蓝牙连接等待不会拖慢HID采样。
- 仅使用仓ID3，不用左右耳电量代替仓电量。未知/越界/失败清除旧数值；离线显示`--`，其他不可用或过期显示`?`。充电编码待确认，不剥离高位。
- 构建主线：`./tools/build_traffic_plugin.ps1 -Architecture x64`（或`x86`），输出`build/traffic-compact-x64/VxeBattery.dll`。默认构建测试均不访问设备。
- 仅查询布丁并验证主线状态/轮播链（不启动鼠标/手柄采集，不与正在运行的HID宿主争抢接口）：`./build/traffic-compact-x64/pudding_case_test.exe ./build/traffic-compact-x64/VxeBattery.dll --collect`。
- 不需要再安装下面的独立`PuddingBattery.dll`；它及相关工具保留作开发/协议回归参考。主线保留现有手柄/图标实现。
- 经用户授权，主线`VxeBattery.dll`已更新到本机TrafficMonitor，正常退出/重启后确认新进程加载0.5.0-pudding候选，配置逐字节不变，9pt和267×64窗口保持。尚未宣称真实任务栏像素/交互或长期稳定性验收完成。

## 当前状态

- 独立原生 `PuddingBattery.dll`，不需要 Python/Node/手机 App 常驻，不复用鼠标 HID 协议。
- 独立开发DLL有三个可选宿主项目：耳机仓、左耳、右耳；主线只将仓电量接入现有轮播，未改已安装的TrafficMonitor配置。
- 初期 Windows 走 JL 认证/RCSP，设备拒绝电量命令；这不能排除同一串口还支持 GAIA。
- 已从用户手机真实 HCI 流量和 App 日志证明：GAIA API 3 / transport 4，左耳 100%、右耳 100%、仓 23%。手机早先的 32% 不是最新值。
- 采集器已改为实际 App 请求链：legacy `GET_API_VERSION` → 支持电池列表 → 三路电量。
- **Windows 已在仓 23% 和 84% 两种真实电量状态下与手机 App 对照一致**，左右耳均为 100%。23% 有两端原始 HCI/GAIA 帧匹配；84% 是 Windows GAIA 帧与手机 App 运行日志匹配，不是第二次完整 HCI 抓包。
- 充电时仓字段出现 DE/DF 越界值；拔掉仓 USB 后先读到 97%，随后继续变化。短时切换记录证明 Windows/手机均为 84%；已校正手机比电脑慢约 320 秒的时钟差，两个样本约相隔 15.45 秒。
- `pudding_battery_validated=true`：有效实时 0..100 数值已启用；不显示猜测、固定值、零值替代或过期值。充电时编码未确认，不擅自剥离高位，目前异常值显示不可用。独立开发DLL不另安装，主线部署状态见上。
- x64/x86 DLL 均在独立测试进程通过真实后台采集：耳机仓 76%、左右耳各 100%；随后独立 native 查询同样返回 100/100/76。两架构各 99 项离线检查和 DLL ABI 测试通过，原鼠标回归 13/13 通过。这些是插件接口实测，不是已安装到 TrafficMonitor 的验收。

## 构建和离线测试

Windows、PowerShell 7.2+、MSVC Build Tools 与 Windows SDK：

```powershell
./tools/build_pudding_plugin.ps1 -Architecture x64
# 32 位宿主：
./tools/build_pudding_plugin.ps1 -Architecture x86
```

输出 `build/pudding-x64/PuddingBattery.dll`（或 x86）。静态 C++ runtime，系统依赖包括 `WS2_32`、`BthProps`、`BCRYPT`、`KERNEL32`。
默认构建运行离线协议检查和无设备访问的 DLL 接口测试；不扫描、配对或连接蓝牙。手机实包脱敏 fixture 为 `tests/fixtures/pudding-phone-gaia.json`，Windows 重连实包为 `tests/fixtures/pudding-windows-gaia.json`；检查包含 legacy RCSP 调查回归、GAIA 流解析及启用/关闭显示的负向用例。

## 手动只读真机探测

先让 Windows 连接已配对的 `MOONDROP Pudding`；不要同时运行多个查询程序。

```powershell
./build/pudding-x64/native_probe.exe
# 验证开发 DLL 的后台采集及显示（在独立测试进程里，不打开 TrafficMonitor）：
./build/pudding-x64/plugin_test.exe ./build/pudding-x64/PuddingBattery.dll --collect
# 要求这次必须读到三路真实百分比：
./build/pudding-x64/plugin_test.exe ./build/pudding-x64/PuddingBattery.dll --collect --expect-numeric
# Windows 蓝牙关闭/设备离线时，要求不显示任何数字：
./build/pudding-x64/plugin_test.exe ./build/pudding-x64/PuddingBattery.dll --collect --expect-unavailable
```

`native_probe` 返回码：`0` = 收到完整 GAIA 三路响应（仍须手机对照）；`2` = 未取得有效仓电量，包括不支持、离线、超时或错误。这不是构建失败。
日志只打印状态、协商版本和非身份性的电量帧，不打印 MAC、序列号、认证挑战/证明或原始 TargetInfo。初期失败证据仍保留于忽略目录，但**当前采集器不再执行 JL 认证或 RCSP 查询**。

手机已经证实的实际电量帧（不是 Windows 成功日志）：

```text
RX FF 04 00 06 00 1D 1B 01 01 64 02 64 03 17
left=100 right=100 case=23
```

DLL 中 `DataRequired` 仅启动一次后台采集，不阻塞 UI。不支持和多个同名设备时停止重试；其余情况间隔 60 秒。宿主没有线程停止回调，采集启动后 DLL 固定到进程结束，进程退出才释放；未启动采集的接口测试可正常卸载。

## 下一步验收

1. 已完成手机电量传输/请求定位；不能仅凭手机回包宣称 Windows 插件完成。
2. 手机临时 HCI 设置已恢复，并已通过关闭/重新开启蓝牙重新加载原日志模式；最终手机蓝牙关闭。仅保留布丁 SPP 的 API/电量包，不收集完整 bugreport，不保存其他设备原始流量。
3. 已完成两种有效电量状态对照、Windows 重复新连接、无蓝牙时 DLL 不显示数字；离线/过期/异常值离线测试覆盖。仍需充电编码、完整断连重连显示、休眠和长期稳定性验收。
4. 数值输出已按真实对照结果启用；x64/x86 生产 DLL 后台数值显示均已在真实设备连接时确认，但不能将独立接口测试当作 TrafficMonitor 安装验收。
5. 现有M3/G5/E3图标和轮播布局此前已由用户确认；本次只接入数据，未改绘制/尺寸。已按用户“更新”请求完成备份、正常退出/替换/重启及配置/模块核验；仍需真实任务栏轮播交互验收。
