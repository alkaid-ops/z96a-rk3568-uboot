
/* 00804284 FUN_00804284 */

undefined8 FUN_00804284(void)

{
  ushort uVar1;
  short sVar2;
  char cVar3;
  int iVar4;
  uint uVar5;
  char *pcVar6;
  ushort uVar7;
  undefined1 local_17;
  short local_16;
  ushort local_14;
  ushort local_12;
  undefined8 local_10;
  undefined8 local_8;
  
  local_16 = 0;
  local_14 = 0;
  local_12 = 0;
  cVar3 = FUN_0083c748(s_sunniwell_z96a_rk3568_laptop_v2_008bba11);
  if (cVar3 == '\0') {
    return 0;
  }
  iVar4 = FUN_0083d930(0x38,s_i2c_fdd40000_008bba31,&local_10);
  if (iVar4 != 0) {
    FUN_0089e7e8(s_z96a_pwr__i2c_bus___s__unavailab_008bba3e,s_i2c_fdd40000_008bba31,iVar4);
    return 0;
  }
  uVar5 = FUN_00844078(local_10,0x6b,1,&local_8);
  if (uVar5 == 0) {
    iVar4 = FUN_008040d0(local_8,0x20,&local_16);
    if (((iVar4 == 0) && (iVar4 = FUN_008040d0(local_8,0x21,&local_14), iVar4 == 0)) &&
       (iVar4 = FUN_008040d0(local_8,0x26,&local_12), iVar4 == 0)) {
      local_17 = 0x65;
      FUN_00843ee0(local_8,0x36,&local_17,1);
      local_17 = 0x4a;
      FUN_00843ee0(local_8,0x37,&local_17,1);
      sVar2 = local_16;
      uVar1 = local_14 & 2;
      uVar5 = (local_12 & 0x7f) * 0x40 + 0xb40;
      uVar7 = local_14 >> 1 & 1;
      FUN_0089e7e8(s_z96a_pwr__SC8886_AC__d_BAT_REMOV_008bbacc,(ulong)(long)local_16 >> 0x1f & 1,
                   uVar7,uVar5);
      if (-1 < sVar2) {
        if (uVar5 < 3000 || uVar1 != 0) {
          FUN_00804108(local_8,0x12,0xffff8000);
          FUN_00871868(s_z96a_power_src_008bbb00,&DAT_008bbb68);
          FUN_0089e7e8(s_z96a_pwr__NO_usable_power__AC_0_B_008bbb6d,uVar7,uVar5);
          return 0;
        }
        FUN_00804108(local_8,0x32,0x1000,0);
        FUN_00804108(local_8,0x12,0xffff8000,0);
        FUN_00871868(s_z96a_power_src_008bbb00,s_battery_008bbb3f);
        pcVar6 = s_z96a_pwr__source_battery___dmV__008bbb47;
        goto LAB_00804324;
      }
      FUN_00804108(local_8,0x32,0x1000,0);
      FUN_00804108(local_8,0x12,0xffff8000,0);
      FUN_00804108(local_8,0x14,0x7f00,0x400);
      FUN_00871868(s_z96a_power_src_008bbb00,s_usb_c_008bbafa);
      pcVar6 = s_z96a_pwr__source_USB_C__charge_c_008bbb0f;
    }
    else {
      pcVar6 = s_z96a_pwr__SC8886_read_failed__sk_008bba8d;
    }
    FUN_0089e7e8(pcVar6);
  }
  else {
    pcVar6 = s_z96a_pwr__SC8886_0x6b_not_found___008bba67;
LAB_00804324:
    FUN_0089e7e8(pcVar6,uVar5);
  }
  return 0;
}


