#! /bin/bash

export ARCH=arm
export CROSS_COMPILE=/home/skynet/buildroot/output/host/usr/bin/arm-linux-

cpus=`grep -c '^processor' /proc/cpuinfo`
jobs=`expr $cpus + 2`

if [ $# == 0 ]; then
	make -j $jobs
	exit
fi

function savedefconfig()
{
	make savedefconfig
	mv defconfig arch/arm/configs/imx_v6_v7_defconfig
}

case $1 in
	"menuconfig") make menuconfig;;
	"defconfig") make imx_v6_v7_defconfig;;
	"savedefconfig") savedefconfig;;
	"cscope") make cscope;;
	"modules_install") make modules_install INSTALL_MOD_PATH=/home/skynet/rootfs;;
	"clean") make clean;;
	"distclean") make distclean;;
	*) echo "Unknown args $1";;
esac
