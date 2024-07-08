
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "hardware/spi.h"
#include "sd_driver/crc.h"

#include "sd_maxim.h"


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

                   //bits:  47-40      39-32   31-24   23-16   15-8    7-0
char cmd0_arg0[6] =      {CMD0,      00,     00,     00,     00,     0x95};
char cmd1_arg40[6] =     {CMD1,      0x40,     00,     00,     00,     0x95};
char cmd8_arg1AA[6] =    {CMD8,      00,     00,     0x01,   0xAA,   0x87};
char cmd55_arg0[6] =     {CMD55,     00,     00,     00,     00,     0x65};
char cmd58_arg0[6] =     {CMD55,     00,     00,     00,     00,     0xFD};


//!** INITIALIZATION STUFF
//https://electronics.stackexchange.com/questions/77417/what-is-the-correct-command-sequence-for-microsd-card-initialization-in-spi
// "if the voltage window field (bits 23-0) of the argument is set to zero, it is called "inquiry CMD41" that does not start initialization [...] shall ignore the other field (bit 31-24) of the argument" Physical Layer 4.2.3.1
// if the voltage window is populated, it is "first CMD41" and proceeding ACMD41 should have the same arg henceforth...

// the response to inquiry is the R3. looks like:
    //  47      46      45-40       39-8        15-8    7-1     0
    //  0       0       111111      OCR         0x00    0xFF
    //  start,  trans,    reserved  32-bit OCR, reserved & end = 0xFF
    //0x3F                          40  XX XX   0x00    0xFF
// above for busy / initializing (0x3F 40 XX XX 00 FF)
// below for initialized         (0x3F C0 XX XX 00 FF
    //0x3F                          C0  XX XX   0x00    0xFF

                        //??// set bit 22: 0100 0000 ...    0x40
uint8_t acmd41_first[6]       =   {ACMD41,    0x40,   0x10,     0x00,   0x00,     0xCD};
uint8_t acmd41_inquiry[6]     = {ACMD41,    0x40,   00,     0x00,   0x00,     0x77};
// I'm not sure what the voltage arg should be. I will try specifying one voltage, and then I will try specifying all voltages if that doesn't work...
                        // set bits 23-15 0xFF 0x10     0xFF,   0x10
uint8_t acmd41_firstv2[6]     = {ACMD41,    0x40,   00,     0xFF,   0x10,     0x7D}; //0x17 for 0x50, 0x77 for 0x40
//char cmd1_arg0[6] =      {CMD1,      00,     00,     00,     00,     0x6B};

uint8_t sd_busy = 0xFF;


void SDspi_init() {
    // Initialize SPI port at 400kHz for SD card initialization
    spi_init(SPI_PORT, 300*1000);

    // Set SPI pin functions
    gpio_set_function(PIN_MISO, GPIO_FUNC_SPI);
    gpio_set_function(PIN_SCK, GPIO_FUNC_SPI);
    gpio_set_function(PIN_MOSI, GPIO_FUNC_SPI);

    //initialize SD power pin
    gpio_init(PIN_SD_POWER);
    gpio_set_dir(PIN_SD_POWER, GPIO_OUT);
    gpio_put(PIN_SD_POWER, 1);  // turn power on

    // Initialize CS pin
    gpio_init(PIN_CS);
    gpio_set_dir(PIN_CS, GPIO_OUT);
    gpio_put(PIN_CS, 1);  // Deselect SD card
}

void CS_sel() {
    gpio_put(PIN_CS,0);
}

void CS_des() {
    gpio_put(PIN_CS,1);
}

int sd_busy_wait_until() {
    uint8_t ret = 0;
    int timeout = 0;
    while( ret != 0xFF && timeout < BUSY_WAIT_TIMEOUT_THRESH) {
        timeout++;
        spi_read_blocking(SPI_PORT, 0xFF, &ret, 1);
        //print_hex(ret);
        //if(ret==0xFF) sleep_ms(500);
    }
    return timeout<BUSY_WAIT_TIMEOUT_THRESH;
}
void sd_busy_wait_for(int bytes) {
    for(int i=0;i<bytes;i++) spi_write_blocking(SPI_PORT, &sd_busy, 1);
}
void sd_cmd(uint8_t *cmd[6], uint8_t *ret_dst) {
    const int response_length = 48 / 8; //48 bits is R1 and R3 response types
    CS_sel(); //assert CS pin low to align bytes and assert SPI mode w/ CMD0
    spi_write_blocking(SPI_PORT, cmd, 6);
    spi_read_blocking(SPI_PORT, 0xFF, ret_dst, response_length);
    CS_des();
}

