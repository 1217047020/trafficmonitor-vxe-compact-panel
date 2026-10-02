# 第三方许可和来源

- 设备图标 M3：Phosphor Icons `mouse-simple`，MIT；SVG 保存在 `traffic-plugin/assets/device-icons/mouse.svg`，完整许可证为 `licenses/Phosphor-Icons-MIT.txt`。来源：https://github.com/phosphor-icons/core
- 设备图标 G5、E3：Solar / 480 Design，`gamepad-minimalistic-linear`、`airbuds-case-open-linear`，CC BY 4.0。来源：https://www.figma.com/community/file/1166831539721848736 ，源数据：https://github.com/iconify/icon-sets/blob/master/json/solar.json 。许可及署名记录：`licenses/Solar-Icons-CC-BY-4.0.txt`；许可全文：https://creativecommons.org/licenses/by/4.0/legalcode.en
  原 SVG 形状保留，仅进行栅格化、单色 alpha 掩码转换与主题着色；不暗示原作者认可本插件。来源明细为 `traffic-plugin/assets/device-icons/sources.json`。掩码在构建时生成，运行时内嵌在 DLL，不增加后台程序或图像库依赖。

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

- web-bluetooth-e87，Felix Herbst，MIT，固定提交 `9e8ba39f48ffb7ef447847b5bf35afc4fe522ed3`：`pudding-plugin/jl_auth.h` 移植其公开杰理双向认证算法，仅保留为历史诊断/离线回归，不用于当前GAIA电量采集。完整许可证 `licenses/web-bluetooth-e87-MIT.txt`。来源：https://github.com/hybridherbst/web-bluetooth-e87/blob/9e8ba39f48ffb7ef447847b5bf35afc4fe522ed3/web/src/jl-auth.ts
- 布丁适配仅阅读社区 GAIA 项目、杰理 Android SDK 与 MOONDROP 官方 APK 以对照协议；不提交或分发这些反编译源码、APK、固件及设备原始身份数据。参见 `docs/pudding-protocol.md` 的来源与验证边界。

`upstream/`、原版发布ZIP和工作解压目录不提交到本仓库。本仓库仅发布插件及相关开发资料，不分发TrafficMonitor、BatteryMonitor、Qt或hidapi二进制。第三方源码保留原始许可证与来源说明。
