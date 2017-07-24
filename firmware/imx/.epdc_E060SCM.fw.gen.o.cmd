cmd_firmware/imx/epdc_E060SCM.fw.gen.o := arm-linux-gnueabihf-gcc -Wp,-MD,firmware/imx/.epdc_E060SCM.fw.gen.o.d  -nostdinc -isystem /opt/gcc-linaro-arm-linux-gnueabihf-4.9-2014.09_linux/bin/../lib/gcc/arm-linux-gnueabihf/4.9.2/include -I/home/linux/github/kernel_3.10.53/arch/arm/include -Iarch/arm/include/generated  -Iinclude -I/home/linux/github/kernel_3.10.53/arch/arm/include/uapi -Iarch/arm/include/generated/uapi -I/home/linux/github/kernel_3.10.53/include/uapi -Iinclude/generated/uapi -include /home/linux/github/kernel_3.10.53/include/linux/kconfig.h -D__KERNEL__ -mlittle-endian  -D__ASSEMBLY__ -mabi=aapcs-linux -mno-thumb-interwork -funwind-tables -marm -D__LINUX_ARM_ARCH__=7 -march=armv7-a  -include asm/unified.h -msoft-float         -c -o firmware/imx/epdc_E060SCM.fw.gen.o firmware/imx/epdc_E060SCM.fw.gen.S

source_firmware/imx/epdc_E060SCM.fw.gen.o := firmware/imx/epdc_E060SCM.fw.gen.S

deps_firmware/imx/epdc_E060SCM.fw.gen.o := \
  /home/linux/github/kernel_3.10.53/arch/arm/include/asm/unified.h \
    $(wildcard include/config/arm/asm/unified.h) \
    $(wildcard include/config/thumb2/kernel.h) \

firmware/imx/epdc_E060SCM.fw.gen.o: $(deps_firmware/imx/epdc_E060SCM.fw.gen.o)

$(deps_firmware/imx/epdc_E060SCM.fw.gen.o):
