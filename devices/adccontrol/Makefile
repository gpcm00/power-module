MODULE_NAME=adcmodul
obj-m += $(MODULE_NAME).o

all: modules dt
	echo Builded Device Tree Overlay and kernel module

modules:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
dt: $(MODULE_NAME).dts
	dtc -@ -I dts -O dtb -o $(MODULE_NAME).dtbo $(MODULE_NAME).dts
clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	rm -rf $(MODULE_NAME).dtbo
	
