KVERSION := "$(shell uname -r)"
KDIR := "/lib/modules/$(KVERSION)/build"
ATOMISP_INC := "drivers/staging/media/atomisp/include"
ccflags-y += -I $(KDIR)/$(ATOMISP_INC)

obj-m += atomisp-ar0330_from_tegra/atomisp-ar0330-from-tegra.o

all:
	make -C $(KDIR) M=$(PWD) modules

clean:
	make -C $(KDIR) M=$(PWD) clean
