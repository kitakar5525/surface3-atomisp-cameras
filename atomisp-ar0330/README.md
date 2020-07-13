### ar0330 driver, modded to use with atomisp

#### build

```bash
# in the root dir of this repo, where Makefile exists
make
```

#### loading the module

```bash
sudo modprobe atomisp_gmin_platform
sudo insmod atomisp-ar0330.ko
```

#### References

Currently, as far as I know, ov8856 is the only upstream driver that supports both DT and ACPI at the same time.
ACPI support is the first, then later added DT support by commit https://github.com/torvalds/linux/commit/0c2c7a1e0d69221b9d489bfd8cf53262d6f82446 ("media: ov8856: Add devicetree support").

Original driver files are from rockchip-linux/kernel (driver for v4.4 but not for atomisp):
- [kernel/ar0330.c at develop-4.4 · rockchip-linux/kernel](https://github.com/rockchip-linux/kernel/blob/develop-4.4/drivers/media/i2c/ar0330.c)