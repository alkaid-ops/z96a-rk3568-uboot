# Z96A charger root-cause status (2026-11-10)

## Confirmed automation interference

The archived watchdog log records recovery runs at 03:35, 03:46, 03:55, 04:10, and 04:20 while the charger reported `online=0` and `Discharging`. Its recovery helper turns the adapter outlet off, then runs `/sbin/poweroff` on the Z96A before restoring the outlet. These were real SoC power-offs, so the earlier 8/15/30/60/90-second measurements around 03:42 onward were not isolated adapter tests. The old watchdog was disabled at 04:28.

The separate 90-second recovery checks at 07:21 and 07:24 occurred after that watchdog was disabled. Those confirm that a 90-second adapter interruption could recover the charger, but they did not establish that shorter interruptions fail.

## Controlled retest

On 2026-11-10 from 11:23 to 11:38, the outlet was tested at 8, 15, 30, 60, and 90 seconds. The on-device rebind daemon was stopped during each test, the `.132` cron monitor was excluded with its shared lock, and each return was observed for at least 120 seconds. Every run changed from `online=0/Discharging` while the outlet was off to `online=1/Not_charging` within about 5 seconds of restoring power, then stayed `online=1` for the full observation. The boot ID remained `46dabe1e-640c-46c5-b3da-b28c146fd323`; no SoC reboot occurred. Battery capacity was 97–98%, so the test validates adapter detection, not charge-current recovery under active charging load.

Full samples: `input-loss-test-full120-20261110.log`.

## Findings and limits

- Confirmed root cause of the unexpected SoC power-offs and contamination of the earlier short-duration tests: the old software watchdog invoked the full power-off recovery script after sustained `online=0`.
- The underlying intermittent hardware/charger fault was not reproduced in the controlled retest. There is no evidence from these tests that a 90-second interruption is required.
- The installed `.84` `bq257xx_charger.ko` hash is `a7845d7815278cce12fb8e097db71dc2a3d97d4b5db2d0a7c54dbd8e48ee5da3`; it differs from the `.132` build containing the bounded `RESET_VINDPM` recovery patch (`4fa9abfab3f62db6461c2483f2d9d96560299219b078146c9b63be38d71952e3`). Both report `7.2.12` vermagic. The patched module is therefore not installed on `.84`; it remains an unvalidated candidate, not an established fix.
- U-Boot contains charger-related initialization and AC-status handling, but it does not execute during a runtime adapter unplug/replug. A U-Boot-only root cause is not supported by this live-system test.

## Current state

`.84` remains on the same boot ID, with adapter `online=1`, battery at 97%, and `Not_charging` at near-full capacity. The Home Assistant outlet is on. The non-rebooting on-device recovery daemon is running. `.132` has only the corrected adapter-recovery cron entry; the old SoC power-off watchdog is archived and disabled.

The `.132` monitor was corrected to parse `Not charging` safely, avoid treating near-full `Not_charging` as a fault, require persistent `online=0`, verify outlet state, and restore power on failures. Its pre-change copy is `monitor.sh.pre-rootcause-20261110`.

## Next diagnostic trigger

If `online=0` recurs, capture the power-supply state, Type-C/PD state, kernel log, and charger status before the adapter recovery monitor acts. The current Linux interface does not expose a confirmed SC8886 VBUS ADC reading; if software state still cannot distinguish adapter-path voltage from SC8886 AC-detect/ACOK behavior, measure adapter output, SC8886 input, and ACOK electrically. Re-run charge-current acceptance only when battery level is low enough that charging should be active.
