# 旧桥接方案（历史记录）

0.1版本曾使用以下链路验证协议：

```text
VXE R1 → BatteryMonitor JS → 本机WebSocket → TrafficMonitor DLL
```

随后0.2移植为Windows原生HID独立采集，0.3在同一DLL加入极简组合面板。
**日常不需要BatteryMonitor，不启动WebSocket，不要同时运行旧桥接采集者。**

保留文件：`traffic-plugin/bridge-plugin.cpp`、`battery_state.h`、`state_test.cpp`、
`tools/build_traffic_bridge.ps1`及原型JS插件，用于来源核对和历史研究。
旧方案安装/卸载脚本依赖本机安装记录，不是当前面板的初次安装入口。

接口固定版本见[第三方说明](../THIRD_PARTY_NOTICES.md)。当前使用见[独立采集说明](native-plugin.md)。
