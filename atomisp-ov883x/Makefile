KVERSION := "$(shell uname -r)"

obj-m += atomisp-ov883x.o

all:
	make -C /lib/modules/$(KVERSION)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(KVERSION)/build M=$(PWD) clean
