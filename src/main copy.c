#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/adc.h"
#include "pico/multicore.h"
#include "pico/bootrom.h"


#include "ssd1306.h"
#include "image.h"
#include "BMSPA_font.h"

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


#include "hardware/spi.h"
#define SPI_PORT spi0
#define PIN_MISO 16
#define PIN_CS   17
#define PIN_SCK  18
#define PIN_MOSI 19

// Initialize SPI and GPIO pins for SD card
void SDspi_init() {
    // Initialize SPI port at 400kHz for SD card initialization
    spi_init(SPI_PORT, 400 * 1000);

    // Set SPI pin functions
    gpio_set_function(PIN_MISO, GPIO_FUNC_SPI);
    gpio_set_function(PIN_SCK, GPIO_FUNC_SPI);
    gpio_set_function(PIN_MOSI, GPIO_FUNC_SPI);

    // Initialize CS pin
    gpio_init(PIN_CS);
    gpio_set_dir(PIN_CS, GPIO_OUT);
    gpio_put(PIN_CS, 1);  // Deselect SD card
}
// Send a single byte over SPI and receive a single byte
uint8_t spi_transfer(uint8_t data) {
    uint8_t received;
    spi_write_read_blocking(SPI_PORT, &data, &received, 1);
    return received;
}
uint8_t sd_read_byte() {
    return spi_transfer(0xFF);
}

// Send command to SD card
uint8_t sd_send_command(uint8_t cmd, uint32_t arg) {
    uint8_t response;

    // Select the SD card by setting CS low
    gpio_put(PIN_CS, 1);

    // Send command
    spi_transfer(cmd | 0x40);  // Command frame
    spi_transfer((arg >> 24) & 0xFF);  // Argument byte 1
    spi_transfer((arg >> 16) & 0xFF);  // Argument byte 2
    spi_transfer((arg >> 8) & 0xFF);   // Argument byte 3
    spi_transfer(arg & 0xFF);          // Argument byte 4
    spi_transfer(0x95);  // CRC for CMD0 with argument 0

    // Wait for response (timeout after 8 bytes)
    for (int i = 0; i < 8; i++) {
        response = spi_transfer(0xFF);
        if (response != 0xFF) {
            break;
        }
    }

    // Deselect the SD card by setting CS high
    gpio_put(PIN_CS, 0);

    return response;
}

// Initialize SD card
int sd_initialize() {
    // Set SPI clock to 400kHz for initialization
    spi_set_baudrate(SPI_PORT, 400 * 1000);

    // Send at least 74 clock cycles with CS high
    gpio_put(PIN_CS, 1);
    for (int i = 0; i < 50; i++) {
        spi_transfer(0x00);
    }

    // Send CMD0 to reset the card and enter SPI mode
    uint8_t CMD0_response = sd_send_command(0, 0);
    if (CMD0_response != 0x01) {
        return CMD0_response;  // Initialization failed
    }

    // Send CMD8 to check voltage range (required for SDHC/SDXC cards)
    /*
    uint8_t response = sd_send_command(8, 0x1AA);
    if (response == 0x01) {
        // Card supports CMD8, proceed with initialization
        // Read and discard the next four bytes (response to CMD8)
        for (int i = 0; i < 4; i++) {
            sd_read_byte();
        }
    } else if (response != 0x05) {
        return -2;  // CMD8 not supported, might be an older card
    }
     */

    // Send CMD55 and ACMD41 until the card is ready
    while (sd_send_command(41, 0) > 1 || sd_send_command(55, 0) > 1) {
        sleep_ms(1);
    }

    // Set SPI clock to maximum frequency (25MHz)
    spi_set_baudrate(SPI_PORT, 25 * 1000 * 1000);

    return 0;  // Initialization successful
}

// Read a single byte from SD card


// Write a single byte to SD card
void sd_write_byte(uint8_t data) {
    spi_transfer(data);
}

// Read a block of data from SD card
int sd_read_block(uint32_t block_addr, uint8_t *buffer) {
    int ret = -1;
    // Send CMD17 to read a single block
    if ((ret = sd_send_command(17, block_addr) )!= 0x00) {
        return ret;  // Error reading block
    }

    // Wait for start block token (0xFE)
    while (sd_read_byte() != 0xFE);

    // Read 512 bytes of data
    for (int i = 0; i < 512; i++) {
        buffer[i] = sd_read_byte();
    }

    // Ignore CRC bytes
    sd_read_byte();
    sd_read_byte();
    return 0;
}

// Write a block of data to SD card
void sd_write_block(uint32_t block_addr, const uint8_t *buffer) {
    // Send CMD24 to write a single block
    if (sd_send_command(24, block_addr) != 0x00) {
        return;  // Error writing block
    }

    // Send start block token (0xFE)
    sd_write_byte(0xFE);

    // Write 512 bytes of data
    for (int i = 0; i < 512; i++) {
        sd_write_byte(buffer[i]);
    }

    // Send dummy CRC bytes
    sd_write_byte(0xFF);
    sd_write_byte(0xFF);

    // Wait for data response token
    while (sd_read_byte() != 0xE5);
}






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

void graphics_engine();
void mux_engine();

int mux_channel = 0;


void second_core_code() {
    
    
    SDspi_init();
    um();
    int sd_init_success = sd_initialize();
    
    disp_init();
    for(int i = 0; i<100; i++) {
        sleep_ms(50);
        ssd1306_clear(&disp);
        display_number(2,i);
        display_number(3,sd_init_success);
        ssd1306_show(&disp);
    }
    return;
    
}



int main() {
    
    stdio_init_all();
    setup_gpios();
    um();
    
    gpio_init(RESET_PIN);
    gpio_pull_down(RESET_PIN);
    um();
    
    
    
    multicore_launch_core1(second_core_code);
    
    //multicore_fifo_push_blocking(1);
    while( reset_pin_check() == false ) {
        continue;
    }
    
    
    int reset_counter = 0;
    while(1) {
        //reset check
        if(sio_hw->gpio_in & (1<<RESET_PIN)){
            ++reset_counter;
            sleep_ms(10);
        }
        if(reset_counter == 100) {
            bootloader_animation();
            reset_usb_boot(0,0);
            return 0;
        }
        
    }

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
    sleep_ms(1000);
    ssd1306_clear(&disp);
    ssd1306_invert(&disp,1);
    //closing animation for screen
    for(int y=0; y<SSD1306_CENTER_Y; y++) {
        ssd1306_draw_line(&disp, 0, y, SSD1306_WIDTH, y);
        ssd1306_draw_line(&disp, 0, SSD1306_HEIGHT-y, SSD1306_WIDTH, SSD1306_HEIGHT-y);
        ssd1306_show(&disp);
    }
    ssd1306_clear(&disp);
        //turn screen off
    ssd1306_poweroff(&disp);
    ssd1306_deinit(&disp);
}




