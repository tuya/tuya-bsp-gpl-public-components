#ifndef __BMI088_H__
#define __BMI088_H__

#include <linux/types.h>
#include <linux/kernel.h>

#define DEVICE_NAME				"bmi088"
#define I2C_RETRY_CNT			(3)
#define MAX_DATA_SIZE			(32)

#define BMI088_DISABLE_ALL		(0x0)
#define BMI088_ACC_ENABLE		(0x1)
#define BMI088_GYR_ENABLE		(0x2)
#define BMI088_ACCGYR_ENABLE	(BMI088_ACC_ENABLE | BMI088_GYR_ENABLE)

#define ABSMIN_ACC				(-32768)
#define ABSMAX_ACC				(32767)
#define ABSMIN_GYRO				(-32768)
#define ABSMAX_GYRO				(32767)


/*************************** BMI08 Accelerometer Macros *****************************/

/** Register map */
/* Accel registers */

/**\name    Accel Chip Id register */
#define BMI08X_REG_ACCEL_CHIP_ID                 (0x00)

/**\name    Accel Error condition register */
#define BMI08X_REG_ACCEL_ERR                     (0x02)

/**\name    Accel Status flag register */
#define BMI08X_REG_ACCEL_STATUS                  (0x03)

/**\name    Accel X LSB data register */
#define BMI08X_REG_ACCEL_X_LSB                   (0x12)

/**\name    Accel X MSB data register */
#define BMI08X_REG_ACCEL_X_MSB                   (0x13)

/**\name    Accel Y LSB data register */
#define BMI08X_REG_ACCEL_Y_LSB                   (0x14)

/**\name    Accel Y MSB data register */
#define BMI08X_REG_ACCEL_Y_MSB                   (0x15)

/**\name    Accel Z LSB data register */
#define BMI08X_REG_ACCEL_Z_LSB                   (0x16)

/**\name    Accel Z MSB data register */
#define BMI08X_REG_ACCEL_Z_MSB                   (0x17)

/**\name    Sensor time byte 0 register */
#define BMI08X_REG_ACCEL_SENSORTIME_0            (0x18)

/**\name    Sensor time byte 1 register */
#define BMI08X_REG_ACCEL_SENSORTIME_1            (0x19)

/**\name    Sensor time byte 2 register */
#define BMI08X_REG_ACCEL_SENSORTIME_2            (0x1A)

/**\name    Accel Interrupt status0 register */
#define BMI08X_REG_ACCEL_INT_STAT_0              (0x1C)

/**\name    Accel Interrupt status1 register */
#define BMI08X_REG_ACCEL_INT_STAT_1              (0x1D)

/**\name    Accel general purpose register 0*/
#define BMI08X_REG_ACCEL_GP_0                    (0x1E)

/**\name    Sensor temperature MSB data register */
#define BMI08X_REG_TEMP_MSB                      (0x22)

/**\name    Sensor temperature LSB data register */
#define BMI08X_REG_TEMP_LSB                      (0x23)

/**\name    Accel general purpose register 4*/
#define BMI08X_REG_ACCEL_GP_4                    (0x27)

/**\name    Accel Internal status register */
#define BMI08X_REG_ACCEL_INTERNAL_STAT           (0x2A)

/**\name    Accel configuration register */
#define BMI08X_REG_ACCEL_CONF                    (0x40)

/**\name    Accel range setting register */
#define BMI08X_REG_ACCEL_RANGE                   (0x41)

/**\name    Accel Interrupt pin 1 configuration register */
#define BMI08X_REG_ACCEL_INT1_IO_CONF            (0x53)

/**\name    Accel Interrupt pin 2 configuration register */
#define BMI08X_REG_ACCEL_INT2_IO_CONF            (0x54)

/**\name    Accel Interrupt latch configuration register */
#define BMI08X_REG_ACCEL_INT_LATCH_CONF          (0x55)

/**\name    Accel Interrupt pin1 mapping register */
#define BMI08X_REG_ACCEL_INT1_MAP                (0x56)

/**\name    Accel Interrupt pin2 mapping register */
#define BMI08X_REG_ACCEL_INT2_MAP                (0x57)

/**\name    Accel Interrupt map register */
#define BMI08X_REG_ACCEL_INT1_INT2_MAP_DATA      (0x58)

/**\name    Accel Init control register */
#define BMI08X_REG_ACCEL_INIT_CTRL               (0x59)

