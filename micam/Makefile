KVERSION := "$(shell uname -r)"

obj-m += atomisp-ov5693.o
obj-m += atomisp-t4ka3drv.o

atomisp-t4ka3drv-objs := atomisp-t4ka3.o dw9761.o

all:
	make -C /lib/modules/$(KVERSION)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(KVERSION)/build M=$(PWD) clean
