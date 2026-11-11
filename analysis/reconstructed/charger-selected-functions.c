
/* selected 00803640 */
/* function 00803640 FUN_00803640 */
/* caller 008036c4 FUN_008036c4 */
/* callee 00871868 FUN_00871868 */
/* callee 00803600 FUN_00803600 */

/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */

undefined8 FUN_00803640(void)

{
  int iVar1;
  char *pcVar2;
  
  DataMemoryBarrier(3,3);
  FUN_00803600(_DAT_fdc20200);
  iVar1 = _DAT_fdc20200;
  DataMemoryBarrier(3,3);
  DataMemoryBarrier(3,3);
  _DAT_fdc20200 = 0x5242c300;
  if (iVar1 == 0x5242c309) {
    pcVar2 = s_setenv_preboot__fastboot_usb_0_008bccf7;
  }
  else {
    if (iVar1 != 0x5242c30c) {
      _DAT_fdc20200 = 0x5242c300;
      return 0;
    }
    pcVar2 = s_setenv_preboot__ums_mmc_0_008bcd1e;
  }
  FUN_00871868(s_preboot_008bcd16,pcVar2);
  return 0;
}



/* selected 0082a014 */
/* function 0082a014 FUN_0082a014 */
/* caller 00802fb0 FUN_00802fb0 */
/* callee 008036d8 FUN_008036d8 */
/* callee 0082a00c FUN_0082a00c */
/* callee 00842200 FUN_00842200 */
/* callee 00871bfc FUN_00871bfc */
/* callee 0082e7a0 FUN_0082e7a0 */
/* callee 00830310 FUN_00830310 */
/* callee 00847228 FUN_00847228 */
/* callee 008718ec FUN_008718ec */
/* callee 00803a88 FUN_00803a88 */
/* callee 0089acb8 FUN_0089acb8 */
/* callee 00802178 FUN_00802178 */
/* callee 0082e978 FUN_0082e978 */
/* callee 0082fbc8 FUN_0082fbc8 */
/* callee 00871afc FUN_00871afc */
/* callee 0089e7e8 FUN_0089e7e8 */
/* callee 00803228 FUN_00803228 */
/* callee 00826770 FUN_00826770 */
/* callee 00800450 FUN_00800450 */
/* callee 00840df8 FUN_00840df8 */
/* callee 0089b6ec FUN_0089b6ec */
/* callee 0082e758 FUN_0082e758 */
/* callee 0082e234 FUN_0082e234 */
/* callee 0082fb48 FUN_0082fb48 */
/* callee 0087bc50 FUN_0087bc50 */
/* callee 0082a004 FUN_0082a004 */
/* callee 0088f29c FUN_0088f29c */
/* callee 0083d0bc FUN_0083d0bc */
/* callee 008a2fdc FUN_008a2fdc */
/* callee 0083d058 FUN_0083d058 */
/* callee 00886464 FUN_00886464 */
/* callee 00826718 FUN_00826718 */
/* callee 008869d0 FUN_008869d0 */
/* callee 0082a394 FUN_0082a394 */
/* callee 0083d044 FUN_0083d044 */
/* callee 00830328 FUN_00830328 */
/* callee 00898da0 FUN_00898da0 */
/* callee 00871cc8 FUN_00871cc8 */
/* callee 0088f3b4 FUN_0088f3b4 */
/* callee 008036c4 FUN_008036c4 */
/* callee 0085a86c thunk_FUN_0085a6fc */
/* callee 00829ffc FUN_00829ffc */
/* callee 0082fa58 FUN_0082fa58 */

void FUN_0082a014(void)

