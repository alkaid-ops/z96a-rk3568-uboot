# Z96A charger runtime recovery

## Verified failure mode

The adapter AC input can disappear and return while the SoC remains on battery.
After a short AC interruption the SC8886 can remain in:

```text
bq257xx-charger online=0
status=Discharging
SC8886 reg 0x20 = 0x0000 (AC_STAT=0)
```

HUSB311/tcpm resets, CC OPEN, TCPCI commands, SC8886 HIZ/RESET_REG/VINDPM
writes, and `bq257xx` MFD rebind do not recover while SC8886 still reports
AC_STAT=0.

## Verified recovery sequence

Two clean tests with a 90 second adapter interruption both recovered without
rebooting or powering off the SoC:

1. Turn the adapter AC supply off for 90 seconds.
2. Turn it on.
3. Wait for SC8886 to report `online=1`.
4. If `status=Not charging`, rebind the `bq257xx` MFD once; if needed the
   on-device daemon rebinds again after 10 seconds.
5. Verify stable `online=1 / Charging / 960000` for at least 8 seconds.

A clean automated test recovered in 39 seconds after AC return (21 seconds
with intermediate status samples, then 8 seconds stability confirmation).

## Components

### Device: `.84`

- `/usr/local/sbin/z96a-charger-runtime-recovery`
- `/etc/init.d/z96a-charger-runtime-recovery`
- Watches `bq257xx-charger` and rebinds `bq257xx` when AC is present but
  charging has not resumed.
- Does not power off or reboot the SoC.

### Control host: `.132`

- `/home/hsy/project/z96a-charger-input-recovery/monitor.sh`
- `/home/hsy/project/z96a-charger-input-recovery/ha_switch.py`
- Cron every 5 minutes.
- On persistent charger loss, performs up to 3 cycles of 90 seconds AC off/on.
- Never invokes SoC `poweroff` or `reboot`.

## Kernel driver change

`0015-z96a-runtime-charger-recovery.patch` adds:

- HUSB311 vendor CC workaround from the downstream driver.
- A manual `/sys/bus/i2c/devices/0-004e/recover` trigger that performs
  HUSB311 software reset plus `tcpm_tcpc_reset()`.

The production recovery path used for charging is the `bq257xx` rebind above;
the HUSB recovery node is retained for Type-C-only diagnostics.

## Backups and rollback

On `.84`:

```text
/root/z96a-runtime-recovery-v3-backup-20261110-040807/
```

Rollback the on-device daemon:

```sh
sudo /etc/init.d/z96a-charger-runtime-recovery stop
sudo update-rc.d -f z96a-charger-runtime-recovery remove
sudo rm -f /usr/local/sbin/z96a-charger-runtime-recovery
sudo rm -f /etc/init.d/z96a-charger-runtime-recovery
```

On `.132`, rollback the input monitor:

```sh
crontab -l | grep -v '/home/hsy/project/z96a-charger-input-recovery/monitor.sh' | crontab -
mv /home/hsy/project/z96a-charger-input-recovery \
   /home/hsy/project/archive/z96a-charger-input-recovery-disabled-$(date +%Y%m%d-%H%M%S)
```
