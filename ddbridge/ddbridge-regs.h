/*
 * ddbridge-regs.h: Digital Devices PCIe bridge driver
 *
 * Copyright (C) 2010-2016 Digital Devices GmbH
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 only, as published by the Free Software Foundation.
 *
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA
 * Or, point your browser to http://www.gnu.org/copyleft/gpl.html
 */

/* Register Definitions */

#define CUR_REGISTERMAP_VERSION_V1  0x00010001
#define CUR_REGISTERMAP_VERSION_V2  0x00020000

#define HARDWARE_VERSION            0x00000000
#define REGISTERMAP_VERSION         0x00000004

/* ------------------------------------------------------------------------- */
/* SPI Controller */

#define SPI_CONTROL     0x10
#define SPI_DATA        0x14

/* ------------------------------------------------------------------------- */
/* GPIO */

#define GPIO_OUTPUT      0x20
#define GPIO_INPUT       0x24
#define GPIO_DIRECTION   0x28

/* ------------------------------------------------------------------------- */
/* MDIO */

#define MDIO_CTRL        0x20
#define MDIO_ADR         0x24
#define MDIO_REG         0x28
#define MDIO_VAL         0x2C

/* ------------------------------------------------------------------------- */

#define BOARD_CONTROL    0x30

/* ------------------------------------------------------------------------- */

/* Interrupt controller
   How many MSI's are available depends on HW (Min 2 max 8)
   How many are usable also depends on Host platform
*/

#define INTERRUPT_BASE   (0x40)

#define INTERRUPT_ENABLE (INTERRUPT_BASE + 0x00)
#define MSI0_ENABLE      (INTERRUPT_BASE + 0x00)
#define MSI1_ENABLE      (INTERRUPT_BASE + 0x04)
#define MSI2_ENABLE      (INTERRUPT_BASE + 0x08)
#define MSI3_ENABLE      (INTERRUPT_BASE + 0x0C)
#define MSI4_ENABLE      (INTERRUPT_BASE + 0x10)
#define MSI5_ENABLE      (INTERRUPT_BASE + 0x14)
#define MSI6_ENABLE      (INTERRUPT_BASE + 0x18)
#define MSI7_ENABLE      (INTERRUPT_BASE + 0x1C)

#define INTERRUPT_STATUS (INTERRUPT_BASE + 0x20)
#define INTERRUPT_ACK    (INTERRUPT_BASE + 0x20)
#define INTERRUPT_ACK1      (INTERRUPT_BASE + 0x24)
#define INTERRUPT_ACK2      (INTERRUPT_BASE + 0x28)

#define INTMASK_CLOCKGEN    (0x00000001)
#define INTMASK_TEMPMON     (0x00000002)

#define INTMASK_I2C1        (0x00000001)
#define INTMASK_I2C2        (0x00000002)
#define INTMASK_I2C3        (0x00000004)
#define INTMASK_I2C4        (0x00000008)

#define INTMASK_CIRQ1       (0x00000010)
#define INTMASK_CIRQ2       (0x00000020)
#define INTMASK_CIRQ3       (0x00000040)
#define INTMASK_CIRQ4       (0x00000080)

#define INTMASK_TSINPUT1    (0x00000100)
#define INTMASK_TSINPUT2    (0x00000200)
#define INTMASK_TSINPUT3    (0x00000400)
#define INTMASK_TSINPUT4    (0x00000800)
#define INTMASK_TSINPUT5    (0x00001000)
#define INTMASK_TSINPUT6    (0x00002000)
#define INTMASK_TSINPUT7    (0x00004000)
#define INTMASK_TSINPUT8    (0x00008000)

#define INTMASK_TSOUTPUT1   (0x00010000)
#define INTMASK_TSOUTPUT2   (0x00020000)
#define INTMASK_TSOUTPUT3   (0x00040000)
#define INTMASK_TSOUTPUT4   (0x00080000)


#define INTERRUPT_V2_CONTROL  (INTERRUPT_BASE + 0x00)
#define INTERRUPT_V2_ENABLE_1 (INTERRUPT_BASE + 0x04)
#define INTERRUPT_V2_ENABLE_2 (INTERRUPT_BASE + 0x08)
#define INTERRUPT_V2_ENABLE_3 (INTERRUPT_BASE + 0x0c)
#define INTERRUPT_V2_ENABLE_4 (INTERRUPT_BASE + 0x10)
#define INTERRUPT_V2_ENABLE_5 (INTERRUPT_BASE + 0x14)
#define INTERRUPT_V2_ENABLE_6 (INTERRUPT_BASE + 0x18)
#define INTERRUPT_V2_ENABLE_7 (INTERRUPT_BASE + 0x1c)

