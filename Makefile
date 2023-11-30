obj-m += scull.o

TEST = ./main
KERNELDIR = /lib/modules/$(shell uname -r)/build
CFLAGS += -std=с99

all:
	make -C $(KERNELDIR) M=$(PWD) modules
	gcc $(TEST).c -o $(TEST)
	make load
	make create

clean:
	make -C $(KERNELDIR) M=$(PWD) clean
	rm -rf $(TEST)
	make rm

load:
	sudo insmod scull.ko

create:
	sudo mknod /dev/scull0 c $$(cat /proc/devices | awk '/scull/ {print $$1}') 0
	sudo mknod /dev/scull1 c $$(cat /proc/devices | awk '/scull/ {print $$1}') 1
	sudo mknod /dev/scull2 c $$(cat /proc/devices | awk '/scull/ {print $$1}') 2

	sudo chmod 777 /dev/scull0
	sudo chmod 777 /dev/scull1
	sudo chmod 777 /dev/scull2

rm:
	sudo rmmod scull.ko
	sudo rm -rf /dev/scull0
	sudo rm -rf /dev/scull1
	sudo rm -rf /dev/scull2
sign:
	/usr/src/linux-headers-6.2.0-36-generic/scripts/sign-file sha256 ~/keys/key1/MOK.priv ~/keys/key1/MOK.pem ./scull.ko
restart:
	make remove 
	make clean 
	make all 
	make sign 
	make load
	make create
