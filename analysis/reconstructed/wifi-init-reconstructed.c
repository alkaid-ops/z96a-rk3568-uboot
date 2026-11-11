/*
 * Evidence-oriented reconstruction, not vendor source and not buildable code.
 * Input: vendor U-Boot image 0, SHA-256 7b1438d3b4253604cb36e04a54bbd5a53024ecb3358f07a0d03224ca9491cfb7.
 * Functions and GPIO numbering follow Ghidra analysis and public Rockchip
 * U-Boot APIs; physical GPIO bank mapping and call success remain unverified.
 */

/* 0x00a02d10, called from 0x00a03064. */
static void reconstructed_power_init(void)
{
    gpio_request(0x15, "power_usb_hub");
    gpio_direction_output(0x15, 1);
    gpio_set_value(0x15, 1);

    gpio_request(0x50, "power_usb_cam");
    gpio_direction_output(0x50, 1);
    gpio_set_value(0x50, 1);

    gpio_request(0, "power_wifi_module");
    gpio_direction_output(0, 1);
    gpio_set_value(0, 1);
}

/*
 * 0x00a19fd8 -> 0x00a9cc90 iterates function pointers at 0x00b044c0.
 * Earlier in that table, 0x00a04228 calls 0x00a04e30: it attempts to load
 * a kernel DTB, replace the working FDT and rescan devices. Failure is
 * ignored by 0x00a04228. The active DTB therefore depends on runtime inputs.
 * The table includes 0x00a03e14 at 0x00b045d8, which calls 0x00a03064,
 * which calls reconstructed_power_init(). Previous errors stop the loop.
 */