#define INTERRUPT_V2_STATUS   (INTERRUPT_BASE + 0x20)
#define INTERRUPT_V2_STATUS_1 (INTERRUPT_BASE + 0x24)
#define INTERRUPT_V2_STATUS_2 (INTERRUPT_BASE + 0x28)
#define INTERRUPT_V2_STATUS_3 (INTERRUPT_BASE + 0x2c)
#define INTERRUPT_V2_STATUS_4 (INTERRUPT_BASE + 0x30)
#define INTERRUPT_V2_STATUS_5 (INTERRUPT_BASE + 0x34)
#define INTERRUPT_V2_STATUS_6 (INTERRUPT_BASE + 0x38)
#define INTERRUPT_V2_STATUS_7 (INTERRUPT_BASE + 0x3c)




/* Modulator registers */

/*  Clock Generator ( Sil598 @ 0xAA I2c ) */
#define CLOCKGEN_BASE       (0x80)
#define CLOCKGEN_CONTROL    (CLOCKGEN_BASE + 0x00)
#define CLOCKGEN_INDEX      (CLOCKGEN_BASE + 0x04)
#define CLOCKGEN_WRITEDATA  (CLOCKGEN_BASE + 0x08)
#define CLOCKGEN_READDATA   (CLOCKGEN_BASE + 0x0C)

/* DAC ( AD9781/AD9783 SPI ) */
#define DAC_BASE            (0x090)
#define DAC_CONTROL         (DAC_BASE)
#define DAC_WRITE_DATA      (DAC_BASE+4)
#define DAC_READ_DATA       (DAC_BASE+8)

#define DAC_CONTROL_INSTRUCTION_REG (0xFF)
#define DAC_CONTROL_STARTIO         (0x100)
#define DAC_CONTROL_RESET           (0x200)

/* Temperature Monitor ( 2x LM75A @ 0x90,0x92 I2c ) */
#define TEMPMON_BASE        (0x1c0)
#define TEMPMON_CONTROL    (TEMPMON_BASE + 0x00)

#define TEMPMON_CONTROL_SCAN        (0x00000001)
#define TEMPMON_CONTROL_AUTOSCAN    (0x00000002)
#define TEMPMON_CONTROL_INTENABLE   (0x00000004)
#define TEMPMON_CONTROL_OVERTEMP    (0x00008000)


/* SHORT Temperature in °C x 256 */
#define TEMPMON_CORE       (TEMPMON_BASE + 0x04)
#define TEMPMON_SENSOR0    (TEMPMON_BASE + 0x04)
#define TEMPMON_SENSOR1    (TEMPMON_BASE + 0x08)
#define TEMPMON_SENSOR2    (TEMPMON_BASE + 0x0C)

#define TEMPMON_FANCONTROL  (TEMPMON_BASE + 0x10)
#define TEMPMON_FANPWM      (0x00000F00)            // PWM speed in 10% steps
#define TEMPMON_FANTACHO    (0x000000FF)            // Rotations in 100/min steps

// V1 Temperature Monitor
// Temperature Monitor TEMPMON_CONTROL & 0x8000 == 0 : ( 2x LM75A @ 0x90,0x92 )
// Temperature Monitor TEMPMON_CONTROL & 0x8000 == 1 : ( 1x LM75A @ 0x90, 1x ADM1032 @ 0x9A )

#define TEMPMON1_CORE       (TEMPMON_SENSOR0)    // SHORT Temperature in °C x 256 (ADM1032 ext)
#define TEMPMON1_SENSOR1    (TEMPMON_BASE + 0x08)    // SHORT Temperature in °C x 256 (LM75A 0x90)
#define TEMPMON1_SENSOR2    (TEMPMON_BASE + 0x0C)    // SHORT Temperature in °C x 256 (LM75A 0x92 or ADM1032 Int)

// V2 Temperature Monitor 2 ADM1032

