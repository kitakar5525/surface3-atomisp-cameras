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
