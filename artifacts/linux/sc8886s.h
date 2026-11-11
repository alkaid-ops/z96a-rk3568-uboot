/**
 * @file sc8886s.h
 * @author AAA编程曹师傅 (github.com/drinktoomuchsax)
 * @brief SC8886S电池充电控制器头文件
 * @version 0.1
 * @date 2025-07-14
 *
 * @copyright Copyright (c) 2025 Tangible Future
 *
 *
 * SC8886S是一个高度集成的同步buck-boost充电控制器，支持:
 * - 1-4节电池充电管理
 * - 宽输入范围:3.5V-24V
 * - NVDC电源路径管理
 * - OTG反向放电功能
 * - 完整的保护机制
 * - I2C接口控制
 *
 */

#ifndef SC8886S_H
#define SC8886S_H

#include <stdbool.h>
#include <stdint.h>

// FreeRTOS includes for mutex support
#if ESP_IDF_VERSION_MAJOR >= 5
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#else
#include "FreeRTOS.h"
#include "semphr.h"
#endif

#ifdef __cplusplus
extern "C"
{
#endif

    /*===============================================    常量定义  ===============================================*/

#define SC8886S_I2C_ADDR 0x6B << 1   // I2C设备地址 (7位)，最后一位读写
#define SC8886S_DEVICE_ID 0x66       // 设备ID
#define SC8886S_MANUFACTURER_ID 0x03 // 制造商ID

#define SC8886S_BATTERY_CELL_NUM 0x02 // 你用几节电池

/* 电压电流范围定义 */
#define SC8886S_VBUS_MIN 3500            // 最小输入电压 (mV)
#define SC8886S_VBUS_MAX 24000           // 最大输入电压 (mV)
#define SC8886S_CHARGE_VOLTAGE_MIN 1024  // 最小充电电压 (mV)
#define SC8886S_CHARGE_VOLTAGE_MAX 19200 // 最大充电电压 (mV)
#define SC8886S_CHARGE_CURRENT_MAX 8128  // 最大充电电流 (mA)
#define SC8886S_INPUT_CURRENT_MAX 6400   // 最大输入电流 (mA)
#define SC8886S_OTG_VOLTAGE_MIN 3000     // OTG最小输出电压 (mV)
#define SC8886S_OTG_VOLTAGE_MAX 24000    // OTG最大输出电压 (mV)
#define SC8886S_OTG_CURRENT_MAX 6400     // OTG最大输出电流 (mA)

    /*=============   寄存器地址定义（小端序，高位在前，低位在后，比如0b00000010，高位在前，低位在后，就是0x02）
     * =====================*/

#define SC8886S_REG_CHARGE_OPTION0_L                                                                                   \
    0x00 // 充电控制配置寄存器0，RW，默认0x0E，控制短路打嗝保护/学习模式/采样电流增益/BATFET LDO/充电使能
#define SC8886S_REG_CHARGE_OPTION0_H                                                                                   \
    0x01 // 充电控制配置寄存器1，RW，默认0xE7，控制低功耗/看门狗/输入电流限制(IDPM)/OTG时充电正常指示/开关噪声/开关频率/直通低纹波

#define SC8886S_REG_CHARGE_CURRENT_L                                                                                   \
    0x02 // 设置充电电流低字节，RW，默认0x00，64mA~8128mA，LSB:64mA，不同串数电池对应充电电流有区别，详见数据手册
#define SC8886S_REG_CHARGE_CURRENT_H                                                                                   \
    0x03 // 设置充电电流高字节，RW，默认0x00，64mA~8128mA，LSB:64mA，不同串数电池对应充电电流有区别，详见数据手册

#define SC8886S_REG_CHARGE_VOLTAGE_L                                                                                   \
    0x04 // 设置充电电压低字节，RW，默认0x00，1024mV~19.2V，LSB:8mV，电池上电由CELL_PRES过程默认配置，设0使得充电电流为0，然后恢复默认配置
#define SC8886S_REG_CHARGE_VOLTAGE_H                                                                                   \
    0x05 // 设置充电电压高字节，RW，默认0x00，1024mV~19.2V，LSB:8mV，电池上电由CELL_PRES过程默认配置，设0使得充电电流为0，然后恢复默认配置

#define SC8886S_REG_OTG_VOLTAGE_L                                                                                      \
    0x06 // OTG输出电压设置低字节，RW，默认0x00，3V~24V，LSB:8mV，与Reg0x34配合使用，详见数据手册
#define SC8886S_REG_OTG_VOLTAGE_H                                                                                      \
    0x07 // OTG输出电压设置高字节，RW，默认0x00，3V~24V，LSB:8mV，与Reg0x34配合使用，详见数据手册

#define SC8886S_REG_OTG_CURRENT_RESERVED 0x08 // 保留寄存器，RW，内部使用不许覆写
#define SC8886S_REG_OTG_CURRENT 0x09          // OTG输出电流设置低字节，RW，默认0x00，50mA~3.2A，LSB:50mA

#define SC8886S_REG_VINDPM_L                                                                                           \
    0x0A // 输入电压限制设置低字节，RW，默认0x00,3.2V~19.52V，LSB:64mV，有3.2V DC OFFSET(0x00 = 3.2v, 0x01 = 3.264v)
#define SC8886S_REG_VINDPM_H                                                                                           \
    0x0B // 输入电压限制设置高字节，RW，默认0x00,3.2V~19.52V，LSB:64mV，有3.2V DC OFFSET(0x00 = 3.2v, 0x01 = 3.264v)

#define SC8886S_REG_VSYSMIN_RESERVED 0x0C // 保留寄存器，RW，内部使用不许覆写
#define SC8886S_REG_VSYSMIN                                                                                            \
    0x0D // 系统最小电压(V_SYSMIN)设置，RW，默认0x00,1024mV~16128mV，LSB:256mV，有160mV DC OFFSET(0x00 = 160mV, 0x01 =
         // 416mV)

#define SC8886S_REG_IINDPM_RESERVED 0x0E // 保留寄存器，RW，内部使用不许覆写
#define SC8886S_REG_IINDPM                                                                                             \
    0x0F // 输入电流限制设置，RW，默认0x41，默认值0xA6（3.3A），有50mA DC OFFSET(0x00 = 50mA, 0x01 = 310mA)

#define SC8886S_REG_CHARGE_STATUS_0                                                                                    \
    0x20 // 充电状态只读寄存器低字节，R/W，默认0x00，各种fault
         // flag(ACOV/BATOC/ACOC/SYSOVP/SYSSHORT/LATCHOFF/OTG_OVP/OTG_UVP)
#define SC8886S_REG_CHARGE_STATUS_1                                                                                    \
    0x21 // 充电状态只读寄存器高字节，R/W，默认0x00，各种功能的状态 flag(V_BUS/输入电流检测/VMIN active
         // protection/VINDPM状态/IINDPM状态/快充状态/预充电状态/OTG状态)

#define SC8886S_REG_PROCHOT_STATUS_L                                                                                   \
    0x22 // PROCHOT状态只读寄存器低字节，RO，默认0x00，各种PROCHOT
         // flag(VDPM/COMP/ICRIT/INORM/IDCHG/VSYS/BAT_REVMOVE/ADA_REMOVE)VSYS/BAT_REVMOVE/ADA_REMOVE)
#define SC8886S_REG_PROCHOT_STATUS_H 0x23 // PROCHOT状态只读寄存器高字节，RO，默认0xA8，配置PROCHOT,检测VAP模式

#define SC8886S_REG_IIN_DPM_RESERVED 0x24 // 保留寄存器，RW，内部使用不许覆写
#define SC8886S_REG_IIN_DPM 0x25          // 实际使用的输入电流限制，RO，默认0x41，50mA~6.4A，LSB:50mA

#define SC8886S_REG_ADC_PSYS 0x26 // ADC读取VSYS电平，RO，默认0x00，8bit ADC，0V~3.06V，LSB:12mV
#define SC8886S_REG_ADC_VBUS 0x27 // ADC读取VBUS电平，RO，默认0x00，8bit ADC，3.2V~19.52V，LSB:64mV

#define SC8886S_REG_ADC_IDCHG 0x28 // ADC读取内部电池放电电流，RO，默认0x00，8bit ADC，0A~32.521A，LSB:256mA
#define SC8886S_REG_ADC_ICHG 0x29  // ADC读取内部充电电流，RO，默认0x00，8bit ADC，0A~8.128A，LSB:64mA

#define SC8886S_REG_ADC_CMPIN 0x2A // ADC读取CMPIN电平，RO，默认0x00，8bit ADC，0mV~3.06V，LSB:12mV
#define SC8886S_REG_ADC_IIN 0x2B   // ADC读取IBUS，RO，默认0x00，8bit ADC，0mA~6.35A，LSB:50mV

#define SC8886S_REG_ADC_VBAT 0x2C // ADC读取VBAT电平，RO，默认0x00，8bit ADC，2.88V~19.2V，LSB:64mV
#define SC8886S_REG_ADC_VSYS 0x2D // ADC读取VSYS电平，RO，默认0x00，8bit ADC，2.88V~19.2V，LSB:64mV

#define SC8886S_REG_MANUFACTURER_ID 0x2E // 制造商ID只读寄存器，RO，固定值0x03，不可写！
#define SC8886S_REG_DEVICE_ID 0x2F       // 芯片设备ID只读寄存器，RO，固定值0x66，不可写！

#define SC8886S_REG_CHARGE_OPT_1_L                                                                                     \
    0x30 // 充电控制配置寄存器1，RW，默认0x11(比较器电平/比较器极性/比较器时间/比较器锁存/使能直通模式(PTM)/使能船运模式(Ship)/使能自动唤醒(缺电自动开始充))
#define SC8886S_REG_CHARGE_OPT_1_H                                                                                     \
    0x31 // 充电控制配置寄存器1，RW，默认0x02(使能IBAT监测/低功耗使能PROCHOT/使能PSYS监测/配置VBUS采样电阻/配置VBAT采样电阻/配置PSYS电阻功耗电流比/配置ILIM_HIZ引脚)

#define SC8886S_REG_CHARGE_OPT_2_L                                                                                     \
    0x32 // 充电控制配置寄存器2，RW，默认0xB7(配置输入电流限制/配置IBAT方向(充电/放电)/Q2_OCP门限/使能适配器过电流保护(EN_ACOC)/配置ACOC门限/使能电池过流保护(EN_BATOC)/配置BATOC门限)
#define SC8886S_REG_CHARGE_OPT_2_H                                                                                     \
    0x33 // 充电控制配置寄存器2，RW，默认0x02(暂时过流时间/峰值功率模式电流触发/峰值功率模式电压触发/是否在overload
         // cycle/是否在relax cycle/total cycle time)

#define SC8886S_REG_CHARGE_OPT_3_L 0x34 // 充电控制配置寄存器3，RW，默认0x30，写不动了，去看数据手册吧
#define SC8886S_REG_CHARGE_OPT_3_H                                                                                     \
    0x35 // 充电控制配置寄存器3，RW，默认0x00，(使能HIZ模式/重置所有寄存器/标定适配器低电压(VINDPM)/使能OTG/使能适配器最大供电优化(ICO)/)

#define SC8886S_REG_PROCHOT_OPT_0_L 0x36 // PROCHOT事件配置低字节，RW，默认0x65
#define SC8886S_REG_PROCHOT_OPT_0_H 0x37 // PROCHOT事件配置高字节，RW，默认0x4A

#define SC8886S_REG_PROCHOT_OPT1_L                                                                                     \
    0x38 // PROCHOT门限设置低字节，RW，默认0xA0，各种PROCHOT的触发事件使能，默认开启低输入电压(VDPM)和输出过流(ICRIT)
#define SC8886S_REG_PROCHOT_OPT1_H 0x39 // PROCHOT门限设置高字节，RW，默认0x81

#define SC8886S_REG_ADC_OPT_L 0x3A // ADC控制设置低字节，RW，默认0x00，CMPIN/VBUS/PSYS/IIN/IDCHG/ICHG/VSYS/VBAT的ADC使能
#define SC8886S_REG_ADC_OPT_H 0x3B // ADC控制设置高字节，RW，默认0x20，ADC的配置，自己看吧，默认每1s采样一次

    /* =====================SC8886S 寄存器位域具体定义===================*/
    typedef enum
    {
        /*reg00h*/
        SYS_SHORT_DISABLE,
        EN_LEARN,
        IADP_GAIN,
        IBAT_GAIN,
        EN_LDO,
        EN_IDPM,
        CHRG_INHIBIT,
        /*reg01h*/
        EN_LWPWR,
        WDTWR_ADJ,
        IDPM_AUTO_DISABLE,
        OTG_ON_CHRGOK,
        EN_OOA,
        PWM_FREQ,
        LOW_PTM_RIPPLE,
        /*reg02h*/
        IINREG,
        /*reg12h*/
        CHARGE_CURRENT,
        /*reg14h*/
        MAX_CHARGE_VOLTAGE,
        /*reg15h*/
        AC_STAT,
        ICO_Done,
        IN_VAP,
        IN_VINDPM,
        IN_IINDPM,
        IN_FCHRG,
        IN_PreCHRG,
        IN_OTG,
        FAULT_ACOV,
        FAULT_BATOC,
        FAULT_ACOC,
        SYSOVP_STAT,
        SYS_SHORT_STAT,
        FAULT_LATCHOFF,
        FAULT_OTG_OVP,
        FAULT_OTG_UVP,
        /*reg22h*/
        STAT_VDPM,
        STAT_COMP,
        STAT_ICRIT,
        STAT_INORM,
        STAT_IDCHG,
        STAT_VSYS,
        STAT_battery_removal,
        STAT_Adapter_removal,
        /*reg23h*/
        STAT_EXIT_VAP,
        STAT_VAP_FAIL,
        PROCHOT_CLEAR,
        PROCHOT_WIDTH,
        EN_PROCHOT_EXT,
        /*reg24h*/
        IIN_DPM,
        /*reg22h*/
        ADC_PSYS, // OUTPUT_SYS_POWER
        ADC_VBUS, // OUTPUT_INPUT_VOL
        /*reg23h*/
        ADC_IDCHG, // OUTPUT_DSG_CUR
        ADC_ICHG,  // OUTPUT_CHG_CUR
        /*reg24h*/
        ADC_IIN,   // OUTPUT_INPUT_CUR,
        ADC_CMPIN, // OUTPUT_CMPIN_VOL,
        /*reg25h*/
        ADC_VSYS, // OUTPUT_SYS_VOL
        ADC_VBAT, // OUTPUT_BAT_VOL
        /*reg26h*/
        EN_IBAT,
        EN_PROCHOT_LPWR,
        EN_PSYS,
        RSNS_RAC,
        RSNS_RSR,
        PSYS_RATIO,
        PTM_PINSEL,
        CMP_REF,
        CMP_POL,
        CMP_DEG,
        FORCE_LATCHOFF,
        EN_PTM,
        EN_SHIP_DCHG,
        AUTO_WAKEUP_EN,
        /* reg2Eh */
        MANUFACTURE_ID, // 厂商ID
        /*reg2Fh*/
        DEVICE_ID, // 设备ID
        /*reg30h*/
        PKPWR_TOVLD_REG,
        EN_PKPWR_IDPM,
        EN_PKPWR_VSYS,
        PKPWER_OVLD_STAT,
        PKPWR_RELAX_STAT,
        PKPWER_TMAX,
        EN_EXTILIM,
        EN_ICHG_IDCHG,
        Q2_OCP,
        // ACX_OCP, // 8886中没启用
        EN_ACOC,
        ACOC_VTH,
        EN_BATOC,
        BATCOC_VTH,
        /*reg34h*/
        EN_CON_VAP,
        OTG_VAP_MODE,
        IL_AVG,
        OTG_RANGE_LOW,
        BATFETOFF_HIZ,
        PSYS_OTG_IDCHG,
        /*reg35h*/
        EN_HIZ,
        RESET_REG,
        RESET_VINDPM,
        EN_OTG,
        EN_ICO_MODE, // 使能ICO功能，Input Current Optimizer，用来识别最大输入电流
        /*reg36h*/
        VSYS_TH1,
        VSYS_TH2,
        INOM_DEG,
        LOWER_PROCHOT_VDPM,
        /*reg37h*/
        ILIM2_VTH,
        ICRIT_DEG,
        PROCHOT_VDPM_80_90,
        // VSYS_VTH,
        // EN_PROCHOT_EXT,
        // PROCHOT_WIDTH,
        // PROCHOT_CLEAR,
        /*reg38h*/
        PROCHOT_VDPM,
        PROCHOT_COMP,
        PROCHOT_ICRIT,
        PROCHOT_INOM,
        PROCHOT_IDCHG,
        PROCHOT_VSYS,
        PROCHOT_BATPRES,
        PROCHOT_ACOK,
        /*REG39*/
        IDCHG_VTH,
        IDCHG_DEG,
        /*reg3Ah*/
        EN_ADC_CMPIN, // 使能ADC比较输入
        EN_ADC_VBUS,  // 使能ADC VBUS
        EN_ADC_PSYS,  // 使能ADC PSYS
        EN_ADC_IIN,   // 使能ADC输入电流
        EN_ADC_IDCHG, // 使能ADC放电电流
        EN_ADC_ICHG,  // 使能ADC充电电流
        EN_ADC_VSYS,  // 使能ADC系统电压
        EN_ADC_VBAT,  // 使能ADC电池电压
        /*reg3Bh*/
        ADC_CONV,
        ADC_START,
        ADC_FULLSCALE,
        /* reg35h */
        OTG_VOLTAGE, // OTG电压
        /* reg3bh */
        OTG_CURRENT, // OTG电流
        /* reg3ch */
        VINDPM, // // 输入电压限制设置，RW，默认0x00,3.2V~19.52V，LSB:64mV，有3.2V DC OFFSET(0x00 = 3.2v, 0x01 = 3.264v)
        /* reg3dh */
        MIN_SYS_VOTAGE, // 最小系统电压
        /* reg3eh */
        INPUT_CURRENT, // 输入电流

        F_MAX_FIELDS
    } sc8886_fields;

    // 描述寄存器中某个位域（一个或多个比特位）的位置和大小
    typedef struct
    {
        uint8_t reg_addr; // 起始寄存器地址
        uint8_t lsb;      // 整体bit域中最低位，从这个位开始
        uint8_t length;   // 占几个bit，一个寄存器总数一定为8bit，但是会有横跨多个寄存器的情况，所以length有可能大于8
    } field_info_t;

    static const field_info_t sc8886s_reg[] = {
        /*REG00*/
        [EN_LEARN]          = {SC8886S_REG_CHARGE_OPTION0_L, 5, 1},
        [SYS_SHORT_DISABLE] = {SC8886S_REG_CHARGE_OPTION0_L, 6, 1},
        [IADP_GAIN]         = {SC8886S_REG_CHARGE_OPTION0_L, 4, 1},
        [IBAT_GAIN]         = {SC8886S_REG_CHARGE_OPTION0_L, 3, 1},
        [EN_LDO]            = {SC8886S_REG_CHARGE_OPTION0_L, 2, 1},
        [EN_IDPM]           = {SC8886S_REG_CHARGE_OPTION0_L, 1, 1},
        [CHRG_INHIBIT]      = {SC8886S_REG_CHARGE_OPTION0_L, 0, 1},
        /*REG01*/
        [EN_LWPWR]          = {SC8886S_REG_CHARGE_OPTION0_H, 7, 1},
        [WDTWR_ADJ]         = {SC8886S_REG_CHARGE_OPTION0_H, 5, 2},
        [IDPM_AUTO_DISABLE] = {SC8886S_REG_CHARGE_OPTION0_H, 3, 1},
        [OTG_ON_CHRGOK]     = {SC8886S_REG_CHARGE_OPTION0_H, 3, 1},
        [EN_OOA]            = {SC8886S_REG_CHARGE_OPTION0_H, 2, 1},
        [PWM_FREQ]          = {SC8886S_REG_CHARGE_OPTION0_H, 1, 1},
        [LOW_PTM_RIPPLE]    = {SC8886S_REG_CHARGE_OPTION0_H, 0, 1},
        /*REG0x02*/
        [CHARGE_CURRENT] = {SC8886S_REG_CHARGE_CURRENT_L, 6, 7},
        /*REG0x04*/
        [MAX_CHARGE_VOLTAGE] = {SC8886S_REG_CHARGE_VOLTAGE_L, 3, 12},
        /*REG06*/
        [OTG_VOLTAGE] = {SC8886S_REG_OTG_VOLTAGE_L, 2, 13},
        /*REG09*/
        [OTG_CURRENT] = {SC8886S_REG_OTG_CURRENT, 0, 7},
        /*REG0B*/
        [VINDPM] = {SC8886S_REG_VINDPM_L, 6, 8},
        /*REG0DD*/
        [MIN_SYS_VOTAGE] = {SC8886S_REG_VSYSMIN, 0, 6},
        /*REG0F*/
        [IINREG] = {SC8886S_REG_IINDPM, 0, 7},
        /*REG20*/
        [FAULT_ACOV]     = {SC8886S_REG_CHARGE_STATUS_0, 7, 1},
        [FAULT_BATOC]    = {SC8886S_REG_CHARGE_STATUS_0, 6, 1},
        [FAULT_ACOC]     = {SC8886S_REG_CHARGE_STATUS_0, 5, 1},
        [SYSOVP_STAT]    = {SC8886S_REG_CHARGE_STATUS_0, 4, 1},
        [SYS_SHORT_STAT] = {SC8886S_REG_CHARGE_STATUS_0, 3, 1},
        [FAULT_LATCHOFF] = {SC8886S_REG_CHARGE_STATUS_0, 2, 1},
        [FAULT_OTG_OVP]  = {SC8886S_REG_CHARGE_STATUS_0, 1, 1},
        [FAULT_OTG_UVP]  = {SC8886S_REG_CHARGE_STATUS_0, 0, 1},
        /*REG21*/
        [AC_STAT]    = {SC8886S_REG_CHARGE_STATUS_1, 7, 1},
        [ICO_Done]   = {SC8886S_REG_CHARGE_STATUS_1, 6, 1},
        [IN_VAP]     = {SC8886S_REG_CHARGE_STATUS_1, 5, 1},
        [IN_VINDPM]  = {SC8886S_REG_CHARGE_STATUS_1, 4, 1},
        [IN_IINDPM]  = {SC8886S_REG_CHARGE_STATUS_1, 3, 1},
        [IN_FCHRG]   = {SC8886S_REG_CHARGE_STATUS_1, 2, 1},
        [IN_PreCHRG] = {SC8886S_REG_CHARGE_STATUS_1, 1, 1},
        [IN_OTG]     = {SC8886S_REG_CHARGE_STATUS_1, 0, 1},
        /*REG22*/
        [STAT_VDPM]            = {SC8886S_REG_PROCHOT_STATUS_L, 7, 1},
        [STAT_COMP]            = {SC8886S_REG_PROCHOT_STATUS_L, 6, 1},
        [STAT_ICRIT]           = {SC8886S_REG_PROCHOT_STATUS_L, 5, 1},
        [STAT_INORM]           = {SC8886S_REG_PROCHOT_STATUS_L, 4, 1},
        [STAT_IDCHG]           = {SC8886S_REG_PROCHOT_STATUS_L, 3, 1},
        [STAT_VSYS]            = {SC8886S_REG_PROCHOT_STATUS_L, 2, 1},
        [STAT_battery_removal] = {SC8886S_REG_PROCHOT_STATUS_L, 1, 1},
        [STAT_Adapter_removal] = {SC8886S_REG_PROCHOT_STATUS_L, 0, 1},
        /*REG23*/
        [EN_PROCHOT_EXT] = {SC8886S_REG_PROCHOT_STATUS_H, 6, 1},
        [PROCHOT_WIDTH]  = {SC8886S_REG_PROCHOT_STATUS_H, 4, 2},
        [PROCHOT_CLEAR]  = {SC8886S_REG_PROCHOT_STATUS_H, 3, 1},
        [STAT_VAP_FAIL]  = {SC8886S_REG_PROCHOT_STATUS_H, 1, 1},
        [STAT_EXIT_VAP]  = {SC8886S_REG_PROCHOT_STATUS_H, 0, 1},
        /*REG24*/
        [IIN_DPM] = {SC8886S_REG_IIN_DPM, 0, 7},
        /*REG26H*/
        [ADC_PSYS] = {SC8886S_REG_ADC_PSYS, 0, 8},
        [ADC_VBUS] = {SC8886S_REG_ADC_VBUS, 0, 8},
        /*REG28H*/
        [ADC_IDCHG] = {SC8886S_REG_ADC_IDCHG, 0, 7},
        [ADC_ICHG]  = {SC8886S_REG_ADC_ICHG, 0, 7},
        /*REG2AH*/
        [ADC_CMPIN] = {SC8886S_REG_ADC_CMPIN, 0, 8},
        [ADC_IIN]   = {SC8886S_REG_ADC_IIN, 0, 8},
        /*REG2cH*/
        [ADC_VBAT] = {SC8886S_REG_ADC_VBAT, 0, 8},
        [ADC_VSYS] = {SC8886S_REG_ADC_VSYS, 0, 8},
        /*REG2E*/
        [MANUFACTURE_ID] = {SC8886S_REG_MANUFACTURER_ID, 0, 8},
        /*REF2F*/
        [DEVICE_ID] = {SC8886S_REG_DEVICE_ID, 0, 8},
        /*REG30*/
        [CMP_REF]        = {SC8886S_REG_CHARGE_OPT_1_L, 7, 1},
        [CMP_POL]        = {SC8886S_REG_CHARGE_OPT_1_L, 6, 1},
        [CMP_DEG]        = {SC8886S_REG_CHARGE_OPT_1_L, 4, 2},
        [FORCE_LATCHOFF] = {SC8886S_REG_CHARGE_OPT_1_L, 3, 1},
        [EN_PTM]         = {SC8886S_REG_CHARGE_OPT_1_L, 2, 1},
        [EN_SHIP_DCHG]   = {SC8886S_REG_CHARGE_OPT_1_L, 1, 1},
        [AUTO_WAKEUP_EN] = {SC8886S_REG_CHARGE_OPT_1_L, 0, 1},
        /*REG31H*/
        [EN_IBAT]         = {SC8886S_REG_CHARGE_OPT_1_H, 7, 1},
        [EN_PROCHOT_LPWR] = {SC8886S_REG_CHARGE_OPT_1_H, 5, 2},
        [EN_PSYS]         = {SC8886S_REG_CHARGE_OPT_1_H, 4, 1},
        [RSNS_RAC]        = {SC8886S_REG_CHARGE_OPT_1_H, 3, 1},
        [RSNS_RSR]        = {SC8886S_REG_CHARGE_OPT_1_H, 2, 1},
        [PSYS_RATIO]      = {SC8886S_REG_CHARGE_OPT_1_H, 1, 1},
        [PTM_PINSEL]      = {SC8886S_REG_CHARGE_OPT_1_H, 0, 1},

        /*REG32*/
        [EN_EXTILIM]    = {SC8886S_REG_CHARGE_OPT_2_L, 7, 1},
        [EN_ICHG_IDCHG] = {SC8886S_REG_CHARGE_OPT_2_L, 6, 1},
        [Q2_OCP]        = {SC8886S_REG_CHARGE_OPT_2_L, 5, 1},
        [EN_ACOC]       = {SC8886S_REG_CHARGE_OPT_2_L, 3, 1},
        [ACOC_VTH]      = {SC8886S_REG_CHARGE_OPT_2_L, 2, 1},
        [EN_BATOC]      = {SC8886S_REG_CHARGE_OPT_2_L, 1, 1},
        [BATCOC_VTH]    = {SC8886S_REG_CHARGE_OPT_2_L, 0, 1},
        /*REG33*/
        [PKPWR_TOVLD_REG]  = {SC8886S_REG_CHARGE_OPT_2_H, 6, 2},
        [EN_PKPWR_IDPM]    = {SC8886S_REG_CHARGE_OPT_2_H, 5, 1},
        [EN_PKPWR_VSYS]    = {SC8886S_REG_CHARGE_OPT_2_H, 4, 1},
        [PKPWER_OVLD_STAT] = {SC8886S_REG_CHARGE_OPT_2_H, 3, 1},
        [PKPWR_RELAX_STAT] = {SC8886S_REG_CHARGE_OPT_2_H, 2, 1},
        [PKPWER_TMAX]      = {SC8886S_REG_CHARGE_OPT_2_H, 0, 2},
        /*REG34*/
        [EN_CON_VAP]     = {SC8886S_REG_CHARGE_OPT_3_L, 6, 1},
        [OTG_VAP_MODE]   = {SC8886S_REG_CHARGE_OPT_3_L, 5, 1},
        [IL_AVG]         = {SC8886S_REG_CHARGE_OPT_3_L, 3, 2},
        [OTG_RANGE_LOW]  = {SC8886S_REG_CHARGE_OPT_3_L, 2, 2},
        [BATFETOFF_HIZ]  = {SC8886S_REG_CHARGE_OPT_3_L, 1, 1},
        [PSYS_OTG_IDCHG] = {SC8886S_REG_CHARGE_OPT_3_L, 0, 1},
        /*REG35*/
        [EN_HIZ]       = {SC8886S_REG_CHARGE_OPT_3_H, 7, 1},
        [RESET_REG]    = {SC8886S_REG_CHARGE_OPT_3_H, 6, 1},
        [RESET_VINDPM] = {SC8886S_REG_CHARGE_OPT_3_H, 5, 1},
        [EN_OTG]       = {SC8886S_REG_CHARGE_OPT_3_H, 4, 1},
        [EN_ICO_MODE]  = {SC8886S_REG_CHARGE_OPT_3_H, 3, 1},
        /*REG36*/
        [VSYS_TH1]           = {SC8886S_REG_PROCHOT_OPT_0_L, 4, 4},
        [VSYS_TH2]           = {SC8886S_REG_PROCHOT_OPT_0_L, 2, 2},
        [INOM_DEG]           = {SC8886S_REG_PROCHOT_OPT_0_L, 1, 1},
        [LOWER_PROCHOT_VDPM] = {SC8886S_REG_PROCHOT_OPT_0_L, 0, 1},
        /*REG37*/
        [ILIM2_VTH]          = {SC8886S_REG_PROCHOT_OPT_0_H, 3, 4},
        [ICRIT_DEG]          = {SC8886S_REG_PROCHOT_OPT_0_H, 1, 2},
        [PROCHOT_VDPM_80_90] = {SC8886S_REG_PROCHOT_OPT_0_H, 0, 1},
        /*REG38*/
        [PROCHOT_VDPM]    = {SC8886S_REG_PROCHOT_OPT1_L, 7, 1},
        [PROCHOT_COMP]    = {SC8886S_REG_PROCHOT_OPT1_L, 6, 1},
        [PROCHOT_ICRIT]   = {SC8886S_REG_PROCHOT_OPT1_L, 5, 1},
        [PROCHOT_INOM]    = {SC8886S_REG_PROCHOT_OPT1_L, 4, 1},
        [PROCHOT_IDCHG]   = {SC8886S_REG_PROCHOT_OPT1_L, 3, 1},
        [PROCHOT_VSYS]    = {SC8886S_REG_PROCHOT_OPT1_L, 2, 1},
        [PROCHOT_BATPRES] = {SC8886S_REG_PROCHOT_OPT1_L, 1, 1},
        [PROCHOT_ACOK]    = {SC8886S_REG_PROCHOT_OPT1_L, 0, 1},
        /*REG39*/
        [IDCHG_VTH] = {SC8886S_REG_PROCHOT_OPT1_H, 2, 6},
        [IDCHG_DEG] = {SC8886S_REG_PROCHOT_OPT1_H, 0, 2},
        /*REG3A*/
        [EN_ADC_CMPIN] = {SC8886S_REG_ADC_OPT_L, 7, 1},
        [EN_ADC_VBUS]  = {SC8886S_REG_ADC_OPT_L, 6, 1},
        [EN_ADC_PSYS]  = {SC8886S_REG_ADC_OPT_L, 5, 1},
        [EN_ADC_IIN]   = {SC8886S_REG_ADC_OPT_L, 4, 1},
        [EN_ADC_IDCHG] = {SC8886S_REG_ADC_OPT_L, 3, 1},
        [EN_ADC_ICHG]  = {SC8886S_REG_ADC_OPT_L, 2, 1},
        [EN_ADC_VSYS]  = {SC8886S_REG_ADC_OPT_L, 1, 1},
        [EN_ADC_VBAT]  = {SC8886S_REG_ADC_OPT_L, 0, 1},
        /*REG3B*/
        [ADC_CONV]      = {SC8886S_REG_ADC_OPT_H, 7, 1},
        [ADC_START]     = {SC8886S_REG_ADC_OPT_H, 6, 1},
        [ADC_FULLSCALE] = {SC8886S_REG_ADC_OPT_H, 5, 1},
    };

    /**
     * @brief 函数返回状态，包含成功、失败、通用错误、I2C通信错误、参数错误、超时错误
     */
    typedef enum
    {
        SC8886S_OK            = 0,  // 操作成功
        SC8886S_FAIL          = -1, // 操作失败
        SC8886S_ERROR         = -2, // 通用错误
        SC8886S_I2C_ERROR     = -3, // I2C通信错误
        SC8886S_PARAM_ERROR   = -4, // 参数错误
        SC8886S_TIMEOUT_ERROR = -5  // 超时错误
    } sc8886s_status_t;

    /*===============================================    低级寄存器操作
     * ===============================================*/

    // 需要用户实现

    sc8886s_status_t sc8886s_i2c_read_reg_8bit(uint8_t reg, uint8_t *value);

    sc8886s_status_t sc8886s_i2c_read_reg_16bit(uint8_t reg, uint16_t *value);

    sc8886s_status_t sc8886s_i2c_write_reg_8bit(uint8_t reg, uint8_t value);

    sc8886s_status_t sc8886s_i2c_write_reg_16bit(uint8_t reg, uint16_t value);

    // 工具函数

    bool is_reg_cross_reg(field_info_t reg_field); // 通过单元测试，20250808

    uint16_t get_bit_field_mask(field_info_t reg_field); // 通过单元测试，20250808

    uint16_t extract_field_from_reg(field_info_t reg_field, uint8_t reg_low,
                                    uint8_t reg_high); // 通过单元测试，20250808

    // 一些好用的二次封装

    sc8886s_status_t modify_reg_value(field_info_t reg_field, uint16_t set_reg_value);

    sc8886s_status_t set_bit_field_value(field_info_t reg_field, uint16_t value); // 通过单元测试，20250808

    sc8886s_status_t get_bit_field_value(field_info_t reg_field, uint16_t *value); // 通过单元测试，20250808

/*===============================================    批量寄存器操作  ===============================================*/

// 批量操作的最大字段数量限制
#define SC8886S_BATCH_MAX_FIELDS 16

    // 单个字段操作信息
    typedef struct
    {
        field_info_t field; // 字段信息
        uint16_t     value; // 要设置的值
    } sc8886s_field_operation_t;

    // 批量操作句柄
    typedef struct
    {
        sc8886s_field_operation_t operations[SC8886S_BATCH_MAX_FIELDS]; // 操作数组
        uint8_t                   operation_count;                      // 当前操作数量
        bool                      is_active;                            // 是否处于批量操作模式
    } sc8886s_batch_handle_t;

    sc8886s_status_t sc8886s_batch_begin(sc8886s_batch_handle_t *handle);

    sc8886s_status_t sc8886s_batch_add_field(sc8886s_batch_handle_t *handle, field_info_t reg_field, uint16_t value);

    sc8886s_status_t sc8886s_batch_commit(sc8886s_batch_handle_t *handle);

    sc8886s_status_t sc8886s_batch_clear(sc8886s_batch_handle_t *handle);

    /*===============================================    枚举类型  ===============================================*/

    typedef enum
    {
        RESET_IDLE    = 0, // 默认不操作，不重置寄存器
        RESET_ALL_REG = 1  // 重置所有寄存器
    } sc8886s_reset_reg_t;

    typedef enum
    {
        EN_LWPWR_PERFORMANCE = 0, // 性能模式
        EN_LWPWR_LOW_POWER   = 1  // 低功耗模式,默认是这个
    } sc8886s_en_lwpwr_t;

    /**
     * @brief 工作模式，包含充电模式、OTG放电模式、高阻抗模式、学习模式、船运模式
     */
    typedef enum
    {
        MODE_CHARGING = 0, // 充电模式
        MODE_OTG      = 1, // OTG放电模式
        MODE_HIZ      = 2, // 高阻抗模式, HIZ for high impedance(Z)
        MODE_LEARN    = 3, // 学习模式
        MODE_SHIP     = 4  // 船运模式
    } sc8886s_work_mode_t;

    /**
     * @brief 充电状态，包含未充电、预充电、快充、恒压充电
     */
    typedef enum
    {
        CHARGE_NOT_CHARGING     = 0, // 未充电
        CHARGE_PRECHARGE        = 1, // 预充电
        CHARGE_FAST_CHARGE      = 2, // 快充
        CHARGE_CONSTANT_VOLTAGE = 3  // 恒压充电
    } sc8886s_charge_stage_t;

    /**
     * @brief PWM频率设置，包含1200kHz、800kHz
     */
    typedef enum
    {
        PWM_FREQ_1200KHZ = 0, // 1200kHz
        PWM_FREQ_800KHZ  = 1  // 800kHz
    } sc8886s_pwm_freq_t;

    /**
     * @brief 看门狗定时器设置，包含禁用、5秒、88秒、175秒
     */
    typedef enum
    {
        WDT_DISABLE = 0, // 禁用看门狗
        WDT_5S      = 1, // 5秒
        WDT_88S     = 2, // 88秒
        WDT_175S    = 3  // 175秒
    } sc8886s_WDTWR_ADJ_t;

    typedef enum
    {
        DISABLE_ADC = 0, // 禁用ADC
        ENABLE_ADC  = 1  // 使能ADC
    } sc8886s_EN_ADC_t;

    typedef enum
    {
        ADC_ONE_SHOT_MODE   = 0, // 单次转换模式
        ADC_CONTINUOUS_MODE = 1  // 连续转换模式
    } sc8886s_ADC_CONV_t;

    typedef enum
    {
        EN_CHARGE_ENABLE  = 0, // 使能充电
        EN_CHARGE_DISABLE = 1  // 禁用充电
    } sc8886s_en_charge_t;

    typedef enum
    {
        DISABLE_ICO_MODE = 0, // 禁用ICO功能
        ENABLE_ICO_MODE  = 1  // 使能ICO功能，Input Current Optimizer，用来识别最大输入电流
    } sc8886s_EN_ICO_MODE_t;

    /*===============================================    结构体定义  ===============================================*/

    typedef enum
    {
        SC8886S_MODE_DISCHARGING = 0,
        SC8886S_MODE_CHARGING,
        SC8886S_MODE_PRECHARGE,
        SC8886S_MODE_FASTCHARGE,
        SC8886S_MODE_OTG,
        SC8886S_MODE_VAP,
        SC8886S_MODE_LEARN,
    } sc8886s_mode_t;

    typedef enum
    {
        SC8886S_BAT_DISCHARGING = -1,
        SC8886S_BAT_IDLE        = 0,
        SC8886S_BAT_CHARGING    = 1,
    } sc8886s_bat_dir_t;

    typedef struct
    {
        bool fault_acov;     // 输入过压
        bool fault_sysovp;   // 系统过压
        bool fault_batovp;   // 电池过压
        bool fault_acoc;     // 输入过流
        bool fault_batoc;    // 电池过流
        bool fault_sysshort; // 系统短路
        bool fault_otg_ovp;  // OTG过压
        bool fault_otg_uvp;  // OTG欠压
        bool fault_thermal;  // 温度保护（由状态组合）
    } sc8886s_fault_flag_t;

    typedef struct
    {
        sc8886s_mode_t mode; // 当前运行模式
        bool           adapter_present;
        bool           ico_done;

        // 电压 / 电流 / 功率
        uint16_t          vbus_mV;
        uint16_t          vbat_mV;
        uint16_t          vsys_mV;
        uint16_t          iin_mA;        // 实时输入电流
        uint16_t          ibat_mA;       // 电池电流（充电 +，放电 -）
        sc8886s_bat_dir_t bat_direction; // 充放电方向
        uint16_t          psys_mW;       // 系统功率

        // 限制值
        uint16_t vsys_min_mV;    // 系统最小电压
        uint16_t ico_current_mA; // 充电头最大输入电流
        uint16_t charge_current_limit_mA;
        uint16_t charge_voltage_limit_mV;

        // 保护/故障
        sc8886s_fault_flag_t fault_flags;

    } sc8886s_info_t;

    /*===============================================    基础操作函数  ===============================================*/

    sc8886s_status_t sc8886s_check_device(void);

    sc8886s_status_t sc8886s_chip_reset(void);

    void sc8886s_delay_ms(uint16_t ms);

    /*===============================================    充电控制函数  ===============================================*/

    /**
     * @brief 设置充电电压
     * @param handle 设备句柄指针
     * @param voltage_mv 充电电压 (mV), 范围1024-19200mV, 8mV步进
     * @return 操作状态
     */
    sc8886s_status_t sc8886s_set_charge_voltage(uint16_t voltage_mv);

    /**
     * @brief 设置充电电流
     * @param handle 设备句柄指针
     * @param current_ma 充电电流 (mA), 范围64-8128mA, 64mA步进
     * @return 操作状态
     */
    sc8886s_status_t sc8886s_set_charge_current(uint16_t current_ma);

    /**
     * @brief 设置输入电流限制
     * @param handle 设备句柄指针
     * @param current_ma 输入电流限制 (mA), 范围50-6400mA, 50mA步进
     * @return 操作状态
     */
    sc8886s_status_t sc8886s_set_input_current_limit(uint16_t current_ma);

    /**
     * @brief 设置输入电压限制
     * @param handle 设备句柄指针
     * @param voltage_mv 输入电压限制 (mV), 范围3200-19520mV, 64mV步进
     * @return 操作状态
     */
    sc8886s_status_t sc8886s_set_input_voltage_limit(uint16_t voltage_mv);

    /**
     * @brief 设置系统最小电压
     * @param handle 设备句柄指针
     * @param voltage_mv 系统最小电压 (mV), 范围1024-16128mV, 256mV步进
     * @return 操作状态
     */
    sc8886s_status_t sc8886s_set_system_min_voltage(uint16_t voltage_mv);

    /*===============================================     OTG控制函数  ===============================================*/

    /**
     * @brief 使能/禁用OTG模式
     * @param handle 设备句柄指针
     * @param enable true=使能, false=禁用
     * @return 操作状态
     */
    sc8886s_status_t sc8886s_enable_otg(bool enable);

    /**
     * @brief 设置OTG输出电压
     * @param handle 设备句柄指针
     * @param voltage_mv OTG输出电压 (mV), 范围3000-24000mV, 8mV步进
     * @return 操作状态
     */
    sc8886s_status_t sc8886s_set_otg_voltage(uint16_t voltage_mv);

    /**
     * @brief 设置OTG输出电流限制
     * @param handle 设备句柄指针
     * @param current_ma OTG输出电流限制 (mA), 范围50-6400mA, 50mA步进
     * @return 操作状态
     */
    sc8886s_status_t sc8886s_set_otg_current_limit(uint16_t current_ma);

    /*===============================================    系统配置函数  ===============================================*/

    /**
     * @brief 设置工作模式
     * @param handle 设备句柄指针
     * @param mode 工作模式
     * @return 操作状态
     */
    sc8886s_status_t sc8886s_set_work_mode(sc8886s_work_mode_t mode);

    /**
     * @brief 设置PWM开关频率
     * @param handle 设备句柄指针
     * @param freq PWM频率设置
     * @return 操作状态
     */
    sc8886s_status_t sc8886s_set_pwm_frequency(sc8886s_pwm_freq_t freq);

    /**
     * @brief 设置看门狗定时器
     * @param handle 设备句柄指针
     * @param wdt_time 看门狗时间设置
     * @return 操作状态
     */
    sc8886s_status_t sc8886s_set_watchdog(sc8886s_WDTWR_ADJ_t wdt_time);

    /**
     * @brief 复位看门狗定时器
     * @param handle 设备句柄指针
     * @return 操作状态
     */
    sc8886s_status_t sc8886s_reset_watchdog();

    /**
     * @brief 进入HIZ高阻抗模式
     * @param handle 设备句柄指针
     * @param enable true=进入HIZ, false=退出HIZ
     * @return 操作状态
     */
    sc8886s_status_t sc8886s_set_hiz_mode(bool enable);

    /*===============================================    状态监控函数  ===============================================*/

    /**
     * @brief 获取故障状态
     * @param handle 设备句柄指针
     * @param fault_flags 故障标志指针
     * @return 操作状态
     */
    sc8886s_status_t sc8886s_get_fault_status(uint8_t *fault_flags);

    /**
     * @brief 清除故障状态
     * @param handle 设备句柄指针
     * @param fault_mask 要清除的故障掩码
     * @return 操作状态
     */
    sc8886s_status_t sc8886s_clear_fault_status(uint8_t fault_mask);

    /*===============================================     ADC监控函数  ===============================================*/

    sc8886s_status_t sc8886s_enable_adc(bool enable, bool continuous);

    sc8886s_status_t sc8886s_adc_read(sc8886_fields which_adc, uint16_t *value);

    /*===============================================    保护功能函数  ===============================================*/

    sc8886s_status_t sc8886s_enable_input_ovp(bool enable);

    sc8886s_status_t sc8886s_enable_battery_ocp(bool enable);

    sc8886s_status_t sc8886s_enable_system_ovp(bool enable);

    sc8886s_status_t sc8886s_enable_thermal_protection(bool enable);

    /*===============================================     PROCHOT功能函数
     * ===============================================*/

    /**
     * @brief 配置PROCHOT信号
     * @param handle 设备句柄指针
     * @param enable_flags PROCHOT使能标志位掩码
     * @return 操作状态
     */
    sc8886s_status_t sc8886s_config_prochot(uint8_t enable_flags);

    /**
     * @brief 读取PROCHOT状态
     * @param handle 设备句柄指针
     * @param status_flags PROCHOT状态标志指针
     * @return 操作状态
     */
    sc8886s_status_t sc8886s_get_prochot_status(uint8_t *status_flags);

    /**
     * @brief 清除PROCHOT状态
     * @param handle 设备句柄指针
     * @return 操作状态
     */
    sc8886s_status_t sc8886s_clear_prochot();

    /*===============================================    外部接口函数  ===============================================*/

    // I2C互斥锁相关函数
    sc8886s_status_t sc8886s_mutex_init(void);
    void             sc8886s_mutex_deinit(void);

    sc8886s_status_t bsp_charge_sc8886_driver_probe(void);
    void             bsp_charge_current_check_handler(void);
    sc8886s_status_t sc8886s_hw_init(void);
    sc8886s_status_t sc8886s_hw_init_batch_example(void); // 批量操作示例
    bool             is_sc8886s_input_present(void);
    bool             is_sc8886s_battery_charge_finish(void);
    sc8886s_status_t sc8886s_get_ico_current(uint16_t *current_ma);
    sc8886s_status_t sc8886s_trigger_ico_measure(void);
    sc8886s_status_t sc8886s_get_VBUS(uint16_t *voltage_mv);
    sc8886s_status_t sc8886s_enable_sleep(void);

    sc8886s_status_t sc8886s_set_IINREG(uint16_t current_mA);

#ifdef __cplusplus
}
#endif

#endif /* __SC8886S_H__ */
