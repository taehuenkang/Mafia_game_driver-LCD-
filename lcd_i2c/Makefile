obj-m := lcd_i2c.o
KDIR := $(HOME)/project_driver/linux
PWD  := $(shell pwd)

all:
	make -C $(KDIR) M=$(PWD) modules

clean:
	make -C $(KDIR) M=$(PWD) clean