/**\name    Accel Self test register */
#define BMI08X_REG_ACCEL_SELF_TEST               (0x6D)

/**\name    Accel Power mode configuration register */
#define BMI08X_REG_ACCEL_PWR_CONF                (0x7C)

/**\name    Accel Power control (switch on or off ) register */
#define BMI08X_REG_ACCEL_PWR_CTRL                (0x7D)

/**\name    Accel Soft reset register */
#define BMI08X_REG_ACCEL_SOFTRESET               (0x7E)

/**\name    Feature Config related Registers */
#define BMI08X_REG_ACCEL_RESERVED_5B             (0x5B)
#define BMI08X_REG_ACCEL_RESERVED_5C             (0x5C)
#define BMI08X_REG_ACCEL_FEATURE_CFG             (0x5E)

/**\name    BMI085 Accel unique chip identifier */
#define BMI085_ACCEL_CHIP_ID                     (0x1F)

/**\name    BMI088 Accel unique chip identifier */
#define BMI088_ACCEL_CHIP_ID                     (0x1E)

/**\name    Accel I2C slave address */
#define BMI08X_ACCEL_I2C_ADDR_PRIMARY            (0x18)
#define BMI08X_ACCEL_I2C_ADDR_SECONDARY          (0x19)

/**\name    Interrupt masks */
#define BMI08X_ACCEL_DATA_READY_INT              (0x80)
#define BMI08X_ACCEL_FIFO_WM_INT                 (0x02)
#define BMI08X_ACCEL_FIFO_FULL_INT               (0x01)

#define BMI08X_GYRO_DATA_READY_INT               (0x80)
#define BMI08X_GYRO_FIFO_WM_INT                  (0x10)
#define BMI08X_GYRO_FIFO_FULL_INT                (0x10)

/**\name    Accel Bandwidth */
#define BMI08X_ACCEL_BW_OSR4                     (0x00)
#define BMI08X_ACCEL_BW_OSR2                     (0x01)
#define BMI08X_ACCEL_BW_NORMAL                   (0x02)

/**\name    BMI085 Accel Range */
#define BMI085_ACCEL_RANGE_2G                    (0x00)
#define BMI085_ACCEL_RANGE_4G                    (0x01)
#define BMI085_ACCEL_RANGE_8G                    (0x02)
#define BMI085_ACCEL_RANGE_16G                   (0x03)

/**\name  BMI088 Accel Range */
#define BMI088_ACCEL_RANGE_3G                    (0x00)
#define BMI088_ACCEL_RANGE_6G                    (0x01)
#define BMI088_ACCEL_RANGE_12G                   (0x02)
#define BMI088_ACCEL_RANGE_24G                   (0x03)

/**\name    Accel Output data rate */
#define BMI08X_ACCEL_ODR_12_5_HZ                 (0x05)
#define BMI08X_ACCEL_ODR_25_HZ                   (0x06)
#define BMI08X_ACCEL_ODR_50_HZ                   (0x07)
#define BMI08X_ACCEL_ODR_100_HZ                  (0x08)
#define BMI08X_ACCEL_ODR_200_HZ                  (0x09)
#define BMI08X_ACCEL_ODR_400_HZ                  (0x0A)
#define BMI08X_ACCEL_ODR_800_HZ                  (0x0B)
#define BMI08X_ACCEL_ODR_1600_HZ                 (0x0C)

/**\name    Accel Self test */
#define BMI08X_ACCEL_SWITCH_OFF_SELF_TEST        (0x00)
#define BMI08X_ACCEL_POSITIVE_SELF_TEST          (0x0D)
#define BMI08X_ACCEL_NEGATIVE_SELF_TEST          (0x09)

/**\name    Accel Power mode */
#define BMI08X_ACCEL_PM_ACTIVE                   (0x00)
#define BMI08X_ACCEL_PM_SUSPEND                  (0x03)

/**\name    Accel Power control settings */
#define BMI08X_ACCEL_POWER_DISABLE               (0x00)
#define BMI08X_ACCEL_POWER_ENABLE                (0x04)

/**\name    Accel internal interrupt pin mapping */
#define BMI08X_ACCEL_INTA_DISABLE                (0x00)
#define BMI08X_ACCEL_INTA_ENABLE                 (0x01)
#define BMI08X_ACCEL_INTB_DISABLE                (0x00)
#define BMI08X_ACCEL_INTB_ENABLE                 (0x02)
#define BMI08X_ACCEL_INTC_DISABLE                (0x00)
#define BMI08X_ACCEL_INTC_ENABLE                 (0x04)

