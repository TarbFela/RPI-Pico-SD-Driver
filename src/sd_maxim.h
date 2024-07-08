//
// Created by The Tragedy of Darth Wise on 7/7/24.
//

#ifndef SD_MAXIM_H
#define SD_MAXIM_H

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "hardware/spi.h"
#include "sd_driver/crc.c"

#include "sd_maxim.c"


#define CMD0    (64 + 0)
#define CMD1    (64 + 1)
#define CMD8    (64 + 8)
#define CMD12   (64 + 12)
#define CMD18   (64 + 18)
#define CMD17   (64 + 17)
#define CMD24   (64 + 24)
#define CMD55   (64 + 55)
#define CMD58   (64 + 58)
#define ACMD41  (64 + 41)


#define SPI_PORT spi0

#define PIN_MISO 16
#define PIN_CS   17
#define PIN_SCK  18
#define PIN_MOSI 19
#define PIN_SD_POWER 20



#define BUSY_WAIT_TIMEOUT_THRESH 200



//!**  HOW TO WRITE DATA... (?) **
//!https://github.com/carlk3/no-OS-FatFS-SD-SPI-RPi-Pico/blob/master/FatFs_SPI/sd_driver/sd_card.c#L207
//!_ send CMD24 (Write Block)
//! send token
//! send data
//! send CRC 16 as such (from above code...)
//    uint16t crc = crc16((void *)buffer, length);
//           sd_spi_write(pSD, crc >> 8);
//           sd_spi_write(pSD, crc);
//!**the response should be:**
// response (uint8_t) & SPI_DATA_RESPONSE_MASK == 0
#define SPI_DATA_RESPONSE_MASK (0x1F)
#define SPI_START_BLOCK (0xFE)    //! For Single Block Read/Write and Multiple Block Read



void SDspi_init();
void CS_sel();
void CS_des();
int sd_busy_wait_until();
void sd_busy_wait_for(int bytes);
void sd_cmd(uint8_t *cmd[6], uint8_t *ret_dst);
uint32_t sd_cmd_v2(uint8_t CMD, uint32_t arg);
int sd_init(uint8_t *data_buffer);
void sd_read_blocks(uint32_t add, uint8_t *dst, uint32_t num_blocks);
void sd_write_block(uint32_t add, uint8_t *src);


#endif //SD_MAXIM_H
