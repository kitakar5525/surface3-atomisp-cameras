#### Trying to get atomisp cameras working on Surface 3

Link to issue: https://github.com/linux-surface/linux-surface/issues/209

References:
- https://github.com/jhand2/surface-camera
  [I referenced GPIO part.]

#### patches dir

You need to apply those patches first for atomisp to work on Surface 3.

#### to build

```bash
make KDIR="/path/to/your/kernel_tree" ATOMISP_INC="drivers/staging/media/atomisp/include"
```

#### atomisp firmware file

You need a firmware file, place it to `/lib/firmware/shisp_2401a0_v21.bin`
You can find one from some places:
- firmware update file for Surface 3 as filename `isp_firmware.bin`, version `irci_stable_bw10p_0518_20150801_0537` (from Microsoft, used on Windows)
- intel-aero/meta-intel-aero-base repo, version `irci_stable_candrpv_0415_20150521_0458` (see "links" section below)

Current atomisp driver warns with the following message about firmware version mismatch:
```bash
atomisp-isp2 0000:00:03.0: Firmware version may not be compatible with this driver
atomisp-isp2 0000:00:03.0: Expecting version 'irci_ecr - master_20150911_0724', but firmware is 'irci_stable_candrpv_0415_20150521_0458'.
```

Take a look at commit [torvalds/linux@33c24f8f](https://github.com/torvalds/linux/commit/33c24f8f5a2716824bb0af959d7eb87c94133cfc).

#### links

You can find atomisp and its sensor drivers for Android (kernel 3.10) on some places:
- https://www.asus.com/us/supportonly/ASUS%20ZenFone%206%20(A600CG)/HelpDesk_Download/
- https://android.googlesource.com/kernel/x86/+/android-x86-grant-3.10-marshmallow-mr1-wear-release/drivers/external_drivers/camera/drivers/media/

Linux kernel for intel-aero also provides atomisp and its sensor drivers (up to linux 4.4):
- https://github.com/intel-aero/linux-kernel/tree/aero-4.4.x

atomisp and its sensor drivers as patch format for Linux v4.4 and atomisp firmware file:
- https://github.com/intel-aero/meta-intel-aero-base/tree/master/recipes-kernel/linux/linux-yocto