/**\name    Accel Soft reset delay */
#define BMI08X_ACCEL_SOFTRESET_DELAY_MS          (1)

/**\name    Mask definitions for ACCEL_ERR_REG register */
#define BMI08X_FATAL_ERR_MASK                    (0x01)
#define BMI08X_ERR_CODE_MASK                     (0x1C)

/**\name    Position definitions for ACCEL_ERR_REG register */
#define BMI08X_CMD_ERR_POS                       (1)
#define BMI08X_ERR_CODE_POS                      (2)

/**\name    Mask definition for ACCEL_STATUS_REG register */
#define BMI08X_ACCEL_STATUS_MASK                 (0x80)

/**\name    Position definitions for ACCEL_STATUS_REG  */
#define BMI08X_ACCEL_STATUS_POS                  (7)

/**\name    Mask definitions for odr, bandwidth and range */
#define BMI08X_ACCEL_ODR_MASK                    (0x0F)
#define BMI08X_ACCEL_BW_MASK                     (0x70)
#define BMI08X_ACCEL_RANGE_MASK                  (0x03)

/**\name    Position definitions for odr, bandwidth and range */
#define BMI08X_ACCEL_BW_POS                      (4)

/**\name    Mask definitions for INT1_IO_CONF register */
#define BMI08X_ACCEL_INT_EDGE_MASK               (0x01)
#define BMI08X_ACCEL_INT_LVL_MASK                (0x02)
#define BMI08X_ACCEL_INT_OD_MASK                 (0x04)
#define BMI08X_ACCEL_INT_IO_MASK                 (0x08)
#define BMI08X_ACCEL_INT_IN_MASK                 (0x10)

/**\name    Position definitions for INT1_IO_CONF register */
#define BMI08X_ACCEL_INT_EDGE_POS                (0)
#define BMI08X_ACCEL_INT_LVL_POS                 (1)
#define BMI08X_ACCEL_INT_OD_POS                  (2)
#define BMI08X_ACCEL_INT_IO_POS                  (3)
#define BMI08X_ACCEL_INT_IN_POS                  (4)

/**\name    Mask definitions for INT1/INT2 mapping register */
#define BMI08X_ACCEL_MAP_INTA_MASK               (0x01)

/**\name    Mask definitions for INT1/INT2 mapping register */
#define BMI08X_ACCEL_MAP_INTA_POS                (0x00)

/**\name    Mask definitions for INT1_INT2_MAP_DATA register */
#define BMI08X_ACCEL_INT1_DRDY_MASK              (0x04)
#define BMI08X_ACCEL_INT2_DRDY_MASK              (0x40)

/**\name    Position definitions for INT1_INT2_MAP_DATA register */
#define BMI08X_ACCEL_INT1_DRDY_POS               (2)
#define BMI08X_ACCEL_INT2_DRDY_POS               (6)

/**\name    Asic Initialization value */
#define BMI08X_ASIC_INITIALIZED                  (0x01)

/*************************** BMI08 Gyroscope Macros *****************************/
/** Register map */
/* Gyro registers */

/**\name    Gyro Chip Id register */
#define BMI08X_REG_GYRO_CHIP_ID                  (0x00)

/**\name    Gyro X LSB data register */
#define BMI08X_REG_GYRO_X_LSB                    (0x02)

/**\name    Gyro X MSB data register */
#define BMI08X_REG_GYRO_X_MSB                    (0x03)

/**\name    Gyro Y LSB data register */
#define BMI08X_REG_GYRO_Y_LSB                    (0x04)

/**\name    Gyro Y MSB data register */
#define BMI08X_REG_GYRO_Y_MSB                    (0x05)

/**\name    Gyro Z LSB data register */
#define BMI08X_REG_GYRO_Z_LSB                    (0x06)

/**\name    Gyro Z MSB data register */
#define BMI08X_REG_GYRO_Z_MSB                    (0x07)

/**\name    Gyro Interrupt status register */
#define BMI08X_REG_GYRO_INT_STAT_1               (0x0A)

/**\name    Gyro FIFO status register */
#define BMI08X_REG_GYRO_FIFO_STATUS              (0x0E)

/**\name    Gyro Range register */
#define BMI08X_REG_GYRO_RANGE                    (0x0F)