#define TEMPMON2_BOARD      (TEMPMON_SENSOR0)    // SHORT Temperature in °C x 256 (ADM1032 int)
#define TEMPMON2_FPGACORE   (TEMPMON_SENSOR1)    // SHORT Temperature in °C x 256 (ADM1032 ext)
#define TEMPMON2_QAMCORE    (TEMPMON_SENSOR2)    // SHORT Temperature in °C x 256 (ADM1032 ext)

/* ------------------------------------------------------------------------- */
/* I2C Master Controller */

#define I2C_COMMAND     (0x00)
#define I2C_TIMING      (0x04)
#define I2C_TASKLENGTH  (0x08)     /* High read, low write */
#define I2C_TASKADDRESS (0x0C)     /* High read, low write */
#define I2C_MONITOR     (0x1C)

#define I2C_SPEED_666   (0x02010202)
#define I2C_SPEED_400   (0x04030404)
#define I2C_SPEED_200   (0x09080909)
#define I2C_SPEED_154   (0x0C0B0C0C)
#define I2C_SPEED_100   (0x13121313)
#define I2C_SPEED_77    (0x19181919)
#define I2C_SPEED_50    (0x27262727)


/* ------------------------------------------------------------------------- */
/* DMA  Controller */

#define DMA_BASE_WRITE        (0x100)
#define DMA_BASE_READ         (0x140)

#define DMA_CONTROL     (0x00)
#define DMA_ERROR       (0x04)

#define DMA_DIAG_CONTROL                (0x1C)
#define DMA_DIAG_PACKETCOUNTER_LOW      (0x20)
#define DMA_DIAG_PACKETCOUNTER_HIGH     (0x24)
#define DMA_DIAG_TIMECOUNTER_LOW        (0x28)
#define DMA_DIAG_TIMECOUNTER_HIGH       (0x2C)
#define DMA_DIAG_RECHECKCOUNTER         (0x30)
#define DMA_DIAG_WAITTIMEOUTINIT        (0x34)
#define DMA_DIAG_WAITOVERFLOWCOUNTER    (0x38)
#define DMA_DIAG_WAITCOUNTER            (0x3C)

#define TS_CONTROL(_io)         (_io->regs + 0x00)
#define TS_CONTROL2(_io)        (_io->regs + 0x04)


/* ------------------------------------------------------------------------- */
/* DMA  Buffer */

#define DMA_BUFFER_CONTROL(_dma)       (_dma->regs + 0x00)
#define DMA_BUFFER_ACK(_dma)           (_dma->regs + 0x04)
#define DMA_BUFFER_CURRENT(_dma)       (_dma->regs + 0x08)
#define DMA_BUFFER_SIZE(_dma)          (_dma->regs + 0x0c)


/* ------------------------------------------------------------------------- */

#define LNB_BASE                     (0x400)
#define LNB_CONTROL(i)               (LNB_BASE + (i) * 0x20 + 0x00)
#define LNB_CMD   (7ULL <<  0)
#define LNB_CMD_NOP    0
#define LNB_CMD_INIT   1
#define LNB_CMD_STATUS 2
#define LNB_CMD_LOW    3
#define LNB_CMD_HIGH   4
#define LNB_CMD_OFF    5
#define LNB_CMD_DISEQC 6
#define LNB_CMD_UNI    7

#define LNB_BUSY  (1ULL <<  4)
#define LNB_TONE  (1ULL << 15)

#define LNB_STATUS(i)                (LNB_BASE + (i) * 0x20 + 0x04)
#define LNB_VOLTAGE(i)               (LNB_BASE + (i) * 0x20 + 0x08)
#define LNB_CONFIG(i)                (LNB_BASE + (i) * 0x20 + 0x0c)
#define LNB_BUF_LEVEL(i)             (LNB_BASE + (i) * 0x20 + 0x10)
#define LNB_BUF_WRITE(i)             (LNB_BASE + (i) * 0x20 + 0x14)

/* ------------------------------------------------------------------------- */
/* CI Interface (only CI-Bridge) */

#define CI_BASE                     (0x400)
#define CI_CONTROL(i)               (CI_BASE + (i) * 32 + 0x00)