{
  int iVar1;
  ulong uVar2;
  undefined8 uVar3;
  char *pcVar4;
  undefined8 uVar5;
  long in_x18;
  long extraout_x18;
  long extraout_x18_00;
  long extraout_x18_01;
  long extraout_x18_02;
  long extraout_x18_03;
  undefined8 *extraout_x18_04;
  long extraout_x18_05;
  long lVar6;
  long extraout_x18_06;
  undefined4 local_8;
  undefined4 uStack_4;
  
  *(ulong *)(in_x18 + 0x40) = *(ulong *)(in_x18 + 0x40) & 0xfffffffffffefeff | 0x201;
  iVar1 = FUN_0082fbc8();
  if (iVar1 == 0) goto LAB_0082a064;
  pcVar4 = s_event_init_008acd43;
LAB_0082a04c:
  do {
    FUN_0089e7e8(s__s____initcall__s___failed_008acd1a,s_initcall_run_r_008abaf8,pcVar4);
    while( true ) {
      FUN_0089acb8();
LAB_0082a064:
      FUN_00802178();
      DAT_008d80b0 = 0xe7ae0;
      *(long *)(extraout_x18 + 0x50) =
           *(long *)(extraout_x18 + 0x50) + *(long *)(extraout_x18 + 0x78);
      FUN_0087bc50();
      lVar6 = extraout_x18_00;
      if (((uint)*(undefined8 *)(extraout_x18_00 + 0x40) >> 0xb & 1) == 0) {
        FUN_00800450(*(undefined8 *)(extraout_x18_00 + 0x60),0);
        lVar6 = extraout_x18_01;
      }
      FUN_0082e978(*(long *)(lVar6 + 0x60) + -0x201f000);
      iVar1 = FUN_0088f29c(*(undefined8 *)(extraout_x18_02 + 0x10),extraout_x18_02 + 0x138);
      if (iVar1 != 0) {
        pcVar4 = s_initr_of_live_008ace70;
        goto LAB_0082a04c;
      }
      FUN_00840df8();
      *(undefined8 *)(extraout_x18_03 + 0x118) = 0;
      iVar1 = FUN_0083d058(0);
      if ((iVar1 != 0) || (iVar1 = FUN_0083d044(), iVar1 != 0)) {
        pcVar4 = s_initr_dm_008ace7e;
        goto LAB_0082a04c;
      }
      iVar1 = FUN_008036d8();
      if (iVar1 != 0) {
        pcVar4 = s_board_init_008ace87;
        goto LAB_0082a04c;
      }
      iVar1 = FUN_0089b6ec();
      if (iVar1 != 0) {
        pcVar4 = s_initr_lmb_008ace92;
        goto LAB_0082a04c;
      }
      iVar1 = FUN_00886464();
      if (iVar1 != 0) {
        pcVar4 = s_efi_memory_init_008ace9c;
        goto LAB_0082a04c;
      }
      uVar2 = FUN_0088f3b4();
      if ((int)uVar2 != 0) {
        FUN_0089e7e8(s_binman_init_failed__d_008aceac);
        pcVar4 = s_initr_binman_008acec3;
        goto LAB_0082a04c;
      }
      iVar1 = FUN_00830310(uVar2,uVar2 & 0xffffffff);
      if (iVar1 != 0) {
        pcVar4 = s_stdio_init_tables_008aced0;
        goto LAB_0082a04c;
      }
      iVar1 = thunk_FUN_0085a6fc();
      if (iVar1 != 0) {
        pcVar4 = s_serial_initialize_008acee2;
        goto LAB_0082a04c;
      }
      FUN_0083d0bc(&local_8,&uStack_4);
      FUN_0089e7e8(s_Core___d_devices___d_uclasses_008acef4,local_8,uStack_4);
      uVar3 = FUN_00898da0();
      FUN_0089e7e8(s___devicetree___s_008acf13,uVar3);
      FUN_0089e7e8(&DAT_008ba358);
      iVar1 = FUN_0082a004();
      if (iVar1 != 0) {
        pcVar4 = s_arch_initr_trap_008acf24;
        goto LAB_0082a04c;
      }
      iVar1 = FUN_0082a00c();
      if (iVar1 != 0) {
        pcVar4 = s_power_init_board_008acf34;
        goto LAB_0082a04c;
      }
      iVar1 = FUN_008869d0();
      if (iVar1 != 0) {
        pcVar4 = s_efi_init_early_008acf45;
        goto LAB_0082a04c;
      }
      FUN_0082e234(s_MMC__008acf54);
      FUN_00847228(*extraout_x18_04);
      iVar1 = FUN_00842200(s_load_environment_008acf5c,1);
      if (iVar1 == 0) {
        FUN_00871bfc(0,0);
        lVar6 = extraout_x18_06;
      }
      else {
        FUN_00871cc8();
        lVar6 = extraout_x18_05;
      }
      FUN_008718ec(s_fdtcontroladdr_008b8e93,*(undefined8 *)(lVar6 + 0x10));
      DAT_008ccc98 = FUN_00871afc(s_loadaddr_008be213,0x10,DAT_008ccc98);
      iVar1 = FUN_00829ffc();
      if (iVar1 != 0) break;
      iVar1 = FUN_0082fb48(10);
      if (iVar1 == 0) {
        iVar1 = FUN_00830328();
        if (iVar1 != 0) {
          pcVar4 = s_stdio_add_devices_008acf82;
          goto LAB_0082a04c;
        }
        iVar1 = FUN_00826770();
        if (iVar1 != 0) {
          pcVar4 = s_jumptable_init_008acf94;
          goto LAB_0082a04c;
        }
        iVar1 = FUN_0082e7a0();
        if (iVar1 != 0) {
          pcVar4 = s_console_init_r_008acfa3;
          goto LAB_0082a04c;
        }
        iVar1 = FUN_0082e758();
        if (iVar1 != 0) {
          pcVar4 = s_console_announce_r_008acfb2;
          goto LAB_0082a04c;
        }
        iVar1 = FUN_0082a394();
        if (iVar1 != 0) {
          pcVar4 = s_show_board_info_008acdd5;
          goto LAB_0082a04c;
        }
        iVar1 = FUN_00803a88();
        if (iVar1 != 0) {
          pcVar4 = s_misc_init_r_008acfc5;
          goto LAB_0082a04c;
        }
        iVar1 = FUN_00803228();
        if (iVar1 != 0) {
          pcVar4 = s_interrupt_init_008acfd1;
          goto LAB_0082a04c;
        }
        iVar1 = FUN_008036c4();
        if (iVar1 != 0) {
          pcVar4 = s_board_late_init_008acfe0;
          goto LAB_0082a04c;
        }
        FUN_0082e234(s_Net__008acff0);
        FUN_008a2fdc();
        iVar1 = FUN_0082fb48(0xb);
        if (iVar1 == 0) {
          FUN_0082fb48(0xe);
          do {
            FUN_00826718();
          } while( true );
        }
        uVar3 = FUN_0082fa58(0xb);
        uVar5 = 0xb;
      }
      else {
        uVar3 = FUN_0082fa58(10);
        uVar5 = 10;
      }
      FUN_0089e7e8(s__s____event__d__s_failed_008acd4e,s_initcall_run_r_008abaf8,uVar5,uVar3);
    }
    pcVar4 = s_cpu_secondary_init_r_008acf6d;
  } while( true );
}