/**\name    Gyro Bandwidth register */
#define BMI08X_REG_GYRO_BANDWIDTH                (0x10)

/**\name    Gyro Power register */
#define BMI08X_REG_GYRO_LPM1                     (0x11)

/**\name    Gyro Soft reset register */
#define BMI08X_REG_GYRO_SOFTRESET                (0x14)

/**\name    Gyro Interrupt control register */
#define BMI08X_REG_GYRO_INT_CTRL                 (0x15)

/**\name    Gyro Interrupt Pin configuration register */
#define BMI08X_REG_GYRO_INT3_INT4_IO_CONF        (0x16)

/**\name    Gyro Interrupt Map register */
#define BMI08X_REG_GYRO_INT3_INT4_IO_MAP         (0x18)

/**\name    Gyro FIFO watermark enable register */
#define BMI08X_REG_GYRO_FIFO_WM_ENABLE           (0x1E)

/**\name    Gyro Self test register */
#define BMI08X_REG_GYRO_SELF_TEST                (0x3C)

/**\name    Gyro Fifo Config 0 register */
#define BMI08X_REG_GYRO_FIFO_CONFIG0             (0x3D)

/**\name    Gyro Fifo Config 1 register */
#define BMI08X_REG_GYRO_FIFO_CONFIG1             (0x3E)

/**\name    Gyro Fifo Data register */
#define BMI08X_REG_GYRO_FIFO_DATA                (0x3F)

/**\name    Gyro unique chip identifier */
#define BMI08X_GYRO_CHIP_ID                      (0x0F)

/**\name    Gyro I2C slave address */
#define BMI08X_GYRO_I2C_ADDR_PRIMARY             (0x68)
#define BMI08X_GYRO_I2C_ADDR_SECONDARY           (0x69)

/**\name    Gyro Range */
#define BMI08X_GYRO_RANGE_2000_DPS               (0x00)
#define BMI08X_GYRO_RANGE_1000_DPS               (0x01)
#define BMI08X_GYRO_RANGE_500_DPS                (0x02)
#define BMI08X_GYRO_RANGE_250_DPS                (0x03)
#define BMI08X_GYRO_RANGE_125_DPS                (0x04)

/**\name    Gyro Output data rate and bandwidth */
#define BMI08X_GYRO_BW_532_ODR_2000_HZ           (0x00)
#define BMI08X_GYRO_BW_230_ODR_2000_HZ           (0x01)
#define BMI08X_GYRO_BW_116_ODR_1000_HZ           (0x02)
#define BMI08X_GYRO_BW_47_ODR_400_HZ             (0x03)
#define BMI08X_GYRO_BW_23_ODR_200_HZ             (0x04)
#define BMI08X_GYRO_BW_12_ODR_100_HZ             (0x05)
#define BMI08X_GYRO_BW_64_ODR_200_HZ             (0x06)
#define BMI08X_GYRO_BW_32_ODR_100_HZ             (0x07)
#define BMI08X_GYRO_ODR_RESET_VAL                (0x80)

/**\name    Gyro Power mode */
#define BMI08X_GYRO_PM_NORMAL                    (0x00)
#define BMI08X_GYRO_PM_DEEP_SUSPEND              (0x20)
#define BMI08X_GYRO_PM_SUSPEND                   (0x80)

/**\name    Gyro data ready interrupt enable value */
#define BMI08X_GYRO_DRDY_INT_DISABLE_VAL         (0x00)
#define BMI08X_GYRO_DRDY_INT_ENABLE_VAL          (0x80)
#define BMI08X_GYRO_FIFO_INT_DISABLE_VAL         (0x00)
#define BMI08X_GYRO_FIFO_INT_ENABLE_VAL          (0x40)
#define BMI08X_GYRO_FIFO_WM_ENABLE_VAL           (0x80)
#define BMI08X_GYRO_FIFO_WM_DISABLE_VAL          (0x00)

/**\name    Gyro data ready map values */
#define BMI08X_GYRO_MAP_DRDY_TO_INT3             (0x01)
#define BMI08X_GYRO_MAP_DRDY_TO_INT4             (0x80)
#define BMI08X_GYRO_MAP_DRDY_TO_BOTH_INT3_INT4   (0x81)
#define BMI08X_GYRO_MAP_FIFO_INT3                (0x04)
#define BMI08X_GYRO_MAP_FIFO_INT4                (0x20)
#define BMI08X_GYRO_MAP_FIFO_BOTH_INT3_INT4      (0x24)

