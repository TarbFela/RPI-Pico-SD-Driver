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

#include "sd_driver/crc.h"

#include "sd_maxim.h"
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





#define AUTO_RESET true

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
                    sprintf(&messages[line_counter%NUMLINES][j*2], "%02X", *(fifo_ptr+j));
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







// Initialize SPI and GPIO pins for SD card



//uint8_t sd_response[16];

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



int cursor(int step) {
    static int i = 0;
    i += step;
    return i - step;
    //print_num(i);
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
    sd_read_blocks(0x00000040, &data_buffer, 1 );
    sleep_ms(1000);
    for(i=0; i<512; i+=8) print_bytes_squish(8,&data_buffer[i]);
    sleep_ms(1000);

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




