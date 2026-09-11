obj-m += bsp_gpio_consumer.o

all:
	$(MAKE) -C ../../../linux-6.12 ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- M=$(CURDIR) modules

clean:
	$(MAKE) -C ../../../linux-6.12 M=$(CURDIR) clean