/**\name    Gyro Soft reset delay */
#define BMI08X_GYRO_SOFTRESET_DELAY              (30)

/**\name    Gyro power mode config delay */
#define BMI08X_GYRO_POWER_MODE_CONFIG_DELAY      (30)

/** Mask definitions for range, bandwidth and power */
#define BMI08X_GYRO_RANGE_MASK                   (0x07)
#define BMI08X_GYRO_BW_MASK                      (0x0F)
#define BMI08X_GYRO_POWER_MASK                   (0xA0)

/** Position definitions for range, bandwidth and power */
#define BMI08X_GYRO_POWER_POS                    (5)

/**\name    Mask definitions for BMI08X_GYRO_INT_CTRL_REG register */
#define BMI08X_GYRO_DATA_EN_MASK                 (0x80)

/**\name    Position definitions for BMI08X_GYRO_INT_CTRL_REG register */
#define BMI08X_GYRO_DATA_EN_POS                  (7)

/**\name    Mask definitions for BMI08X_GYRO_INT3_INT4_IO_CONF_REG register */
#define BMI08X_GYRO_INT3_LVL_MASK                (0x01)
#define BMI08X_GYRO_INT3_OD_MASK                 (0x02)
#define BMI08X_GYRO_INT4_LVL_MASK                (0x04)
#define BMI08X_GYRO_INT4_OD_MASK                 (0x08)

/**\name    Position definitions for BMI08X_GYRO_INT3_INT4_IO_CONF_REG register */
#define BMI08X_GYRO_INT3_OD_POS                  (1)
#define BMI08X_GYRO_INT4_LVL_POS                 (2)
#define BMI08X_GYRO_INT4_OD_POS                  (3)

/**\name    Mask definitions for BMI08X_GYRO_INT_EN_REG register */
#define BMI08X_GYRO_INT_EN_MASK                  (0x80)

/**\name    Position definitions for BMI08X_GYRO_INT_EN_REG register */
#define BMI08X_GYRO_INT_EN_POS                   (7)

/**\name    Mask definitions for BMI088_GYRO_INT_MAP_REG register */
#define BMI08X_GYRO_INT3_MAP_MASK                (0x01)
#define BMI08X_GYRO_INT4_MAP_MASK                (0x80)

/**\name    Position definitions for BMI088_GYRO_INT_MAP_REG register */
#define BMI08X_GYRO_INT3_MAP_POS                 (0)
#define BMI08X_GYRO_INT4_MAP_POS                 (7)

/**\name    Mask definitions for BMI088_GYRO_INT_MAP_REG register */
#define BMI088_GYRO_INT3_MAP_MASK                (0x01)
#define BMI088_GYRO_INT4_MAP_MASK                (0x80)

/**\name    Position definitions for BMI088_GYRO_INT_MAP_REG register */
#define BMI088_GYRO_INT3_MAP_POS                 (0)
#define BMI088_GYRO_INT4_MAP_POS                 (7)

/**\name    Mask definitions for GYRO_SELF_TEST register */
#define BMI08X_GYRO_SELF_TEST_EN_MASK            (0x01)
#define BMI08X_GYRO_SELF_TEST_RDY_MASK           (0x02)
#define BMI08X_GYRO_SELF_TEST_RESULT_MASK        (0x04)
#define BMI08X_GYRO_SELF_TEST_FUNCTION_MASK      (0x08)

/**\name    Position definitions for GYRO_SELF_TEST register */
#define BMI08X_GYRO_SELF_TEST_RDY_POS            (1)
#define BMI08X_GYRO_SELF_TEST_RESULT_POS         (2)
#define BMI08X_GYRO_SELF_TEST_FUNCTION_POS       (3)

/**\name    Gyro Fifo configurations */
#define BMI08X_GYRO_FIFO_OVERRUN_MASK            (0x80)
#define BMI08X_GYRO_FIFO_OVERRUN_POS             (0x07)
#define BMI08X_GYRO_FIFO_MODE_MASK               (0xC0)
#define BMI08X_GYRO_FIFO_MODE_POS                (0x06)
#define BMI08X_GYRO_FIFO_TAG_MASK                (0x80)
#define BMI08X_GYRO_FIFO_TAG_POS                 (0x07)
#define BMI08X_GYRO_FIFO_DATA_SELECT_MASK        (0x03)
#define BMI08X_GYRO_FIFO_FRAME_COUNT_MASK        (0x7F)
#define BMI08X_GYRO_FIFO_WM_LEVEL_MASK           (0x7F)