#define CI_DO_ATTRIBUTE_RW(i)       (CI_BASE + (i) * 32 + 0x04)
#define CI_DO_IO_RW(i)              (CI_BASE + (i) * 32 + 0x08)
#define CI_READDATA(i)              (CI_BASE + (i) * 32 + 0x0c)
#define CI_DO_READ_ATTRIBUTES(i)    (CI_BASE + (i) * 32 + 0x10)

#define CI_RESET_CAM                    (0x00000001)
#define CI_POWER_ON                     (0x00000002)
#define CI_ENABLE                       (0x00000004)
#define CI_BLOCKIO_ENABLE               (0x00000008)
#define CI_BYPASS_DISABLE               (0x00000010)
#define CI_DISABLE_AUTO_OFF             (0x00000020)

#define CI_CAM_READY                    (0x00010000)
#define CI_CAM_DETECT                   (0x00020000)
#define CI_READY                        (0x80000000)
#define CI_BLOCKIO_ACTIVE               (0x40000000)
#define CI_BLOCKIO_RCVDATA              (0x20000000)
#define CI_BLOCKIO_SEND_PENDING         (0x10000000)
#define CI_BLOCKIO_SEND_COMPLETE        (0x08000000)

#define CI_READ_CMD                     (0x40000000)
#define CI_WRITE_CMD                    (0x80000000)

#define CI_BLOCKIO_SEND(i)              (CI_BASE + (i) * 32 + 0x14)
#define CI_BLOCKIO_RECEIVE(i)           (CI_BASE + (i) * 32 + 0x18)

#define CI_BLOCKIO_SEND_COMMAND         (0x80000000)
#define CI_BLOCKIO_SEND_COMPLETE_ACK    (0x40000000)
#define CI_BLOCKIO_RCVDATA_ACK          (0x40000000)

#define CI_BUFFER_BASE                  (0x3000)
#define CI_BUFFER_SIZE                  (0x0800)
#define CI_BLOCKIO_BUFFER_SIZE          (CI_BUFFER_SIZE/2)

#define CI_BUFFER(i)                  (CI_BUFFER_BASE + (i) * CI_BUFFER_SIZE)
#define CI_BLOCKIO_RECEIVE_BUFFER(i)  (CI_BUFFER_BASE + (i) * CI_BUFFER_SIZE)
#define CI_BLOCKIO_SEND_BUFFER(i)  \
	(CI_BUFFER_BASE + (i) * CI_BUFFER_SIZE + CI_BLOCKIO_BUFFER_SIZE)

// V1

#define VCO1_BASE           (0xC0)
#define VCO1_CONTROL        (VCO1_BASE + 0x00)
#define VCO1_DATA           (VCO1_BASE + 0x04)  /* 24 Bit */
/* 1 = Trigger write, resets when done */
#define VCO1_CONTROL_WRITE  (0x00000001)
/* 0 = Put VCO into power down */
#define VCO1_CONTROL_CE     (0x00000002)
/* Muxout from VCO (usually = Lock) */
#define VCO1_CONTROL_MUXOUT (0x00000004)

#define VCO2_BASE           (0xC8)
#define VCO2_CONTROL        (VCO2_BASE + 0x00)
#define VCO2_DATA           (VCO2_BASE + 0x04)  /* 24 Bit */
/* 1 = Trigger write, resets when done */
#define VCO2_CONTROL_WRITE  (0x00000001)
/* 0 = Put VCO into power down */
#define VCO2_CONTROL_CE     (0x00000002)
/* Muxout from VCO (usually = Lock) */
#define VCO2_CONTROL_MUXOUT (0x00000004)

#define VCO3_BASE           (0xD0)
#define VCO3_CONTROL        (VCO3_BASE + 0x00)
#define VCO3_DATA           (VCO3_BASE + 0x04)  /* 32 Bit */
/* 1 = Trigger write, resets when done */
#define VCO3_CONTROL_WRITE  (0x00000001)
/* 0 = Put VCO into power down */
#define VCO3_CONTROL_CE     (0x00000002)
/* Muxout from VCO (usually = Lock) */
#define VCO3_CONTROL_MUXOUT (0x00000004)

// V2

