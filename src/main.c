#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/adc.h"
#include "pico/multicore.h"
#include "pico/bootrom.h"
#include "hardware/spi.h"

#include "ssd1306.h"
#include "image.h"
#include "BMSPA_font.h"

#include "sd_driver/crc.c"
//#include "sd_driver/sd_card.c"
//#include "sd_driver/sd_spi.c"
//#include "sd_driver/spi.c"

const uint8_t num_chars_per_disp[]={7,7,7,5};
const uint8_t *fonts[1]= {BMSPA_font};
ssd1306_t disp;


uint32_t analog_in;

// PIN # DEFS
#define RESET_PIN           0
#define ON_PIN              1
#define SCREEN_POWER_PIN    4
#define ADC_INPUT_PIN       26
#define MUX_PIN_S0 12

// SCREEN PARAMS
#define SSD1306_HEIGHT      64
#define SSD1306_WIDTH       128
#define SSD1306_CENTER_X    64
#define SSD1306_CENTER_Y    32

#define CONFINE_X 0x7F
#define CONFINE_Y 0x3F

// TEXT PARAMS
#define DISPLAY_LEFT_MARGIN 5
#define ROW_HEIGHT 8

// OTHER PARAMS
#define SLEEPTIME           10

void disp_init() {
    disp.external_vcc=false;
    ssd1306_init(&disp, SSD1306_WIDTH, SSD1306_HEIGHT, 0x3C, i2c1);
    ssd1306_poweron(&disp);
    ssd1306_clear(&disp);
}

void setup_gpios(void);
void bootup_animation(void);
void display_ADC(void);
void display_text(int row, const char *s) {
    int x = DISPLAY_LEFT_MARGIN;
    int y = row*ROW_HEIGHT;
    ssd1306_draw_string(&disp, x, y, 1, s);
}
void display_adc_value(int row, int channel) {
    adc_select_input(channel);
    uint32_t value = adc_read();
    char buf[20];
    sprintf(buf, "%d, 0x%X", value, value);
    display_text(row, buf);
}
void display_number(int row, int number) {
    char buf[20];
    sprintf(buf, "%d", number);
    display_text(row, buf);
}
void display_byte(int row, uint8_t number) {
    char buf[20];
    sprintf(buf, "%d", number);
    display_text(row, buf);
}
void display_hex(int row, int val) {
    char buf[20];
    sprintf(buf, "0x%X", val);
    display_text(row, buf);
}
int reset_pin_check() {
    return (sio_hw->gpio_in & (1<<RESET_PIN) != 0);
}

void bootloader_animation();


void um() {sleep_ms(50);}

void print_num(int num) {
    multicore_fifo_push_blocking('N');
    multicore_fifo_push_blocking(num);
}
void print_hex(int num) {
    multicore_fifo_push_blocking('H');
    multicore_fifo_push_blocking(num);
}
void print_hex_pair(int num1, int num2) {
    multicore_fifo_push_blocking('P');
    multicore_fifo_push_blocking(num2);
    multicore_fifo_push_blocking(num1);
}
void print_bytes(int len, uint8_t *ptr) { // max is 6...
    multicore_fifo_push_blocking('B');
    multicore_fifo_push_blocking(len);
    multicore_fifo_push_blocking(ptr);
}
void print_bytes_squish(int len, uint8_t *ptr) { // max is ??
    multicore_fifo_push_blocking('D');
    multicore_fifo_push_blocking(len);
    multicore_fifo_push_blocking(ptr);
}


