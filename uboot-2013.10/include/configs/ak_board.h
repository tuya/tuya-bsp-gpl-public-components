#ifndef __AK_BOARD__H
#define __AK_BOARD__H

#define CONFIG_MIPI_CORE_BOARD
#define CONFIG_MIPI_CORE_BOARD

#if defined(CONFIG_TUYA_PRODUCT_ZTC400W) || defined(CONFIG_TUYA_PRODUCT_SC082WS03) || defined(CONFIG_TUYA_PRODUCT_SC034WNV3) || defined(CONFIG_TUYA_PRODUCT_SC015WU4) || defined(CONFIG_TUYA_PRODUCT_SC015WU4V2) || defined(CONFIG_TUYA_PRODUCT_SC192W13Z)
#define CONFIG_DRAM_128M
#else
#define CONFIG_DRAM_64M
#endif

/*for spi bus width 4 wires */
/*#define CONFIG_SPI_FLASH_BUSWIDTH_4X*/

/*for RT-thread DTB addr*/
/*#define CONFIG_RTT_DTB_ADDR*/


/*need two boot config*/
/*#define CONFIG_SPL_BOOT*/

/*
the flag is for SPI NOR for SPINAND,
open the flag is SPINAND, close is SPINOR
*/

/*#define CONFIG_SPINAND_FLASH*/

/*
* Enable the double mac
*/
/* #define AK_DOUBLE_ETH_SUPPORT */

#endif/* __AK_BOARD__H */