/* selected 0083c600 */
/* function 0083c600 FUN_0083c600 */
/* caller 00832bdc FUN_00832bdc */
/* caller 0080bea4 FUN_0080bea4 */
/* caller 00808718 FUN_00808718 */
/* caller 00836c00 FUN_00836c00 */
/* caller 00809f58 FUN_00809f58 */
/* caller 0080bd68 FUN_0080bd68 */
/* caller 008560e0 FUN_008560e0 */
/* caller 00808800 FUN_00808800 */
/* caller 0084799c FUN_0084799c */
/* caller 008134c4 FUN_008134c4 */
/* caller 00837830 FUN_00837830 */
/* caller 00853938 FUN_00853938 */
/* caller 0080bdc0 FUN_0080bdc0 */
/* caller 00851408 FUN_00851408 */
/* caller 008472fc FUN_008472fc */
/* caller 00832b5c FUN_00832b5c */
/* caller 00809f80 FUN_00809f80 */
/* caller 00832c48 FUN_00832c48 */
/* caller 00853b1c FUN_00853b1c */
/* caller 00843ee0 FUN_00843ee0 */
/* caller 0080858c FUN_0080858c */
/* caller 00809f1c FUN_00809f1c */
/* caller 00888394 FUN_00888394 */
/* caller 0080bfa0 FUN_0080bfa0 */
/* caller 00853e64 FUN_00853e64 */
/* caller 008539f8 FUN_008539f8 */
/* caller 00812a50 FUN_00812a50 */
/* caller 00888610 FUN_00888610 */
/* caller 0082ce40 FUN_0082ce40 */
/* caller 00843d6c FUN_00843d6c */
/* caller 0082cc94 FUN_0082cc94 */
/* caller 008539d0 FUN_008539d0 */
/* caller 0087b498 FUN_0087b498 */
/* caller 00854038 FUN_00854038 */
/* caller 00847914 FUN_00847914 */
/* caller 0085f1e8 FUN_0085f1e8 */
/* caller 008088b8 FUN_008088b8 */