/*! @name Gyro Fifo interrupt map */
#define BMI08X_GYRO_FIFO_INT3_MASK               (0x04)
#define BMI08X_GYRO_FIFO_INT3_POS                (0x02)
#define BMI08X_GYRO_FIFO_INT4_MASK               (0x20)
#define BMI08X_GYRO_FIFO_INT4_POS                (0x05)

/**\name   Gyro FIFO definitions */
#define BMI08X_GYRO_FIFO_TAG_ENABLED             (0x01)
#define BMI08X_GYRO_FIFO_TAG_DISABLED            (0x00)
#define BMI08X_GYRO_FIFO_MODE_BYPASS             (0x00)
#define BMI08X_GYRO_FIFO_MODE                    (0x01)
#define BMI08X_GYRO_FIFO_MODE_STREAM             (0x02)
#define BMI08X_GYRO_FIFO_XYZ_AXIS_ENABLED        (0x00)
#define BMI08X_GYRO_FIFO_X_AXIS_ENABLED          (0x01)
#define BMI08X_GYRO_FIFO_Y_AXIS_ENABLED          (0x02)
#define BMI08X_GYRO_FIFO_Z_AXIS_ENABLED          (0x03)
#define BMI08X_GYRO_FIFO_XYZ_AXIS_FRAME_SIZE     (0x06)
#define BMI08X_GYRO_FIFO_SINGLE_AXIS_FRAME_SIZE  (0x02)
#define BMI08X_GYRO_FIFO_1KB_BUFFER              (1024)

/*************************** Common Macros for both Accel and Gyro *****************************/
/**\name    SPI read/write mask to configure address */
#define BMI08X_SPI_RD_MASK                       (0x80)
#define BMI08X_SPI_WR_MASK                       (0x7F)

/**\name API success code */
#define BMI08X_OK                                (0)

/**\name API error codes */
#define BMI08X_E_NULL_PTR                        (-1)
#define BMI08X_E_COM_FAIL                        (-2)
#define BMI08X_E_DEV_NOT_FOUND                   (-3)
#define BMI08X_E_OUT_OF_RANGE                    (-4)
#define BMI08X_E_INVALID_INPUT                   (-5)
#define BMI08X_E_CONFIG_STREAM_ERROR             (-6)
#define BMI08X_E_RD_WR_LENGTH_INVALID            (-7)
#define BMI08X_E_INVALID_CONFIG                  (-8)
#define BMI08X_E_FEATURE_NOT_SUPPORTED           (-9)

/**\name API warning codes */
#define BMI08X_W_SELF_TEST_FAIL                  (1)

/***\name    Soft reset Value */
#define BMI08X_SOFT_RESET_CMD                    (0xB6)

/**\name    Enable/disable macros */
#define BMI08X_DISABLE                           (0)
#define BMI08X_ENABLE                            (1)

/*! @name To define warnings for FIFO activity */
#define BMI08X_W_FIFO_EMPTY                      (1)
#define BMI08X_W_PARTIAL_READ                    (2)

/**\name    Constant values macros */
#define BMI08X_SENSOR_DATA_SYNC_TIME_MS          (1)
#define BMI08X_DELAY_BETWEEN_WRITES_MS           (1)
#define BMI08X_SELF_TEST_DELAY_MS                (3)
#define BMI08X_POWER_CONFIG_DELAY                (5)
#define BMI08X_SENSOR_SETTLE_TIME_MS             (30)
#define BMI08X_SELF_TEST_DATA_READ_MS            (50)
#define BMI08X_ASIC_INIT_TIME_MS                 (500)

#define BMI08X_CONFIG_STREAM_SIZE                (6144)

/**\name    Sensor time array parameter definitions */
#define BMI08X_SENSOR_TIME_MSB_BYTE              (2)
#define BMI08X_SENSOR_TIME_XLSB_BYTE             (1)
#define BMI08X_SENSOR_TIME_LSB_BYTE              (0)

/**\name   int pin active state */
#define BMI08X_INT_ACTIVE_LOW                    (0)
#define BMI08X_INT_ACTIVE_HIGH                   (1)

/**\name   interrupt pin output definition  */
#define BMI08X_INT_MODE_PUSH_PULL                (0)
#define BMI08X_INT_MODE_OPEN_DRAIN               (1)

