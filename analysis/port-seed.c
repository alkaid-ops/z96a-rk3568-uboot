/*
 * Non-buildable port seed for Z96A factory U-Boot hardware initialization.
 * Mapping and API ownership must be verified against U-Boot 2026.01.
 *
 * Mainline insertion point:
 *   board/rockchip/z96a_rk3568/board.c::rk_board_late_init()
 *
 * Factory evidence:
 *   0x00a03e14 -> 0x00a03064 -> 0x00a02d10
 *   0x00a7dcc0 -> 0x00a7e3bc
 */
#include <common.h>
#include <dm.h>
#include <asm/gpio.h>

/* Global GPIO numbers from the 2017.09 binary; bank mapping needs confirmation. */
#define Z96A_GPIO_POWER_USB_HUB       0x15
#define Z96A_GPIO_POWER_USB_CAM       0x50
#define Z96A_GPIO_POWER_WIFI_MODULE   0x00

static int z96a_factory_power_gpio_init(void)
{
	int ret;

	ret = gpio_request(Z96A_GPIO_POWER_USB_HUB, "power_usb_hub");
	if (!ret) {
		gpio_direction_output(Z96A_GPIO_POWER_USB_HUB, 1);
		gpio_set_value(Z96A_GPIO_POWER_USB_HUB, 1);
	}

	ret = gpio_request(Z96A_GPIO_POWER_USB_CAM, "power_usb_cam");
	if (!ret) {
		gpio_direction_output(Z96A_GPIO_POWER_USB_CAM, 1);
		gpio_set_value(Z96A_GPIO_POWER_USB_CAM, 1);
	}

	ret = gpio_request(Z96A_GPIO_POWER_WIFI_MODULE, "power_wifi_module");
	if (!ret) {
		gpio_direction_output(Z96A_GPIO_POWER_WIFI_MODULE, 1);
		gpio_set_value(Z96A_GPIO_POWER_WIFI_MODULE, 1);
	}

	return 0;
}
