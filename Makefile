obj-m += joystick_gpio_driver.o

all:
	make -C /lib/modules/6.1.21-v8+/build M=$(PWD) modules

clean:
	make -C /lib/modules/6.1.21-v8+/build M=$(PWD) clean