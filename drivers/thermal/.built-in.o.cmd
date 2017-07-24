cmd_drivers/thermal/built-in.o :=  arm-linux-gnueabihf-ld -EL    -r -o drivers/thermal/built-in.o drivers/thermal/thermal_sys.o drivers/thermal/imx_thermal.o drivers/thermal/device_cooling.o 
