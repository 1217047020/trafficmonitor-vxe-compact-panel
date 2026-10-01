# 第三方许可和来源

- BatteryMonitor v1.4.260908，JKWTCN，MIT：发布宿主、hidapi DLL、插件 API 和响应头候选依据。源码提交 `20d278eb577fd775c454427e3eb9cf73e89d97cd`。
  来源：https://github.com/JKWTCN/BatteryMonitor
  完整许可证：`licenses/BatteryMonitor-MIT.txt`。
- atk-tool，Levon Kirakosyan，MIT：设备匹配、只读电量命令、请求校验及候选电量字段偏移。固定源码提交 `fb4e0c65b5fa060dd4a16fa0ecd1639b9e0903fa`。
  来源：https://github.com/noosxe/atk-tool
  完整许可证：`licenses/atk-tool-MIT.txt`。

- TrafficMonitor V1.86，Zhong Yang，Anti 996 License 1.0：插件接口头文件；提交 `02a817a069bac6bf4d263b5209d9c1b07fe2f950`。接口复制在 `traffic-plugin/vendor/PluginInterface.h`，完整许可证 `licenses/TrafficMonitor-Anti996.txt`。来源：https://github.com/zhongyang219/TrafficMonitor
- nlohmann/json 3.12.0，Niels Lohmann，MIT：JSON解析；原始单头文件 `traffic-plugin/vendor/json.hpp`，完整许可证 `licenses/nlohmann-json-MIT.txt`。来源：https://github.com/nlohmann/json/tree/v3.12.0
- 当前0.3独立DLL不编译上述JSON库，也不链接Qt/hidapi/BatteryMonitor；JSON源仅用于保留的0.1桥接版。
- HIDAPI 0.15.0，固定提交 `d6b2a974608dec3b76fb1e36c189f22b9cf3650c`：仅核对Windows API使用和64报告输入缓冲行为，不编译或链接该库。来源：https://github.com/libusb/hidapi/tree/hidapi-0.15.0

`upstream/`、原版发布ZIP和工作解压目录不提交到本仓库。本仓库仅发布插件及相关开发资料，不分发TrafficMonitor、BatteryMonitor、Qt或hidapi二进制。第三方源码保留原始许可证与来源说明。