#define MAX2871_BASE            (0xC0)
#define MAX2871_CONTROL         (MAX2871_BASE + 0x00)
#define MAX2871_OUTDATA         (MAX2871_BASE + 0x04)  // 32 Bit
#define MAX2871_INDATA          (MAX2871_BASE + 0x08)  // 32 Bit
#define MAX2871_CONTROL_WRITE   (0x00000001)   // 1 = Trigger write, resets when done
#define MAX2871_CONTROL_CE      (0x00000002)   // 0 = Put VCO into power down
#define MAX2871_CONTROL_MUXOUT  (0x00000004)   // Muxout from VCO 
#define MAX2871_CONTROL_LOCK    (0x00000008)   // Lock from VCO 

#define FSM_BASE                (0x200)
#define FSM_CONTROL             (FSM_BASE + 0x00)

#define FSM_CONTROL_ENABLE      (0x00000100)

#define FSM_CMD_MASK            (0x00000087)
#define FSM_CMD_STATUS          (0x00000080)
#define FSM_CMD_RESET           (0x00000080)
#define FSM_CMD_POWERDOWN       (0x00000081)
#define FSM_CMD_SETUP           (0x00000082)

#define FSM_STATUS              (FSM_BASE + 0x00)
#define FSM_STATUS_READY        (0x00010000)
#define FSM_STATUS_QAMREADY     (0x00020000)


#define FSM_CAPACITY            (FSM_BASE + 0x04)
#define FSM_CAPACITY_MAX        (0x3F000000)  
#define FSM_CAPACITY_CUR        (0x003F0000)  
#define FSM_CAPACITY_INUSE      (0x0000003F)  

#define FSM_GAIN                (FSM_BASE + 0x10)
#define FSM_GAINMASK            (0x000000FF)

#define FSM_GAIN_N1             (0x000000FE)
#define FSM_GAIN_N2             (0x000000A1)
#define FSM_GAIN_N4             (0x00000066)
#define FSM_GAIN_N8             (0x00000048)
#define FSM_GAIN_N16            (0x0000002D)
#define FSM_GAIN_N24            (0x00000029)
#define FSM_GAIN_N96            (0x00000011)


// Attenuator/VGA

#define RF_ATTENUATOR   (0xD8)
#define RF_ATTENUATOR   (0xD8)
/*  0x00 =  0 dB
    0x01 =  1 dB
      ...
    0x1F = 31 dB
*/

#define RF_VGA  (0xDC)
/* Only V2 */
/* 8 bit range 0 - 31.75 dB Gain  */
	
/* VGA Gain for same output level as V1 Modulator */
#define RF_VGA_GAIN_N8          (85)
#define RF_VGA_GAIN_N16         (117)
#define RF_VGA_GAIN_N24         (122)

#define RF_VGA_GAIN_MAX         (200)


/* V1 only */

#define RF_POWER        (0xE0)
#define RF_POWER_BASE       (0xE0)
#define RF_POWER_CONTROL    (RF_POWER_BASE + 0x00)
#define RF_POWER_DATA       (RF_POWER_BASE + 0x04)

#define RF_POWER_CONTROL_START     (0x00000001)
#define RF_POWER_CONTROL_DONE      (0x00000002)
#define RF_POWER_CONTROL_VALIDMASK (0x00000700)
#define RF_POWER_CONTROL_VALID     (0x00000500)


/* --------------------------------------------------------------------------
   Output control
*/

#define IQOUTPUT_BASE           (0x240)
#define IQOUTPUT_CONTROL        (IQOUTPUT_BASE + 0x00)
#define IQOUTPUT_CONTROL2       (IQOUTPUT_BASE + 0x04)
#define IQOUTPUT_PEAK_DETECTOR  (IQOUTPUT_BASE + 0x08)
#define IQOUTPUT_POSTSCALER     (IQOUTPUT_BASE + 0x0C)
#define IQOUTPUT_PRESCALER      (IQOUTPUT_BASE + 0x10)

#define IQOUTPUT_EQUALIZER_0    (IQOUTPUT_BASE + 0x14)
#define IQOUTPUT_EQUALIZER_1    (IQOUTPUT_BASE + 0x18)
#define IQOUTPUT_EQUALIZER_2    (IQOUTPUT_BASE + 0x1C)
#define IQOUTPUT_EQUALIZER_3    (IQOUTPUT_BASE + 0x20)
#define IQOUTPUT_EQUALIZER_4    (IQOUTPUT_BASE + 0x24)
#define IQOUTPUT_EQUALIZER_5    (IQOUTPUT_BASE + 0x28)
#define IQOUTPUT_EQUALIZER_6    (IQOUTPUT_BASE + 0x2C)
#define IQOUTPUT_EQUALIZER_7    (IQOUTPUT_BASE + 0x30)
#define IQOUTPUT_EQUALIZER_8    (IQOUTPUT_BASE + 0x34)
#define IQOUTPUT_EQUALIZER_9    (IQOUTPUT_BASE + 0x38)
#define IQOUTPUT_EQUALIZER_10   (IQOUTPUT_BASE + 0x3C)

