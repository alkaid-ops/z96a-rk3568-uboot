# 核查记忆点（Verification Memory）

生成时间：2026-11-13（Asia/Shanghai）
适用机器：Z96A（RK3568）Devuan，`.84`
角色定位：**我负责核查/验证，不代替实现方改他的仓库。**

## 0. 职责与边界（重要）

- 镜像、启动链、功能项的**事实核查**由我做；发现问题后给出证据与原因，是否需要修、怎么修由对方决定。
- **不要动 `z96a-devuan-image`（实现方的仓库/工作区）**。我的产出写在本工作目录 `rea-z97a`。
- 需要改动时只动本目录的文档/分析；真机操作前先确认授权（尤其 eMMC/U-Boot/分区表/刷写）。

## 1. 真机访问

```bash
# 密码方式（root 的 publickey 不一定可用）
sshpass -p 1234 ssh -o StrictHostKeyChecking=no \
  -o PreferredAuthentications=password -o PubkeyAuthentication=no \
  user@192.168.31.84 'echo 1234 | sudo -S <cmd>'

# 分区
/dev/sda1  BOOT(vfat)   /boot      # U 盘
/dev/sda2  DEVUAN(ext4) /          # U 盘根
/dev/mmcblk0p1/p2                  # eMMC（故障时可从 U 盘挂载它来改文件）
```

从 eMMC 系统改 U 盘（离线修，不用重启）：

```bash
mount /dev/sda1 /mnt/ub ; mount /dev/sda2 /mnt/ur
mkdir -p /mnt/ur/boot && mount --bind /mnt/ub /mnt/ur/boot
chroot /mnt/ur /bin/sh -c 'export PATH=/usr/sbin:/usr/bin:/sbin:/bin; \
  update-initramfs -u -k 7.2.12 && mkimage -C none -A arm64 -O linux -T ramdisk \
  -n initramfs-7.2.12 -d /boot/initrd.img-7.2.12 /boot/uInitrd'
```

## 2. 权威时间测量（别再被日志骗）

**用 `/proc/<pid>/stat` 第 22 字段（start_time，单位 tick）**：

```bash
hz=$(getconf CLK_TCK)
for n in slim Xorg; do p=$(pgrep -x "$n"|head -1); \
  st=$(awk '{print $22}' /proc/$p/stat); \
  awk -v t="$st" -v h="$hz" -v n="$n" 'BEGIN{printf "%s start=%.2fs\n", n, t/h}'; done
```

启动锚点用 dmesg：

```bash
dmesg | grep -E "Freeing initrd|Run /init|mounted filesystem|Initialized rockchip|fb0"
```

**不要相信 `Xorg.0.log` 的 `[ 74.6]` 当 uptime**：那是 X 自身时间基准，系统时钟被 RTC/NTP 跳变时会误导。
（本轮就出现过：日志 `[74.6]` 被当成 74.6s，实机 `<pid>/stat` 也确实是 74.5s，但**原因**是 rcS 被 NFS 拖住，不是动画。）

## 3. 「镜像已验证」核查清单

### 3.1 直接看构建缓存里的真实 rootfs（最快、无需上机）

```bash
cd <image-repo>
ls -dt output/cache/chroot-* | head          # 最新一次构建
L=output/cache/chroot-...;                   # p2=rootfs, p1=boot
ls  $L/p2/etc/rcS.d/ | grep -iE 'rpc|nfs'    # 启动顺序里的坑
ls  $L/p2/etc/init.d/.depend.*               # 是否又被 insserv 重建
ls  $L/p2/var/lib/alsa/asound.state          # 音频状态是否随镜像
grep -n 'Sprite(Image' $L/p2/usr/share/plymouth/themes/alkaid/alkaid.script
head -6 $L/p2/etc/initramfs-tools/modules
```

注意：缓存里 `p1/initrd.img` 可能不是最终 initrd（以设备 `/boot/initrd.img-*` 为准）。

### 3.2 内核 deb 是否等于内核树的最新产物（**HEVC 就是这么丢的**）

```bash
D=output/z96a-v2/linux-image-*.deb
mkdir -p /tmp/kimg && dpkg-deb -x "$D" /tmp/kimg
md5sum /tmp/kimg/boot/Image /boot/Image            # 设备上取 /boot/Image
md5sum /tmp/kimg/lib/modules/*/kernel/drivers/media/platform/rockchip/rkvdec/rockchip-vdec.ko
md5sum sources/linux/drivers/media/platform/rockchip/rkvdec/rockchip-vdec.ko
strings sources/linux/.../rockchip-vdec.ko | grep -c vdpu346   # 期望 >0
strings /tmp/kimg/.../rockchip-vdec.ko     | grep -c vdpu346   # 若 0 => deb 里是旧模块
```

- 本轮实测：树里 `rockchip-vdec.ko`（03:39）含 `vdpu346`，deb 里（03:14）没有 → 设备
  `/dev/video2` 只枚举 `S264`(H.264)，`mpv --hwdec=v4l2request` 报
  `Could not create device` → 4K HEVC 只能软解。
- **判据**：deb 内模块 hash/字符串必须与 `sources/<kernel-tree>` 编译产物一致，光看 `Image` 一致不够。

