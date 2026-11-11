# Z96A 充电排查：已核实的持久记录

更新时间：2026-11-10 15:35，中国时间。此文是当时的快照，操作前必须重新检查设备状态。

**用户明确要求（15:39）取消所有充电定时及恢复服务，未经新授权不得恢复。** `.84` 的 `z96a-charger-runtime-recovery` 已停止并取消开机启动；`.132` 的 charger monitor cron 已删除。下文的服务描述仅用于解释历史，不代表当前启用。

用户随后要求清理 `.84` 多余文件：自动恢复脚本、init 脚本、充电诊断辅助脚本及临时日志已删除；驱动和启动文件备份保留。换线采样日志保存在本机 `rootcause-20261110/cable-compare-20261110.log`。

## 7:27 配置及证据

- 7:27 的 Type-C 补丁版本确实称为 **v5**。`tcpci_rt1711h.ko` SHA256 为 `06fe8814f103bf6d803c38ea090067941db3e69467395d9b227c6b8c9ebbd557`；7:26 的构建输出和 04:25 的 initramfs 校验均可在 Codex 历史中核实。
- 当时 `uInitrd` SHA256 为 `0995128a92c4c96bf29e27eb42aeedc320b71e40ee5f52ff6b1cc6c7f42ef410`，包含上述 v5 模块。不要再声称未找到 v5，或将 v3 哈希 `7df56ee924dcdd27b905ebdac94015e01760bd18e1231c4de424fb161c36e459` 当成 v5。
- 可恢复的 v5 模块和 uInitrd 位于 `.84` 的 `/root/z96a-rootcause-backup-20261110/`；v3 模块在 `/root/z96a-runtime-recovery-v3-backup-20261110-040807/`。系统从 U 盘启动，禁止写 eMMC、U-Boot、GPT 或分区表。
- 7:27 的运行时恢复由 `.84` 的 `/usr/local/sbin/z96a-charger-runtime-recovery` 和 `.132` 的 `/home/hsy/project/z96a-charger-input-recovery/monitor.sh` 配合：后者在输入故障时控制适配器断电约 90 秒后恢复；前者在输入回来但应充电却未充电时尝试重绑定 `bq257xx` MFD。旧整机断电看门狗已移除，不要重新启用。
- 原始记录显示 7:21、7:24 两次适配器 90 秒断电后恢复充电，设备未重启。用户还报告过立即、间隔数秒、间隔 60 秒插回后约 3 秒恢复；这是用户观察，不能无日志证明每次由重绑定触发，也不能否认其观察。

## 2026-11-10 复核

- 15:27 的历史核对确认此前“没有 v5”的说法错误。15:28 左右将磁盘上的模块从 v3 恢复为 v5；`uInitrd` 已是上述 v5 文件，`/sys/bus/i2c/devices/0-004e/recover` 存在。磁盘上 v3 备份为 `/root/z96a-rootcause-backup-20261110/tcpci_rt1711h-v3-before-restore.ko`。
- 15:29 测试适配器断电 5 秒再上电，约 30 秒内仍 `online=0 / Discharging`；恢复服务已运行。随后 90 秒适配器断电使 `online=1 / Not charging` 恢复，电量约 95%。
- 手动重绑定一次 `bq257xx` MFD 后观察到 `online=1 / Charging / 960000`（电流单位 µA）。接近满电可能影响是否充电；该单次观察不能单独证明因果，更不能证明全部故障已修复。
- 在 `online=0`、充电器状态寄存器 `0x20=0x0000` 的现场，单独重绑定 `bq257xx` 曾无效。不要把“输入未识别”和“输入已识别但未充电”混为同一个已证实根因。90 秒断电及重绑定是恢复方法，不是根因已解决的证据。
- 插座自动恢复与手动测试必须使用 `.132` 的 `/tmp/z96a-charger-input-recovery.lock` 协调；检查监控是否正在运行。确保异常退出后插座开启，并核实真实充电状态。电量接近满电时 `Not charging` 可以正常。
- 适配器断电并整机关机后曾未自动开机，用户手动按电源键才启动。不能断言是 v3 造成；不要声称已确认关机或自动启动，除非实际验证。

参考：`CHARGER-RUNTIME-RECOVERY.md`、`CHARGER-ROOT-CAUSE-STATUS-20261110.md`；较旧文档如与本记录冲突，以带时间戳的原始记录和本记录的更正为准。`.132` 测试记录：`/home/hsy/project/z96a-charger-input-recovery/principle-test-20261110-150917.log` 和 `recovery.log`。