#define IQOUTPUT_EQUALIZER(i)   (IQOUTPUT_EQUALIZER_0 + (i) * 4)

#define IQOUTPUT_CONTROL_RESET              (0x00000001)
#define IQOUTPUT_CONTROL_ENABLE             (0x00000002)
#define IQOUTPUT_CONTROL_RESET_PEAK         (0x00000004)
#define IQOUTPUT_CONTROL_ENABLE_PEAK        (0x00000008)
#define IQOUTPUT_CONTROL_BYPASS_EQUALIZER   (0x00000010)


/* Modulator Base  V1 */

#define MODULATOR_BASE          (0x200)
#define MODULATOR_CONTROL         (MODULATOR_BASE)
#define MODULATOR_IQTABLE_END     (MODULATOR_BASE+4)
#define MODULATOR_IQTABLE_INDEX   (MODULATOR_BASE+8)
#define MODULATOR_IQTABLE_DATA    (MODULATOR_BASE+12)

#define MODULATOR_IQTABLE_INDEX_CHANNEL_MASK  (0x000F0000)
#define MODULATOR_IQTABLE_INDEX_IQ_MASK       (0x00008000)
#define MODULATOR_IQTABLE_INDEX_ADDRESS_MASK  (0x000007FF)
#define MODULATOR_IQTABLE_INDEX_SEL_I         (0x00000000)
#define MODULATOR_IQTABLE_INDEX_SEL_Q     (MODULATOR_IQTABLE_INDEX_IQ_MASK)
#define MODULATOR_IQTABLE_SIZE    (2048)


/* Modulator Channels */

#define CHANNEL_BASE dev->link[0].info->regmap->channel->base

#define CHANNEL_CONTROL(i)          (CHANNEL_BASE + (i) * 64 + 0x00)
#define CHANNEL_SETTINGS(i)         (CHANNEL_BASE + (i) * 64 + 0x04)
#define CHANNEL_SETTINGS2(i)        (CHANNEL_BASE + (i) * 64 + 0x08)
#define CHANNEL_RATE_INCR(i)        (CHANNEL_BASE + (i) * 64 + 0x0C)
#define CHANNEL_PCR_ADJUST_OUTL(i)  (CHANNEL_BASE + (i) * 64 + 0x10)
#define CHANNEL_PCR_ADJUST_OUTH(i)  (CHANNEL_BASE + (i) * 64 + 0x14)
#define CHANNEL_PCR_ADJUST_INL(i)   (CHANNEL_BASE + (i) * 64 + 0x18)
#define CHANNEL_PCR_ADJUST_INH(i)   (CHANNEL_BASE + (i) * 64 + 0x1C)
#define CHANNEL_PCR_ADJUST_ACCUL(i) (CHANNEL_BASE + (i) * 64 + 0x20)
#define CHANNEL_PCR_ADJUST_ACCUH(i) (CHANNEL_BASE + (i) * 64 + 0x24)
#define CHANNEL_PKT_COUNT_OUT(i)    (CHANNEL_BASE + (i) * 64 + 0x28)
#define CHANNEL_PKT_COUNT_IN(i)     (CHANNEL_BASE + (i) * 64 + 0x2C)
#define CHANNEL_KF(i)               (CHANNEL_BASE + (i) * 64 + 0x30)
#define CHANNEL_LF(i)               (CHANNEL_BASE + (i) * 64 + 0x34)

#define CHANNEL_CONTROL_RESET               (0x00000001)
#define CHANNEL_CONTROL_ENABLE_DVB          (0x00000002)
#define CHANNEL_CONTROL_ENABLE_IQ           (0x00000004)
#define CHANNEL_CONTROL_ENABLE_SOURCE       (0x00000008)
#define CHANNEL_CONTROL_ENABLE_PCRADJUST    (0x00000010)
#define CHANNEL_CONTROL_FREEZE_STATUS       (0x00000100)