undefined8 FUN_0083c600(long param_1)

{
  return *(undefined8 *)(param_1 + 0x30);
}



/* selected 00844078 */
/* function 00844078 FUN_00844078 */
/* caller 00804284 FUN_00804284 */
/* caller 00844180 FUN_00844180 */
/* caller 00819cf8 FUN_00819cf8 */
/* callee 0083c1fc FUN_0083c1fc */
/* callee 0089e6a4 FUN_0089e6a4 */
/* callee 0083cbb0 FUN_0083cbb0 */
/* callee 0083c3d4 FUN_0083c3d4 */
/* callee 0089c998 FUN_0089c998 */
/* callee 0083c4b8 FUN_0083c4b8 */
/* callee 0082ea78 FUN_0082ea78 */
/* callee 0083c498 FUN_0083c498 */
/* callee 0083dfec FUN_0083dfec */

int FUN_00844078(undefined8 param_1,uint param_2,uint param_3,long *param_4)

{
  int iVar1;
  long lVar2;
  uint *puVar3;
  long local_30;
  long local_28;
  undefined1 auStack_20 [32];
  
  FUN_0083c498(param_1,&local_30);
  while (local_30 != 0) {
    puVar3 = (uint *)FUN_0083c3d4();
    if (*puVar3 == (param_2 & (puVar3[3] ^ 0xffffffff))) {
      iVar1 = FUN_0083c1fc(local_30);
      if (iVar1 != 0) {
        return iVar1;
      }
      local_28 = local_30;
      goto LAB_00844150;
    }
    FUN_0083c4b8(&local_30);
  }
  FUN_0089e6a4(auStack_20,0x1e,s_generic__x_008b8104,param_2);
  lVar2 = FUN_0089c998(auStack_20);
  if (lVar2 == 0) {
    iVar1 = -0xc;
  }
  else {
    iVar1 = FUN_0083cbb0(param_1,s_i2c_generic_chip_drv_008b810f,lVar2,&local_28);
    if (iVar1 == 0) {
      puVar3 = (uint *)FUN_0083c3d4(local_28);
      *puVar3 = param_2;
      puVar3[1] = param_3;
      iVar1 = FUN_0083c1fc(local_28);
      if (iVar1 == 0) {
LAB_00844150:
        *param_4 = local_28;
        return 0;
      }
      FUN_0083dfec();
    }
    FUN_0082ea78(lVar2);
  }
  return iVar1;
}


