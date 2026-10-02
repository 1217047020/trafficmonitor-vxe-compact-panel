# 布丁协议调查（2026-10-02）

## 最新证据：手机控制通道已抓到

用户连接安卓手机并开启 USB 调试后，ADB 只读取目标 App 日志与蓝牙日志。App 包版本 `2.26.1c-260930ai`，目标蓝牙 peer 与 Windows 配对布丁一致。
完整 HCI 抓包只保留该 peer 的 SPP API/电量 GAIA 帧；未保留其他设备、HFP、音频、配对密钥或整机 bugreport。

```text
TX FF 01 00 00 00 0A 03 00                 GET_API_VERSION (legacy transport1)
RX FF 04 00 04 00 0A 83 00 00 04 03 01     status0 / transport4 / API3.1
TX FF 04 00 00 00 1D 1A 00                 GET_SUPPORTED_BATTERIES
RX FF 04 00 02 00 1D 1B 00 01 02           lists left1 / right2
TX FF 04 00 02 00 1D 1A 01 01 02           GET_BATTERY_LEVELS
RX FF 04 00 06 00 1D 1B 01 01 64 02 64 03 17
                                             left100 / right100 / case23
```

**支持列表没列出仓 ID3，但电量响应自动包含 ID3。** 不可因列表仅有 1/2 而丢弃仓字段，也不要擅自把左耳数量当仓电量。
同一链路重复多次均返回仓 23%，与手机 App `syncBatteryLevel ... budscase: 23` 日志一致。

随后 Windows 实际收到 `FF040006001D1B010164026403FF`：仓字段 FF 不属于 0..100，必须显示不可用，不能保留旧 23%、映射成零或按充电高位剥离成 127%。具体物理触发条件尚未确定，保留为负向 fixture。

用户进一步确认“两只耳机在仓内、仓盖打开、USB 充电、Windows 连接、手机蓝牙关闭”后，两次新建连接分别返回 `FF040006001D1B010164026403DE`、`FF040006001D1B010164026403DF`。仓原始值为 222/223，仍不是有效百分比；目前没有手机 App 对照证明其编码含义，**不能擅自去掉高位解释为 94%/95% 或认定为充电标志**。已加入拒绝异常值且不覆盖旧输出的回归测试。

仅拔掉耳机仓 USB 线，其他物理状态保持不变后，两次新连接均返回 `FF040006001D1B01016402640361`，即左 100%、右 100%、仓 97%。这证明当前充电条件会影响仓字段，但没有证明充电时的编码规则；仓 97% 仍待手机 App 新鲜读数对照，数值开关保持关闭。

随后通过短时自动记录切换前后的实际数据，Windows 三次新连接返回 `FF040006001D1B01016402640354`（100/100/84），手机重新连接后的 App 日志也持续为 100/100/84。校准发现手机时钟比电脑慢约 319.86 秒，校准不确定度约 1.05 秒；最后一个 Windows 样本与第一个重新连接后的手机样本相隔约 15.45 秒。此前直接比较两端墙上时间所得“约 12 分钟”不是可靠计时依据。仓 23% 与 84% 两状态对照现已通过，0..100 数值输出已启用，充电异常值仍拒绝。这次手机证据是 App 日志，未开启完整 HCI 日志。

采集器已改为这条初始化链，无需 JL 双向认证。GAIA framed length 排除四字节 vendor/command 头；API-major3 与 transport-version4 是不同层次。
Windows 随后通过生产 `query()` 最小链路，在三个新建 RFCOMM 连接中重复读到同一电量帧，与手机一致；开发 DLL 后台采集也成功，第二个变化后的仓 84% 也已完成 App 对照。
脱敏 fixture：`tests/fixtures/pudding-phone-gaia.json`、`tests/fixtures/pudding-windows-gaia.json`。

临时 HCI 属性已恢复到原模式；蓝牙服务需重启/关闭再开启才能重新加载。手机安装 APK 的拉取曾截断，未把这些损坏 APK 用作代码证据。以下是调查历程，不能覆盖以上更新证据。

## 证据优先级与结论

初期真机 SDP 将标准 SPP 服务指向 **JL_SPP / RFCOMM channel 1**，JL 认证与 RCSP 路径可执行。现在的手机抓包证明同一设备也使用 GAIA 控制：**服务标签不是唯一协议证明**，亦不能把 RCSP 电量“不支持”泛化为设备无法读仓电量。

| 阶段 | 实测 | 能否证明仓电量 |
|---|---|---|
| 仅枚举已配对、名称严格匹配的设备 | 唯一匹配 | 否 |
| SDP / RFCOMM | JL_SPP channel 1，连接成功 | 否 |
| 双向认证 | 两方向 challenge/proof/pass 均校验通过 | 否 |
| RCSP GetTargetInfo `0x03` | 成功，SDK 标识 `jl_sdk_ac697_publish` | 否；其中数量字段不能挪作仓电量 |
| RCSP GetADVInfo `0xC1` / mask 1 | 回应 status 2，未知命令 | 否 |
| RCSP GetSysInfo `0x07` | 回应 status 2 | 否 |
| 认证前、认证后的 GAIA v4 查询 | 未取得有效电量回复 | 否 |
| 对缓存经典地址建立 BLE 对象 | 未取得 BLE 对象 | 否；不证明不存在另一个 BLE 地址 |
| 20 秒按 MOONDROP/Pudding 名称筛选广告 | 未发现匹配广告 | 否；仅代表当时可见性 |