### 3.3 initrd 内容

```bash
rm -rf /tmp/vv && mkdir /tmp/vv && cd /tmp/vv
xzcat /boot/initrd.img-7.2.12 | cpio -idm --quiet
cat conf/modules                                  # 期望前三条含 DRM
ls usr/lib/modules/*/kernel/drivers/gpu/drm/rockchip/rockchipdrm.ko
ls usr/share/plymouth/themes/alkaid/*.png | wc -l # 120
grep -c 'global.idx = 0' usr/share/plymouth/themes/alkaid/alkaid.script
grep -c sleep scripts/init-premount/plymouth      # 期望 0（不要加等待）
```

## 4. Z96A 启动链关键结论（本轮实证）

要让「LOGO 一直保持 → 完整动画 → 登录页」同时成立，需要**三个条件同时满足**：

1. **cmdline 带 `clk_ignore_unused pd_ignore_unused`**
   U-Boot 交棒的 VOP 扫描在 DRM probe 前没有消费者，`clk_disable_unused()` / genpd
   未使用域回收会关掉 VO 时钟/电源域 → 扫描停止、LOGO 变黑。
   （内核 `0018` 里 VO genpd 已设 `GENPD_FLAG_ALWAYS_ON`，但时钟仍需这两个参数兜住。）
2. **DRM 放进 initrd（`panel-edp → analogix_dp → rockchipdrm`）**
   - 放 initrd：DRM ~7.46s，splash **8.00s**，X ~30s ⇒ 动画窗口 ~24s，6.1s 正片放得完。
   - 留在 rootfs：DRM 10–17s 才 probe，splash 随之后移，slim ~20s 就退 splash
     ⇒ 窗口仅 ~4.5s，正片被截断（观感「没播完」）。
3. **主题从 `f0000` 起步**
   `f0080` 之后是「LOGO 构图」的循环段；从 80 起步会把正片的**大球体整段跳过**。
   循环逻辑：`if (global.idx >= 120) global.idx = 80;`（80–119 是接缝 1.8/255 的无缝段）。

## 5. 踩过/已验证的坑

| 坑 | 现象 | 结论 |
|---|---|---|
| premount 里 `sleep 0.1` 轮询 `/dev/dri/by-path/...` 或 `/dev/fb0` | splash 与 root mount 同时晚 ~4.5s（root 8.4→13.1s） | **原版 premount 不要加等待**；它撞进 initramfs udev 冷插拔窗口 |
| `LD_DEBUG` / `LD_BIND_NOW` 让「plymouthd 启动快」 | 误以为是加速 | 假相关，根因不在 plymouthd |
| 用 DRM ioctl 在启动早期抓 scanout | 板子直接挂（无网络） | **不要在 initramfs 里抢 DRM 做采集**；采集用 X 起来后的 kmsgrab |
| rcS 里 `S14rpcbind` / `S15nfs-common` | 24s→61s 卡 RPC/NFS，X 74.5s | `custom.txt` 里删 `.depend.*` 会被 apt/insserv **重建**；需 `update-rc.d -f` 关服务，并在 stage2 末尾再删 |
| `/var/lib/alsa/asound.state` 缺失 | 音频能响但不恢复音量/路由 | 仓库需有 `files/asound/z96a-v2.state`（`staging` 只在文件存在时拷贝） |
| `files/asound/` 只有 lepotato/tritium | z96a 无 state | 同上，属缺文件而非补丁 |

## 6. 常用只读检查命令

```bash
# 启动链
dmesg | grep -E "Run /init|mounted filesystem|Initialized rockchip|fb0|slim"
cat /proc/cmdline

# 显示/登录
echo "vt=$(cat /sys/class/tty/tty0/active) slim=$(pgrep -c slim) xorg=$(pgrep -c Xorg)"
ls /var/log/Xorg.0.log.old 2>/dev/null   # 有 .old 说明 X 重启过

# 音频
aplay -l; amixer -c 1 sget Master; amixer -c 1 sget 'Internal Speakers'
speaker-test -D plughw:1,0 -c 2 -t sine -f 440 -l 1

# 视频硬解能力
v4l2-ctl -d /dev/video1 --list-formats-out   # hantro: H.264/MPEG2/VP8
v4l2-ctl -d /dev/video2 --list-formats-out   # rockchip_vdec: 期望含 HEVC 的 slice 格式
mpv --no-config --no-audio --vo=null --hwdec=v4l2request <file> 2>&1 | grep -i "Could not create"

# 电源
sudo -u user dbus-send --system --print-reply --dest=org.freedesktop.login1 \
  /org/freedesktop/login1 org.freedesktop.login1.Manager.CanPowerOff
```

## 7. 复核「别人给的结论」时先问三件事

1. 数字从哪来？（`/proc/<pid>/stat` ≠ X 日志时间戳）
2. 是**产物**还是**源码**？（补丁在源码里 ≠ 装进镜像；必须比对 deb 内 hash/字符串）
3. 因果还是相关？（例如「X 晚」不是因为动画，而是 rcS 的 NFS）

---

相关记录：`docs/CHARGER-VERIFIED-MEMORY.md`、`docs/BATTERY-GAUGE-MEMORY.md`（同类记忆文件）。
