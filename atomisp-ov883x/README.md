### ov8830/ov8835 driver, trying to get it working with recent linux kernel

#### build

```bash
# in the root dir of this repo, where Makefile exists
make
```

#### loading the module

```bash
sudo modprobe atomisp_gmin_platform
sudo insmod atomisp-ov883x.ko
```

#### References

Currently, as far as I know, ov8856 is the only upstream driver that supports both DT and ACPI at the same time.
ACPI support is the first, then later added DT support by commit https://github.com/torvalds/linux/commit/0c2c7a1e0d69221b9d489bfd8cf53262d6f82446 ("media: ov8856: Add devicetree support").

Original driver files are from ZenfoneArea/android_kernel_asus_zenfone5 (old driver for android, intended to use with atomisp):
- [linux/modules/camera/drivers/media/i2c/ov8830.c](https://github.com/ZenfoneArea/android_kernel_asus_zenfone5/blob/master/linux/modules/camera/drivers/media/i2c/ov8830.c)
- [linux/modules/camera/drivers/media/i2c/ov8830.h](https://github.com/ZenfoneArea/android_kernel_asus_zenfone5/blob/master/linux/modules/camera/drivers/media/i2c/ov8830.h)
- [linux/modules/camera/drivers/media/i2c/ov8835.h](https://github.com/ZenfoneArea/android_kernel_asus_zenfone5/blob/master/linux/modules/camera/drivers/media/i2c/ov8835.h)
- [linux/kernel/arch/x86/platform/intel\-mid/device\_libs/platform\_ov8830.c](https://github.com/ZenfoneArea/android_kernel_asus_zenfone5/blob/master/linux/kernel/arch/x86/platform/intel-mid/device_libs/platform_ov8830.c)
- [linux/kernel/arch/x86/platform/intel\-mid/device\_libs/platform\_ov8830.h](https://github.com/ZenfoneArea/android_kernel_asus_zenfone5/blob/master/linux/kernel/arch/x86/platform/intel-mid/device_libs/platform_ov8830.h)