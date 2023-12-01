obj-m += scull.o

TEST = ./main
KERNELDIR = /lib/modules/$(shell uname -r)/build
CFLAGS += -std=с99
KBUILD_CFLAGS   +=  -g -Wall

all:
	make -C $(KERNELDIR) M=$(shell pwd) modules
	gcc $(TEST).c -o $(TEST)
	make load
	make create

clean:
	make -C $(KERNELDIR) M=$(shell pwd) clean
	rm -rf $(TEST)
	make rm

load:
	sudo insmod scull.ko

create:
	sudo mknod /dev/scull0 c $$(cat /proc/devices | awk '/scull/ {print $$1}') 0

	sudo chmod 777 /dev/scull0

rm:
	sudo rmmod scull.ko
	sudo rm /dev/scull0