#define NUMLINES 8
#define MESSAGESIZE 20
void second_core_code() {
    int j;
    char messages[NUMLINES][MESSAGESIZE]; //messages are looped-thru using mod operator
    for(int i = 0; i<NUMLINES; i++) {
        for( j =0; j<MESSAGESIZE-1; j++) {
            messages[i][j] = '0';
        }
        messages[i][j] = '\0';
    }
    uint16_t line_counter = 0;
    
    int alt = 0;
    int fifo_receive;
    int fifo_receive2;
    uint8_t *fifo_ptr;
    
    disp_init();

    while(1) {
        
        for(int i = 0; i<100 && multicore_fifo_rvalid() == 0 && reset_pin_check() == 0; i++) sleep_ms(10); //wait for a sec or for fifo push
        if(reset_pin_check()) return;
        
        if(multicore_fifo_rvalid()) {
            line_counter++; //next line
            fifo_receive = multicore_fifo_pop_blocking();
            if(fifo_receive == 'N') { // print a number
                fifo_receive = multicore_fifo_pop_blocking();
                sprintf(messages[line_counter%NUMLINES],"%d",fifo_receive);
            }
            else if(fifo_receive == 'H') { // print a hex
                fifo_receive = multicore_fifo_pop_blocking();
                sprintf(messages[line_counter%NUMLINES],"%X",fifo_receive);
            }
            else if(fifo_receive == 'P') { // print a hex
                fifo_receive2 = multicore_fifo_pop_blocking();
                fifo_receive = multicore_fifo_pop_blocking();
                sprintf(messages[line_counter%NUMLINES],"%X  %X",fifo_receive, fifo_receive2);
            }
            else if(fifo_receive == 'B') {
                fifo_receive = multicore_fifo_pop_blocking(); // number of bytes. Usually 6...
                fifo_ptr = multicore_fifo_pop_blocking(); // ptr
                for(int j = 0; j< fifo_receive && (j+1)*2 < MESSAGESIZE; j++) {
                    sprintf(&messages[line_counter%NUMLINES][j*3], " %02X", *(fifo_ptr+j));
                }
            }
            else if(fifo_receive == 'D') {
                fifo_receive = multicore_fifo_pop_blocking(); // number of bytes. Usually 6...
                fifo_ptr = multicore_fifo_pop_blocking(); // ptr
                for(int j = 0; j< fifo_receive && (j+1)*2 < MESSAGESIZE; j++) {
                    sprintf(&messages[line_counter%NUMLINES][j*3], "%02X", *(fifo_ptr+j));
                }
            }
        }
        
        ssd1306_clear(&disp);
        for(int i = 0; i < NUMLINES; i++) {
            display_text(i, messages[(line_counter + i + 1)%NUMLINES]); //show each message on ascending lines
        }
                      
        //if((alt = 1 - alt) == 1)
        //ssd1306_draw_square(&disp,120,100,5,5); //blinky squarey
        //else ssd1306_clear_square(&disp,100,100,5,5);
        
        ssd1306_show(&disp);
        
    }
}

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

#define AUTO_RESET true

// Initialize SPI and GPIO pins for SD card
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
                    //bits:  47-40      39-32   31-24   23-16   15-8    7-0
uint8_t cmd0_arg0[6] =      {CMD0,      00,     00,     00,     00,     0x95};
uint8_t cmd1_arg40[6] =     {CMD1,      0x40,     00,     00,     00,     0x95};
uint8_t cmd8_arg1AA[6] =    {CMD8,      00,     00,     0x01,   0xAA,   0x87};
uint8_t cmd55_arg0[6] =     {CMD55,     00,     00,     00,     00,     0x65};
uint8_t cmd58_arg0[6] =     {CMD55,     00,     00,     00,     00,     0xFD};
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
uint8_t acmd41_first[6] =   {ACMD41,    0x40,   0x10,     0x00,   0x00,     0xCD};
uint8_t acmd41_inquiry[6] = {ACMD41,    0x40,   00,     0x00,   0x00,     0x77};
// I'm not sure what the voltage arg should be. I will try specifying one voltage, and then I will try specifying all voltages if that doesn't work...
                        // set bits 23-15 0xFF 0x10     0xFF,   0x10
uint8_t acmd41_firstv2[6] = {ACMD41,    0x40,   00,     0xFF,   0x10,     0x7D}; //0x17 for 0x50, 0x77 for 0x40
//uint8_t cmd1_arg0[6] =      {CMD1,      00,     00,     00,     00,     0x6B};

uint8_t sd_busy = 0xFF;

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


//uint8_t sd_response[16];
void sd_cmd(uint8_t *cmd[6], uint8_t *ret_dst) {
    const int response_length = 48 / 8; //48 bits is R1 and R3 response types
    CS_sel(); //assert CS pin low to align bytes and assert SPI mode w/ CMD0
    spi_write_blocking(SPI_PORT, cmd, 6);
    spi_read_blocking(SPI_PORT, 0xFF, ret_dst, response_length);
    CS_des();
}
/*
void sd_write_block(const uint8_t *buffer, uint32_t length, uint8_t *ret_dst) {
    uint16_t crc = crc16((void *)buffer, length);
    uint8_t token = SPI_START_BLOCK;
    uint8_t cmd = CMD24;
    uint8_t response = 0xFF;
    
    spi_write_blocking(SPI_PORT, &cmd, 1);
    spi_write_blocking(SPI_PORT, &token, 1);
    spi_write_blocking(SPI_PORT, buffer, length);
    spi_read_blocking(SPI_PORT, 0xFF, ret_dst, 6);
}
 */

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

int cursor(int step) {
    static int i = 0;
    i += step;
    return i - step;
    //print_num(i);
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
            sleep_ms(1000);
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
        sleep_ms(500);
    }
    
    sleep_ms(100);
}


