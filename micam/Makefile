KVERSION := "$(shell uname -r)"
KDIR := "/lib/modules/$(KVERSION)/build"
ATOMISP_INC := "drivers/staging/media/atomisp/include"
ccflags-y += -I $(KDIR)/$(ATOMISP_INC)

obj-m += atomisp-ov5693.o
obj-m += atomisp-t4ka3drv.o

atomisp-t4ka3drv-objs := atomisp-t4ka3.o dw9761.o

all:
	make -C $(KDIR) M=$(PWD) modules

clean:
	make -C $(KDIR) M=$(PWD) clean
