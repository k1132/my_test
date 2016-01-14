KERN_DIR =  /work/NUC970BSP/Install/nuc970bsp/linux-3.10.x/

all:
	make -C $(KERN_DIR) M=`pwd` modules 

clean:
	make -C $(KERN_DIR) M=`pwd` modules clean
	rm -rf modules.order

#obj-m	+= weigen.o
obj-m	+= adc_drv.o

