KVERSION := "$(shell uname -r)"

obj-m += atomisp-ov5693.o

all:
	make -C /lib/modules/$(KVERSION)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(KVERSION)/build M=$(PWD) clean
