# Z96A Armbian build provenance

This repository contains the Armbian build recipe, not copies of the Linux or
U-Boot source trees. Generated worktrees under `cache/` and build products under
`output/` are intentionally ignored by Git.

## Supported image

- Board: `z96a-v2-mainline`
- Branch: `mainline`
- Release: `bookworm`
- Image type: `minimal`
- Boot method: extlinux
- Kernel: `7.2.11`

## Pinned sources

| Component | Repository | Revision |
|---|---|---|
| Linux | `https://github.com/yjun123/linux.git` | `b2513489889ed574e1c3dc0da36f27b02ea8af46` |
| U-Boot | `https://github.com/kemp233/u-boot-1.git` | `8e474aa605a86de64316ba6da8dc089402976380` |

## Board patches

The kernel recipe is under `patch/kernel/rockchip-rk3568-z96a/mainline/` and
includes the eDP PHY clock fix, Wi-Fi GPIO0_A0 power regulator, pwrseq reset-GPIO
fix, BOE NV140FHM-N43 panel support, and RK3568 VDPU346 decoder patches.

## Continuity

The previously validated USB-first image has SHA256
`fcba97bea125828109202f50d9260be7f6c5453b9569a7713451e3668a67f6c9`.
It remains available on the build machine as a known-good fallback.

## GitHub Actions

`.github/workflows/z96a-tag-build.yml` runs only for tag pushes. It uses direct
upstream sources without mirror variables and builds only the Minimal Z96A
configuration above. The workflow uploads the image, checksum file, build log
metadata, and kernel/U-Boot deb packages as workflow artifacts and release
assets.