#define CHANNEL_CONTROL_CMD_MASK            (0x0000F000)
#define CHANNEL_CONTROL_CMD_STATUS          (0x00008000)
#define CHANNEL_CONTROL_CMD_FREE            (0x00008000)
#define CHANNEL_CONTROL_CMD_SETUP           (0x00009000)
#define CHANNEL_CONTROL_CMD_MUTE            (0x0000A000)
#define CHANNEL_CONTROL_CMD_UNMUTE          (0x0000B000)

#define CHANNEL_CONTROL_RESET_ERROR         (0x00010000)
#define CHANNEL_CONTROL_ACTIVE              (0x00400000)
#define CHANNEL_CONTROL_BUSY                (0x00800000)
#define CHANNEL_CONTROL_ERROR_CMD           (0x10000000)
#define CHANNEL_CONTROL_ERROR_SYNC          (0x20000000)
#define CHANNEL_CONTROL_ERROR_UNDERRUN      (0x40000000)
#define CHANNEL_CONTROL_ERROR_FATAL         (0x80000000)

#define CHANNEL_SETTINGS_QAM_MASK           (0x00000007)
#define CHANNEL_SETTINGS_QAM16              (0x00000000)
#define CHANNEL_SETTINGS_QAM32              (0x00000001)
#define CHANNEL_SETTINGS_QAM64              (0x00000002)
#define CHANNEL_SETTINGS_QAM128             (0x00000003)
#define CHANNEL_SETTINGS_QAM256             (0x00000004)

#define CHANNEL_SETTINGS2_OUTPUT_MASK       (0x0000007F)

#define KFLF_MAX                            (0x07FFFFFFUL)
#define KF_INIT(Symbolrate)                 (Symbolrate)
#define LF_INIT(Symbolrate)                 (9000000UL)
#define MIN_SYMBOLRATE                      (1000000)
#define MAX_SYMBOLRATE                      (7100000)


/* OCTONET */

#define ETHER_BASE       (0x100)
#define ETHER_CONTROL    (ETHER_BASE + 0x00)
#define ETHER_LENGTH     (ETHER_BASE + 0x04)

#define RTP_MASTER_BASE      (0x120)
#define RTP_MASTER_CONTROL          (RTP_MASTER_BASE + 0x00)
#define RTP_RTCP_INTERRUPT          (RTP_MASTER_BASE + 0x04)
#define RTP_MASTER_RTCP_SETTINGS    (RTP_MASTER_BASE + 0x0c)

#define STREAM_BASE       (0x400)
#define STREAM_CONTROL(i)        (STREAM_BASE + (i) * 0x20 + 0x00)
#define STREAM_RTP_PACKET(i)        (STREAM_BASE + (i) * 0x20 + 0x04)
#define STREAM_RTCP_PACKET(i)       (STREAM_BASE + (i) * 0x20 + 0x08)
#define STREAM_RTP_SETTINGS(i)      (STREAM_BASE + (i) * 0x20 + 0x0c)
#define STREAM_INSERT_PACKET(i)     (STREAM_BASE + (i) * 0x20 + 0x10)

#define STREAM_PACKET_OFF(i) ((i) * 0x200)
#define STREAM_PACKET_ADR(i) (0x2000 + (STREAM_PACKET_OFF(i)))

#define STREAM_PIDS(i) (0x4000 + (i) * 0x400)

#define TS_CAPTURE_BASE (0x0140)
#define TS_CAPTURE_CONTROL       (TS_CAPTURE_BASE + 0x00)
#define TS_CAPTURE_PID           (TS_CAPTURE_BASE + 0x04)
#define TS_CAPTURE_RECEIVED      (TS_CAPTURE_BASE + 0x08)
#define TS_CAPTURE_TIMEOUT       (TS_CAPTURE_BASE + 0x0c)
#define TS_CAPTURE_TABLESECTION  (TS_CAPTURE_BASE + 0x10)

#define TS_CAPTURE_MEMORY (0x7000)

#define PID_FILTER_BASE       (0x800)
#define PID_FILTER_SYSTEM_PIDS(i)     (PID_FILTER_BASE + (i) * 0x20)
#define PID_FILTER_PID(i, j)     (PID_FILTER_BASE + (i) * 0x20 + (j) * 4)