初期“JL_SPP 可能仅为 OTA 通道”的解释未被证明，应改为分别跟踪 GAIA 控制初始化与 JL/RCSP OTA 分支，不因服务标签或某一个分支失败排除其他分支。

## 已验证状态机

```text
Windows already-paired exact-name device
  -> RFCOMM SPP connect (bounded nonblocking I/O)
  -> client: 00 + random challenge[16]
  <- device: 01 + proof[16] (verify public JL cipher)
  -> client: 02 + ASCII "pass"
  <- device: 00 + challenge[16]
  -> client: 01 + proof[16]
  <- device: 02 + ASCII "pass"
  -> RCSP read-only command
  <- matching opcode + sequence + status
  -> close socket / no setting or firmware changes
```

实际挑战与响应不公开。算法来自 MIT 项目的公开 SDK 密码算法，不读取系统凭据。

## RCSP 布局与脱敏回包

```text
FE DC BA | flags | opcode | body_length_BE16 | body | EF
request flags=C0, body=sequence + parameters
response flags=00, body=status + sequence + response_data
```

只有 opcode、sequence 与响应类型同时匹配才接受。RFCOMM 是字节流，解析器支持分片/粘包；限制单次输入 4096、帧体 1024、缓存 8192 字节。无效帧、未知响应、缺失仓字段不转为百分比。

一条真实、无设备身份字段的失败请求/响应：

```text
TX FE DC BA C0 C1 00 05 03 00 00 00 01 EF
RX FE DC BA 00 C1 00 02 02 03 EF
                         ^^ status=2 / UNKNOWN_CMD
```

重新连接后复用固定 sequence 曾无回复，换 sequence 后恢复；当前单次查询每会话随机 sequence，不发送写配置命令，也不爆破未知 opcode。

## 候选电量 TLV（只在离线合成测试中覆盖）

杰理 Android SDK 的 ADV 格式：属性 `length | type | value`，length 包含 type 不包含 length；type 0 的三字节分别为左耳、右耳、仓，低 7 位为 quantity，高位为充电标志。

**当前 Pudding 未返回该属性，不能宣称这是它的实际电量格式。** `decode_adv` 只作为候选解析器，要求完整三字节、唯一属性、仓数量 0..100；异常值、重复和截断都拒绝。测试中的 `100 / 100 / 32` 明确是合成 fixture，不是原始抓包。

可复现验证：`tools/build_pudding_plugin.ps1` 运行已公开认证测试向量、协议分片/粘包、真实失败帧和负向用例。手动真机复现见 `docs/pudding-plugin.md`，具体日志和 APK/反编译产物仅保存在被 Git 忽略的工作目录。

## 来源（源代码不能覆盖真机结果）

- [PuddingPods GAIA 适配说明](https://github.com/lingbai-rong/PuddingPods/blob/f527afe503749d7d2ceadbf5529c0639d7de1f7f/PUDDING_ADAPTATION.md)：三路 GAIA 电量候选；当前串口不符其前提。
- [MOONDROP 官方 App 页面](https://moondroplab.com/cn/moondrop-link)：从页面提供的 Android 下载链接取得 APK，未安装或修改。
- APK SHA256：`04756B49ACFEA523758D86101C96C1824B7B209AE3A8836C07837088E795D2D5`。jadx 有部分反编译错误，未把错误方法当已证实行为。
- APK `V3BatteryPlugin`：支持电池列表查询与电量查询；`RfcommClient` / `TransportProtocol`：SPP 与 GAIA 传输；`JieliFotaHandler`：杰理 OTA。仅用于找下一条验证路径，尚未和用户手机安装版本及运行请求关联。
- [杰理官方 Android SDK](https://github.com/Jieli-Tech/Android-JL_Bluetooth)：RCSP 封包、ADV TLV、status 2 的语义参考。
- [公开 JL 认证移植来源](https://github.com/hybridherbst/web-bluetooth-e87/blob/9e8ba39f48ffb7ef447847b5bf35afc4fe522ed3/web/src/jl-auth.ts)：MIT；许可证与来源已保留。

## 尚未完成（更新）

App 电量通道、Windows 实机复现及仓 23%/84% 两状态对照已完成，数值显示已开启；x64/x86 各 99 项离线检查和被动 DLL ABI 检查通过。充电编码、长期稳定性、休眠/重启与最终任务栏预览/安装未完成。不能将这些局部通过当作全部验收成功。
