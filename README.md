#### Trying to get atomisp cameras working on Surface 3

Link to issue: https://github.com/linux-surface/linux-surface/issues/209

References:
- https://github.com/jhand2/surface-camera
  [I referenced GPIO part.]

#### patches dir

You need to apply those patches first for atomisp to work on Surface 3.

#### links

You can find atomisp and its sensor drivers for Android (kernel 3.10) on some places:
- https://www.asus.com/us/supportonly/ASUS%20ZenFone%206%20(A600CG)/HelpDesk_Download/
- https://android.googlesource.com/kernel/x86/+/android-x86-grant-3.10-marshmallow-mr1-wear-release/drivers/external_drivers/camera/drivers/media/

Linux kernel for intel-aero also provides atomisp and its sensor drivers (up to linux 4.4):
- https://github.com/intel-aero/linux-kernel/tree/aero-4.4.x
