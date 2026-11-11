# Z96A mainline patch directory

Base kernel: `yjun123/linux` commit
`b2513489889ed574e1c3dc0da36f27b02ea8af46`.

The base already contains the Z96A DTS and eDP, ES7202, SC8886 and
HUSB311 support. The patches in this directory add:

- `0001`: keep the eDP PHY GRF bus clock enabled.
- `0002`: model GPIO0_A0 as the RTL8822CS power enable and use stable
  50 MHz non-UHS SDIO.
- `0003`: fix `mmc-pwrseq-simple` when `CONFIG_RESET_GPIO` is enabled.
- `0004`: add the BOE NV140FHD-N43 eDP panel mode.
- `0005`-`0007`: backport the FROMLIST v3 VDPU346 (`rkvdec2`) support for
  RK3566/RK3568, including binding, driver variant and RK356X DT nodes.

The VDPU346 module was boot-tested on Z96A with both H.264 and H.265:
300-frame 720p test clips produced 300 rkvdec interrupts each, with
NV12 output and no IOMMU faults.

Video node numbering can change between boots because Hantro and rkvdec
are independent drivers. The board userspace installs stable aliases:

```text
/dev/video-rkvdec
/dev/media-rkvdec
```

Example H.265 hardware decode test:

```sh
gst-launch-1.0 filesrc location=test.mp4 ! qtdemux ! h265parse \
  ! v4l2slh265dec video-device=/dev/video-rkvdec \
    media-device=/dev/media-rkvdec ! fakesink sync=false
```
