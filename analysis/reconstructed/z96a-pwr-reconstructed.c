/*
 * Human-readable reconstruction of the charger/power-source path in the
 * Z96A U-Boot 2026.01 image. This is not original source and is not buildable.
 *
 * Evidence:
 *   U-Boot image-0 SHA-256: 28bafb63aaaa82e9f940b151cb5ebed6f4551634d11ff3c680f346bc0fbffb0c
 *   z96a-pwr function:       0x00804284
 *   16-bit read helper:      0x008040d0
 *   masked 16-bit update:    0x00804108
 *   byte write helper:       0x00843ee0
 */

static void z96a_pwr_reconstructed(void)
{
    i2c_bus_t *bus;
    i2c_dev_t *sc8886;
    u16 status_20;       /* bytes 0x20..0x21 */
    u16 status_21;       /* bytes 0x21..0x22 */
    u16 adc_26;          /* bytes 0x26..0x27 */
    bool ac;
    bool battery_removed;
    u32 battery_mv;

    if (!machine_is_sunniwell_z96a_rk3568_laptop_v2())
        return;

    if (i2c_get_bus("i2c@fdd40000", &bus))
        return;

    if (i2c_get_device(bus, 0x6b, 1, &sc8886))
        return;

    if (i2c_read16(sc8886, 0x20, &status_20) ||
        i2c_read16(sc8886, 0x21, &status_21) ||
        i2c_read16(sc8886, 0x26, &adc_26))
        return;

    /* U-Boot explicitly restores the SC8886 PROCHOT defaults. */
    i2c_write8(sc8886, 0x36, 0x65);
    i2c_write8(sc8886, 0x37, 0x4a);

    ac = !!(status_20 & BIT(15));       /* AC_STAT */
    battery_removed = !!(status_21 & BIT(1));
    battery_mv = (adc_26 & GENMASK(6, 0)) * 64 + 2880;

    if (!ac) {
        if (battery_mv < 3000 || battery_removed) {
            i2c_update16(sc8886, 0x12, 0x8000, 0);
            setenv("z96a_power_src", "none");
            return;
        }

        i2c_update16(sc8886, 0x32, 0x1000, 0); /* clear EN_OTG */
        i2c_update16(sc8886, 0x12, 0x8000, 0);
        setenv("z96a_power_src", "battery");
        return;
    }

    i2c_update16(sc8886, 0x32, 0x1000, 0);          /* clear EN_OTG */
    i2c_update16(sc8886, 0x12, 0x8000, 0);
    i2c_update16(sc8886, 0x14, 0x7f00, 0x0400);    /* field=4, 1024 mA */
    setenv("z96a_power_src", "usb-c");
}

/*
 * Separate reconstruction from the factory-style 2017.09 U-Boot charger path.
 * Function 0x00a7e3bc is reached after charger-DTS parsing at 0x00a7dcc0.
 * Unlike the later z96a-pwr path, this code programs the BQ25703 register block
 * directly but also does not reset the QC protocol chip or VINDPM.
 */
static void vendor_charger_init_reconstructed(i2c_dev_t *charger, bool usb_high_current)
{
    u16 option0;

    i2c_read16(charger, 0x00, &option0);
    i2c_write16(charger, 0x00, option0 & 0x1fff);   /* clear watchdog field */
    i2c_write16(charger, 0x0c, 0x1200);              /* minimum system voltage */
    i2c_write16(charger, 0x0e, usb_high_current ? 0x1e00 : 0x0a00);

    if (charger_ac_present(charger))
        i2c_write16(charger, 0x02, 0x0400);          /* 16 * 64 mA = 1024 mA */
}
