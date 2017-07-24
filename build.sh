make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- imx6dl-sabresd-ldo.dtb -j4
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- uImage LOADADDR=0x12000000 -j8 

cp arch/arm/boot/dts/imx6dl-sabresd-ldo.dtb ./
#cp arch/arm/boot/dts/imx6dl-sabresd-ldo.dtb  ../../vendor/imx6dl-sabresd-ldo.dtb
cp arch/arm/boot/uImage ./
#cp arch/arm/boot/uImage ../../vendor/uImage