/**\name    Sensor bit resolution */
#define BMI08X_16_BIT_RESOLUTION                 (16)

/**\name    Convert milliseconds to microseconds */
#define BMI08X_MS_TO_US(X)                       (X * 1000)

/*********************************BMI08X FIFO Macros**********************************/
/** Register map */
/*! @name FIFO Header Mask definitions */
#define BMI08X_FIFO_HEADER_ACC_FRM               (0x84)
#define BMI08X_FIFO_HEADER_ALL_FRM               (0x9C)
#define BMI08X_FIFO_HEADER_SENS_TIME_FRM         (0x44)
#define BMI08X_FIFO_HEADER_SKIP_FRM              (0x40)
#define BMI08X_FIFO_HEADER_INPUT_CFG_FRM         (0x48)
#define BMI08X_FIFO_HEAD_OVER_READ_MSB           (0x80)
#define BMI08X_FIFO_SAMPLE_DROP_FRM              (0x50)

/* Accel registers */
#define BMI08X_FIFO_LENGTH_0_ADDR                (0x24)
#define BMI08X_FIFO_LENGTH_1_ADDR                (0x25)
#define BMI08X_FIFO_DATA_ADDR                    (0x26)
#define BMI08X_FIFO_DOWNS_ADDR                   (0x45)
#define BMI08X_FIFO_WTM_0_ADDR                   (0x46)
#define BMI08X_FIFO_WTM_1_ADDR                   (0x47)
#define BMI08X_FIFO_CONFIG_0_ADDR                (0x48)
#define BMI08X_FIFO_CONFIG_1_ADDR                (0x49)

/*! @name FIFO sensor data lengths */
#define BMI08X_FIFO_ACCEL_LENGTH                 (6)
#define BMI08X_FIFO_WTM_LENGTH                   (2)
#define BMI08X_FIFO_LENGTH_MSB_BYTE              (1)
#define BMI08X_FIFO_DATA_LENGTH                  (2)
#define BMI08X_FIFO_CONFIG_LENGTH                (2)
#define BMI08X_SENSOR_TIME_LENGTH                (3)
#define BMI08X_FIFO_SKIP_FRM_LENGTH              (1)
#define BMI08X_FIFO_INPUT_CFG_LENGTH             (1)

/*! @name FIFO byte counter mask definition */
#define BMI08X_FIFO_BYTE_COUNTER_MSB_MASK        (0x3F)

/*! @name FIFO frame masks */
#define BMI08X_FIFO_LSB_CONFIG_CHECK             (0x00)
#define BMI08X_FIFO_MSB_CONFIG_CHECK             (0x80)
#define BMI08X_FIFO_INTR_MASK                    (0x5C)

/*name FIFO config modes */
#define BMI08X_ACC_STREAM_MODE                   (0x00)
#define BMI08X_ACC_FIFO_MODE                     (0x01)

/*name Mask definitions for FIFO configuration modes */
#define BMI08X_ACC_FIFO_MODE_CONFIG_MASK         (0x01)

/*! @name Mask definitions for FIFO_CONFIG_1 register */
#define BMI08X_ACCEL_EN_MASK                     (0x40)
#define BMI08X_ACCEL_INT1_EN_MASK                (0x08)
#define BMI08X_ACCEL_INT2_EN_MASK                (0x04)

/*name Position definitions for FIFO_CONFIG_1 register */
#define BMI08X_ACCEL_EN_POS                      (6)
#define BMI08X_ACCEL_INT1_EN_POS                 (3)
#define BMI08X_ACCEL_INT2_EN_POS                 (2)

/*! @name Position definitions for FIFO_DOWNS register */
#define BMI08X_ACC_FIFO_DOWNS_MASK               (0xF0)

/*! @name FIFO down sampling bit positions */
#define BMI08X_ACC_FIFO_DOWNS_POS                (0x04)

/*! @name FIFO down sampling user macros */
#define BMI08X_ACC_FIFO_DOWN_SAMPLE_0            (0)
#define BMI08X_ACC_FIFO_DOWN_SAMPLE_1            (1)
#define BMI08X_ACC_FIFO_DOWN_SAMPLE_2            (2)
#define BMI08X_ACC_FIFO_DOWN_SAMPLE_3            (3)
#define BMI08X_ACC_FIFO_DOWN_SAMPLE_4            (4)
#define BMI08X_ACC_FIFO_DOWN_SAMPLE_5            (5)
#define BMI08X_ACC_FIFO_DOWN_SAMPLE_6            (6)
#define BMI08X_ACC_FIFO_DOWN_SAMPLE_7            (7)