//!** OK LET'S FIGURE OUT HOW TO READ THIS STUFF...
#define CMD25_START_TOKEN 0xFC
#define CMD25_STOP_TOKEN 0xFD
#define READ_START_TOKEN 0xFE
// https://onlinedocs.microchip.com/pr/GUID-F9FE1ABC-D4DD-4988-87CE-2AFD74DEA334-en-US-3/index.html?GUID-48879CB2-9C60-4279-8B98-E17C499B12AF
//
// CMD18 is READ_MULTIPLE_BLOCK
// CMD18 arg: 32-bit address
// CMD18 keeps reading until CMD12 (STOP_TRANSMISSION)
void sd_read_blocks(uint32_t add, uint8_t *dst, uint32_t num_blocks) {
    // turn address into arg
    uint8_t cmd[6] = {CMD18, add>>24, add>>16, add>>8, add, 0x00};
    // get crc7
    char crc = (crc7(&cmd, 5) << 1 ) +1; //!** NOT SURE ABOUT WHETHER CMD NEEDS TO BE &CMD...
    cmd[5] = crc;
    
    uint8_t cmd12[6] = {CMD12, 0, 0, 0, 0, 0x00};
    crc = (crc7(&cmd12,5) << 1) + 1;
    cmd12[5] = crc;
    
    uint8_t response = 0xFF;
    
    CS_des();       //!** this may be unneccesary and even counterproductive but it might align the bytes correctly...
    sleep_ms(10);
    CS_sel();
    spi_write_blocking(SPI_PORT, cmd, 6);
    spi_read_blocking(SPI_PORT, 0xFF, &response, 1);
    if( (response & 0x1F) != 0x05) { //invalid response
        print_num(response);
        spi_write_blocking(SPI_PORT, cmd12, 6); // stop read...
        return;
    }
    spi_read_blocking(SPI_PORT, 0xFF, dst, num_blocks*512); // I think data packets are 512 bytes
    spi_write_blocking(SPI_PORT, cmd12, 6); // stop read...
    return;
    
}

void idle_until_reset() {
    int counter;
    while(1) {
        if( reset_pin_check()) {
            counter++;
            sleep_ms(5);
        }
        if(counter>100) break;
    }
}

int main() {
    int i, j = 0;
    stdio_init_all();
    setup_gpios();
    gpio_init(RESET_PIN);
    gpio_pull_down(RESET_PIN);
    multicore_launch_core1(second_core_code);
    
    
    
    sleep_ms(500); //give the card time to power up...
    
    /****BUSY WAIT**/
    //for(i=0;i<10;i++) spi_write_blocking(SPI_PORT, &sd_busy, 1);
    
    uint8_t mess[6] = {0xDE,0xAD,0xBE,0xEF,0x01,0x23};
    
    uint8_t data_buffer[1536]; // make the data buffer
    for(i = 0 ; i<1536; i++) data_buffer[i] = 0; // populate it with zeros
    
    sd_init(&data_buffer[0]);
    sleep_ms(1000);
    
    
    for(i = 0 ; i<1536; i++) data_buffer[i] = 0; // populate it with zeros
    
    sd_read_blocks(0x00400000, &data_buffer, 1 );
    
    for(i=0; i<512; i+=8) print_bytes_squish(8,&data_buffer[i]);
    
    sleep_ms(3000);
    //idle_until_reset();
    bootloader_animation();
    reset_usb_boot(0,0);
    return 0;
    
}


void setup_gpios(void) {
    i2c_init(i2c1, 400000);
    gpio_init(1);
    gpio_set_dir(1, GPIO_OUT);
    sio_hw->gpio_out |= 0x1<<1;
    gpio_set_function(2, GPIO_FUNC_I2C);
    gpio_set_function(3, GPIO_FUNC_I2C);
    gpio_pull_up(2);
    gpio_pull_up(3);
    
    gpio_init(SCREEN_POWER_PIN);
    gpio_set_dir(SCREEN_POWER_PIN, GPIO_OUT);
    
    sleep_ms(1000);
    sio_hw->gpio_out |= (1<<SCREEN_POWER_PIN);
}


void bootloader_animation() {
    // RESET SEQUENCE...
    multicore_reset_core1();
    um();
    
    ssd1306_deinit(&disp);
    um();
    
    disp.external_vcc=false;
    ssd1306_init(&disp, SSD1306_WIDTH, SSD1306_HEIGHT, 0x3C, i2c1);
    ssd1306_poweron(&disp);
    ssd1306_clear(&disp);
    sleep_ms(1000);
    
        //display reset message
    const char *reset_message[]= {"REBOOT", "TO", "USB"};
    
    for(int i=0;i<3;i++) {
        display_text(i+2, reset_message[i]);
        ssd1306_show(&disp);
    }
    sleep_ms(500);
    ssd1306_clear(&disp);
    ssd1306_invert(&disp,1);
    //closing animation for screen
    for(int y=0; y<SSD1306_CENTER_Y; y++) {
        ssd1306_draw_line(&disp, 0, 2*y, SSD1306_WIDTH, y);
        ssd1306_draw_line(&disp, 0, SSD1306_HEIGHT-2*y+1, SSD1306_WIDTH, SSD1306_HEIGHT-y);
        ssd1306_show(&disp);
    }
    ssd1306_clear(&disp);
        //turn screen off
    ssd1306_poweroff(&disp);
    ssd1306_deinit(&disp);
}




