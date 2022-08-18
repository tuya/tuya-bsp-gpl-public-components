目前uboot 同时支持如下两种启动 kernel 方式：
1.bootz 启动
需要两个文件：1）ak3916ev300.dtb 设备数镜像  2）zimage kernel镜像

通过T卡启动使用如下：
fatload mmc 0 80308000 ak3916ev300.dtb
fatload mmc 0 80008000  zimage
fdt addr 80308000
bootz 80008000 - 80308000

reading zimage
2974136 bytes read in 1 ms (2.8 GiB/s)
Kernel image @ 0x80008000 [ 0x000000 - 0x2d61b8 ]
## Flattened Device Tree blob at 80308000
   Booting using the fdt blob at 0x80308000
cdh:using: FDT
   Loading Device Tree to 8393e000, end 83942e7e ... OK

Starting kernel ...

Uncompressing Linux... done, booting the kernel.
Anyka Linux Kernel Version: ANYKA_VERSION



2.bootm 启动
需要三个文件：1）ak3916ev300.dtb 设备数镜像  2）zimage kernel镜像  3）FIT-uImage.its(语法与DTS一致，目的是生成镜像树文件)
 ./mkimage -f FIT-uImage.its   FIT-uImage.itb(这个镜像用于bootm 启动引导)

通过T卡启动使用如下：
fatload mmc 0 0x82008000(注意地址与zimage 的load address 不能一样)  FIT-uImage.itb
bootm 0x82008000

reading FIT-uImage.itb
2983463 bytes read in 1 ms (2.8 GiB/s)
## Loading kernel from FIT Image at 82008000 ...
   Using 'conf@cloud39e' configuration
   Trying 'kernel@cloud39e' kernel subimage
     Description:  Unify(TODO) Linux kernel for project cloud39e
     Type:         Kernel Image
     Compression:  uncompressed
     Data Start:   0x820080f8
     Data Size:    2974136 Bytes = 2.8 MiB
     Architecture: ARM
     OS:           Linux
     Load Address: 0x80008000
     Entry Point:  0x80008000
   Verifying Hash Integrity ... OK
   Loading Kernel Image ... OK

Starting kernel ...

Uncompressing Linux... done, booting the kernel.
Anyka Linux Kernel Version: ANYKA_VERSION