/*! @name Mask definitions for INT1_INT2_MAP_DATA register */
#define BMI08X_ACCEL_INT2_FWM_MASK               (0x20)
#define BMI08X_ACCEL_INT2_FFULL_MASK             (0x10)
#define BMI08X_ACCEL_INT1_FWM_MASK               (0x02)
#define BMI08X_ACCEL_INT1_FFULL_MASK             (0x01)

/*! @name Positions definitions for INT1_INT2_MAP_DATA register */
#define BMI08X_ACCEL_INT1_FWM_POS                (1)
#define BMI08X_ACCEL_INT2_FFULL_POS              (4)
#define BMI08X_ACCEL_INT2_FWM_POS                (5)

/**\name    Absolute value */
#ifndef BMI08X_ABS
#define BMI08X_ABS(a)                            ((a) > 0 ? (a) : -(a))
#endif

/**\name    Utility Macros  */
#define BMI08X_SET_LOW_BYTE                      (0x00FF)
#define BMI08X_SET_HIGH_BYTE                     (0xFF00)
#define BMI08X_SET_LOW_NIBBLE                    (0x0F)

/**\name Macro to SET and GET BITS of a register */
#define BMI08X_SET_BITS(reg_var, bitname, val) \
    ((reg_var & ~(bitname##_MASK)) | \
     ((val << bitname##_POS) & bitname##_MASK))

#define BMI08X_GET_BITS(reg_var, bitname)        ((reg_var & (bitname##_MASK)) >> \
                                                  (bitname##_POS))

#define BMI08X_SET_BITS_POS_0(reg_var, bitname, val) \
    ((reg_var & ~(bitname##_MASK)) | \
     (val & bitname##_MASK))

#define BMI08X_GET_BITS_POS_0(reg_var, bitname)  (reg_var & (bitname##_MASK))

#define BMI08X_SET_BIT_VAL_0(reg_var, bitname)   (reg_var & ~(bitname##_MASK))

/**\name     Macro definition for difference between 2 values */
#define BMI08X_GET_DIFF(x, y)                    ((x) - (y))

/**\name     Macro definition to get LSB of 16 bit variable */
#define BMI08X_GET_LSB(var)                      (uint8_t)(var & BMI08X_SET_LOW_BYTE)

/**\name     Macro definition to get MSB of 16 bit variable */
#define BMI08X_GET_MSB(var)                      (uint8_t)((var & BMI08X_SET_HIGH_BYTE) >> 8)

#define DEBUG_MSG       (0)

#if DEBUG_MSG
#define DBG_MSG(format, ...)    printk(KERN_INFO format "\n", ## __VA_ARGS__)
#else
#define DBG_MSG(format, ...)
#endif

/*************************************************************************/

#define BMI08X_ACCEL_DATA_SYNC_ADR              (0x02)
#define BMI08X_ACCEL_DATA_SYNC_LEN              (1)
#define BMI08X_ACCEL_DATA_SYNC_MODE_MASK        (0x0003)
#define BMI08X_ACCEL_DATA_SYNC_MODE_SHIFT       (0)

#define BMI08X_ACCEL_DATA_SYNC_MODE_OFF         (0x00)
#define BMI08X_ACCEL_DATA_SYNC_MODE_400HZ       (0x01)
#define BMI08X_ACCEL_DATA_SYNC_MODE_1000HZ      (0x02)
#define BMI08X_ACCEL_DATA_SYNC_MODE_2000HZ      (0x03)



typedef enum {
    active = 0,
    suspend
} POWER_MODE_ACCEL;

typedef enum {
    normal = 0,
    deep_suspend,
    normal_suspend
} POWER_MODE_GYRO;

typedef enum {
    range_3g = 0,
    range_6g,
    range_12g,
    range_24g
} RANGE_ACCEL;

typedef enum {
    range_2000dps = 0,
    range_1000dps,
    range_500dps,
    range_250dps,
    range_125dps
} RANGE_GYRO;

typedef struct {
    struct i2c_client *client;
	struct input_dev *input_dev;
	struct delayed_work work;
	int delay;
	int run_flag;
	int enSensors;
	atomic_t open_flag;
} bmi088_data;

#endif