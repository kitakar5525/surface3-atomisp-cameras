#### Trying to get atomisp cameras working on Surface 3

Link to issue: https://github.com/linux-surface/linux-surface/issues/209

#### atomisp pci driver

Take a look at my kernel repo. The upstreamed atomisp is currently not
working.

Patches for atomisp (also contain sensor drivers):
- https://github.com/kitakar5525/linux-kernel/commits/mainline+upst_atomisp

Patches for kernel core (like regulators) (also contains patches unrelated
to atomisp, maybe I need to create dedicated branch for atomisp later):
- https://github.com/kitakar5525/linux-kernel/commits/DEBUG/mainline+surface+k5

#### to build sensor drivers

Sensor drivers are already included in my kernel tree mentioned above,
but if you want to build drivers here:

```bash
make KDIR="/path/to/your/kernel_tree" ATOMISP_INC="drivers/staging/media/atomisp/include"
```

#### loading out-of-tree sensor modules

Current atomisp driver requires you to load sensor modules _before_
atomisp main pci driver.

Here is the example:

```bash
# unload atomisp pci driver
sudo rmmod atomisp

# load drivers needed for atomisp first for insmod
# for sensor drivers
sudo modprobe media # needed for older LTS
sudo modprobe videodev
sudo modprobe v4l2_common # needed for older LTS
sudo modprobe v4l2_async # if using async_register
# for atomisp
sudo modprobe videobuf-core
sudo modprobe videobuf-vmalloc

# load atomisp drivers
sudo insmod atomisp-ar0330.ko
sudo insmod atomisp-ov883x.ko
# IIRC, modprobe works but try insmod instead if weird
sudo modprobe atomisp dbg_level=1 #dyndbg
```

#### atomisp firmware file

You need a firmware file, place it to `/lib/firmware/shisp_2401a0_v21.bin`
The firmware version should be `irci_stable_candrpv_0415_20150521_0458`.
You can download it from intel-aero (https://github.com/intel-aero/meta-intel-aero-base/tree/master/recipes-kernel/linux/linux-yocto)

Just in case, the version and hash of firmware which I downloaded from
intel-aero is the following:

```bash
$ strings /lib/firmware/shisp_2401a0_v21.bin | grep 2015
irci_stable_candrpv_0415_20150521_0458

$ sha256sum /lib/firmware/shisp_2401a0_v21.bin
e89359f4e4934c410c83d525e283f34c5fcce9cb5caa75ad8a32d66d3842d95c  /lib/firmware/shisp_2401a0_v21.bin
```

Note that the other version of firmware may not work.

#### links

You can find atomisp and its sensor drivers for Android (kernel 3.10) on some places:
- https://www.asus.com/us/supportonly/ASUS%20ZenFone%206%20(A600CG)/HelpDesk_Download/
- https://android.googlesource.com/kernel/x86/+/android-x86-grant-3.10-marshmallow-mr1-wear-release/drivers/external_drivers/camera/drivers/media/

Linux kernel for intel-aero also provides atomisp and its sensor drivers (up to linux 4.4):
- https://github.com/intel-aero/linux-kernel/tree/aero-4.4.x

atomisp and its sensor drivers as patch format for Linux v4.4 and atomisp firmware file:
- https://github.com/intel-aero/meta-intel-aero-base/tree/master/recipes-kernel/linux/linux-yocto
