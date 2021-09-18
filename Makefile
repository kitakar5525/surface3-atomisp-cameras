KVERSION := "$(shell uname -r)"
KDIR := "/lib/modules/$(KVERSION)/build"
ATOMISP_INC := "drivers/staging/media/atomisp/include"
ccflags-y += -I $(KDIR)/$(ATOMISP_INC)

obj-m += atomisp-ar0330/atomisp-ar0330.o
obj-m += atomisp-ov883x/atomisp-ov883x.o

obj-y += micam/

all:
	make -C $(KDIR) M=$(PWD) modules

clean:
	make -C $(KDIR) M=$(PWD) clean
