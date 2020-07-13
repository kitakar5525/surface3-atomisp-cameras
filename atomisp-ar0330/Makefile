KVERSION := "$(shell uname -r)"

obj-m += atomisp-ar0330.o

all:
	make -C /lib/modules/$(KVERSION)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(KVERSION)/build M=$(PWD) clean
