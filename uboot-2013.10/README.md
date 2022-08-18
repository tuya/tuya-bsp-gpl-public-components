# U-Boot 2013.10.0 #

## 版本说明 ##

* 支持V500/37D和h3b芯片
* 支持spiflash启动
* 支持从TF卡、以太网tftp、串口下载镜像和升级镜像
* 支持uboot自升级
* 支持NFS调试
* 支持DTS，同内核共用一份dtb
* 支持标准分区
* 后续支持
    1. _支持spinand_  
    2. _SPL分级加载_
    3. _falcon快速启动_
    4. 支持标准spi nand坏块管理算法驱动

## 使用说明 ##

    make O=/tmp/build distclean
    make O=/tmp/build NAME_config
    make O=/tmp/build all
或者

    make distclean
    make NAME_config #针对V500 spiflash的配置
    make all
平台ak37d编译使用说明：
    make O=../ubd distclean
    make O=../ubd anycloud_ak37d_config
    make O=../ubd all

平台ak39ev33x编译使用说明：
    make O=../ubd distclean
    make O=../ubd anycloud_ak39ev33x_config
    make O=../ubd all