uint32_t sd_cmd_v2(uint8_t CMD, uint32_t arg) {
    uint8_t crc = 0;
    char cmd[6] = {CMD, arg>>24, arg>>16, arg>>8, arg, crc};
    crc = (crc7(&cmd, 5) << 1 ) +1;
    cmd[5] = crc;

    print_bytes(6,&cmd); // just for debugging...



}

int sd_init(uint8_t *data_buffer) {
    int i, j = 0;
    SDspi_init();

    sleep_ms(500);

    CS_des();
    sd_busy_wait_until(16); //send 74+ clock pulses

    //CMD0
    sd_cmd(     &cmd0_arg0,      &data_buffer[cursor(6)]);

    sd_busy_wait_for(10);

    //CMD8
    sd_cmd(     &cmd8_arg1AA,    &data_buffer[cursor(6)]);

    //CMD58 a few times...
    for(j=0;j<8;j++) {
        sd_busy_wait_for(10);
        sd_cmd(     &cmd58_arg0,     &data_buffer[cursor(6)]);
        if(data_buffer[cursor(0) - 1] == 0xFF) cursor(-6);
    }

    //CMD 55 & CMD 41 a few times
    for(j=0;j<80;j++) {
        sd_cmd( &cmd55_arg0,     &data_buffer[cursor(6)]);
        sd_busy_wait_for(10);
        sd_cmd( &acmd41_first,   &data_buffer[cursor(6)]);
        sd_busy_wait_for(10);
        //sleep_ms(50);
        if(data_buffer[cursor(0) - 6] == 0x00 || data_buffer[cursor(0) - 5] == 0x00) {
            print_num(80085); // BOOBS == DONE
            sleep_ms(500);
            break;
        }

    }
    //CMD 58 to get OCR (has been returning something like 90 20 00 00 FF or something?)
    sd_cmd(     &cmd58_arg0,     &data_buffer[cursor(6)]);

    // print that stuff out
    while(i>=0) {
        print_num(i/6);
        if(data_buffer[i] == 0x00) {
            i = cursor(-6);
            continue;
        }
        print_bytes(6,&data_buffer[i]);
        i = cursor(-6);
        sleep_ms(250);
    }

    sleep_ms(250);
}

void sd_read_blocks(uint32_t add, uint8_t *dst, uint32_t num_blocks) {
    // turn address into arg
    char cmd[6] = {CMD18, add>>24, add>>16, add>>8, add, 0x00};
    // get crc7
    char crc = (crc7(&cmd[0], 5) << 1 ) +1; //!** NOT SURE ABOUT WHETHER CMD NEEDS TO BE &CMD...
    cmd[5] = crc;

    char cmd12[6] = {CMD12, 0, 0, 0, 0, 0x00};
    crc = (crc7(&cmd12[6],5) << 1) + 1;
    cmd12[5] = crc;

    uint8_t response[6];
    /*
    CS_des();       //!** this may be unneccesary and even counterproductive but it might align the bytes correctly...
    sleep_ms(10);

     */
    CS_sel();
    spi_write_blocking(SPI_PORT, &cmd, 6);
	CD_des();
    spi_read_blocking(SPI_PORT, 0xFF, &response, 6);
    print_num(5346);
    print_bytes(6,&response);
    sleep_ms(1000);
    // spi_write_blocking(SPI_PORT, &cmd12, 6); // stop read...

       /*
    if( (response & 0x1F) != 0x05) { //invalid response
        print_num(response);
        spi_write_blocking(SPI_PORT, &cmd12, 6); // stop read...
        return;
    }
    print_num(response);
    spi_read_blocking(SPI_PORT, 0xFF, dst, num_blocks*512); // I think data packets are 512 bytes
    spi_write_blocking(SPI_PORT, &cmd12, 6); // stop read...
    return;
    
        */




}

void sd_write_block(uint32_t add, uint8_t *src) {
    // turn address into arg
    char cmd[6] = {CMD24, add>>24, add>>16, add>>8, add, 0x00};
    // get crc7
    char crc = (crc7(&cmd, 5) << 1 ) +1; //!** NOT SURE ABOUT WHETHER CMD NEEDS TO BE &CMD...
    cmd[5] = crc;

    uint8_t response = 0xFF;
    CS_des();
    sleep_ms(10);
    CS_sel();
    spi_write_blocking(SPI_PORT, &cmd, 6);
}

 







